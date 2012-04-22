//
//  main.cpp
//  apnFeedback
//
//  Created by Juan Guerrero on 4/20/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include <sys/stat.h>
#include <iostream>
#include "APNSFeedbackThread.h"
#include "ThreadDispatcher.h"

#ifndef DEFAULT_CERTIFICATE_PATH
#define DEFAULT_CERTIFICATE_PATH "/Users/coredumped/Dropbox/MacOSXProjects/pmm_production.pem"
#endif

#ifndef DEFAULT_CERTIFICATE_KEY
#define DEFAULT_CERTIFICATE_KEY DEFAULT_CERTIFICATE_PATH
#endif

int main(int argc, const char * argv[])
{
	std::string certificate = DEFAULT_CERTIFICATE_PATH;
	std::string privateKey = DEFAULT_CERTIFICATE_KEY;
	for (int i = 1; i < argc; i++) {
		certificate = argv[i];
		privateKey = argv[i];
	}
	pmm::fdbckLog.open("/tmp/apn-feedback.log");
	pmm::fdbckLog.setTag("APN Feedback");
	SSL_library_init();
	SSL_load_error_strings();
	struct stat st;
	if (stat(certificate.c_str(), &st) != 0) {
		std::cout << "Unable to find SSL certificate." << std::endl;
		return 1;
	}
	pmm::APNSFeedbackThread apnFeedback;
	apnFeedback.useForProduction();
	apnFeedback.setCertPath(certificate);
	apnFeedback.setKeyPath(certificate);
	apnFeedback.checkInterval = 60;
	pmm::fdbckLog << "Starting thread..." << pmm::NL;
	
	pmm::ThreadDispatcher::start(apnFeedback);
	while (true) {

		sleep(1);
	}
    return 0;
}

