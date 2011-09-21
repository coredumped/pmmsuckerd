//
//  main.cpp
//  pmmsuckerd
//
//  Created by Juan V. Guerrero on 9/17/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#ifndef DEFAULT_PMM_SERVICE_URL
#define DEFAULT_PMM_SERVICE_URL "https://pmmserver.appspot.com/pmmsuckerd"
#endif
#include "PMMSuckerSession.h"

int main (int argc, const char * argv[])
{
	std::string pmmServiceURL = DEFAULT_PMM_SERVICE_URL;
	for (int i = 1; 1 < argc; i++) {
		std::string arg = argv[i];
		if (arg.find("-h") == 0 && (i + 1) < argc) {
			pmmServiceURL = argv[++i];
		}
	}
	//Register to PMMService...
	pmm::SuckerSession session;
	session.register2PMM();
    return 0;
}

