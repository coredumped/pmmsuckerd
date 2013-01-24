//
//  main.cpp
//  obj-datastore-test
//
//  Created by Juan V. Guerrero on 1/22/13.
//
//
#include "ObjectDatastore.h"
#include "ConcurrentTest.h"
#include "ThreadDispatcher.h"
#include <iostream>
#include <sstream>
#include <map>
#include <unistd.h>

int main(int argc, const char * argv[])
{
	pmm::ObjectDatastore objStore;
	std::map<std::string, std::string> corr;
	std::cout << "Test #1: Linear store and read" << std::endl;
	for (int i = 0; i < 10; i++) {
		std::stringstream key, val;
		key << "key" << i;
		val << i;
		objStore.put(key.str(), val.str());
		corr[key.str()] = val.str();
	}
	std::cout << " Store contents" << std::endl;
	int succ = 0;
	for (int i = 0; i < 10; i++) {
		std::stringstream key;
		std::string val;
		key << "key" << i;
		if (objStore.get(val, key.str())) {
			std::cout << "  " << key.str() << ": " << val << " ";
			if (corr[key.str()].compare(val) == 0) {
				std::cout << "OK";
				succ++;
			}
			else {
				std::cout << "FAILED";
			}
			std::cout << std::endl;
		}
	}
	std::cout << "RESULT1: ";
	if (succ == 10) {
		std::cout << "OK" << std::endl;
	}
	else {
		std::cout << "FAILED" << std::endl;
	}
	std::cout << "\nTest #2: Looking for an non-existing item" << std::endl;
	std::string value;
	if (objStore.get(value, "unk")) {
		std::cout << "RESULT2: FAILED" << std::endl;
	}
	else {
		std::cout << "RESULT2: OK" << std::endl;
	}
	
	std::cout << "\nTest #3: Concurrent non-linear access using 16 threads" << std::endl;
	pmmstore::ConcurrentTest *cThread = new pmmstore::ConcurrentTest[16];
	for (int i = 0; i < 16; i++) {
		cThread[i].objStore = &objStore;
	}
	std::cout << " Starting tests..." << std::endl;
	for (int i = 0; i < 16; i++) {
		pmm::ThreadDispatcher::start(cThread[i]);
	}
	std::cout << " Waiting for tests to complete..." << std::endl;
	sleep(1);
	while (true) {
		int finished = 0;
		for (int i = 0; i < 16; i++) {
			if (!cThread[i].isRunning) {
				finished++;
			}
		}
		if(finished >= 16) break;
		sleep(1);
	}
	succ = 0;
	for (int i = 0; i < 16; i++) {
		if (cThread[i].succ == 2000) {
			succ++;
		}
	}
	if (succ != 16) {
		std::cout << "RESULT3: FAILED, succ=" << succ << std::endl;
	}
	else {
		std::cout << "RESULT3: OK" << std::endl;
	}
    return 0;
}

