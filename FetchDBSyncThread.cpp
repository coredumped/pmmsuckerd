//
//  FetchDBSyncThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 4/8/13.
//
//

#include "FetchDBSyncThread.h"
#include "MTLogger.h"
#include "FetchedMailsCache.h"

namespace pmm {
	
	FetchDBSyncThread::FetchDBSyncThread() {
		items2SaveQ = 0;
	}
	
	void FetchDBSyncThread::operator()() {
		if (items2SaveQ == 0) {
			pmm::Log << "Unable to start fetchdb sync thread, the save queue is NULL!!!" << pmm::NL;
			throw GenericException("FetchDB sync queue is NULL!!!");
		}
		FetchedMailsCache fetchedMails;
		int idx = 0;
		pmm::Log << "Starting FetchDBSyncThread main loop..." << pmm::NL;
		while (true) {
			pmmrpc::FetchDBItem theItem;
			while (items2SaveQ->extractEntry(theItem)) {
				//Save to fetched mails cache
				fetchedMails.addEntry2(theItem.email, theItem.uid);
				if (idx++ % 100 == 0) {
					pmm::Log << "INFO: " << idx << " items synched" << pmm::NL;
				}
			}
			sleep(100000L);
		}
	}
}