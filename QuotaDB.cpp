//
//  QuotaDB.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 1/5/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include "QuotaDB.h"
#include "UtilityFunctions.h"
#include "MTLogger.h"
#include "GenericException.h"

#ifndef DEFAULT_QUOTA_DB_TABLE
#define DEFAULT_QUOTA_DB_TABLE "quotainfo"
#endif
#ifndef DEFAULT_QUOTA_DB_DATAFILE
#define DEFAULT_QUOTA_DB_DATAFILE "quota.db"
#endif

namespace pmm {
	
	static bool _initialized = false;
	static const char *quota_table = DEFAULT_QUOTA_DB_TABLE;
	static const char *quota_datafile = DEFAULT_QUOTA_DB_DATAFILE;
	static sqlite3 *_uniqueConn = NULL;
	static time_t __lastOpen = 0, __lastCheck = 0;
	static Mutex __connM;
	
	static sqlite3 *_connect2QuotaDB(){
		sqlite3 *dbConn = NULL;
		__connM.lock();
		if (__lastOpen == 0) __lastOpen = time(0);
		else if(__lastOpen != __lastCheck && _uniqueConn != NULL && __lastOpen % 300 == 0){
			sqlite3_close(_uniqueConn);
			_uniqueConn = NULL;
			__lastCheck = __lastOpen;
		}
		if (_uniqueConn == NULL) {
			int errcode = sqlite3_open_v2(quota_datafile, &dbConn, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, NULL);
			//int errcode = sqlite3_open(quota_datafile, &dbConn);
			if (errcode != SQLITE_OK) {
				__connM.unlock();
				throw GenericException(sqlite3_errmsg(dbConn));
			}
			_uniqueConn = dbConn;
		}
		else dbConn = _uniqueConn;
		__connM.unlock();
		return dbConn;
	}
	
	static int _remainingQuotaGet(const std::string &emailAccount, sqlite3 *dbConn){
		int quotaval = 0;
		std::stringstream sqlCmd, errmsg;
		sqlite3_stmt *statement;
		char *sztail = NULL;
		sqlCmd << "SELECT quota FROM " << quota_table << " WHERE email='" << emailAccount << "'";
		int errCode = sqlite3_prepare_v2(dbConn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			std::stringstream errmsg;
			errmsg << "Unable to execute query " << sqlCmd.str() << " due to: " << sqlite3_errmsg(dbConn);
#ifdef DEBUG
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			quotaval = sqlite3_column_int(statement, 0);
		}
		sqlite3_finalize(statement);	
		return quotaval;

	}
	
	void QuotaDB::init(sqlite3 *conn){
		sqlite3 *dbConn = conn;
		if(conn == NULL) dbConn = _connect2QuotaDB();
		if(!pmm::tableExists(dbConn, quota_table)){
			std::stringstream createCmd, errmsg;
			char *errmsg_s;
			createCmd << "CREATE TABLE " << quota_table << " (";
			createCmd << "email TEXT primary key,";
			createCmd << "quota INTEGER DEFAULT 0)";
			int errcode = sqlite3_exec(dbConn, createCmd.str().c_str(), 0, 0, &errmsg_s);
			if(errcode != SQLITE_OK){
				errmsg << "Unable to create quota info table: " << errmsg_s << ". Create command was: " << createCmd.str();
				pmm::Log << errmsg.str() << pmm::NL;
				throw GenericException(errmsg.str());
			}
		}
		//if(conn == NULL) sqlite3_close(dbConn);
		_initialized = true;
	}
	
	
	bool QuotaDB::decrease(const std::string &emailAccount){
		int quotaval = 0;
		sqlite3 *dbConn = _connect2QuotaDB();
		if (!_initialized) init(dbConn);
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s;
		sqlCmd << "UPDATE " << quota_table << " SET quota=quota-1 WHERE email='" << emailAccount << "'";
		int errcode = sqlite3_exec(dbConn, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to decrease quota: " << errmsg_s << ". Insert command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}	
		//Retrieve remaing quota
		quotaval = _remainingQuotaGet(emailAccount, dbConn);
		//sqlite3_close(dbConn);		
		if(quotaval <= 0) return false;
		return true;
	}
	
	void QuotaDB::set(const std::string &emailAccount, int value, sqlite3 *conn){
		sqlite3 *dbConn = conn;
		if(conn == NULL) dbConn = _connect2QuotaDB();
		if (!_initialized) init(dbConn);
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s;
		sqlCmd << "INSERT OR REPLACE INTO " << quota_table << " (email, quota) values ('" << emailAccount << "', " << value << ")";
		int errcode = sqlite3_exec(dbConn, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to set quota for user: " << errmsg_s << ". Insert command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}	
		//if(conn == NULL) sqlite3_close(dbConn);
	}
	
	void QuotaDB::increase(const std::string &emailAccount, int value){
		sqlite3 *dbConn = _connect2QuotaDB();
		if (!_initialized) init(dbConn);
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s;
		sqlCmd << "UPDATE " << quota_table << " SET quota=quota + " << value << " WHERE email='" << emailAccount << "'";
		int errcode = sqlite3_exec(dbConn, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to increase quota: " << errmsg_s << ". Insert command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}	
		//sqlite3_close(dbConn);				
	}
	
	void QuotaDB::removeAccount(const std::string &emailAccount) {
		sqlite3 *dbConn = _connect2QuotaDB();
		if (!_initialized) init(dbConn);
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s;
		sqlCmd << "DELETE FROM " << quota_table << " WHERE email='" << emailAccount << "'";
		int errcode = sqlite3_exec(dbConn, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to remove quota info: " << errmsg_s << ". Insert command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}			
	}
	
	void QuotaDB::clearData(){
		sqlite3 *dbConn = _connect2QuotaDB();
		if (!_initialized) init(dbConn);
		std::stringstream sqlCmd, errmsg;
		char *errmsg_s;
		sqlCmd << "DELETE FROM " << quota_table;
		int errcode = sqlite3_exec(dbConn, sqlCmd.str().c_str(), 0, 0, &errmsg_s);
		if(errcode != SQLITE_OK){
			errmsg << "Unable to clear quota data: " << errmsg_s << ". Delete command was: " << sqlCmd.str();
			pmm::Log << errmsg.str() << pmm::NL;
			throw GenericException(errmsg.str());			
		}	
		//sqlite3_close(dbConn);						
	}
	
	int QuotaDB::remaning(const std::string &emailAccount){
		int retval;
		sqlite3 *dbConn = _connect2QuotaDB();
		retval = _remainingQuotaGet(emailAccount, dbConn);
		//sqlite3_close(dbConn);
		return retval;
	}
	
	bool QuotaDB::have(const std::string &emailAccount){
		if (remaning(emailAccount) > 0) {
			return true;
		}
		return false;
	}
}
