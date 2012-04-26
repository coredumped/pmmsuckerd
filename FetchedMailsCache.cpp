//
//  FetchedMailsCache.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/9/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#include "FetchedMailsCache.h"
#include "Mutex.h"
#include "UtilityFunctions.h"
#include <iostream>
#include <sstream>
#include <map>
#include <string.h>
#ifdef __linux__
#include <stdint.h>
#endif
#include <sys/stat.h>
#ifndef DEFAULT_DATA_FILE
#define DEFAULT_DATA_FILE "fetchedmails.db"
#endif

#ifndef DEFAULT_FETCHED_MAILS_TABLE_NAME
#define DEFAULT_FETCHED_MAILS_TABLE_NAME "fetched_mails"
#endif

#ifndef DEFAULT_DBCONN_MAX_OPEN_TIME
#define DEFAULT_DBCONN_MAX_OPEN_TIME 600
#endif


namespace pmm {
	MTLogger CacheLog;
	static const char *fetchedMailsTable = DEFAULT_FETCHED_MAILS_TABLE_NAME;
	static Mutex fM;
	
	class UniqueDBDescriptor {
	protected:
		int maxOpenTime;
	public:
		sqlite3 *conn;
		time_t openedOn;
		UniqueDBDescriptor(){
			conn = NULL;
			openedOn = time(0);
			maxOpenTime = DEFAULT_DBCONN_MAX_OPEN_TIME;
		}
		UniqueDBDescriptor(const UniqueDBDescriptor &u){
			conn = u.conn;
			openedOn = u.openedOn;
			maxOpenTime = u.maxOpenTime;
		}
		void closeConn(){
			if(conn != NULL) sqlite3_close(conn);
		}
		void autoRefresh(){
			if(time(0) - openedOn >= maxOpenTime){
				closeConn();
			}
		}
	};
	
	static std::map<std::string, UniqueDBDescriptor> uConnMap;
	
	static void createDirIfNotExists(const std::string &theDir){
		struct stat st;
		if (stat(theDir.c_str(), &st) != 0) {
			std::vector<std::string> subPath;
			splitString(subPath, theDir, "/");
			std::stringstream currentPath;
			for (size_t i = 0; i < subPath.size(); i++) {
				if(i == 0) currentPath << subPath[i];
				else currentPath << "/" << subPath[i];
				if (currentPath.str().size() > 0) {
					mkdir(currentPath.str().c_str(), 0755);
					chmod(currentPath.str().c_str(), 0755);
				}
			}
		}
	}
	
	static bool getDataFile(std::string &dbFile, const std::string &email){
		std::stringstream dbf, basedir;
		//Build directory hash here
		basedir << "fetchdb/";
		basedir << email[0] << "/";
		basedir << email[1] << "/";
		basedir << email[2];
		createDirIfNotExists(basedir.str());
		dbf << basedir.str() << "/";
		//Prepare database file build here
		dbf << std::hex;
		dbf.width(2);
		dbf.fill('0');
		for (int i = 0; i < email.size(); i++) {
			if(i == 0) dbf << ((uint32_t)email[i] & 0x000000FF);
			else dbf << "-" << ((uint32_t)email[i] & 0x000000FF);
		}
		dbf << ".db";
		dbFile = dbf.str();
		struct stat st;
		if(stat(dbFile.c_str(), &st) != 0) return true;
		return false;
	}
	
	static sqlite3 *getUConnection(const std::string &email){
		sqlite3 *theConn = NULL;
		fM.lock();
		try {
			if (uConnMap.find(email) != uConnMap.end()) {
				theConn = uConnMap[email].conn;
			}
		} 
		catch (...) {
			CacheLog << "Unable to get connection for user: " << email << " something weird happened while querying the connection datamap" << pmm::NL;
		}
		fM.unlock();
		return theConn;
	}
	
	static void setUConnection(const std::string &email, sqlite3 *conn){
		fM.lock();
		uConnMap[email].conn = conn;
		uConnMap[email].openedOn = time(0);
		fM.unlock();
	}
	
	static void autoRefreshUConn(const std::string &email){
		fM.lock();
		uConnMap[email].autoRefresh();
		fM.unlock();
	}
	
	sqlite3 *FetchedMailsCache::openDatabase(){
		if (sqlite3_threadsafe() && dbConn != 0) {
			return dbConn;
		}
		int errCode = sqlite3_open_v2(datafile.c_str(), &dbConn, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, NULL);
		if (errCode != SQLITE_OK) {
			throw GenericException(sqlite3_errmsg(dbConn));
		}
		return dbConn;
	}
	
	static Mutex migrM;
	
	sqlite3 *FetchedMailsCache::openDatabase(const std::string &email){
		sqlite3 *theConn;
		theConn = getUConnection(email);
		if (sqlite3_threadsafe() && theConn != 0) {
			return theConn;
		}
		int errCode;
		std::string dbf;
		bool createTable = getDataFile(dbf, email);
		//errCode = sqlite3_open(dbf.c_str(), &theConn);
		errCode = sqlite3_open_v2(dbf.c_str(), &theConn, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, NULL);
		if (errCode != SQLITE_OK) {
			throw GenericException(sqlite3_errmsg(theConn));
		}
		setUConnection(email, theConn);
		if(createTable){
			verifyTables(theConn, email);
			//Migrate missing data from older database
			struct stat st;
			if (stat(datafile.c_str(), &st) == 0) {
				migrM.lock();
				sqlite3 *old_db = openDatabase();
				std::stringstream sqlCmd;
				sqlite3_stmt *statement;
				
				char *sztail;
				sqlCmd << "SELECT uniqueid,timestamp FROM " << fetchedMailsTable << " WHERE email='" << email << "'";		
				errCode = sqlite3_prepare_v2(old_db, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
				if (errCode != SQLITE_OK) {
					std::stringstream errmsg;
					errmsg << "Unable to migrate data from old database: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(old_db);
					closeDatabase(old_db);
#ifdef DEBUG
					CacheLog << errmsg.str() << pmm::NL;
#endif
					migrM.unlock();
					throw GenericException(errmsg.str());
				}
				bool logged = false;
				char *errmsg_s = 0;
				time_t t1 = time(0);
				while (sqlite3_step(statement) == SQLITE_ROW) {
					if (!logged) {
						logged = true;
						CacheLog << "Migrating data (" << email << ") from old database..." << pmm::NL;
					}
					sqlCmd.str(std::string());
					sqlCmd << "INSERT OR REPLACE INTO " << fetchedMailsTable << " (uniqueid,timestamp) VALUES (";
					sqlCmd << "'" << sqlite3_column_text(statement, 0) << "',";
					sqlCmd << "'" << sqlite3_column_text(statement, 1) << "')";
					errCode = sqlite3_exec(theConn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
					if (errCode != SQLITE_OK) {
						std::stringstream errmsg;
						errmsg << "Unable to migrate data from old database: " << sqlCmd.str() << " due to: " << errmsg_s;
						closeDatabase(old_db);
#ifdef DEBUG
						CacheLog << errmsg.str() << pmm::NL;
#endif
						migrM.unlock();
						throw GenericException(errmsg.str());
					}
					
				}
				CacheLog << "Migration of " << email << " took " << (int)(time(0) - t1) << " seconds" << pmm::NL;
				sqlite3_finalize(statement);
				//Erase old entries
				sqlCmd.str(std::string());
				sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE email='" << email << "'";
				errCode = sqlite3_exec(old_db, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
				if (errCode != SQLITE_OK) {
					std::stringstream errmsg;
					errmsg << "Unable to remove old data from old database: " << sqlCmd.str() << " due to: " << errmsg_s;
					closeDatabase(old_db);
#ifdef DEBUG
					CacheLog << errmsg.str() << pmm::NL;
#endif
					migrM.unlock();
					throw GenericException(errmsg.str());
				}			
				closeDatabase(old_db);
				migrM.unlock();
			}
		}
		return theConn;		
	}
	
	void FetchedMailsCache::closeDatabase(sqlite3 *db){
		if (sqlite3_threadsafe() == 0) {
			sqlite3_close(db);
		}
	}
	
	FetchedMailsCache::FetchedMailsCache(){
		datafile = DEFAULT_DATA_FILE;
		dbConn = 0;
		verifyTables();
	}
	
	FetchedMailsCache::FetchedMailsCache(const std::string &_datafile){
		datafile = _datafile;
		dbConn = 0;
		verifyTables();
	}
	
	FetchedMailsCache::~FetchedMailsCache(){
		if (dbConn != 0 && sqlite3_threadsafe()) {
			sqlite3_close(dbConn);
		}
	}
	
#ifdef OLD_CACHE_INTERFACE
	void FetchedMailsCache::addEntry(const std::string &email, uint32_t uid){
		std::stringstream input;
		input << uid;
		addEntry(email, input.str());
	}
	
	void FetchedMailsCache::addEntry(const std::string &email, const std::string &uid){
#ifdef ENABLE_MULTIPLE_TABLES
		sqlite3 *conn = openDatabase(email);
#else
		sqlite3 *conn = openDatabase();
#endif
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "INSERT INTO " << fetchedMailsTable << " (timestamp,email,uniqueid) VALUES (" << time(NULL);
		sqlCmd << ",'" << email << "',";
		sqlCmd << "'" << uid << "' )";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			if (errCode == SQLITE_CONSTRAINT) {
				CacheLog << "Duplicate entry in database found: " << email << "(" << uid << ") resetting database..." << pmm::NL;
				closeDatabase(conn);
				dbConn = 0;
			}
			else{
				closeDatabase(conn);
				std::stringstream errmsg;
				errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
				CacheLog << errmsg.str() << pmm::NL;
#endif
				throw GenericException(errmsg.str());
			}
		}
#ifdef ENABLE_MULTIPLE_TABLES
		autoRefreshUConn(email);
#else
		closeDatabase(conn);
#endif
	}
#endif
	
	void FetchedMailsCache::addEntry2(const std::string &email, const std::string &uid){
		sqlite3 *conn = openDatabase(email);
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "INSERT OR REPLACE INTO " << fetchedMailsTable << " (timestamp,uniqueid) VALUES (" << time(0) << ",'" << uid << "' )";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			sqlite3_close(conn);
			setUConnection(email, 0);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		autoRefreshUConn(email);
	}
	void FetchedMailsCache::addEntry2(const std::string &email, uint32_t &uid){
		sqlite3 *conn = openDatabase(email);
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "INSERT OR REPLACE INTO " << fetchedMailsTable << " (timestamp,uniqueid) VALUES (" << time(0) << "," << uid << ")";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			sqlite3_close(conn);
			setUConnection(email, 0);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		autoRefreshUConn(email);		
	}
#ifdef OLD_CACHE_INTERFACE
	bool FetchedMailsCache::entryExists(const std::string &email, uint32_t uid){
		bool ret = false;
		sqlite3 *conn = openDatabase();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;

		char *sztail;
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE email='" << email << "' AND uniqueid='" << (int)uid << "'";		
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			if (sqlite3_column_int(statement, 0) > 0) {
				ret = true;
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify that an entry(" << email << "," << uid << ") exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			sqlite3_finalize(statement);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		closeDatabase(conn);
		return ret;
	}
	
	bool FetchedMailsCache::entryExists(const std::string &email, const std::string &uid){
		bool ret = false;
		sqlite3 *conn = openDatabase();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		
		char *sztail;
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE email='" << email << "' AND uniqueid='" << uid << "'";		
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			if (sqlite3_column_int(statement, 0) > 0) {
				ret = true;
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify that an entry(" << email << "," << uid << ") exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			sqlite3_finalize(statement);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		closeDatabase(conn);
		return ret;
	}
#endif
	
	bool FetchedMailsCache::entryExists2(const std::string &email, const std::string &uid){
		bool ret = false;
		sqlite3 *conn = openDatabase(email);
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		
		char *sztail;
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE uniqueid='" << uid << "'";		
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
			setUConnection(email, 0);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			if (sqlite3_column_int(statement, 0) > 0) {
				ret = true;
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify that an entry(" << email << "," << uid << ") exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			sqlite3_finalize(statement);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		autoRefreshUConn(email);
		return ret;
	}
	
	bool FetchedMailsCache::entryExists2(const std::string &email, uint32_t uid){
		bool ret = false;
		sqlite3 *conn = openDatabase(email);
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		
		char *sztail;
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE uniqueid=" << uid;		
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
			setUConnection(email, 0);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			if (sqlite3_column_int(statement, 0) > 0) {
				ret = true;
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify that an entry(" << email << "," << uid << ") exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			sqlite3_finalize(statement);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		autoRefreshUConn(email);
		return ret;
	}	
	
#ifdef OLD_CACHE_INTERFACE
	bool FetchedMailsCache::hasAllThesePOP3Entries(const std::string &email, carray *msgList){
		bool ret = false;
		sqlite3 *conn = openDatabase();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		char *sztail;
		int msgCount = carray_count(msgList);
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE email='" << email << "' AND uniqueid IN (";
		for (int i = 0; i < msgCount; i++) {
			struct mailpop3_msg_info *msg_info = (struct mailpop3_msg_info *)carray_get(msgList, i);
			if(i == 0) sqlCmd << "'" << msg_info->msg_uidl << "'";
			else sqlCmd << ", '" << msg_info->msg_uidl << "'";
		}
		sqlCmd << ")";
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			int countval = sqlite3_column_int(statement, 0);
#ifdef DEBUG
			CacheLog << "Verifying if multiple POP3 entries exists in our database: " << sqlCmd.str() << " given=" << msgCount << " db has=" << countval << pmm::NL;
#endif
			if (countval == msgCount) {
				ret = true;
			}
			else {
				//Determine if there are any new entries
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify if a list of entries exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			sqlite3_finalize(statement);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		closeDatabase(conn);
		return ret;		
	}
#endif
	
	bool FetchedMailsCache::hasAllTheseEntries(const std::string &email, carray *msgList){
		bool ret = false;
		sqlite3 *conn = openDatabase(email);
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		char *sztail;
		int msgCount = carray_count(msgList);
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE uniqueid IN (";
		for (int i = 0; i < msgCount; i++) {
			struct mailpop3_msg_info *msg_info = (struct mailpop3_msg_info *)carray_get(msgList, i);
			if(i == 0) sqlCmd << "'" << msg_info->msg_uidl << "'";
			else sqlCmd << ", '" << msg_info->msg_uidl << "'";
		}
		sqlCmd << ")";
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			int countval = sqlite3_column_int(statement, 0);
#ifdef DEBUG
			CacheLog << "Verifying if multiple POP3 entries exists in our database: " << sqlCmd.str() << " given=" << msgCount << " db has=" << countval << pmm::NL;
#endif
			if (countval == msgCount) {
				ret = true;
			}
			else {
				//Determine if there are any new entries
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify if a list of entries exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			sqlite3_finalize(statement);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		autoRefreshUConn(email);
		return ret;		
	}
	
#ifdef OLD_CACHE_INTERFACE
	void FetchedMailsCache::expireOldEntries(){
#warning TODO: Implement a way to expire old e-mail entries plus a trigger to activate it
		sqlite3 *conn = openDatabase();
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE timestamp < " << (int)time(NULL) - 5184000;
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		closeDatabase(conn);
	}
#endif
	
	void FetchedMailsCache::expireOldEntries(const std::string &email){
#warning TODO: Implement a way to expire old e-mail entries plus a trigger to activate it
		sqlite3 *conn = openDatabase(email);
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE timestamp < " << (int)time(NULL) - 5184000;
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		autoRefreshUConn(email);
	}
	
#ifdef OLD_CACHE_INTERFACE
	void FetchedMailsCache::removeMultipleEntries(const std::string &email, const std::vector<uint32_t> &uidList){
		if(uidList.size() == 0) return;
		sqlite3 *conn = openDatabase();
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE email='" << email << "' AND uniqueid IN (";
		for (size_t i = 0; i < uidList.size(); i++) {
			if (i == 0) sqlCmd << (int)uidList[i];
			else sqlCmd << "," << (int)uidList[i];
		}
		sqlCmd << ")";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				std::stringstream errmsg;
				errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
				CacheLog << errmsg.str() << pmm::NL;
#endif
				throw GenericException(errmsg.str());
		}
		closeDatabase(conn);
	}
	void FetchedMailsCache::removeEntriesNotInSet(const std::string &email, const std::vector<uint32_t> &uidSet){
		if(uidSet.size() == 0) return;
		sqlite3 *conn = openDatabase();
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE email='" << email << "' AND uniqueid NOT IN (";
		for (size_t i = 0; i < uidSet.size(); i++) {
			if (i == 0) sqlCmd << (int)uidSet[i];
			else sqlCmd << "," << (int)uidSet[i];
		}
		sqlCmd << ")";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		closeDatabase(conn);		
	}
	
	void FetchedMailsCache::removeAllEntriesOfEmail(const std::string &email){
		sqlite3 *conn = openDatabase();
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE email='" << email << "'";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		closeDatabase(conn);
	}
#endif
	
	void FetchedMailsCache::removeMultipleEntries2(const std::string &email, const std::vector<uint32_t> &uidList){
		if(uidList.size() == 0) return;
		sqlite3 *conn = openDatabase(email);
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE uniqueid IN (";
		for (size_t i = 0; i < uidList.size(); i++) {
			if (i == 0) sqlCmd << (int)uidList[i];
			else sqlCmd << "," << (int)uidList[i];
		}
		sqlCmd << ")";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		autoRefreshUConn(email);
	}
	void FetchedMailsCache::removeEntriesNotInSet2(const std::string &email, const std::vector<uint32_t> &uidSet){
		if(uidSet.size() == 0) return;
		sqlite3 *conn = openDatabase(email);
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable << " WHERE uniqueid NOT IN (";
		for (size_t i = 0; i < uidSet.size(); i++) {
			if (i == 0) sqlCmd << (int)uidSet[i];
			else sqlCmd << "," << (int)uidSet[i];
		}
		sqlCmd << ")";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		autoRefreshUConn(email);
	}
	
	void FetchedMailsCache::removeAllEntriesOfEmail2(const std::string &email){
		sqlite3 *conn = openDatabase(email);
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "DELETE FROM " << fetchedMailsTable;
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		autoRefreshUConn(email);
	}

	
	void FetchedMailsCache::verifyTables(){
		sqlite3 *conn = openDatabase();
		closeDatabase(conn);
	}

	void FetchedMailsCache::verifyTables(sqlite3 *conn, const std::string &email){
		if(!tableExists(conn, fetchedMailsTable)){
			std::stringstream errmsg;
			char *errmsg_s;
			int errCode;
			std::stringstream createCmd;
			createCmd << "CREATE TABLE " << fetchedMailsTable << " (";
			createCmd << "timestamp INTEGER,";
			createCmd << "uniqueid  TEXT)";
			errCode = sqlite3_exec(conn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
				CacheLog << errmsg.str() << pmm::NL;
#endif
				throw GenericException(errmsg.str());
			}
			createCmd.str(std::string());
			createCmd << "CREATE INDEX ftchTstampl_IDX ON "  << fetchedMailsTable << " (timestamp)";
			errCode = sqlite3_exec(conn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				setUConnection(email, 0);
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
				CacheLog << errmsg.str() << pmm::NL;
#endif
				throw GenericException(errmsg.str());
			}
		}
	}
}
