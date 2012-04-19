//
//  SharedVector.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_SharedVector_h
#define PMM_Sucker_SharedVector_h
#include <vector>
#include <stdexcept>
#include "Mutex.h"
#include "GenericException.h"

namespace pmm {
	
	template<class T>
	class SharedVector {
	private:
		Mutex m;
		std::vector<T> dataVec;
	protected:
	public:
		SharedVector(){ }
		virtual ~SharedVector(){ }
		
		SharedVector(const SharedVector<T> &v){
			v.m.lock();
			dataVec = v.dataVec;
			v.m.unlock();
		}
		
		void push_back(const T &o){
			m.lock();
			try {
				dataVec.push_back(o);
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		SharedVector<T> &operator=(const std::vector<T> &v){
			m.lock();
			try {
				dataVec = v;
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
			return *this;
		}
		
		SharedVector<T> &operator=(const SharedVector<T> &v){
			std::vector<T> newVec;
			v.m.lock();
			newVec = v.dataVec;
			v.m.unlock();
			m.lock();
			dataVec = newVec;
			m.unlock();
			return *this;
		}
		
		void addVector(const std::vector<T> &v){
			m.lock();
			try {
				for (size_t i = 0; i < v.size(); i++) {
					dataVec.push_back(v[i]);
				}
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		void copyTo(const std::vector<T> &v){
			m.lock();
			try {
				v = dataVec;
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		T operator[](size_t i){
			return at(i);
		}
		
		T at(size_t i){
			T ret;
			m.lock();
			try {
				ret = dataVec[i];
			} catch (std::out_of_range &oore1) {
				m.unlock();
				throw;
			}
			m.unlock();
			return T(ret);
		}
		
		size_t size(){
			size_t siz = 0;
			m.lock();
			siz = dataVec.size();
			m.unlock();
			return siz;
		}
		
		void erase(size_t idx) {
			m.lock();
			try {
				dataVec.erase(dataVec.begin() + idx);
			} catch (...) {
				m.unlock();
				throw ;
			}
			m.unlock();
		}
		
		void clear(){
			m.lock();
			try {
				dataVec.clear();
			} catch (...) {
				m.unlock();
				throw ;
			}
			m.unlock();			
		}
		
		void beginCriticalSection(){
			m.lock();
		}
		
		T &getValue(size_t idx){
			return dataVec[idx];
		}
		
		void unlockedClear(){
			dataVec.clear();
		}
		
		void unlockedErase(size_t idx) {
			dataVec.erase(dataVec.begin() + idx);
		}
		
		size_t unlockedSize(){
			return dataVec.size();
		}
		
		T &atUnlocked(size_t i){
			return dataVec[i];
		}
		
		void endCriticalSection(){
			m.unlock();
		}

		void copyTo(std::vector<T> &newVector){
			m.lock();
			newVector = dataVec;
			m.unlock();
		}
		
		void unlockedCopyTo(std::vector<T> &newVector){
			newVector = dataVec;
		}
	};
}

#endif
