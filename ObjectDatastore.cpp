//
//  ObjectDatastore.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 1/22/13.
//
//

#include "ObjectDatastore.h"
#include "Atomic.h"
#include "MTLogger.h"
#include "UtilityFunctions.h"
#include <sstream>
#include <sqlite3.h>
#ifndef DEFAULT_NAMESPACE_STRING
#define DEFAULT_NAMESPACE_STRING "000"
#endif

#ifndef DEFAULT_ObjectDatastore_DATAFILE
#define DEFAULT_LOCALCONFIG_DATAFILE "obj-datastore.db"
#endif
#ifndef DEFAULT_OBJECT_TABLENAME
#define DEFAULT_OBJECT_TABLENAME "objstore"
#endif

static const std::string __default_config_namespace = DEFAULT_NAMESPACE_STRING;
static const std::string __default_obj_table = DEFAULT_OBJECT_TABLENAME;
static pmm::AtomicFlag _initialized;

namespace pmm {
	
	void *ObjectDatastore::connect2DB() {
		sqlite3 *qDB;
		if(sqlite3_open_v2(datafile.c_str(), &qDB, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK){
			pmm::Log << "Unable to open " << datafile << " datafile: " << sqlite3_errmsg(qDB) << pmm::NL;
			throw GenericException(sqlite3_errmsg(qDB));
		}
		dbConn = (void *)qDB;
		return qDB;
	}
	
	void ObjectDatastore::closeDB() {
		if(dbConn != NULL) sqlite3_close((sqlite3 *)dbConn);
		dbConn = NULL;
	}
	
	void ObjectDatastore::initializeDB() {
		connect2DB();
		if (!pmm::tableExists((sqlite3 *)dbConn, __default_obj_table)) {
			std::stringstream createCmd;
			char *errmsg;
			createCmd << "CREATE TABLE " << __default_obj_table << " (";
			createCmd << "namespace	TEXT DEFAULT '" << __default_config_namespace << "',";
			createCmd << "key		TEXT,";
			createCmd << "value		TEXT";
			createCmd << ")";
			if(sqlite3_exec((sqlite3 *)dbConn, createCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK){
				pmm::Log << "Unable to object datastore: " << errmsg << pmm::NL;
				throw GenericException(errmsg);
			}
			//Create indexes
			createCmd.str(std::string());
			createCmd << "CREATE UNIQUE INDEX objkey_indx ON " << __default_obj_table << " (namespace,key) ";
			if(sqlite3_exec((sqlite3 *)dbConn, createCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK){
				pmm::Log << "Unable to create object datastore index: " << errmsg << pmm::NL;
				throw GenericException(errmsg);
			}
		}
	}
	
	ObjectDatastore::ObjectDatastore() {
		datafile = DEFAULT_LOCALCONFIG_DATAFILE;
		dbConn = NULL;
		initializeDB();
	}
	
	
	void ObjectDatastore::put(const std::string &key, const std::string &value) {
		put(key, value, __default_config_namespace);
	}
	
	void ObjectDatastore::put(const std::string &key, const std::string &value, const std::string &nsp) {
		masterLock.writeLock();
		char *errmsg;
		std::stringstream sqlCmd;
		sqlCmd << "INSERT OR REPLACE INTO " << __default_obj_table << " (namespace, key, value) VALUES ('" << nsp << "', '" << key << "', '" << value << "')";
		if (sqlite3_exec((sqlite3 *)dbConn, sqlCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK) {
			pmm::Log << "Unable to put value '" << value << "' for key(" << nsp << "," << key << "), using: " << sqlCmd.str() << ": " << errmsg << pmm::NL;
		}
		masterLock.unlock();
	}
	
	bool ObjectDatastore::get(std::string &value, const std::string &key) {
		return get(value, key, __default_config_namespace);
	}
	
	bool ObjectDatastore::get(std::string &value, const std::string &key, const std::string &nsp) {
		bool gotData = false;
		masterLock.readLock();
		value = "";
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		char *szTail;
		sqlCmd << "SELECT value FROM " << __default_obj_table << " where namespace='" << nsp << "' and key='" << key << "'";
		if(sqlite3_prepare_v2((sqlite3 *)dbConn, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&szTail) == SQLITE_OK){
			if (sqlite3_step(statement) == SQLITE_ROW) {
				value = (const char *)sqlite3_column_text(statement, 0);
				gotData = true;
			}
			sqlite3_finalize(statement);
		}
		else {
			const char *errmsg = sqlite3_errmsg((sqlite3 *)dbConn);
			pmm::Log << "Unable to retrieve to retrieve object(" << nsp << "," << key << ") value using " << sqlCmd.str() << " due to: " << errmsg << pmm::NL;
		}
		masterLock.unlock();
		return gotData;
	}
}