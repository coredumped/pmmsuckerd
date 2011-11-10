//
//  FetchedMailsCache.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/9/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#include "FetchedMailsCache.h"
#include "Mutex.h"
#include <iostream>
#include <sstream>
#include <string.h>
#ifndef DEFAULT_DATA_FILE
#define DEFAULT_DATA_FILE "fetchedmails.db"
#endif

#ifndef DEFAULT_FETCHED_MAILS_TABLE_NAME
#define DEFAULT_FETCHED_MAILS_TABLE_NAME "fetched_mails"
#endif


namespace pmm {
	MTLogger CacheLog;
	
	static const char *fetchedMailsTable = DEFAULT_FETCHED_MAILS_TABLE_NAME;
	
	sqlite3 *FetchedMailsCache::openDatabase(){
		if (sqlite3_threadsafe() && dbConn != 0) {
			return dbConn;
		}
#ifdef DEBUG
		CacheLog << "DEBUG: Opening database" << pmm::NL;
#endif
		int errCode = sqlite3_open(datafile.c_str(), &dbConn);
		if (errCode != SQLITE_OK) {
			throw GenericException(sqlite3_errmsg(dbConn));
		}
		return dbConn;
	}
	
	void FetchedMailsCache::closeDatabase(sqlite3 *db){
		if (sqlite3_threadsafe() == 0) {
#ifdef DEBUG
			CacheLog << "DEBUG: Closing database" << pmm::NL;
#endif
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
	
	void FetchedMailsCache::addEntry(const std::string &email, uint32_t uid){
		std::stringstream input;
		input << uid;
		addEntry(email, input.str());
	}
	
	void FetchedMailsCache::addEntry(const std::string &email, const std::string &uid){
		sqlite3 *conn = openDatabase();
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "INSERT INTO " << fetchedMailsTable << " (timestamp,email,uniqueid) VALUES (" << time(NULL);
		sqlCmd << ",'" << email << "',";
		sqlCmd << "'" << uid << "' )";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			if (errCode == SQLITE_CONSTRAINT) {
				CacheLog << "Duplicate entry in database: " << email << "(" << uid << ") resetting database..." << pmm::NL;
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
		closeDatabase(conn);
	}
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
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		closeDatabase(conn);
		return ret;
	}
	
	bool FetchedMailsCache::entryExists(const std::string &email, const std::string &uid){
		std::stringstream input(uid);
		int uid_v;
		input >> uid_v ;
		return entryExists(email, uid_v);
	}
	
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
#ifdef DEBUG
		CacheLog << "Verifying if multiple POP3 entries exists in our database: " << sqlCmd.str() << pmm::NL;
#endif
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
			if (sqlite3_column_int(statement, 0) == msgCount) {
				ret = true;
			}
			else {
				//Determine if there are any new entries
			}
		}
		if(errCode != SQLITE_DONE){
			std::stringstream errmsg;
			errmsg << "Unable to verify if a list of entries exists from query: " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		closeDatabase(conn);
		return ret;		
	}
	
	void FetchedMailsCache::expireOldEntries(){
#warning TODO: Implement a way to expire old e-mail entries plus a trigger to activate it
	}
	
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

	void FetchedMailsCache::verifyTables(){
		sqlite3 *conn = openDatabase();
		std::string sqlCmd = "SELECT name FROM sqlite_master";
		sqlite3_stmt *statement;
		std::stringstream errmsg;
		char *sztail, *errmsg_s;
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.c_str(), (int)sqlCmd.size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			errmsg << "Unable to execute query " << sqlCmd << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		bool gotFetchedMailsTable = false;
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			char *tableName = (char *)sqlite3_column_text(statement, 0);
			if (strcmp(tableName, fetchedMailsTable) == 0) gotFetchedMailsTable = true;
		}
		if(errCode != SQLITE_DONE){
			errmsg << "Unable to retrieve values from query: " << sqlCmd << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
#ifdef DEBUG
			CacheLog << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		if(!gotFetchedMailsTable){
			std::stringstream createCmd;
			createCmd << "CREATE TABLE " << fetchedMailsTable << " (";
			createCmd << "timestamp INTEGER,";
			createCmd << "email     TEXT,";
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
			createCmd << "CREATE INDEX ftchEmail_IDX ON "  << fetchedMailsTable << " (email)";
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
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
				CacheLog << errmsg.str() << pmm::NL;
#endif
				throw GenericException(errmsg.str());
			}
			createCmd.str(std::string());
			createCmd << "CREATE UNIQUE INDEX ftchEmailUID_IDX ON "  << fetchedMailsTable << " (email,uniqueid)";
			errCode = sqlite3_exec(conn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
#ifdef DEBUG
				CacheLog << errmsg.str() << pmm::NL;
#endif
				throw GenericException(errmsg.str());
			}
		}
		closeDatabase(conn);
	}
}
