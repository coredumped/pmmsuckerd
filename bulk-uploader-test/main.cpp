//
//  main.cpp
//  bulk-uploader-test
//  Tests whether we can succefully upload messages in bulk to a given PMM Sucker
//
//  Created by Juan V. Guerrero on 1/15/13.
//
//
#include <iostream>
#include "SharedQueue.h"
#include "PMMSuckerSession.h"
#include "MessageGeneratorThread.h"
#include "ThreadDispatcher.h"
#include<unistd.h>

#ifndef DEFAULT_PMM_SUCKER_URL
#define DEFAULT_PMM_SUCKER_URL "http://127.0.0.1:8888/pmmsuckerd"
#endif

#ifndef DEFAULT_TEST_TIME2LIVE
#define DEFAULT_TEST_TIME2LIVE 60
#endif

#ifndef DEFAULT_MIN_GENERATION_INTERVAL
#define DEFAULT_MIN_GENERATION_INTERVAL 250
#endif

int main(int argc, const char * argv[])
{
	std::string suckerURL =  DEFAULT_PMM_SUCKER_URL;
	int min_generation_interval = DEFAULT_MIN_GENERATION_INTERVAL;
	int test_time2live = DEFAULT_TEST_TIME2LIVE;
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg.compare("-url") == 0 && (i + 1) < argc) {
			suckerURL = argv[++i];
		}
	}
	pmm::SuckerSession session(suckerURL);
	pmm::SharedQueue<pmm::NotificationPayload> msgQueue;

	std::cout << "Registering..." << std::endl;
	if(!session.register2PMM()) {
		std::cerr << "Unable to register to PMM sucker located at: " << suckerURL;
		return 1;
	}
	std::vector<pmm::MailAccountInfo> allAddresses;
	session.retrieveEmailAddresses(allAddresses);
	std::cout << "Received " << allAddresses.size() << " e-mail accounts" << std::endl;
	//Start bulk uploading message generator tests
	pmmtest::MessageGeneratorThread msgGenerator(min_generation_interval);
	msgGenerator.msgQueue = &msgQueue;
	std::cout << "Starting message generator thread..." << std::endl;
	pmm::ThreadDispatcher::start(msgGenerator);
	time_t start_t = time(0);
	time_t now = start_t;
	std::cout << "Starting uploader loop..." << std::endl;
	int msgs_sent = 0;
	while ((now - start_t) < test_time2live) {
		sleep(2);
		if (msgQueue.size() > 0) {
			msgs_sent += session.uploadMultipleNotificationMessages(msgQueue);
		}
		now = time(0);
		std::cout << "\r" << msgs_sent << " messages uploaded       \r";
		std::cout.flush();
	}
	std::cout << "\nAll done" << std::endl;
    return 0;
}

