//
//  FetchDBSyncThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 4/8/13.
//
//

#ifndef __PMM_Sucker__FetchDBSyncThread__
#define __PMM_Sucker__FetchDBSyncThread__
#include "GenericThread.h"
#include "SharedQueue.h"
#include "pmmrpc_types.h"

namespace pmm {
	
	class FetchDBSyncThread : public GenericThread {
	private:
	protected:
	public:
		pmm::SharedQueue<pmmrpc::FetchDBItem> *items2SaveQ;
		FetchDBSyncThread();
		void operator()();
	};
}

#endif
