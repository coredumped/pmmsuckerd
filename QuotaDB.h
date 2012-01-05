//
//  QuotaDB.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 1/5/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_QuotaDB_h
#define PMM_Sucker_QuotaDB_h
#include<string>
#include <sqlite3.h>
#include "DataTypes.h"

namespace pmm {
	class QuotaDB {
	protected:
		static void init(sqlite3 *conn = NULL);
	public:
		static bool decrease(const std::string &emailAccount);
		static void set(const std::string &emailAccount, int value, sqlite3 *conn = NULL);
		static void increase(const std::string &emailAccount, int value = 1);
		static int remaning(const std::string &emailAccount);
		static bool have(const std::string &emailAccount);
		static void clearData();
	};
}



#endif
