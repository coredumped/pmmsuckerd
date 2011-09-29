//
//  APNSNotificationThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "APNSNotificationThread.h"
#include "Mutex.h"

namespace pmm {

#ifdef DEBUG
	static Mutex m;
#endif
	
	APNSNotificationThread::APNSNotificationThread(){
		
	}
	
	APNSNotificationThread::~APNSNotificationThread(){
		
	}
	
	void APNSNotificationThread::operator()(){
		size_t i;
		while (true) {
#ifdef DEBUG
			i++;
			if(i % 10 == 0){
				m.lock();
				std::cout << "DEBUG: APNSNotificationThread=0x" << std::hex << (long)pthread_self() << " keepalive still tickling!!!" << std::endl;
				m.unlock();
			}
#endif
			//Verify if there are any ending notifications in the notification queue
			usleep(250000);
		}
	}

	void APNSNotificationThread::notifyTo(const std::string &devToken, const std::string &msg){
		
	}
}