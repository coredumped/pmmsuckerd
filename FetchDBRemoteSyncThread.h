//
//  FetchDBRemoteSyncThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 4/9/13.
//
//

#ifndef __PMM_Sucker__FetchDBRemoteSyncThread__
#define __PMM_Sucker__FetchDBRemoteSyncThread__

#include "GenericThread.h"
#include "PMMSuckerRPC.h"
#include "SharedQueue.h"
#include "SharedVector.h"
#include "PMMSuckerInfo.h"
#include "MTLogger.h"

namespace pmm {
	class FetchDBRemoteSyncThread : public GenericThread {
	private:
	protected:
	public:
		SharedVector<PMMSuckerInfo> *siblingSuckers;
		FetchDBRemoteSyncThread();
		FetchDBRemoteSyncThread(SharedVector<PMMSuckerInfo> *siblingSuckers_);
		void operator()();
	};
	
	extern SharedQueue<pmmrpc::FetchDBItem> RemoteFetchDBSyncQueue;
	extern MTLogger rmtSyncLog;
}

#endif
