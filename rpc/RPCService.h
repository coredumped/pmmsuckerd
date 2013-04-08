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
#include "GenericThread.h"
#include <map>

namespace pmm {
	
	class RPCService : public GenericThread {
	private:
	protected:

	public:
		pmm::SharedVector< std::map<std::string, std::map<std::string, std::string> > > *rtCommandV;
		int port;
		RPCService();
		RPCService(int _port);
		void operator()();
	};
	
}

#endif
