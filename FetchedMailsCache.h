//
//  FetchedMailsCache.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/9/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_FetchedMailsCache_h
#define PMM_Sucker_FetchedMailsCache_h
#include "GenericException.h"
#include "Mutex.h"
#include <sqlite3.h>
#include <string>

namespace pmm {
	class FetchedMailsCache {
	private:
		std::string datafile;
		sqlite3 *dbConn;
		void verifyTables();
	protected:
		sqlite3 *openDatabase();
		void closeDatabase(sqlite3 *db);
	public:
		FetchedMailsCache();
		FetchedMailsCache(const std::string &_datafile);
		~FetchedMailsCache();
		void addEntry(const std::string &email, const std::string &uid);
		void addEntry(const std::string &email, uint32_t uid);
		
		bool entryExists(const std::string &email, const std::string &uid);
		bool entryExists(const std::string &email, uint32_t uid);
		
		void expireOldEntries();
	};
}


#endif
