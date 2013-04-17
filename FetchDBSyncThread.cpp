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
#include "MailSuckerThread.h"

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
		time_t start = time(0);
		while (time(0) - start <= 86400) {
			pmmrpc::FetchDBInitialSyncItem theItem;
			size_t total = items2SaveQ->size();
			while (items2SaveQ->extractEntry(theItem)) {
				//Save to fetched mails cache
				fetchedMails.addEntry2(theItem.email, theItem.uids, false);
				if (++idx % 100 == 0) {
					pmm::Log << "INFO: " << idx << "/" << (int)items2SaveQ->size() << " items synched" << pmm::NL;
				}
			}
			if (total == 0) {
				if(pmm::mailboxPollBlocked == true) pmm::mailboxPollBlocked = false;
				sleep(1);
			}
			else{
				usleep(10000L);
			}
		}
		pmm::Log << "INFO: Stopping FetchDBSyncThread, no need to have it running!!!" << pmm::NL;
	}
}