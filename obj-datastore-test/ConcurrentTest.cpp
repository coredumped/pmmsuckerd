//
//  ConcurrentTest.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 1/22/13.
//
//

#include "ConcurrentTest.h"
#include <cstdlib>
#include <map>
#include <sstream>
#include <unistd.h>
#ifndef DEFAULT_TEST_COUNT
#define DEFAULT_TEST_COUNT 2000
#endif

namespace pmmstore {
	
	void ConcurrentTest::operator()() {
		std::map<std::string, std::string> tracker;
		//int succ = 0, written = 0;
		succ = DEFAULT_TEST_COUNT;
		succeeded = false;
		std::stringstream tid;
		tid << std::hex << threadid;
		std::string nsp = tid.str();
		for (int i = 0; i < DEFAULT_TEST_COUNT; i++) {
			std::stringstream key;
			std::string value;
			int keyindex = i;
			key << "key" << keyindex;
			if (rand() % 2 == 0) {
				//Save data
				std::stringstream val;
				val << rand() % 2000 + 50;
				objStore->put(key.str(), val.str(), nsp);
				tracker[key.str()] = val.str();
				//succ++;
			}
			else {
				//Read data
				bool gotIt = objStore->get(value, key.str(), nsp);
				if (tracker.find(key.str()) == tracker.end()) {
					if (gotIt != false) {
						succ--;
					}
				}
				else {
					if (gotIt == false) {
						succ--;
					}
				}
			}
			usleep(rand() % 20 + 10);
		}
		std::cout << "Thread " << threadid << ": succ=" << succ << std::endl;
	}
	
}