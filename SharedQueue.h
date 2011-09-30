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
			queueData.push_back(entry);
			m.unlock();
		}
		
		T peek(){
			T val;
			m.lock();
			val = queueData[0];
			m.unlock();
			return T(val);			
		}
		
		T extractEntry(){
			T val;
			m.lock();
			val = queueData[0];
			queueData.erase(queueData.begin());
			m.unlock();
			return T(val);
		}
		
		size_t size(){
			size_t s = 0;
			m.lock();
			s = queueData.size();
			m.unlock();
		}
	};
}


#endif
