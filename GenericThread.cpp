//
//  GenericThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <pthread.h>
#include "GenericThread.h"

namespace pmm {
	GenericThread::GenericThread(){

	}
	
	GenericThread::~GenericThread(){
		
	}
	
	void GenericThread::onStart(){
#ifdef DEBUG
		std::cerr << "DEBUG: Starting thread " << (unsigned long)pthread_self() << std::endl;
#endif
	}
	
	void GenericThread::operator()(){
#ifdef DEBUG
		std::cerr << "DEBUG: Look I am thread "  << (unsigned long)pthread_self() << " running in the background :-)" << std::endl;
#endif
	}
	
	void GenericThread::onFinish(){
#ifdef DEBUG
		std::cerr << "DEBUG: Finishing thread " << (unsigned long)pthread_self() << std::endl;
#endif
	}

}
