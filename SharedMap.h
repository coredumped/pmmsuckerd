//
//  SharedMap.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 8/18/12.
//
//

#ifndef PMM_Sucker_SharedMap_h
#define PMM_Sucker_SharedMap_h
#include "RWLock.h"
#include "Mutex.h"

namespace pmm {
	
	template<class X, class Y>
	class SharedMap : protected RWLock {
	private:
		std::map<X, Y> theMap;
	protected:
	public:
		SharedMap(){
			
		}
		
		SharedMap(const SharedMap &s){
			s.readLock();
			std::map<X, Y> m = s.theMap;
			s.unlock();
			writeLock();
			theMap = m;
			unlock();
		}
		
		void copy(const std::map<X, Y> &m){
			writeLock();
			theMap = m;
			unlock();
		}
		
		void copyTo(std::map<X, Y> &m){
			readLock();
			m = theMap;
			unlock();
		}
		
		void put(const X &key, const Y &value){
			writeLock();
			theMap[key] = value;
			unlock();
		}
		
		Y &get(const X &key){
			Y val;
			readLock();
			val = theMap[key];
			unlock();
			return Y(val);
		}
		
		void get(const X &key, Y &val){
			readLock();
			val = theMap[key];
			unlock();
		}
		
		Y &operator[](const X &key){
			return this->get(key);
		}
		
		bool containsKey(const X &key){
			bool ret = true;
			readLock();
			if(theMap.find(key) == theMap.end()) ret = false;
			unlock();
			return ret;
		}
		
		void getAllKeys(std::vector<X> &theKeys){
			readLock();
			std::map<X, Y>::iterator iter;
			for (iter = theMap.begin(); iter != theMap.end(); iter++) {
				theKeys.push_back(iter->first);
			}
			unlock();
		}
	};
}


#endif
