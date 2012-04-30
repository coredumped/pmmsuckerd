//
//  UserPreferences.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 2/20/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "UserPreferences.h"
#include "MTLogger.h"
#include "UtilityFunctions.h"
#include "GenericException.h"

#ifndef DEFAULT_PREF_DB_DATAFILE
#define DEFAULT_PREF_DB_DATAFILE "preferences.db"
#endif

#ifndef DEFAULT_PREF_TABLE_NAME
#define DEFAULT_PREF_TABLE_NAME "preferences"
#endif

#ifndef DEFAULT_PREF_UPDATE_INTERVAL
#define DEFAULT_PREF_UPDATE_INTERVAL 30
#endif
#ifndef DEFAULT_PREF_DOMAIN
#define DEFAULT_PREF_DOMAIN "user"
#endif

namespace pmm {
	
	static sqlite3 *__prefDB = NULL;
	static const char *prefDBDatafile = DEFAULT_PREF_DB_DATAFILE;
	static const char *prefTable = DEFAULT_PREF_TABLE_NAME;
	static int defaultPrefUpdateInterval = DEFAULT_PREF_UPDATE_INTERVAL;
	static const char *defaultPrefDomain = DEFAULT_PREF_DOMAIN;
	
	static const char *alertToneKey = "defaultAlertTone";
	
	static sqlite3 *connect2PrefDB(){
		if (__prefDB == NULL) {
			if(sqlite3_open(prefDBDatafile, &__prefDB) != SQLITE_OK){
				pmm::Log << "Unable to open " << prefDBDatafile << " datafile: " << sqlite3_errmsg(__prefDB) << pmm::NL;
				throw GenericException(sqlite3_errmsg(__prefDB));
			}
			//Create preferences table if it does not exists
			if(!pmm::tableExists(__prefDB, prefTable)){
				std::stringstream createCmd;
				char *errmsg;
				createCmd << "CREATE TABLE " << prefTable << " (";
				createCmd << "email	TEXT PRIMARY KEY,";
				createCmd << "domain TEXT DEFAULT '" << defaultPrefDomain << "',";
				createCmd << "settingKey TEXT NOT NULL,";
				createCmd << "settingValue TEXT DEFAULT ''";
				createCmd << ")";
				if(sqlite3_exec(__prefDB, createCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK){
					pmm::Log << "Unable to create preferences table: " << errmsg << pmm::NL;
					throw GenericException(errmsg);
				}
			}
		}
		return __prefDB;
	}
	
	PreferenceEngine::PreferenceEngine(){
		db = NULL;
	}
	
	PreferenceEngine::~PreferenceEngine(){
		sqlite3_close(db);
	}
	
	void PreferenceEngine::defaultAlertTone(std::string &alertTonePath, const std::string &emailAccount){
		sqlite3 *dbConn = connect2PrefDB();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		char *szTail;
		sqlCmd << "SELECT settingValue FROM " << prefTable << " WHERE email='" << emailAccount << "' and settingKey='" << alertToneKey << "'";
		if(sqlite3_prepare_v2(dbConn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&szTail) == SQLITE_OK){
			if (sqlite3_step(statement) == SQLITE_ROW) {
				alertTonePath = (char *)sqlite3_column_text(statement, 0);
			}
			sqlite3_finalize(statement);
		}
		else {
			const char *errmsg = sqlite3_errmsg(dbConn);
			pmm::Log << "Unable to retrieve default alert tone for " << emailAccount << ": " << errmsg << " using " << sqlCmd.str() << pmm::NL;
			throw GenericException(sqlite3_errmsg(dbConn));
		}
		if (alertTonePath.size() == 0) {
			alertTonePath = "pmm.caf";
		}
	}
	
	void PreferenceEngine::userPreferenceGet(std::map<std::string, std::string> &preferenceMap, const std::string &emailAccount, const std::string &domain){
		db = connect2PrefDB();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		char *szTail;
		sqlCmd << "SELECT settingKey,settingValue FROM " << prefTable << " WHERE email='" << emailAccount << "' and domain='" << domain << "'";
		if(sqlite3_prepare_v2(db, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&szTail) == SQLITE_OK){
			while (sqlite3_step(statement) == SQLITE_ROW) {
				const char *key = (const char *)sqlite3_column_text(statement, 0);
				const char *value = (const char *)sqlite3_column_text(statement, 1);
				preferenceMap[key] = value;
			}
			sqlite3_finalize(statement);
		}
		else {
			const char *errmsg = sqlite3_errmsg(db);
			pmm::Log << "Unable to retrieve user preferences for " << emailAccount << ": " << errmsg << " using " << sqlCmd.str() << pmm::NL;
			throw GenericException(sqlite3_errmsg(db));
		}
	}
	
	void PreferenceEngine::destroyPreferences(const std::string &emailAccount){
		db = connect2PrefDB();
		std::stringstream sqlCmd;
		char *errmsg;
		sqlCmd << "DELETE FROM " << prefTable << " WHERE email='" << emailAccount << "'";
		if (sqlite3_exec(db, sqlCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK) {
			pmm::Log << "CRITICAL: Unable to remove preferences of user: " << emailAccount << errmsg << pmm::NL;
		}
	}
	
	void PreferenceEngine::operator()(){
		db = connect2PrefDB();
#warning TODO: Remember to load emailAccount preferences from appengine
		//Load preferences from appengine here
		while (true) {
			sleep(defaultPrefUpdateInterval);
			PreferenceQueueItem item;
			while (preferenceQueue->extractEntry(item)) {
				//Update entries in queue
				char *errmsg;
				std::stringstream sqlCmd;
				if(item.settingDomain.size() == 0) item.settingDomain = defaultPrefDomain;
				sqlCmd << "INSERT OR REPLACE INTO " << prefTable << " (email,domain,settingKey,settingValue) values ('" << item.emailAccount << "', '" << item.settingDomain << "', '" << item.settingKey << "', '"<< item.settingValue << "')";
				if (sqlite3_exec(db, sqlCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK) {
					preferenceQueue->add(item);
					pmm::Log << "Unable to update preference(" << item.emailAccount << " " << item.settingKey << "=" << item.settingValue << ") in database: " << errmsg << pmm::NL;
					usleep(500000);
				}
			}
		}
	}
}