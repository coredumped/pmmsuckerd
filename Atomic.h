//
//  Atomic.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 7/10/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_Atomic_h
#define PMM_Sucker_Atomic_h
#include "Mutex.h"
#include "RWLock.h"

namespace pmm {
#ifdef USE_RWLOCK
	class AtomicFlag : protected RWLock {
#else
	class AtomicFlag : protected Mutex {
#endif
	private:
		bool theFlag;
	protected:
	public:
		AtomicFlag();
		AtomicFlag(bool initialValue);
		void set(bool newValue);
		bool get();
		void toggle();
		
		operator bool (){
			bool v;
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			v = theFlag;
			unlock();
			return v;
		}
		
		
		bool operator=(bool newValue);
		bool operator==(bool anotherValue);
		bool operator==(AtomicFlag &another);
		
		bool operator!=(bool anotherValue);
		bool operator!=(AtomicFlag &another);
	};
	
#ifdef USE_RWLOCK
	template <class T>
	class AtomicVar : protected RWLock {
#else
	template <class T>
	class AtomicVar : protected Mutex {
#endif
	private:
		T theValue;
	protected:
	public:
		AtomicVar(){}
		AtomicVar(T initialValue):T(initialValue){ }
		void set(T newValue){
#ifdef USE_RWLOCK
			writeLock();
#else
			lock();
#endif
			theValue = newValue;
			unlock();
		}
		T get(){
			T v;
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			v = theValue;
			unlock();
			return v;
		}
		operator T (){
			T v;
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			v = theValue;
			unlock();
			return v;
		}
		T operator=(T newValue){
			set(newValue);
			return newValue;
		}
		bool operator==(T anotherValue){
			bool ret = false;
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			if (theValue == anotherValue) ret = true;
			unlock();
			return ret;
		}
		bool operator==(AtomicVar<T> &another){
			bool ret = false;
			T anotherValue = another.get();
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			if (theValue == anotherValue) ret = true;
			unlock();
			return ret;			
		}
		
		T operator!=(bool anotherValue){
			bool ret = true;
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			if (theValue == anotherValue) ret = false;
			unlock();
			return ret;			
		}
		T operator!=(AtomicVar<T> &another){
			bool ret = true;
			T anotherValue = another.get();
#ifdef USE_RWLOCK
			readLock();
#else
			lock();
#endif
			if (theValue == anotherValue) ret = false;
			unlock();
			return ret;						
		}
	};
}

#endif
