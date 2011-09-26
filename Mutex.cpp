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

}