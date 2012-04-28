//
//  SharedSet.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 4/27/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_SharedSet_h
#define PMM_Sucker_SharedSet_h
#include <set>
#include <string>
#include "Mutex.h"
#include "MTLogger.h"

namespace pmm {
	template <class T>
	class SharedSet {
		std::set<T> theSet;
		Mutex m;
	public:
		std::string name;
		SharedSet(){}
		
		void insert(const T &v){
			m.lock();
			try {
				theSet.insert(v);
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		bool contains(const T &v){
			bool ret = false;
			m.lock();
			try {
				if(theSet.find(v) != theSet.end()) ret = true;
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();		
			return ret;
		}
		
		void erase(const T &v){
			m.lock();
			try {
				theSet.erase(v);
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
	};
}

#endif
