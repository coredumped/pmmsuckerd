//
//  main.cpp
//  pmmsuckerd
//
//  Created by Juan V. Guerrero on 9/17/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "PMMSuckerSession.h"
#include "ServerResponse.h"

#ifndef DEFAULT_REGISTER_INTERVAL
#define DEFAULT_REGISTER_INTERVAL 600
#endif

int main (int argc, const char * argv[])
{
	std::string pmmServiceURL = DEFAULT_PMM_SERVICE_URL;
	int registerInterval = DEFAULT_REGISTER_INTERVAL;
	for (int i = 1; 1 < argc; i++) {
		std::string arg = argv[i];
		if (arg.find("-h") == 0 && (i + 1) < argc) {
			pmmServiceURL = argv[++i];
		}
		else if(arg.find("--req-membership") == 0 && (i + 1) < argc){
			try {
#ifdef DEBUG
				std::cout << "Requesting to central pmm service cluster... ";
				std::cout.flush();
#endif
				pmm::SuckerSession session(pmmServiceURL);
				if(session.reqMembership(argv[++i])) std::cout << "OK" << std::endl;
				return 0;
			} catch (pmm::ServerResponseException &se1) {
				std::cerr << "Unable to request membership to server: " << se1.errorDescription << std::endl;
				return 1;
			}
		}
	}
	pmm::SuckerSession session(pmmServiceURL);
	//1. Register to PMMService...
	try {
		session.register2PMM();
	} catch (pmm::ServerResponseException &se1) {
		if (se1.errorCode == pmm::PMM_ERROR_SUCKER_DENIED) {
			std::cerr << "Unable to register, permission denied." << std::endl;
		}
		else {
			try{
				//Try to ask for membership automatically or report if a membership has already been asked
				session.reqMembership("Automated membership petition, please help!!!");
				std::cerr << "Membership request issued to pmm controller, try again later" << std::endl;
			}
			catch(pmm::ServerResponseException  &se2){ 
				std::cerr << "Failed to request membership automatically: " << se2.errorDescription << std::endl;
			}
		}
		return 1;
	}
	//Registration succeded, retrieve max
#ifdef DEBUG
	std::cout << "Initial registration succeded!!!" << std::endl;
#endif
	//2. Request accounts to poll
	//3. Start APNS notification threads, validate remote devTokens
	//4. Dispatch polling threads for imap
	//5. Dispatch polling threads for pop3
	//6. After registration time ends, close every connection, return to Step 1
    return 0;
}

