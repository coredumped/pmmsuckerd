//
//  SilentMode.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 12/4/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#include "SilentMode.h"
#include "MTLogger.h"
#include "Mutex.h"
#include "GenericException.h"
#include "UtilityFunctions.h"
#include <iostream>
#include <sqlite3.h>
#ifndef DEFAULT_SILENT_MODE_DATAFILE
#define DEFAULT_SILENT_MODE_DATAFILE "silent.db"
#endif
#ifndef DEFAULT_SILENT_MODE_LOOKUP_TABLE
#define DEFAULT_SILENT_MODE_LOOKUP_TABLE "silent_settings"
#endif

namespace pmm {
	static sqlite3 *silentDB = NULL;
	//static Mutex sM;
	static const char *silentModeDatafile = DEFAULT_SILENT_MODE_DATAFILE;
	static const char *silentModeTable = DEFAULT_SILENT_MODE_LOOKUP_TABLE;
	
	void SilentMode::initialize(){
		char *errmsg_s = NULL;
		pmm::Log << "Initializing SilentMode handler..." << pmm::NL;
		int errcode = sqlite3_open(silentModeDatafile, &silentDB);
		if (errcode != SQLITE_OK) {
			throw GenericException(sqlite3_errmsg(silentDB));
		}
		//Verify that all required tables exist
		if (!pmm::tableExists(silentDB, silentModeTable)) {
			//Create table here
			std::stringstream createCmd;
			std::stringstream errmsg;
			createCmd << "CREATE TABLE " << silentModeTable << " (";
			createCmd << "email TEXT primary key,";
			createCmd << "startHour INTEGER,";
			createCmd << "startMinute INTEGER,";
			createCmd << "endHour INTEGER,";
			createCmd << "endMinute INTEGER)";
			errcode = sqlite3_exec(silentDB, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if(errcode != SQLITE_OK){
				errmsg << "Unable to create silent mode table: " << errmsg_s << ". Create command was: " << createCmd.str();
				pmm::Log << errmsg.str() << pmm::NL;
				throw GenericException(errmsg.str());
			}
		}
	}
	
	void SilentMode::set(const std::string &email, int sHour, int sMinute, int eHour, int eMinute){
		if(silentDB == NULL) initialize();
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s = NULL;
		sqlCmd << "INSERT OR REPLACE INTO " << silentModeTable << " (email,startHour,startMinute,endHour,endMinute) VALUES ('" << email << "', " << sHour << ", " << sMinute << ", " << eHour << ", " << eMinute << ")";
		int errcode = sqlite3_exec(silentDB, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to insert entry in silent mode table: " << errmsg_s << ". Insert command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}
	}
	
	void SilentMode::clear(const std::string &email){
		if(silentDB == NULL) initialize();		
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s = NULL;
		sqlCmd << "DELETE FROM " << silentModeTable << " WHERE email='" << email << "'";
		int errcode = sqlite3_exec(silentDB, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to remove entry from silent mode table: " << errmsg_s << ". Delete command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}
	}
	
	bool SilentMode::isInEffect(const std::string &email, struct tm *theTime){
		bool ret = false;
		if(silentDB == NULL) initialize();
		std::stringstream sqlCmd, errmsg;
		char *sztail = NULL;
		sqlite3_stmt *statement;
		sqlCmd << "SELECT startHour,startMinute,endHour,endMinute FROM  " << silentModeTable << " WHERE email='" << email << "'";
		int errCode = sqlite3_prepare_v2(silentDB, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(silentDB);
#ifdef DEBUG
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			int refTime = theTime->tm_hour * 60 + theTime->tm_min;
			int startHour = sqlite3_column_int(statement, 0);
			int startMinute = sqlite3_column_int(statement, 1);
			int endHour = sqlite3_column_int(statement, 2);
			int endMinute = sqlite3_column_int(statement, 3);
			int sTime = startHour * 60 + startMinute;
			int eTime = endHour * 60 + endMinute;
			if (refTime >= sTime && refTime <= eTime) {
				ret = true;
			}
		}
		sqlite3_finalize(statement);
		return ret;
	}

}