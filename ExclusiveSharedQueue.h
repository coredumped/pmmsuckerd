//
//  ExclusiveSharedQueue.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 4/19/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_ExclusiveSharedQueue_h
#define PMM_Sucker_ExclusiveSharedQueue_h

#include "Mutex.h"
#include "MTLogger.h"
#include <string>
#include <vector>
#include <queue>
#include <map>
#include<set>

namespace pmm {
	
	template<class T>
	class ExclusiveSharedQueue {
	private:
		Mutex m;
		std::vector<T>queueData;
	protected:
	public:
		std::string name;
		ExclusiveSharedQueue(){ }
		ExclusiveSharedQueue(const std::string &nName){ name = nName; }
		virtual ~ExclusiveSharedQueue(){ }
				
		void add(const T &entry){
			m.lock();
			try {
				bool found = false;
				for (size_t i = 0; i < queueData.size(); i++) {
					if (queueData[i] == entry) {
						found = true;
						break;
					}
				}
				if(!found){
					queueData.push_back(entry);
					if(queueData.size() % 500 == 0) pmm::Log << "There is a queue (" << name << ") with " << (int)queueData.size() << " pending elements, this is completely abnormal!!!" << pmm::NL;					
				}
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
				
		bool extractEntry(T &val){
			bool got_entry = false;;
			m.lock();
			try{
				if (!queueData.empty()) {
					val = queueData.front();
					queueData.erase(queueData.begin());
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