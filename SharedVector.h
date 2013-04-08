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
#include "RWLock.h"
#include "GenericException.h"

namespace pmm {
	
	template<class T>
	class SharedVector {
	private:
#ifdef USE_RWLOCK
		RWLock m;
#else
		Mutex m;
#endif
		std::vector<T> dataVec;
	protected:
	public:
		SharedVector(){ }
		virtual ~SharedVector(){ }
		
		SharedVector(const SharedVector<T> &v){
#ifdef USE_RWLOCK
			v.m.readLock();
#else
			v.m.lock();
#endif
			dataVec = v.dataVec;
			v.m.unlock();
		}
		
		void push_back(const T &o){
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
			try {
				dataVec.push_back(o);
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		SharedVector<T> &operator=(const std::vector<T> &v){
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
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
#ifdef USE_RWLOCK
			v.m.readLock();
#else
			v.m.lock();
#endif
			newVec = v.dataVec;
			v.m.unlock();
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
			dataVec = newVec;
			m.unlock();
			return *this;
		}
		
		void addVector(const std::vector<T> &v){
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
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
#ifdef USE_RWLOCK
			m.readLock();
#else
			m.lock();
#endif
			try {
				v = dataVec;
			} catch (...) {
				m.unlock();
				throw;
			}
			m.unlock();
		}
		
		void moveTo(std::vector<T> &v) {
#ifdef USE_RWLOCK
			m.readLock();
#else
			m.lock();
#endif
			try {
				v = dataVec;
				dataVec.clear();
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
#ifdef USE_RWLOCK
			m.readLock();
#else
			m.lock();
#endif
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
#ifdef USE_RWLOCK
			m.readLock();
#else
			m.lock();
#endif
			siz = dataVec.size();
			m.unlock();
			return siz;
		}
		
		void erase(size_t idx) {
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
			try {
				dataVec.erase(dataVec.begin() + idx);
			} catch (...) {
				m.unlock();
				throw ;
			}
			m.unlock();
		}
		
		void clear(){
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
			try {
				dataVec.clear();
			} catch (...) {
				m.unlock();
				throw ;
			}
			m.unlock();			
		}
		
		void beginCriticalSection(){
#ifdef USE_RWLOCK
			m.writeLock();
#else
			m.lock();
#endif
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
#ifdef USE_RWLOCK
			m.readLock();
#else
			m.lock();
#endif
			newVector = dataVec;
			m.unlock();
		}
		
		void unlockedCopyTo(std::vector<T> &newVector){
			newVector = dataVec;
		}
	};
}

#endif
