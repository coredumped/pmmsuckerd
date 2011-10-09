//
//  SharedQueue.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_SharedQueue_h
#define PMM_Sucker_SharedQueue_h
#include "Mutex.h"
#include <string>
#include <vector>
#include <map>

namespace pmm {
	
	template<class T>
	class SharedQueue {
	private:
		Mutex m;
		std::vector<T>queueData;
	protected:
	public:
		SharedQueue(){ }
		virtual ~SharedQueue(){ }
		
		void add(T &entry){
			m.lock();
			try {
				queueData.push_back(entry);
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		void add(const T &entry){
			m.lock();
			try {
				queueData.push_back(entry);
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		T peek(){
			T val;
			m.lock();
			try {
				val = queueData[0];
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
			return T(val);			
		}
		
		void extractEntry(T &val){
			m.lock();
			try{
				val = queueData[0];
				queueData.erase(queueData.begin());
			}
			catch(...){
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		size_t size(){
			size_t s = 0;
			m.lock();
			try {
				s = queueData.size();
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
			return s;
		}
	};
}


#endif
