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
#include "RWLock.h"
#include "MTLogger.h"

namespace pmm {
	template <class T>
	class SharedSet {
		std::set<T> theSet;
#ifdef USE_RWLOCK
		RWLock m;
#else
		Mutex m;
#endif
	public:
		std::string name;
		SharedSet(){}
		
		void insert(const T &v){
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
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
#ifdef USE_RWLOCK
			m.readLock();
#else
			m.lock();
#endif
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
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
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
