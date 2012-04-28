//
//  main.cpp
//  fetchCacheTest
//
//  Created by Juan Guerrero on 4/24/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include "FetchedMailsCache.h"
#include "GenericThread.h"
#include "SharedQueue.h"
#include "ThreadDispatcher.h"
#include <iostream>
#include <sstream>

class TestItem {
public:
	std::string email;
	std::string msgId;
};

pmm::SharedQueue<TestItem> insertionQueue;


class CacheTester : public pmm::GenericThread {
protected:
	pmm::FetchedMailsCache fetchCache;
public:
	pmm::AtomicVar<int> processed;
	CacheTester(){ 
		processed = 0;
	}
	void operator()(){
		int iteration = 0;
		time_t startTime = time(0);
		time_t lastProcessing = startTime;
		while (true) {
			TestItem t;
			while (insertionQueue.extractEntry(t)) {
				lastProcessing = time(0);
				int v = processed;
				v++;
				processed = v;
				iteration++;
				if (!fetchCache.entryExists2(t.email, t.msgId)) {
					usleep(rand() % 1000 + 1000); //Simulate we're doing some work
					fetchCache.addEntry2(t.email, t.msgId);
					

				}
				if(iteration % 1000 == 0) fetchCache.expireOldEntries(t.email);
				if(iteration % (rand() % 1000 + 1) == 0){
					std::vector<uint32_t> uids;
					for (size_t x = 0; x < 2000; x++) {
						uint32_t v = rand() % 200 + 100;
						uids.push_back(v);
					}
					fetchCache.removeEntriesNotInSet2(t.email, uids);
				}
			}
			if(lastProcessing - startTime > 10) break;
		}
	}
};

int main(int argc, const char * argv[])
{
	CacheTester ct1, ct2, ct3, ct4;
	int maxTime = 600;
	const char *eAddresses[] = {
		"user1@email.com",
		"user2@email.com",
		"user3@email.com",
		"user.44.1@email.com",
		"user.44.2@email.com",
		"ryoma.nagare@gmail.com",
		"whatever2@gmail.com",
		"whatever33@gmail.com",
		"jafar2020@hotmail.com",
		"whatever3@gmail.com",
		"lekjula2008@hotmail.com",
	};
	insertionQueue.name = "InsertionQ";
	pmm::CacheLog.open("cachedb.log");
	pmm::ThreadDispatcher::start(ct1);
	pmm::ThreadDispatcher::start(ct2);
	pmm::ThreadDispatcher::start(ct3);
	pmm::ThreadDispatcher::start(ct4);
	time_t startTime = time(0);
	int iteration = 0;
	while (time(0) - startTime < maxTime) {
		for (size_t i = 0; i < 11; i++) {
			TestItem t;
			t.email = eAddresses[i];
			std::stringstream sId;
			sId << (rand() % 600 + 5);
			t.msgId = sId.str();
			insertionQueue.add(t);
			iteration++;
			if(iteration % 2999999 == 0){
				int timediff = (int)(time(0) - startTime);
				std::cout << "\nIteration: " << iteration << " remaining: " << (maxTime - timediff) << " secs" << std::endl;
				std::cout << "\tct1=" << ct1.processed << " ct2=" << ct2.processed << " ct3=" << ct3.processed << " ct4=" << ct4.processed << std::endl;
			}
		}
		usleep(250000);
	}
	std::cout << "Waiting for all thread to finish consuming data..." << std::endl;
	while (ct1.isRunning && ct2.isRunning && ct3.isRunning && ct4.isRunning) {
		std::cout << "\tct1=" << ct1.processed << " ct2=" << ct2.processed << " ct3=" << ct3.processed << " ct4=" << ct4.processed << std::endl;
		sleep(2);
	}
	std::cout << iteration << " iterations computed!!!" << std::endl;
	std::cout << "\nAll done" << std::endl;
    return 0;
}

