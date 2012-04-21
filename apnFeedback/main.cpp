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
#define DEFAULT_CERTIFICATE_PATH "/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem"
#endif

#ifndef DEFAULT_CERTIFICATE_KEY
#define DEFAULT_CERTIFICATE_KEY DEFAULT_CERTIFICATE_PATH
#endif

int main(int argc, const char * argv[])
{
	std::string certificate = DEFAULT_CERTIFICATE_PATH;
	std::string privateKey = DEFAULT_CERTIFICATE_KEY;
	struct stat st;
	if (stat(certificate.c_str(), &st) != 0) {
		std::cout << "Unable to find SSL certificate." << std::endl;
		return 1;
	}
	pmm::APNSFeedbackThread apnFeedback;
	apnFeedback.useForProduction();
	apnFeedback.setCertPath(certificate);
	apnFeedback.setKeyPath(certificate);
	
	pmm::ThreadDispatcher::start(apnFeedback);
	while (true) {

		sleep(1);
	}
    return 0;
}

