//
//  RWLock.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 7/9/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include "RWLock.h"
#include <pthread.h>
#include <cerrno>
#include <sstream>

namespace pmm {
	
	RWLock::RWLock(){
		int retval = pthread_rwlock_init(&theLock, NULL);
		switch (retval) {
			case 0:
				return;
				break;
			case EAGAIN:
				throw GenericException("Not enough resources to instantiate rw-lock");
				break;
			case ENOMEM:
				throw GenericException("Not enough memory to instantiate rw-lock");
				break;
			case EPERM:
				throw GenericException("Not enough privileges to instantiate rw-lock");
				break;
			case EINVAL:
				throw GenericException("Invalid rw-lock attributes assigned, can't create rw-lock.");
			default:
				throw GenericException("Can't create rw-lock");
				break;
		}
	}
	
	RWLock::~RWLock(){
		pthread_rwlock_destroy(&theLock);
	}
	
	void RWLock::readLock(){
		int retval = pthread_rwlock_rdlock(&theLock);
		if(retval != 0){
			std::stringstream errmsg;
			errmsg << "Unable to read-lock, error: " << retval;
			throw GenericException(errmsg.str());
		}
	}
	
	void RWLock::unlock(){
		int retval = pthread_rwlock_unlock(&theLock);
		if(retval != 0){
			std::stringstream errmsg;
			errmsg << "Unable to unlock rw-lock, error: " << retval;
			throw GenericException(errmsg.str());
		}
	}
	
	void RWLock::writeLock(){
		int retval = pthread_rwlock_wrlock(&theLock);
		if(retval != 0){
			std::stringstream errmsg;
			errmsg << "Unable to write-lock, error: " << retval;
			throw GenericException(errmsg.str());
		}
	}
		
}