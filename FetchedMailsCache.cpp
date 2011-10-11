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
#ifdef DEBUG
	extern Mutex mout;
#endif
	
	static const char *fetchedMailsTable = DEFAULT_FETCHED_MAILS_TABLE_NAME;
	
	sqlite3 *FetchedMailsCache::openDatabase(){
		if (sqlite3_threadsafe() && dbConn != 0) {
			return dbConn;
		}
		int errCode = sqlite3_open(datafile.c_str(), &dbConn);
		if (errCode != SQLITE_OK) {
			throw GenericException(sqlite3_errmsg(dbConn));
		}
		return dbConn;
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
	
	void FetchedMailsCache::addEntry(const std::string &email, uint32_t uid){
		std::stringstream input;
		input << uid;
		addEntry(email, input.str());
	}
	
	void FetchedMailsCache::addEntry(const std::string &email, const std::string &uid){
		sqlite3 *conn = openDatabase();
		char *errmsg_s;
		std::stringstream sqlCmd;
		sqlCmd << "INSERT INTO " << fetchedMailsTable << " (timestamp,email,uniqueid) VALUES (" << time(NULL) << ",'" << email << "'," << "'" << uid << "')";
		int errCode = sqlite3_exec(conn, sqlCmd.str().c_str(), NULL, NULL, &errmsg_s);
		if (errCode != SQLITE_OK) {
			closeDatabase(conn);
			std::stringstream errmsg;
			errmsg << "Unable to execute command: " << sqlCmd.str() << " due to: " << errmsg_s;
			throw GenericException(errmsg.str());
		}
		closeDatabase(conn);
	}
	
	bool FetchedMailsCache::entryExists(const std::string &email, const std::string &uid){
		bool ret = false;
		sqlite3 *conn = openDatabase();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		std::stringstream errmsg;
		char *sztail;
		sqlCmd << "SELECT count(1) FROM " << fetchedMailsTable << " WHERE email='" << email << "' AND uniqueid='" << uid << "'";		
		int errCode = sqlite3_prepare_v2(conn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			if (sqlite3_column_int(statement, 0) > 0) {
				ret = true;
			}
		}
		if(errCode != SQLITE_DONE){
			errmsg << "Unable to verify that an entry(" << email << "," << uid << ") exists from query: " << sqlCmd << " due to: " << sqlite3_errmsg(conn);
			closeDatabase(conn);
			throw GenericException(errmsg.str());			
		}
		closeDatabase(conn);
		return ret;
	}
	
	bool FetchedMailsCache::entryExists(const std::string &email, uint32_t uid){
		std::stringstream input;
		input << uid;
		return entryExists(email, input.str());
	}
	
	void FetchedMailsCache::expireOldEntries(){
#warning TODO: Implement a way to expire old e-mail entries plus a trigger to activate it
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
				throw GenericException(errmsg.str());
			}
			createCmd.str(std::string());
			createCmd << "CREATE INDEX ftchEmail_IDX ON "  << fetchedMailsTable << " (email)";
			errCode = sqlite3_exec(conn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
				throw GenericException(errmsg.str());
			}
			createCmd.str(std::string());
			createCmd << "CREATE INDEX ftchTstampl_IDX ON "  << fetchedMailsTable << " (timestamp)";
			errCode = sqlite3_exec(conn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
				throw GenericException(errmsg.str());
			}
			createCmd.str(std::string());
			createCmd << "CREATE UNIQUE INDEX ftchEmailUID_IDX ON "  << fetchedMailsTable << " (email,uniqueid)";
			errCode = sqlite3_exec(conn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if (errCode != SQLITE_OK) {
				closeDatabase(conn);
				errmsg << "Unable to execute command: " << createCmd.str() << " due to: " << errmsg_s;
				throw GenericException(errmsg.str());
			}
		}
		closeDatabase(conn);
	}
}
