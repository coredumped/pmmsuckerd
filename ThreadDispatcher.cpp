//
//  ThreadDispatcher.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <errno.h>
#include <sstream>
#include "ThreadDispatcher.h"
#include "Mutex.h"

namespace pmm {
#ifdef DEBUG
	extern Mutex mout;
#endif
	
	static void *runThreadInBackground(GenericThread &gt){
		gt.isRunning = true;
		gt.onStart();
		gt();
		gt.onFinish();
		gt.isRunning = false;
		return NULL;
	}
	
	void ThreadDispatcher::start(GenericThread &gt, size_t stackSize){
		pthread_t theThread;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if(stackSize > 1024){
			//pthread_attr_getstacksize(&attr, &stackSize);
			/*mout.lock();
			 std::cerr << "Creating thread " << (long)&theThread << " with " << stackSize << " stack size" << std::endl;
			 mout.unlock();*/
			pthread_attr_setstacksize(&attr, stackSize);
		}
		pthread_t *ptr = &theThread;
		gt.threadid = (long) *ptr;
		int retval = pthread_create(&theThread, &attr, (void * (*)(void *))runThreadInBackground, (void *)&gt);
		if (retval == EAGAIN) {
			throw ThreadExecutionException("There are not enough resources to start a new thread");
		}
		else if(retval == EINVAL){
			throw ThreadExecutionException("Unable to create thread in detached state, sorry");
		}
		else if(retval != 0){
			std::stringstream errmsg;
			errmsg << "Unable to start thread, error=" << retval;
			throw ThreadExecutionException(errmsg.str());
		}
		while (gt.isRunning != true) {
			usleep(100);
		}
	}

	ThreadExecutionException::ThreadExecutionException(){
		
	}
	
	ThreadExecutionException::ThreadExecutionException(const std::string &_errmsg){
		errmsg = _errmsg;
	}

	std::string ThreadExecutionException::message(){
		return std::string(errmsg);
	}
}