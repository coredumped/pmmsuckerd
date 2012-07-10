//
//  Atomic.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 7/10/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include "Atomic.h"
#include <iostream>

namespace pmm {
#ifdef USE_RWLOCK
	AtomicFlag::AtomicFlag() : RWLock(){
#else
	AtomicFlag::AtomicFlag() : Mutex(){
#endif
		theFlag = false;
	}
	
#ifdef USE_RWLOCK
	AtomicFlag::AtomicFlag(bool initialValue) : RWLock() {
#else
	AtomicFlag::AtomicFlag(bool initialValue) : Mutex() {
#endif
		theFlag = initialValue;
	}
		
	void AtomicFlag::set(bool newValue){
#ifdef USE_RWLOCK
		writeLock();
#else
		lock();
#endif
		theFlag = newValue;
		unlock();
	}
	
	bool AtomicFlag::get(){
		bool ret = false;
#ifdef USE_RWLOCK
		readLock();
#else
		lock();
#endif
		ret = theFlag;
		unlock();
		return ret;
	}
	
	void AtomicFlag::toggle(){
#ifdef USE_RWLOCK
		writeLock();
#else
		lock();
#endif
		if (theFlag) {
			theFlag = false;
		}
		else {
			theFlag = true;
		}
		unlock();
	}
	
	bool AtomicFlag::operator=(bool newValue){
#ifdef USE_RWLOCK
		writeLock();
#else
		lock();
#endif
		theFlag = newValue;
		unlock();		
		return newValue;
	}
	
	bool AtomicFlag::operator==(bool anotherValue){
		bool ret = false;
#ifdef USE_RWLOCK
		readLock();
#else
		lock();
#endif
		if (theFlag == anotherValue) ret = true;
		unlock();
		return ret;
	}
	
	bool AtomicFlag::operator==(AtomicFlag &another) {
		bool ret = false;
#ifdef USE_RWLOCK
		readLock();
#else
		lock();
#endif
		if (theFlag == another.get()) {
			ret = true;
		}
		unlock();
		return ret;
	}
	
	bool AtomicFlag::operator!=(bool anotherValue){
		bool ret = true;
#ifdef USE_RWLOCK
		readLock();
#else
		lock();
#endif
		if (theFlag == anotherValue) ret = false;
		unlock();
		return ret;
	}
	
	bool AtomicFlag::operator!=(AtomicFlag &another) {
		bool ret = true;
#ifdef USE_RWLOCK
		readLock();
#else
		lock();
#endif
		if (theFlag == another.get()) {
			ret = false;
		}
		unlock();
		return ret;
	}
}