//
//  MessageUploaderThread.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/18/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#include "MessageUploaderThread.h"
#include "GenericException.h"
#include "MTLogger.h"
#include <iostream>

namespace pmm {
	MessageUploaderThread::MessageUploaderThread(){
		pmmStorageQueue = NULL;
		session = NULL;
	}
	
	MessageUploaderThread::~MessageUploaderThread(){
		
	}
	
	void MessageUploaderThread::operator()(){
		if (pmmStorageQueue == NULL) throw GenericException("Unable to start MessageUploaderThread, the pmmStorageQueue is NULL!!!");
		if (session == NULL) throw GenericException("Unable to start MessageUploaderThread, the session is NULL!!!");
		pmm::Log << "Starting message uploader thread..." << pmm::NL;
		while (true) {
			NotificationPayload np;
			while (pmmStorageQueue->extractEntry(np)) {
				try {
#ifdef DEBUG
					pmm::Log << "Uploading message to: " << np.origMailMessage.to << pmm::NL;
#endif
					session->uploadNotificationMessage(np);
				} catch (pmm::HTTPException &htex1) {
					pmm::Log << "Can't upload message due to: " << htex1.errorMessage() << ", will retry in the next cycle." << pmm::NL;
					sleep(1);
					pmmStorageQueue->add(np);
				}
			}
			usleep(250000);
		}
	}
}

