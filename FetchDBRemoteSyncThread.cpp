//
//  FetchDBRemoteSyncThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 4/9/13.
//
//
#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "MTLogger.h"
#include "FetchDBRemoteSyncThread.h"
#include "PMMSuckerInfo.h"


namespace pmm {
	MTLogger rmtSyncLog;
	
	class PMMSuckerRemoteClient {
	private:
	protected:
		boost::shared_ptr<apache::thrift::transport::TTransport> socket;
		boost::shared_ptr<apache::thrift::transport::TTransport> transport;
		boost::shared_ptr<apache::thrift::protocol::TProtocol> protocol;

	public:
		pmmrpc::PMMSuckerRPCClient *client;
		PMMSuckerInfo suckerInfo;
		
		PMMSuckerRemoteClient() {
			client = 0;
		}
		
		PMMSuckerRemoteClient(const PMMSuckerInfo &info_) {
			client = 0;
			suckerInfo = info_;
		}

		
		~PMMSuckerRemoteClient(){
			this->close();
			delete client;
		}
		
		void open(){
			if (client == 0) {
				if (suckerInfo.hostname.size() == 0) {
					throw GenericException("Unable to connect to remote PMMSucker with a NULL hostname.");
				}
				socket = boost::shared_ptr<apache::thrift::transport::TTransport>(new apache::thrift::transport::TSocket(suckerInfo.hostname, suckerInfo.port));
				transport = boost::shared_ptr<apache::thrift::transport::TTransport>(new apache::thrift::transport::TBufferedTransport(socket));
				protocol = boost::shared_ptr<apache::thrift::protocol::TProtocol>(new apache::thrift::protocol::TBinaryProtocol(transport));
				client = new pmmrpc::PMMSuckerRPCClient(protocol);
				transport->open();
			}
			if (client != 0 && !transport->isOpen()) {
				transport->open();
			}
		}
		
		void close(){
			if (client != 0 && transport->isOpen()) {
				transport->close();
			}
		}
	};
	
	
	
	SharedQueue<pmmrpc::FetchDBItem> RemoteFetchDBSyncQueue("RemoteFetchDBSyncQueue");
	
	FetchDBRemoteSyncThread::FetchDBRemoteSyncThread() {
		siblingSuckers = NULL;
	}
	
	FetchDBRemoteSyncThread::FetchDBRemoteSyncThread(SharedVector<PMMSuckerInfo> *siblingSuckers_){
		siblingSuckers = siblingSuckers_;
	}
	
	void FetchDBRemoteSyncThread::operator()() {
		std::map<std::string, PMMSuckerRemoteClient *> connPool;
		if (siblingSuckers == NULL) {
			pmm::Log << "PANIC: Unable to start the FetchDBSyncRemoteThread because the siblingSuckers vector is NULL" << pmm::NL;
			throw GenericException("Unable to start with a NULL siblingSuckers vector");
		}
		pmm::Log << "Starting remote synchronizer thread..." << pmm::NL;
		while (true) {
			bool upLoadFailed = false;
			pmmrpc::FetchDBItem item2Save;
			//Refresh or add suckerInfo to connPool list
			siblingSuckers->beginCriticalSection();
			for (size_t i = 0; i < siblingSuckers->unlockedSize(); i++) {
				std::string suckerId = siblingSuckers->atUnlocked(i).suckerID;
				if (connPool.find(suckerId) == connPool.end()) {
					//Add new sucker definition to map cache
					pmm::rmtSyncLog << "Adding " << suckerId << "(" << siblingSuckers->atUnlocked(i).hostname << ") to connection pool..." << pmm::NL;
					connPool[suckerId] = new PMMSuckerRemoteClient(siblingSuckers->atUnlocked(i));
				}
			}
			siblingSuckers->endCriticalSection();
			while (RemoteFetchDBSyncQueue.extractEntry(item2Save)) {
				for (std::map<std::string, PMMSuckerRemoteClient *>::iterator iter = connPool.begin(); iter != connPool.end(); iter++) {
					PMMSuckerRemoteClient *theClient = iter->second;
					try {
						theClient->open();
						theClient->client->fetchDBPutItem(item2Save.email, item2Save.uid);
					} catch (apache::thrift::TException &tex1) {
						pmm::rmtSyncLog << "Unable to upload " << item2Save.uid << " to " << iter->first << ": "
						<< tex1.what() << pmm::NL;
						upLoadFailed = true;
						RemoteFetchDBSyncQueue.add(item2Save);
						break;
					}

				}
			}
			if (time(0) % 60 == 0 || upLoadFailed) {
				//Release connections
				for (std::map<std::string, PMMSuckerRemoteClient *>::iterator iter = connPool.begin(); iter != connPool.end(); iter++) {
					PMMSuckerRemoteClient *theClient = iter->second;
					theClient->close();
				}
			}
			if (upLoadFailed) {
				pmm::rmtSyncLog << "Retrying failed uid upload in 30 seconds..." << pmm::NL;
				sleep(30);
			}
			else{
				sleep(1);
			}
		}
	}
}