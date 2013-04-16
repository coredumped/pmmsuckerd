//
//  RPCService.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 4/1/13.
//
//

#include "RPCService.h"
#include "PMMSuckerRPC.h"
#include "MTLogger.h"
#include "FetchedMailsCache.h"
#include "PMMSuckerSession.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

namespace  pmmrpc {
	class PMMSuckerRPCHandler : virtual public PMMSuckerRPCIf {
	protected:
		pmm::FetchedMailsCache fetchedMails;
	public:
		pmm::SharedVector< std::map<std::string, std::map<std::string, std::string> > > *rtCommandV;
		PMMSuckerRPCHandler(pmm::SharedVector< std::map<std::string, std::map<std::string, std::string> > > *rtCommandV_) {
			// Your initialization goes here
			rtCommandV = rtCommandV_;
		}
		
		void getAllEmailAccounts(std::vector<std::string> & _return) throw (GenericException) {
			GenericException ex1;
			ex1.errorCode = 666;
			ex1.errorMessage = "Method not implemented";
			throw ex1;
		}
		
		bool fetchDBPutItem(const std::string& email, const std::string& uid){
			fetchedMails.addEntry2(email, uid);
			return true;
		}
		
		void fetchDBPutItemAsync(const std::string& email, const std::string& uid){
			GenericException ex1;
			ex1.errorCode = 666;
			ex1.errorMessage = "Method not implemented";
			throw ex1;
		}
		
		void fetchDBGetItems(std::vector<FetchDBItem> & _return, const std::string& email) throw (GenericException) {
			GenericException ex1;
			ex1.errorCode = 666;
			ex1.errorMessage = "Method not implemented";
			throw ex1;
		}
		
		bool notificationPayloadPush() throw (GenericException) {
			GenericException ex1;
			ex1.errorCode = 666;
			ex1.errorMessage = "Method not implemented";
			throw ex1;
			return true;
		}
		
		bool commandSubmit(const Command& cmd) throw (GenericException) {
			std::map<std::string, std::map<std::string, std::string> > theCmd;
			theCmd[cmd.name] = cmd.parameter;
			rtCommandV->push_back(theCmd);
			return true;
		}
		
		bool emailAccountRegister(const MailAccountInfo& m) throw (GenericException) {
			GenericException ex1;
			ex1.errorCode = 666;
			ex1.errorMessage = "Method not implemented";
			throw ex1;
			return true;
		}
		
		bool emailAccountUnregister(const std::string& email) throw (GenericException) {
			GenericException ex1;
			ex1.errorCode = 666;
			ex1.errorMessage = "Method not implemented";
			throw ex1;
			return true;
		}
		
	};
}

namespace pmm {
	
	RPCService::RPCService() {
		port = DEFAULT_PMM_SERVICE_PORT;
	}
	
	RPCService::RPCService(int _port) {
		port = _port;
	}
	
	void RPCService::operator()(){
		if (rtCommandV == 0) {
			pmm::Log << "Unable to start rpc service with not real-time command vector" << pmm::NL;
			abort();
		}
		shared_ptr<pmmrpc::PMMSuckerRPCHandler> handler(new pmmrpc::PMMSuckerRPCHandler(rtCommandV));
		shared_ptr<TProcessor> processor(new pmmrpc::PMMSuckerRPCProcessor(handler));
		shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
		shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
		shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
		
		shared_ptr<concurrency::ThreadManager> tman = concurrency::ThreadManager::newSimpleThreadManager(4);
		shared_ptr<concurrency::PosixThreadFactory> tfact = shared_ptr<concurrency::PosixThreadFactory>(new concurrency::PosixThreadFactory());
		tman->threadFactory(tfact);
		tman->start();
		pmm::Log << "INFO: Thrift listener starting..." << pmm::NL;
		server::TThreadPoolServer server(processor, serverTransport, transportFactory, protocolFactory, tman);
		server.serve();
	}
}