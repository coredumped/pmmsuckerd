//
//  Mutex.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/25/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <pthread.h>
#include "Mutex.h"

namespace pmm {
	
	Mutex::Mutex(){
		pthread_mutex_init(&theMutex, NULL);
	}
	
	Mutex::~Mutex() {
		pthread_mutex_destroy(&theMutex);
	}
	
	void Mutex::lock() {
		pthread_mutex_lock(&theMutex);
	}
	
	void Mutex::unlock() {
		pthread_mutex_unlock(&theMutex);
	}

	AtomicFlag::AtomicFlag() : Mutex(){
		theFlag = false;
	}
	
	AtomicFlag::AtomicFlag(bool initialValue) : Mutex() {
		theFlag = initialValue;
	}
	void AtomicFlag::set(bool newValue){
		lock();
		theFlag = newValue;
		unlock();
	}
	
	bool AtomicFlag::get(){
		bool ret = false;
		lock();
		ret = theFlag;
		unlock();
		return ret;
	}
	
	void AtomicFlag::toggle(){
		lock();
		if (theFlag) {
			theFlag = false;
		}
		else {
			theFlag = true;
		}
		unlock();
	}
	
	bool AtomicFlag::operator=(bool newValue){
		lock();
		theFlag = newValue;
		unlock();		
		return newValue;
	}
	
	bool AtomicFlag::operator==(bool anotherValue){
		bool ret = false;
		lock();
		if (theFlag == anotherValue) ret = true;
		unlock();
		return ret;
	}

	bool AtomicFlag::operator==(AtomicFlag &another) {
		bool ret = false;
		lock();
		if (theFlag == another.get()) {
			ret = true;
		}
		unlock();
		return ret;
	}
		
	bool AtomicFlag::operator!=(bool anotherValue){
		bool ret = true;
		lock();
		if (theFlag == anotherValue) ret = false;
		unlock();
		return ret;
	}
	
	bool AtomicFlag::operator!=(AtomicFlag &another) {
		bool ret = true;
		lock();
		if (theFlag == another.get()) {
			ret = false;
		}
		unlock();
		return ret;
	}

}