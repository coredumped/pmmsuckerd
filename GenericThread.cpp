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
#include "MTLogger.h"

namespace pmm {
	GenericThread::GenericThread(){

	}
	
	GenericThread::~GenericThread(){
		
	}
	
	void GenericThread::onStart(){
#ifdef DEBUG
		pmm::Log << "DEBUG: Starting thread " << pmm::NL;
#endif
	}
	
	void GenericThread::operator()(){
#ifdef DEBUG
		pmm::Log << "DEBUG: Look I am thread running in the background :-)" << pmm::NL;
#endif
	}
	
	void GenericThread::onFinish(){
#ifdef DEBUG
		pmm::Log << "DEBUG: Finishing thread " << pmm::NL;
#endif
	}

}
