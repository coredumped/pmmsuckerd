//
//  LocalConfig.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 1/22/13.
//
//

#ifndef __PMM_Sucker__ObjectDatastore__
#define __PMM_Sucker__ObjectDatastore__
#include "RWLock.h"
#include <iostream>

namespace pmm {
	class ObjectDatastore {
	private:
		RWLock masterLock;
		std::string datafile;
		void *dbConn;
		void *connect2DB();
		void closeDB();
		void initializeDB();
	public:
		ObjectDatastore();
		
		void put(const std::string &key, const std::string &value);
		void put(const std::string &key, const std::string &value, const std::string &nsp);
		
		bool get(std::string &value, const std::string &key);
		bool get(std::string &value, const std::string &key, const std::string &nsp);
	};
}

#endif /* defined(__PMM_Sucker__ObjectDatastore__) */
