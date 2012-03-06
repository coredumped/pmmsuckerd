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
#include "MTLogger.h"
#include <string>
#include <vector>
#include <queue>
#include <map>

namespace pmm {
	
	template<class T>
	class SharedQueue {
	private:
		Mutex m;
		std::queue<T>queueData;
	protected:
	public:
		SharedQueue(){ }
		virtual ~SharedQueue(){ }
		
		/*void add(T &entry){
		 m.lock();
		 try {
		 queueData.push(entry);
		 } catch (...) {
		 m.unlock();
		 throw;
		 }
		 m.unlock();
		 }*/
		
		void add(const T &entry){
			m.lock();
			try {
				queueData.push(entry);
				if(queueData.size() % 100) pmm::Log << "There is a queue with " << (int)queueData.size() << " pending elements, this is completely abnormal!!!" << pmm::NL;
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
				val = queueData.front();
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
			return T(val);			
		}
		
		bool extractEntry(T &val){
			bool got_entry = false;;
			m.lock();
			try{
				if (queueData.size() != 0) {
					val = queueData.front();
					queueData.pop();
					got_entry = true;
				}
			}
			catch(...){
				m.unlock();
				throw;
			}
			m.unlock();
			return got_entry;
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
