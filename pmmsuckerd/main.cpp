//
//  main.cpp
//  pmmsuckerd
//
//  Created by Juan V. Guerrero on 9/17/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#ifndef DEFAULT_PMM_SERVICE_URL
#define DEFAULT_PMM_SERVICE_URL "https://pmmservice.appspot.com/pmmsuckerd"
#endif

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
    return 0;
}

