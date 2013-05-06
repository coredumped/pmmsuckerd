//
//  RPCService.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 4/1/13.
//
//

#ifndef __PMM_Sucker__RPCService__
#define __PMM_Sucker__RPCService__

#include "pmmrpc_types.h"
#include "pmmrpc_constants.h"
#include "SharedVector.h"
#include "SharedQueue.h"
#include "GenericThread.h"
#include "MTLogger.h"
#include <map>

namespace pmm {
	
	class RPCService : public GenericThread {
	private:
	protected:

	public:
		pmm::SharedVector< std::map<std::string, std::map<std::string, std::string> > > *rtCommandV;
		int port;
		pmm::SharedQueue<pmmrpc::FetchDBInitialSyncItem> *items2SaveQ;
		RPCService();
		RPCService(int _port);
		RPCService(int _port, pmm::SharedQueue<pmmrpc::FetchDBInitialSyncItem> *items2SaveQ_);
		void operator()();
	};
	
	extern MTLogger rpcLog;
}

#endif
