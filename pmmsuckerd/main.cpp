//
//  main.cpp
//  pmmsuckerd
//
//  Created by Juan V. Guerrero on 9/17/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <sstream>
#include <openssl/ssl.h>
#include <stdlib.h>
#include <fstream>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "ServerResponse.h"
#include "PMMSuckerSession.h"
#include "APNSNotificationThread.h"
#include "ThreadDispatcher.h"
#include "SharedQueue.h"
#include "NotificationPayload.h"
#include "IMAPSuckerThread.h"
#include "POP3SuckerThread.h"
#include "UtilityFunctions.h"
#include "MTLogger.h"
#include "MessageUploaderThread.h"
#include "SilentMode.h"
#include "QuotaDB.h"
#include "UserPreferences.h"
#include "APNSFeedbackThread.h"
#include "PendingNotificationStore.h"
#include "ObjectDatastore.h"
#include "RPCService.h"
#include "PMMSuckerRPC.h"
#include "FetchDBSyncThread.h"
#include "FetchDBRemoteSyncThread.h"
#include "dirent.h"
//#include "SharedMap.h"
#ifndef DEFAULT_MAX_NOTIFICATION_THREADS
#define DEFAULT_MAX_NOTIFICATION_THREADS 4
#endif
#ifndef DEFAULT_MAX_POP3_POLLING_THREADS
#define DEFAULT_MAX_POP3_POLLING_THREADS 4
#endif
#ifndef DEFAULT_MAX_IMAP_POLLING_THREADS
#define DEFAULT_MAX_IMAP_POLLING_THREADS 8
#endif
#ifndef DEFAULT_MAX_MESSAGE_UPLOADER_THREADS
#define DEFAULT_MAX_MESSAGE_UPLOADER_THREADS 2
#endif

#ifndef DEFAULT_SSL_CERTIFICATE_PATH
#define DEFAULT_SSL_CERTIFICATE_PATH "/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem"
#endif
#ifndef DEFAULT_SSL_PRIVATE_KEY_PATH
#define DEFAULT_SSL_PRIVATE_KEY_PATH "/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem"
#endif

#ifndef DEFAULT_DEVEL_SSL_CERTIFICATE_PATH
#define DEFAULT_DEVEL_SSL_CERTIFICATE_PATH "pmm_devel.pem"
#endif
#ifndef DEFAULT_DEVEL_SSL_PRIVATE_KEY_PATH
#define DEFAULT_DEVEL_SSL_PRIVATE_KEY_PATH "pmm_devel.pem"
#endif


#ifndef DEFAULT_LOGFILE
#define DEFAULT_LOGFILE "pmmsuckerd.log"
#endif
#ifndef DEFAULT_THREAD_STACK_SIZE
#define DEFAULT_THREAD_STACK_SIZE 8388608
#endif
#ifndef DEFAULT_COMMAND_POLLING_INTERVAL
#define DEFAULT_COMMAND_POLLING_INTERVAL 30
#endif

#ifdef SEND_PUSH_KEEPALIVES
#ifndef DEFAULT_KEEPALIVE_DEVTOKEN
#define DEFAULT_KEEPALIVE_DEVTOKEN "4ab904318d30a0ecee730b369ea6b4acb9ab67c10023e5b497c25e035e353af5"
#endif
#endif
#ifndef DEFAULT_INVALID_TOKEN_FILE
#define DEFAULT_INVALID_TOKEN_FILE "invalid-devices.dat"
#endif

#ifndef DEFAULT_DUMMY_MODE_ENABLED
#define DEFAULT_DUMMY_MODE_ENABLED false
#endif

void printHelpInfo();
pmm::SuckerSession *globalSession;
pmm::APNSNotificationThread *globalNotifThreads;
pmm::SharedQueue<pmm::NotificationPayload> *globalNotificationQueue;
size_t globalMaxNotificationThreads = DEFAULT_MAX_NOTIFICATION_THREADS;

void emergencyUnregister();
void signalHandler(int s);

void disableAccountsWithExceededQuota(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &accounts);
void updateAccountQuotas(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, int> &quotaInfo);
void updateAccountProperties(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo);
void addNewEmailAccount(pmm::SuckerSession &session, pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, size_t *assignationIndex, const std::string &emailAccount);
void addNewEmailAccount(pmm::SuckerSession &session, pmm::SharedQueue<pmm::MailAccountInfo> *addQueue, const std::string &emailAccount);
//void removeEmailAccount(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo);
void removeEmailAccount(pmm::SharedQueue<std::string> *rmQueue, const std::string &emailAccount);
void relinquishDevTokenNotification(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, const std::string &devToken);
void updateEmailNotificationDevices(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &params);
//void updateMailAccountQuota(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo, pmm::SharedQueue<pmm::NotificationPayload> *notificationQueue);

void retrieveAndSaveSilentModeSettings(const std::vector<pmm::MailAccountInfo> &emailAccounts);
void broadcastMessageToAll(std::map<std::string, std::string> &params, pmm::SharedQueue<pmm::NotificationPayload> &notificationQueue, pmm::SharedQueue<pmm::NotificationPayload> &pmmStorageQueue);

bool sync2RemotePoller(const pmm::PMMSuckerInfo &suckerInfo);

int main (int argc, const char * argv[])
{
	bool enableDevelAPNS = false;
	bool dummyMode = DEFAULT_DUMMY_MODE_ENABLED;
	std::string pmmServiceURL = DEFAULT_PMM_SERVICE_URL;
	std::string logFilePath = DEFAULT_LOGFILE;
	size_t maxNotificationThreads = DEFAULT_MAX_NOTIFICATION_THREADS;
	size_t maxIMAPSuckerThreads = DEFAULT_MAX_IMAP_POLLING_THREADS;
	size_t maxPOP3SuckerThreads = DEFAULT_MAX_POP3_POLLING_THREADS;
	size_t maxMessageUploaderThreads = DEFAULT_MAX_MESSAGE_UPLOADER_THREADS;
	size_t threadStackSize = DEFAULT_THREAD_STACK_SIZE;
	std::string sslCertificatePath = DEFAULT_SSL_CERTIFICATE_PATH;
	std::string sslPrivateKeyPath = DEFAULT_SSL_PRIVATE_KEY_PATH;
	std::string sslDevelCertificatePath = DEFAULT_DEVEL_SSL_CERTIFICATE_PATH;
	std::string sslDevelPrivateKeyPath = DEFAULT_DEVEL_SSL_PRIVATE_KEY_PATH;
	std::string invalidTokensFile = DEFAULT_INVALID_TOKEN_FILE;
	
	int commandPollingInterval = DEFAULT_COMMAND_POLLING_INTERVAL;
	pmm::SharedQueue<pmm::NotificationPayload> notificationQueue("NotificationQueue");
	pmm::SharedQueue<pmm::NotificationPayload> develNotificationQueue("DevelNotificationQueue");
	
	pmm::SharedVector<std::string> quotaUpdateVector;
	pmm::SharedQueue<pmm::NotificationPayload> pmmStorageQueue("StorageQueue");
	pmm::SharedQueue<pmm::QuotaIncreasePetition> quotaIncreaseQueue("QuotaIncreaseQueue");
	pmm::SharedQueue<pmm::PreferenceQueueItem> preferenceSetQueue("PreferencesMgmntQueue");
	
	pmm::SharedQueue<pmm::MailAccountInfo> addIMAPAccountQueue("AddIMAPAccountQueue");
	pmm::SharedQueue<std::string> rmIMAPAccountQueue("RemoveIMAPAccountQueue");
	pmm::SharedQueue<pmm::MailAccountInfo> addPOP3AccountQueue("AddPOP3AccountQueue");
	pmm::SharedQueue<std::string> rmPOP3AccountQueue("RemovePOP3AccountQueue");
	
	//pmm::SharedQueue<pmm::DevtokenQueueItem> devTokenAddQueue("DeviceTokenAddQueue");
	//pmm::SharedQueue<pmm::DevtokenQueueItem> devTokenRelinquishQueue("DeviceTokenRelinquishQueue");
	pmm::SharedQueue<std::string> invalidTokenQ;
	pmm::SharedQueue<std::string> develInvalidTokenQ;
	pmm::SharedQueue<std::string> gmailAuthRequestedQ;
	std::set<std::string> gmailAuthReqAlreadySent;
	
	
	pmm::SharedVector<pmm::MailAccountInfo> mailAccounts2Refresh;
	//pmm::SharedMap<std::string, int> statCounter;
	
	//Holds commands and parameters received from the realtime thrift service
	pmm::SharedVector< std::map<std::string, std::map<std::string, std::string> > > rtCommandV;
	
	pmm::SharedQueue<pmmrpc::FetchDBItem> fetchDBItems2SaveQ;
	//FetchDB item sync queue
	pmm::FetchDBSyncThread fetchDBSyncThread;
	fetchDBSyncThread.items2SaveQ = &fetchDBItems2SaveQ;
	
	pmm::PreferenceEngine preferenceEngine;
	size_t imapAssignationIndex = 0, popAssignationIndex = 0;
	SSL_library_init();
	SSL_load_error_strings();
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg.find("--url") == 0 && (i + 1) < argc) {
			pmmServiceURL = argv[++i];
		}
		else if(arg.find("--req-membership") == 0 && (i + 1) < argc){
			try {
#ifdef DEBUG
				std::cout << "Requesting to central pmm service cluster... ";
				std::cout.flush();
#endif
				pmm::SuckerSession session(pmmServiceURL);
				if(session.reqMembership("Explicit membership request from CLI.", argv[++i])) std::cout << "OK" << std::endl;
				return 0;
			} catch (pmm::ServerResponseException &se1) {
				std::cerr << "Unable to request membership to server: " << se1.errorDescription << std::endl;
				return 1;
			}
		}
		else if(arg.compare("--help") == 0){
			printHelpInfo();
			return 0;
		}
		else if(arg.compare("--max-nthreads") == 0 && (i + 1) < argc){
			std::stringstream input(argv[++i]);
			input >> maxNotificationThreads;
		}
		else if(arg.compare("--max-imap-threads") == 0 && (i + 1) < argc) {
			std::stringstream input(argv[++i]);
			input >> maxIMAPSuckerThreads;
		}
		else if(arg.compare("--max-pop3-threads") == 0 && (i + 1) < argc) {
			std::stringstream input(argv[++i]);
			input >> maxPOP3SuckerThreads;
		}
		else if(arg.compare("--ssl-certificate") == 0 && (i + 1) < argc){
			sslCertificatePath = argv[++i];
		}
		else if(arg.compare("--ssl-private-key") == 0 && (i + 1) < argc){
			sslPrivateKeyPath = argv[++i];
		}
		else if(arg.compare("--devel-ssl-certificate") == 0 && (i + 1) < argc){
			sslDevelCertificatePath = argv[++i];
		}
		else if(arg.compare("--devel-ssl-private-key") == 0 && (i + 1) < argc){
			sslDevelPrivateKeyPath = argv[++i];
		}
		else if(arg.compare("--log") == 0 && (i + 1) < argc){
			logFilePath = argv[++i];
		}
		else if(arg.compare("--command-polling-interval") == 0 && (i + 1) < argc){
			std::stringstream input(argv[++i]);
			input >> commandPollingInterval;
		}
		else if(arg.compare("--devel-apns") == 0){
			enableDevelAPNS = true;
		}
		else if(arg.compare("--dummy") == 0){
			dummyMode = true;
		}
	}
	pmm::Log.open(logFilePath);
	pmm::CacheLog.open("mailcache.log");
	pmm::CacheLog.setTag("FetchedMailsCache");
	pmm::APNSLog.open("apns.log");
	pmm::APNSLog.setTag("APNSNotificationThread");
	pmm::fdbckLog.open("apn-feedback.log");
	pmm::fdbckLog.setTag("APNSFeedbackThread");
	pmm::imapLog.open("imap-fetch.log");
	pmm::imapLog.setTag("IMAPSuckerThread");
	pmm::pop3Log.open("pop3-fetch.log");
	pmm::pop3Log.setTag("POP3SuckerThread");
	
	pmm::ObjectDatastore localConfig;

	int rpc_port;
	bool allowsIMAP;
	bool allowsPOP3;
	std::string theSecret;
	pmm::configValueGetInt("port", rpc_port);
	pmm::configValueGetBool("allowsIMAP", allowsIMAP);
	pmm::configValueGetBool("allowsPOP3", allowsPOP3);
	pmm::configValueGetString("secret", theSecret);
	
	pmm::ThreadDispatcher::start(fetchDBSyncThread);
	pmm::RPCService rpcService;
	rpcService.items2SaveQ = &fetchDBItems2SaveQ;
	rpcService.rtCommandV = &rtCommandV;
	//Start RPC service
	pmm::ThreadDispatcher::start(rpcService);

	pmm::SharedVector<pmm::PMMSuckerInfo> siblingSuckers;
	pmm::SuckerSession session(pmmServiceURL, allowsIMAP, allowsPOP3, theSecret, "fetchdb/", rpc_port);
	session.siblingSuckers = &siblingSuckers;
	theSecret = "";
	
	preferenceEngine.preferenceQueue = &preferenceSetQueue;
	//1. Register to PMMService...
	try {
		session.dummyMode = dummyMode;
		session.register2PMM();
	} catch (pmm::ServerResponseException &se1) {
		if (se1.errorCode == pmm::PMM_ERROR_SUCKER_DENIED) {
			std::cerr << "Unable to register, permission denied." << std::endl;
			pmm::Log << "Unable to register, permission denied." << pmm::NL;
			try{
				//Try to ask for membership automatically or report if a membership has already been asked
				session.reqMembership("Automated membership petition, please help!!!");
				std::cerr << "Membership request issued to pmm controller, try again later" << std::endl;
			}
			catch(pmm::ServerResponseException  &se2){
				pmm::Log << "Failed to request membership automatically: " << se2.errorDescription << pmm::NL;
				std::cerr << "Failed to request membership automatically: " << se2.errorDescription << std::endl;
			}
		}
		else {
			std::cerr << "Unable to register: " << se1.errorDescription << std::endl;
			pmm::Log << "Unable to register: " << se1.errorDescription << pmm::NL;
		}
		return 1;
	}
	pmm::FetchDBRemoteSyncThread fetchDBRemoteSyncThread(&siblingSuckers);
	pmm::ThreadDispatcher::start(fetchDBRemoteSyncThread);
	//Registration succeded, retrieve max
#ifdef DEBUG
	std::cout << "Initial registration succeded!!!" << std::endl;
#endif
	globalSession = &session;
	std::set_terminate(emergencyUnregister);
	//2. Request accounts to poll
	std::vector<pmm::MailAccountInfo> emailAccounts;
	session.retrieveEmailAddresses(emailAccounts);
	//3. Save email account information to local datastore, perform full database cleanup
	pmm::QuotaDB::clearData();
	for (size_t i = 0; i < emailAccounts.size(); i++) {
		pmm::QuotaDB::set(emailAccounts[i].email(), emailAccounts[i].quota);
	}
	retrieveAndSaveSilentModeSettings(emailAccounts);
	//4. Start APNS notification threads, validate remote devTokens
	pmm::APNSNotificationThread *notifThreads = new pmm::APNSNotificationThread[maxNotificationThreads];
	globalNotifThreads = notifThreads;
	globalMaxNotificationThreads = maxNotificationThreads;
	pmm::APNSNotificationThread develNotifThread;
	pmm::MessageUploaderThread *msgUploaderThreads = new pmm::MessageUploaderThread[maxMessageUploaderThreads];
	pmm::IMAPSuckerThread *imapSuckingThreads = new pmm::IMAPSuckerThread[maxIMAPSuckerThreads];
	pmm::POP3SuckerThread *pop3SuckingThreads = new pmm::POP3SuckerThread[maxPOP3SuckerThreads];
	
	pmm::APNSFeedbackThread feedbackThread;
	feedbackThread.setKeyPath(sslPrivateKeyPath);
	feedbackThread.setCertPath(sslCertificatePath);
	feedbackThread.useForProduction();
	
	//Load invalid device tokens into each notification thread
	{
		std::ifstream tfile(invalidTokensFile.c_str());
		if (tfile.good()) {
			while (!tfile.eof()) {
				std::string tokenItem;
				std::getline(tfile, tokenItem);
				if(tokenItem.size() > 0){
					pmm::Log << " * Adding " << tokenItem << " to invalid token cache on each notification thread." << pmm::NL;
					for (size_t idx = 0; idx < maxNotificationThreads; idx++) {
						notifThreads[idx].invalidTokenSet.insert(tokenItem);
					}
				}
			}
			tfile.close();
		}
	}
	for (size_t i = 0; i < maxNotificationThreads; i++) {
		//1. Initializa notification thread...
		//2. Start thread
		notifThreads[i].useForProduction();
		notifThreads[i].notificationQueue = &notificationQueue;
		notifThreads[i].setCertPath(sslCertificatePath);
		notifThreads[i].setKeyPath(sslPrivateKeyPath);
		notifThreads[i].invalidTokens = &invalidTokenQ;
		notifThreads[i].dummyMode = dummyMode;
		pmm::ThreadDispatcher::start(notifThreads[i], threadStackSize);
		//sleep(1);
	}
	pmm::Log << "Starting development notification thread..." << pmm::NL;
	develNotifThread.notificationQueue = &develNotificationQueue;
	develNotifThread.setCertPath(sslDevelCertificatePath);
	develNotifThread.setKeyPath(sslDevelPrivateKeyPath);
	develNotifThread.invalidTokens = &develInvalidTokenQ;
	develNotifThread.dummyMode = dummyMode;
	pmm::ThreadDispatcher::start(develNotifThread, threadStackSize);
	//sleep(1);
	
	for (size_t i = 0; i < maxMessageUploaderThreads; i++) {
		msgUploaderThreads[i].session = &session;
		msgUploaderThreads[i].pmmStorageQueue = &pmmStorageQueue;
		msgUploaderThreads[i].dummyMode = dummyMode;
		pmm::ThreadDispatcher::start(msgUploaderThreads[i], threadStackSize);
	}
	
	//Initiate Preference Management Engine
	pmm::ThreadDispatcher::start(preferenceEngine, threadStackSize);
	std::vector<pmm::MailAccountInfo> imapAccounts, pop3Accounts;
	pmm::splitEmailAccounts(emailAccounts, imapAccounts, pop3Accounts);
	//5. Dispatch polling threads for imap
	for (size_t k = 0; k < imapAccounts.size(); k++){
		imapSuckingThreads[imapAssignationIndex++].emailAccounts.push_back(imapAccounts[k]);
		if (imapAssignationIndex >= maxIMAPSuckerThreads) {
			imapAssignationIndex = 0;
		}
	}
	
	//Install SEGFAULT signal handler
	signal(SIGSEGV, signalHandler);
	
	
	for (size_t i = 0; i < maxIMAPSuckerThreads; i++) {
		imapSuckingThreads[i].notificationQueue = &notificationQueue;
		imapSuckingThreads[i].quotaUpdateVector = &quotaUpdateVector;
		imapSuckingThreads[i].pmmStorageQueue = &pmmStorageQueue;
		imapSuckingThreads[i].quotaIncreaseQueue = &quotaIncreaseQueue;
		imapSuckingThreads[i].addAccountQueue = &addIMAPAccountQueue;
		imapSuckingThreads[i].rmAccountQueue = &rmIMAPAccountQueue;
		//imapSuckingThreads[i].devTokenAddQueue = &devTokenAddQueue;
		//imapSuckingThreads[i].devTokenRelinquishQueue = &devTokenRelinquishQueue;
		imapSuckingThreads[i].develNotificationQueue = &develNotificationQueue;
		imapSuckingThreads[i].mailAccounts2Refresh = &mailAccounts2Refresh;
		imapSuckingThreads[i].gmailAuthRequestedQ = &gmailAuthRequestedQ;
		imapSuckingThreads[i].localConfig = &localConfig;
		pmm::ThreadDispatcher::start(imapSuckingThreads[i], threadStackSize);
		usleep(10000);
	}
	//6. Dispatch polling threads for POP3
	for (size_t k = 0; k < pop3Accounts.size(); k++) {
		pop3SuckingThreads[popAssignationIndex++].emailAccounts.push_back(pop3Accounts[k]);
		if (popAssignationIndex >= maxPOP3SuckerThreads) {
			popAssignationIndex = 0;
		}
	}
	for (size_t i = 0; i < maxPOP3SuckerThreads; i++) {
		pop3SuckingThreads[i].notificationQueue = &notificationQueue;
		pop3SuckingThreads[i].quotaUpdateVector = &quotaUpdateVector;
		pop3SuckingThreads[i].pmmStorageQueue = &pmmStorageQueue;
		pop3SuckingThreads[i].quotaIncreaseQueue = &quotaIncreaseQueue;
		pop3SuckingThreads[i].addAccountQueue = &addPOP3AccountQueue;
		pop3SuckingThreads[i].rmAccountQueue = &rmPOP3AccountQueue;
		//pop3SuckingThreads[i].devTokenAddQueue = &devTokenAddQueue;
		//pop3SuckingThreads[i].devTokenRelinquishQueue = &devTokenRelinquishQueue;
		pop3SuckingThreads[i].develNotificationQueue = &develNotificationQueue;
		pop3SuckingThreads[i].mailAccounts2Refresh = &mailAccounts2Refresh;
		pop3SuckingThreads[i].gmailAuthRequestedQ = &gmailAuthRequestedQ;
		pop3SuckingThreads[i].localConfig = &localConfig;
		pmm::ThreadDispatcher::start(pop3SuckingThreads[i], threadStackSize);
		usleep(10000);
	}
	globalNotificationQueue = &notificationQueue;
	
	//7. Dispatch the APN feedback interrogator thread
	pmm::ThreadDispatcher::start(feedbackThread, threadStackSize);
	
	//8. After registration time ends, close every connection, return to Step 1
	int tic = 1;
	std::map<std::string, int> quotas;
	bool keepRunning = true;
	
	//9. Dispatch pending notifications not sent due to a unexpected crash or unhandled exception
	pmm::PendingNotificationStore::loadPayloads(&notificationQueue);
	//Local counters
	int cntAccountsAdded = 0;
	int cntAccountsRemoved = 0;
	int cntAccountsUpdated = 0;
	int cntNoQuota = 0;
	int cntDeviceReg = 0;
	int cntDeviceUnReg = 0;
	while (keepRunning) {
		try {
			session.performAutoRegister();
		} catch (pmm::HTTPException &httex) {
			pmm::Log << "CRITICAL: Unable to re-register, something's wrong with app engine: " << httex.errorMessage() << pmm::NL;
			sleep(1);
			continue;
		}
		std::string lastInvalidToken;
		std::string gmailAccount2ReqAdditionalAuth;
		while (gmailAuthRequestedQ.extractEntry(gmailAccount2ReqAdditionalAuth)) {
			if (gmailAuthReqAlreadySent.find(gmailAccount2ReqAdditionalAuth) == gmailAuthReqAlreadySent.end()) {
				gmailAuthReqAlreadySent.insert(gmailAccount2ReqAdditionalAuth);
				pmm::Log << "INFO: Reporting " << gmailAccount2ReqAdditionalAuth << " to authorize PushMeMail in Gmail" << pmm::NL;
				//Send a complimentary e-mail to user reporting that additional steps are required
				//to authorize account polling
				session.notifyGmailAdditionalAuth(gmailAccount2ReqAdditionalAuth, "en");
			}
		}
		
		while (invalidTokenQ.extractEntry(lastInvalidToken)) {
			pmm::Log << "CRITICAL: " << lastInvalidToken << " is an invalid token, no more messages will be sent to it!!" << pmm::NL;
			//First update each notification thread internal cache
			for (size_t idx = 0; idx < maxNotificationThreads; idx++) {
				notifThreads[idx].newInvalidDevToken = lastInvalidToken;
			}
			try {
				//Here we update appengine and tell it we have one new invalid device token
				std::vector<std::string> vx;
				vx.push_back(lastInvalidToken);
				session.reportInvalidDeviceToken(vx);
				//Save token to local file
				std::ofstream tfile(invalidTokensFile.c_str(), std::ios_base::app);
				tfile << lastInvalidToken << "\n";
				tfile.close();
			}
			catch (pmm::HTTPException &httex) {
				break;
			}
		}
		if (tic % 43200 == 0){
			pmm::PendingNotificationStore::eraseOldPayloads();
		}
		if (tic % 300 == 0){
			//Save stats to log
			int sent = 0, failed = 0;
			for (int j = 0; j < maxNotificationThreads; j++) {
				sent += notifThreads[j].cntMessageSent;
				failed += notifThreads[j].cntMessageFailed;
				notifThreads[j].cntMessageSent = 0;
				notifThreads[j].cntMessageFailed = 0;
			}
			pmm::Log << "=================================================================" << pmm::NL;
			std::map<std::string, double> statMap;
			statMap["notitications.sent"] = sent;
			statMap["notitications.failed"] = failed;
			pmm::Log << "STAT: Notifications sent: " << (double)(sent / 300.0) << "/sec failed: " << (double)(failed / 300.0) << "/sec" << pmm::NL;
			int msgRetrieved = 0, acctTotal = 0, bytesDlds = 0, failedLogins = 0;
			for (int j = 0; j < maxIMAPSuckerThreads; j++) {
				msgRetrieved += imapSuckingThreads[j].cntRetrievedMessages;
				acctTotal += imapSuckingThreads[j].cntAccountsTotal;
				//bytesDlds += imapSuckingThreads[j].cntBytesDownloaded;
				failedLogins += imapSuckingThreads[j].cntFailedLoginAttempts;
				imapSuckingThreads[j].cntRetrievedMessages = 0;
				imapSuckingThreads[j].cntBytesDownloaded = 0;
				imapSuckingThreads[j].cntFailedLoginAttempts = 0;
			}
			statMap["imap.received"] = msgRetrieved;
			statMap["imap.accounts.monitored"] = acctTotal;
			statMap["imap.data.downloaded"] = bytesDlds;
			statMap["imap.login.failed"] = failedLogins;
			pmm::Log << "STAT: IMAP messages retrieved: " << (double)(msgRetrieved / 300.0) << "/sec. Monitored accounts: " << acctTotal << pmm::NL;
			pmm::Log << "STAT: IMAP downloaded data: ";
			if (bytesDlds > 1048576){
				pmm::Log << (double)(bytesDlds / 1048576.0) << "M";
			}
			else if (bytesDlds > 1024 == 0) {
				pmm::Log << (double)(bytesDlds / 1024.0) << "K";
			}
			else {
				pmm::Log << bytesDlds << "bytes";
			}
			pmm::Log << ". Rate: " << (double)(bytesDlds / 300.0) << " bytes/sec" << pmm::NL;
			pmm::Log << "STAT: IMAP Failed logins: " << failedLogins << ". Rate: " << (double)(failedLogins / 300.0) << "/sec" << pmm::NL;

			msgRetrieved = 0;
			acctTotal = 0;
			bytesDlds = 0;
			failedLogins = 0;
			for (int j = 0; j < maxPOP3SuckerThreads; j++) {
				msgRetrieved += pop3SuckingThreads[j].cntRetrievedMessages;
				acctTotal += pop3SuckingThreads[j].cntAccountsTotal;
				bytesDlds += pop3SuckingThreads[j].cntBytesDownloaded;
				failedLogins += pop3SuckingThreads[j].cntFailedLoginAttempts;
				pop3SuckingThreads[j].cntRetrievedMessages = 0;
				pop3SuckingThreads[j].cntBytesDownloaded = 0;
				pop3SuckingThreads[j].cntFailedLoginAttempts = 0;
			}
			statMap["pop3.received"] = msgRetrieved;
			statMap["pop3.accounts.monitored"] = acctTotal;
			statMap["pop3.data.downloaded"] = bytesDlds;
			statMap["pop3.login.failed"] = failedLogins;
			pmm::Log << "STAT: POP3 messages retrieved: " << (double)(msgRetrieved / 300.0) << "/sec. Monitored accounts: " << acctTotal << pmm::NL;
			pmm::Log << "STAT: POP3 downloaded data: ";
			if (bytesDlds > 1048576){
				pmm::Log << (double)(bytesDlds / 1048576.0) << "M";
			}
			else if (bytesDlds > 1024 == 0) {
				pmm::Log << (double)(bytesDlds / 1024.0) << "K";
			}
			else {
				pmm::Log << bytesDlds << "bytes";
			}
			pmm::Log << ". Rate: " << (double)(bytesDlds / 300.0) << " bytes/sec" << pmm::NL;
			pmm::Log << "STAT: POP3 Failed logins: " << failedLogins << ". Rate: " << (double)(failedLogins / 300.0) << "/sec" << pmm::NL;
			statMap["emailAccounts.added"] = cntAccountsAdded;
			statMap["emailAccounts.removed"] = cntAccountsRemoved;
			statMap["emailAccounts.updated"] = cntAccountsUpdated;
			statMap["quota.zero"] = cntNoQuota;
			statMap["device.reg"] = cntDeviceReg;
			statMap["device.unreg"] = cntDeviceUnReg;
			cntAccountsAdded = cntAccountsRemoved = cntAccountsUpdated = cntNoQuota = cntDeviceReg = cntDeviceUnReg = 0;
			session.putStatMultiple(statMap);
		}
		if (tic % 45 == 0) {
			//Process quota updates if any
			std::vector<std::string> quotaVec;
			quotaUpdateVector.copyTo(quotaVec);
			if (quotaVec.size() > 0) {
				pmm::Log << "Sending quota updates to app engine..." << pmm::NL;
				quotaUpdateVector.beginCriticalSection();
				quotaUpdateVector.unlockedCopyTo(quotaVec);
				for (size_t i = 0; i < quotaVec.size(); i++) {
					if (quotas.find(quotaVec[i]) == quotas.end()) {
						quotas[quotaVec[i]] = 0;
					}
					quotas[quotaVec[i]] = quotas[quotaVec[i]] + 1;
				}
				quotaUpdateVector.unlockedClear();
				quotaUpdateVector.endCriticalSection();
				//Report quota changes to pmm service.
				try{
					if(session.reportQuotas(quotas)){
						//updateAccountQuotas(imapSuckingThreads, maxIMAPSuckerThreads, quotas);
						//updateAccountQuotas(pop3SuckingThreads, maxPOP3SuckerThreads, quotas);
						quotas.clear();
					}
				}
				catch(pmm::HTTPException &htex0){
					pmm::Log << "Unable to update upstream quotas due to: " << htex0.errorMessage() << ", I will retry in the next cycle" << pmm::NL;
				}
				//In case we failed to report any quotas the service will re-report them again
			}
		}
		if(tic % 86400 == 0){
			//Cleanup the fetch mail cache database
			pmm::Log << "Expiring old POP3 e-mail previews from cache..." << pmm::NL;
			//pmm::FetchedMailsCache fCache;
			//fCache.expireOldEntries();
		}

		bool gotRTCommands = false;
		if (rtCommandV.size() > 0) {
			gotRTCommands = true;
		}
		if(tic % commandPollingInterval == 0 || gotRTCommands){ //Server commands processing
			try{
				bool doCmdCheck = false;
				if (!gotRTCommands) {
					if(doCmdCheck == false && session.fnxHasPendingTasks()){
						doCmdCheck = true;
					}
					int nNotif = (int) notificationQueue.size();
					if(nNotif > 0) pmm::Log << "Notification queue has " << nNotif << " pending elements" << pmm::NL;
				}
				else {
					doCmdCheck = true;
				}
				if(doCmdCheck){
					std::vector< std::map<std::string, std::map<std::string, std::string> > > tasksToRun;
					size_t nTasks;
					if (gotRTCommands) {
						//Copy the rtCommand set to tasksToRun
						rtCommandV.moveTo(tasksToRun);
						nTasks = tasksToRun.size();
					}
					else {
						nTasks = session.getPendingTasks(tasksToRun);
					}
					for (size_t i = 0 ; i < nTasks; i++) {
						//Define iterator, run thru every single key to determine the command, if needed also make use of any parameters
						std::map<std::string, std::map<std::string, std::string> >::iterator iter = tasksToRun[i].begin();
						std::string command = iter->first;
						std::map<std::string, std::string> parameters = iter->second;
						if(command.compare("shutdown") == 0){
							keepRunning = false;
							break;
						}
						else if (command.compare(pmm::Commands::quotaExceeded) == 0){
							disableAccountsWithExceededQuota(imapSuckingThreads, maxIMAPSuckerThreads, parameters);
							//disableAccountsWithExceededQuota(pop3SuckingThreads, maxPOP3SuckerThreads, parameters);
						}
						else if (command.compare(pmm::Commands::accountPropertyChanged) == 0){
							if (parameters["mailboxType"].compare("IMAP") == 0) {
								updateAccountProperties(imapSuckingThreads, maxIMAPSuckerThreads, parameters);
							}
							else {
								updateAccountProperties(pop3SuckingThreads, maxPOP3SuckerThreads, parameters);
							}
							cntAccountsUpdated++;
						}
						else if (command.compare(pmm::Commands::mailAccountQuotaChanged) == 0){
							pmm::QuotaIncreasePetition p;
							p.emailAddress = parameters["email"];
							std::stringstream input(parameters["quota"]);
							input >> p.quotaValue;
							quotaIncreaseQueue.add(p);
							/*std::stringstream lnx;
							 lnx << "User " << p.emailAddress << " has increased her quota to: " << p.quotaValue;
							 pmm::NotificationPayload np(DEFAULT_KEEPALIVE_DEVTOKEN, lnx.str(), 1, "sln.caf");
							 notificationQueue.add(np);*/
						}
						else if (command.compare(pmm::Commands::newMailAccountRegistered) == 0){
							if (parameters["mailboxType"].compare("IMAP") == 0) {
								//addNewEmailAccount(session, imapSuckingThreads, maxIMAPSuckerThreads, &imapAssignationIndex, parameters["email"]);
								addNewEmailAccount(session, &addIMAPAccountQueue, parameters["email"]);
							}
							else {
								//addNewEmailAccount(session, pop3SuckingThreads, maxPOP3SuckerThreads, &popAssignationIndex, parameters["email"]);
								addNewEmailAccount(session, &addPOP3AccountQueue, parameters["email"]);
							}
							cntAccountsAdded++;
						}
						else if (command.compare(pmm::Commands::relinquishDevToken) == 0){
							for (std::map<std::string, std::string>::iterator reliter = parameters.begin(); reliter != parameters.end(); reliter++) {
								pmm::DevtokenQueueItem item;
								item.email = reliter->first;
								item.devToken = reliter->second;
								item.expirationTimestamp = time(0) + 10; //Every thread has at least 10 seconds to release a device token
								//devTokenRelinquishQueue.add(item);
								//Add this device token to evry single monitoring thread
								for (int j = 0; j < maxIMAPSuckerThreads; j++) {
									imapSuckingThreads[j].devTokenRelinquishQueue.add(item);
								}
								for (int j = 0; j < maxPOP3SuckerThreads; j++) {
									pop3SuckingThreads[j].devTokenRelinquishQueue.add(item);
								}

							}
						}
						else if (command.compare(pmm::Commands::refreshDeviceTokenList) == 0){
							std::set<std::string> addedTokens;
							for (std::map<std::string, std::string>::iterator reliter = parameters.begin(); reliter != parameters.end(); reliter++) {
								pmm::DevtokenQueueItem item;
								item.email = reliter->first;
								item.devToken = reliter->second;
								item.expirationTimestamp = time(0) + 60;
								if(addedTokens.find(item.devToken) == addedTokens.end()){
									addedTokens.insert(item.devToken);
									//Now schedule the device token to be removed from any APNS invalidating list
									for (size_t j = 0; j < maxNotificationThreads; j++) {
										notifThreads[j].permitDeviceToken.add(item.devToken);
										std::stringstream cmd;
#warning TODO: Find a better way to edit the invalidTokensFile
										cmd << "cat " << invalidTokensFile << " | grep -v " << item.devToken << " > /tmp/invtoken.dat && cp -f /tmp/invtoken.dat " << invalidTokensFile;
										system(cmd.str().c_str());
									}
								}
								//Add device token to any appropiate IMAP e-mail accounts
								for (int j = 0; j < maxIMAPSuckerThreads; j++) {
									imapSuckingThreads[j].devTokenAddQueue.add(item);
								}
								//Add device token to any appropiate POP3 e-mail accounts
								for (int j = 0; j < maxPOP3SuckerThreads; j++) {
									pop3SuckingThreads[j].devTokenAddQueue.add(item);
								}

							}
						}
						else if (command.compare(pmm::Commands::deleteEmailAccount) == 0){
							if (parameters.find("mailboxType") != parameters.end()) {
								if (parameters["mailboxType"].compare("IMAP") == 0) {
									removeEmailAccount(&rmIMAPAccountQueue, parameters["email"]);
								}
								else {
									removeEmailAccount(&rmPOP3AccountQueue, parameters["email"]);
								}
								cntAccountsRemoved++;
							}
							else {
								pmm::Log << "Malformed account remove request received!!!" << pmm::NL;
								abort();
							}
						}
						else if (command.compare(pmm::Commands::silentModeSet) == 0){
							pmm::SilentMode::set(parameters["email"], atoi(parameters["startHour"].c_str()), atoi(parameters["startMinute"].c_str()), atoi(parameters["endHour"].c_str()), atoi(parameters["endMinute"].c_str()));
						}
						else if (command.compare(pmm::Commands::silentModeClear) == 0){
							pmm::SilentMode::clear(parameters["email"]);
						}
						else if (command.compare(pmm::Commands::refreshUserPreference) == 0){
							pmm::PreferenceQueueItem prefItem;
							prefItem.emailAccount = parameters["email"];
							prefItem.settingKey = parameters["settingKey"];
							prefItem.settingValue = parameters["settingValue"];
							preferenceSetQueue.add(prefItem);
							if (getenv("DEBUG")) {
								pmm::Log << "Refreshing user(" << prefItem.emailAccount << ") " << prefItem.settingKey << "=" << prefItem.settingValue << pmm::NL;
							}
						}
						else if (command.compare(pmm::Commands::broadcastMessage) == 0){
							pmm::Log << "Prepare to broadcast message: " << parameters["message"] << " to " << parameters["count"] << " devices, hang on tight!!!" << pmm::NL;
							broadcastMessageToAll(parameters, notificationQueue, pmmStorageQueue);
						}
						else if (command.compare(pmm::Commands::refreshEmailAccount) == 0){
							//Tricky one, prepare to refresh user account
							//Retrieve information of account...
							pmm::MailAccountInfo info;
							if(session.retrieveEmailAddressInfo(info, parameters["email"])){
								pmm::Log << "INFO: Sending account " << parameters["email"] << "=" << info.email() << " to the update queue..." << pmm::NL;
								mailAccounts2Refresh.push_back(info);
								cntAccountsUpdated++;
							}
							else {
								pmm::Log << "CRITICAL: Unable to retrieve " << parameters["email"] << " entry from pmm service :-(" << pmm::NL;
							}
						}
						else if (command.compare(pmm::Commands::sendMessageToDevice) == 0) {
							std::string emailAccount = parameters["emailAccount"];
							//Find device token
							for (size_t l = 0; l < emailAccounts.size(); l++) {
								std::string theEmail = emailAccounts[l].email();
								if (emailAccount.compare(theEmail) == 0) {
									bool useDevel = emailAccounts[l].devel;
									std::vector<std::string> devTokens = emailAccounts[l].devTokens();
									pmm::Log << "Sending direct message to " << theEmail << pmm::NL;
									for (size_t m = 0; m < devTokens.size(); m++) {
										pmm::NotificationPayload np(devTokens[m], parameters["msg"]);
										np.isSystemNotification = true;
										if(useDevel) develNotificationQueue.add(np);
										else notificationQueue.add(np);
									}
									break;
								}
							}
						}
						else if (command.compare(pmm::Commands::sendNoQuotaMessageToDevice) == 0) {
							std::string emailAccount = parameters["emailAccount"];
							for (size_t l = 0; l < emailAccounts.size(); l++) {
								std::string theEmail = emailAccounts[l].email();
								if (emailAccount.compare(theEmail) == 0) {
									bool useDevel = emailAccounts[l].devel;
									std::vector<std::string> devTokens = emailAccounts[l].devTokens();
									pmm::Log << "Sending no-quota direct message to " << theEmail << pmm::NL;
									for (size_t m = 0; m < devTokens.size(); m++) {
										pmm::NoQuotaNotificationPayload npi(devTokens[m], theEmail);
										if(useDevel) develNotificationQueue.add(npi);
										else notificationQueue.add(npi);
									}
									break;
								}
							}
						}
						else if (command.compare(pmm::Commands::sync2RemotePoller) == 0) {
							//Add remote fetchdb sync code in here!!!
							pmm::PMMSuckerInfo suckerInfo;
							suckerInfo.suckerID = parameters["suckerID"];
							std::string currentSuckerID;
							session.getSuckerID(currentSuckerID);
							if (currentSuckerID.compare(suckerInfo.suckerID) != 0){
								pmm::mailboxPollBlocked = true;
								suckerInfo.secret = parameters["secret"];
								suckerInfo.hostname = parameters["host"];
								std::istringstream num(parameters["port"]);
								num >> suckerInfo.port;
								suckerInfo.allowsIMAP = pmm::getBoolFromString(parameters["allowsIMAP"]);
								suckerInfo.allowsPOP3 = pmm::getBoolFromString(parameters["allowsPOP3"]);
								//Wait 2 seconds before starting sync...
								sleep(2);
								sync2RemotePoller(suckerInfo);
								pmm::mailboxPollBlocked = false;
							}
							else {
								pmm::Log << "INFO: ignoring self-made full sync request" << pmm::NL;
							}
						}
						else {
							pmm::Log << "CRITICAL: Unknown command received from central controller: " << command << pmm::NL;
						}
					}
				}
			}
			catch(pmm::HTTPException &htex1){
				pmm::Log << "CRITICAL: Got " << htex1.errorMessage() << " while trying to retrieve pending tasks, I'll retry in the next cycle, in the meantime please verify the logs" << pmm::NL;
			}
		}
		//Sleep for a second, we don't want to hog the CPU right?
		sleep(1);
		tic++;
	}
	session.unregisterFromPMM();
    return 0;
}

void printHelpInfo() {
	std::cout << "Argument           Parameter  Help text" << std::endl;
	std::cout << "--help                        Shows this help message." << std::endl;
	std::cout << "--req-membership   <email>    Asks for membership in the PMM Controller cluster" << std::endl;
	std::cout << "--url              <URL>      Specifies the PMMServer service URL" << std::endl;
	std::cout << "--max-nthreads	 <number>	Changes the maximum amount of threads to allocate for push notification handling" << std::endl;
	std::cout << "--max-imap-threads <number>   Specifies the amount of threads to dispatch for IMAP mailbox sucking" << std::endl;
	std::cout << "--max-pop3-threads <number>   Specifies the amount of threads to dispatch for POP3 mailbox sucking" << std::endl;
	std::cout << "--ssl-certificate  <file>     Path where the SSL certificate for APNS is located" << std::endl;
	std::cout << "--ssl-private-key  <file>     Path where the SSL certificate private key is located" << std::endl;
}

void emergencyUnregister(){
	std::cerr << "Some weird exception just happened, need to save some application state..." << std::endl;
	for (size_t i = 0; i < globalMaxNotificationThreads; i++) {
		globalNotifThreads[i].stopExecution = true;
	}
	//Save current notification information for future retrieval...
	std::cerr << "Saving pending notifications for future dispatch..." << std::endl;
	pmm::PendingNotificationStore::savePayloads(globalNotificationQueue);
	std::cerr << "Triggering emergency unregister, some unhandled exception ocurred :-(" << std::endl;
	globalSession->unregisterFromPMM();
	//Save exit time...
	std::ofstream of("apns-logout-time.log", std::ios_base::trunc);
	of << time(0);
	of.close();
	std::set_terminate(abort);
	abort();
}

void signalHandler(int s){
	signal(SIGSEGV, SIG_DFL);
	if(s == SIGSEGV) emergencyUnregister();
	abort();
}

void disableAccountsWithExceededQuota(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &accounts){
	//Disable email accounts that require it
	/*for (size_t k = 0; k < nElems; k++) {
	 mailSuckerThreads[k].emailAccounts.beginCriticalSection();
	 for (std::map<std::string, std::string>::iterator iter2 = accounts.begin(); iter2 != accounts.end(); iter2++) {
	 for (size_t l = 0; l < mailSuckerThreads[k].emailAccounts.unlockedSize(); l++) {
	 if (mailSuckerThreads[k].emailAccounts.atUnlocked(l).email().compare(iter2->second) == 0) {
	 mailSuckerThreads[k].emailAccounts.atUnlocked(l).quota = 0;
	 mailSuckerThreads[k].emailAccounts.atUnlocked(l).isEnabled = false;
	 #ifdef DEBUG
	 pmm::Log << "disableAccountsWithExceededQuota: disabling monitoring for: " << mailSuckerThreads[k].emailAccounts.atUnlocked(l).email() << pmm::NL;
	 #endif
	 std::vector<std::string> dt = mailSuckerThreads[k].emailAccounts.atUnlocked(l).devTokens();
	 std::stringstream msg;
	 msg << "You have ran out of quota on " << mailSuckerThreads[k].emailAccounts.atUnlocked(l).email() << ", you may purchase more to keep receiving notifications.";
	 for (size_t z = 0; z < dt.size(); z++) {
	 pmm::NotificationPayload np(dt[z], msg.str());
	 np.isSystemNotification = false;
	 mailSuckerThreads[k].notificationQueue->add(np);
	 }
	 }
	 }
	 }
	 mailSuckerThreads[k].emailAccounts.endCriticalSection();
	 }*/
}

void updateAccountQuotas(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, int> &quotaInfo){
	for (std::map<std::string, int>::iterator iter = quotaInfo.begin(); iter != quotaInfo.end(); iter++) {
		for (size_t j = 0; j < nElems; j++) {
			mailSuckerThreads[j].emailAccounts.beginCriticalSection();
			for (size_t k = 0; k < mailSuckerThreads[j].emailAccounts.unlockedSize(); k++) {
				if (mailSuckerThreads[j].emailAccounts.atUnlocked(k).email().compare(iter->first) == 0) {
					mailSuckerThreads[j].emailAccounts.atUnlocked(k).quota -= iter->second;
					if(mailSuckerThreads[j].emailAccounts.atUnlocked(k).quota <= 0){
						mailSuckerThreads[j].emailAccounts.atUnlocked(k).isEnabled = false;
#ifdef DEBUG
						pmm::Log << "updateAccountQuotas: disabling monitoring for: " << mailSuckerThreads[j].emailAccounts.atUnlocked(k).email() << pmm::NL;
#endif
					}
#ifdef DEBUG
					else {
						pmm::Log << "updateAccountQuotas: " << mailSuckerThreads[j].emailAccounts.atUnlocked(k).email() << " has been notified " << iter->second << " times, remaining=" << mailSuckerThreads[j].emailAccounts.atUnlocked(k).quota << pmm::NL;
					}
#endif
				}
			}
			mailSuckerThreads[j].emailAccounts.endCriticalSection();
		}
	}
}

void updateAccountProperties(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo) {
	std::string mailAccount = mailAccountInfo["email"];
	bool accountFound = false;
	for (size_t j = 0; j < nElems && !accountFound; j++) {
		mailSuckerThreads[j].emailAccounts.beginCriticalSection();
		for (size_t k = 0; k < mailSuckerThreads[j].emailAccounts.unlockedSize() && !accountFound; k++) {
			if (mailAccount.compare(mailSuckerThreads[j].emailAccounts.atUnlocked(k).email()) == 0) {
				//Update all metadata;
				int serverPort, _useSSL;
				bool useSSL;
				std::stringstream input(mailAccountInfo["serverPort"]);
				input >> serverPort;
				std::stringstream input2(mailAccountInfo["useSSL"]);
				input2 >> _useSSL;
				if(_useSSL == 0) useSSL = false;
				else useSSL = true;
				//Build new devtoken vector
				std::vector<std::string> devTokens;
				std::string devToken_s = mailAccountInfo["devTokens"];
				size_t cpos;
				while ((cpos = devToken_s.find(",")) != devToken_s.npos) {
					std::string dTok = devToken_s.substr(0, cpos);
					devTokens.push_back(dTok);
					devToken_s = devToken_s.substr(cpos + 1);
				}
				if(devToken_s.size() > 0) devTokens.push_back(devToken_s);
				mailSuckerThreads[j].emailAccounts.atUnlocked(k).updateInfo(mailAccountInfo["password"],
																			mailAccountInfo["serverAddress"],
																			serverPort,
																			devTokens,
																			useSSL);
				accountFound = true;
			}
		}
		mailSuckerThreads[j].emailAccounts.endCriticalSection();
	}
}

void addNewEmailAccount(pmm::SuckerSession &session, pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, size_t *assignationIndex, const std::string &emailAccount) {
	//First retrieve information about the account...
	pmm::MailAccountInfo m;
	if(session.retrieveEmailAddressInfo(m, emailAccount)){
		size_t idx = *assignationIndex;
		mailSuckerThreads[idx++].emailAccounts.push_back(m);
		if(idx >= nElems) idx = 0;
		*assignationIndex = idx;
		
		std::stringstream msg;
		if(emailAccount.find("hotmail") == emailAccount.npos){
			msg << "Monitoring of " + emailAccount + " has been enabled :-)";
		}
		else {
			msg << "Monitoring of " + emailAccount + " has been enabled :-)\n\nNotice: polling of hotmail is not realtime.";
		}
		std::vector<std::string> myDevTokens = m.devTokens();
		for (size_t i = 0; i < myDevTokens.size(); i++) {
			pmm::NotificationPayload np(myDevTokens[i], msg.str());
			np.isSystemNotification = true;
			mailSuckerThreads[0].notificationQueue->add(np);
		}
		pmm::QuotaDB::set(m.email(), m.quota);
	}
#ifdef DEBUG
	else {
		pmm::Log << "WARNING: No information returned from app engine regarding " << emailAccount << ", perhaps it is being monitored by another sucker?" << pmm::NL;
	}
#endif
}

void addNewEmailAccount(pmm::SuckerSession &session, pmm::SharedQueue<pmm::MailAccountInfo> *addQueue, const std::string &emailAccount){
	pmm::MailAccountInfo m;
	if(session.retrieveEmailAddressInfo(m, emailAccount)){
		addQueue->add(m);
		pmm::QuotaDB::set(m.email(), m.quota);
		//std::stringstream lnx;
		//lnx << "User " << m.username() << " has added a new e-mail account: " << m.email();
		//pmm::NotificationPayload np(DEFAULT_KEEPALIVE_DEVTOKEN, lnx.str(), 1, "sln.caf");
	}
#ifdef DEBUG
	else {
		pmm::Log << "WARNING: No information returned from app engine regarding " << emailAccount << ", perhaps it is being monitored by another sucker?" << pmm::NL;
	}
#endif
}

/*void removeEmailAccount(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo){
 if(mailAccountInfo.size() == 0) return;
 for (size_t i = 0; i < nElems; i++) {
 mailSuckerThreads[i].emailAccounts.beginCriticalSection();
 for (size_t j = 0; j < mailSuckerThreads[i].emailAccounts.unlockedSize(); j++) {
 std::string theEmail = mailSuckerThreads[i].emailAccounts.atUnlocked(j).email();
 if (theEmail.compare(mailAccountInfo["email"]) == 0) {
 pmm::Log << "Removing e-mail account " << mailAccountInfo["email"] << " because it was deleted from the client app." << pmm::NL;
 mailSuckerThreads[i].emailAccounts.unlockedErase(j);
 mailSuckerThreads[i].emailAccounts.endCriticalSection();
 mailAccountInfo.erase(theEmail);
 return;
 }
 }
 mailSuckerThreads[i].emailAccounts.endCriticalSection();
 }
 }*/

void removeEmailAccount(pmm::SharedQueue<std::string> *rmQueue, const std::string &emailAccount){
	rmQueue->add(emailAccount);
}

void relinquishDevTokenNotification(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, const std::string &devToken) {
	pmm::Log << "Relinquishing notifications to " << devToken << pmm::NL;
	for (size_t i = 0; i < nElems; i++) {
		mailSuckerThreads[i].emailAccounts.beginCriticalSection();
		for (size_t j = 0; j < mailSuckerThreads[i].emailAccounts.unlockedSize(); j++) {
			pmm::MailAccountInfo m = mailSuckerThreads[i].emailAccounts.atUnlocked(j);
			std::vector<std::string> myDevTokens = m.devTokens();
			for (size_t k = 0; k < myDevTokens.size(); k++) {
				if (myDevTokens[k].compare(devToken) == 0) {
					//Erase the devToken
					mailSuckerThreads[i].emailAccounts.atUnlocked(j).deviceTokenRemove(devToken);
					pmm::Log << mailSuckerThreads[i].emailAccounts.atUnlocked(j).email() << " will no longer receive notifications on device " << devToken << pmm::NL;
					break;
				}
			}
		}
		mailSuckerThreads[i].emailAccounts.endCriticalSection();
	}
}

void updateEmailNotificationDevices(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &params) {
	for (size_t i = 0; i < nElems; i++) {
		mailSuckerThreads[i].emailAccounts.beginCriticalSection();
		for (size_t j = 0; j < mailSuckerThreads[i].emailAccounts.unlockedSize(); j++) {
			if(params.size() == 0) return;
			pmm::MailAccountInfo m = mailSuckerThreads[i].emailAccounts.atUnlocked(j);
			for (std::map<std::string, std::string>::iterator iter = params.begin(); iter != params.end(); iter++) {
				std::string email = iter->first;
				std::string devToken = iter->second;
				if (m.email().compare(email) == 0) {
					pmm::Log << "Adding device " << devToken << " as recipient for messages of: " << email << pmm::NL;
					mailSuckerThreads[i].emailAccounts.atUnlocked(j).deviceTokenAdd(devToken);
					break;
				}
			}
		}
		mailSuckerThreads[i].emailAccounts.endCriticalSection();
	}
}

void retrieveAndSaveSilentModeSettings(const std::vector<pmm::MailAccountInfo> &emailAccounts){
	std::vector<std::string> allAccounts;
	for (size_t i = 0; i < emailAccounts.size(); i++) {
		allAccounts.push_back(emailAccounts[i].email());
	}
	//Now we retrieve account information...
	std::map<std::string, std::map<std::string, int> > info;
	if(globalSession->silentModeInfoGet(info, allAccounts)){
		pmm::Log << "Saving silent mode info..." << pmm::NL;
		for (std::map<std::string, std::map<std::string, int> >::iterator iter = info.begin(); iter != info.end(); iter++) {
			std::string theAccount = iter->first;
			pmm::Log << theAccount << ": ";
			pmm::Log << info[theAccount]["startHour"] << ":";
			pmm::Log << info[theAccount]["startMinute"] << " -> ";
			pmm::Log << info[theAccount]["endHour"] << ":";
			pmm::Log << info[theAccount]["endMinute"] << pmm::NL;
			//Save data here
			pmm::SilentMode::set(theAccount,
								 info[theAccount]["startHour"],
								 info[theAccount]["startMinute"],
								 info[theAccount]["endHour"],
								 info[theAccount]["endMinute"]
								 );
		}
	}
}

void broadcastMessageToAll(std::map<std::string, std::string> &params, pmm::SharedQueue<pmm::NotificationPayload> &notificationQueue, pmm::SharedQueue<pmm::NotificationPayload> &pmmStorageQueue){
	int count = atoi(params["count"].c_str());
	for (int i = 0; i < count; i++) {
		std::stringstream key;
		key << "i" << i;
		std::string token = params[key.str()];
		pmm::Log << " * Sending via broadcast (" << token << "): " << params["message"] << pmm::NL;
		pmm::NotificationPayload np(token, params["message"], 911);
		notificationQueue.add(np);
		//pmmStorageQueue.add(np);
	}
}

static void sendDBContents(pmmrpc::PMMSuckerRPCClient *client, const std::string &dbFile, const std::string &email_){
	sqlite3 *theDB = 0;
	int errCode;
	do {
		if((errCode = sqlite3_open_v2(dbFile.c_str(), &theDB, SQLITE_OPEN_READONLY|SQLITE_OPEN_FULLMUTEX, NULL)) == SQLITE_OK){
			std::stringstream sqlCmd;
			sqlite3_stmt *stmt = 0;
			char *pzTail;
			//sqlCmd << "SELECT uniqueid,timestamp FROM " << pmm::DefaultFetchDBTableName << " order by timestamp desc";
			sqlCmd << "SELECT uniqueid FROM " << pmm::DefaultFetchDBTableName << " order by timestamp desc";
			std::string query = sqlCmd.str();
			errCode = sqlite3_prepare_v2(theDB, query.c_str(), (int)query.size(), &stmt, (const char **)&pzTail);
			if (errCode == SQLITE_OK){
				while ((errCode = sqlite3_step(stmt)) == SQLITE_ROW) {
					//Read averything here
					char *uniqueid = (char *)sqlite3_column_text(stmt, 0);
					//time_t timestamp = (time_t)sqlite3_column_int(stmt, 1);
					/*pmmrpc::FetchDBItem fitem;
					fitem.uid = uniqueid;
					fitem.timestamp = (int32_t)timestamp;*/
					client->fetchDBPutItemAsync(email_, uniqueid);
				}
				sqlite3_finalize(stmt);
			}
			else {
				sqlite3_close(theDB);
				pmm::Log << "Unable to read database " << dbFile << " for syncing: errorcode=" << errCode << ", " << sqlite3_errmsg(theDB) << pmm::NL;
				throw pmm::GenericException("Unable to read full datafile!!!");
			}
			sqlite3_close(theDB);
			errCode = SQLITE_OK;
		}
		else if(errCode == SQLITE_BUSY){
			pmm::Log << "Retrying, the database was busy" << pmm::NL;
			usleep(2500);
		}
		else {
			pmm::Log << "Unable to open database " << dbFile << " for syncing: errorcode=" << errCode << ", " << sqlite3_errmsg(theDB) << pmm::NL;
			throw pmm::GenericException("Unable to open database syncing");
		}
	}while (errCode == SQLITE_BUSY);
}

static void decodeEmailFromDBFile(const std::string &dbfile, std::string &email_){
	std::stringstream theEmail;
	std::vector<std::string> parts1, parts;
	pmm::splitString(parts1, dbfile, ".");
	pmm::splitString(parts, parts1[0], "-");
	for(size_t i = 0; i < parts.size(); i++) {
		unsigned char theChars[2] = { 0, 0};
		std::istringstream hexVal(parts[i]);
		uint32_t theChar;
		hexVal >> std::hex >> theChar;
		std::cout << std::hex << theChar << std::endl;
		theChars[0] = theChar & 0x000000FF;
		theEmail << theChars;
	}
	email_ = theEmail.str();
}

static void finddbAndSyncDBFiles(const std::string &startDir, pmmrpc::PMMSuckerRPCClient *client){
	DIR *theDir = opendir(startDir.c_str());
	struct dirent *dData;
	while ((dData = readdir(theDir)) != NULL) {
		//First level
		if (dData->d_type == DT_DIR && strncmp(dData->d_name, "..", 2) != 0 && strcasecmp(dData->d_name, ".") != 0) {
			std::stringstream thePath;
			thePath << startDir << "/" << dData->d_name;
			finddbAndSyncDBFiles(thePath.str(), client);
		}
		else if (dData->d_type == DT_REG) {
			size_t theLen = strlen(dData->d_name);
			if (theLen > 15) {
				char *endptr = dData->d_name + (theLen - 3);
				if (strncmp(endptr, ".db", 3) == 0) {
					std::stringstream fullPath;
					fullPath << startDir << "/" << dData->d_name;
					std::string email_;
					decodeEmailFromDBFile(dData->d_name, email_);
					pmm::Log << "INFO: Sending [" << email_ << "] " << fullPath.str() << "..." << pmm::NL;
					sendDBContents(client, fullPath.str(), email_);
				}
			}
		}
	}
	closedir(theDir);
	
}

bool sync2RemotePoller(const pmm::PMMSuckerInfo &suckerInfo) {
	bool ret = true;
	pmm::Log << "INFO: Initiating initial sync feed of " << suckerInfo.suckerID << " (" << suckerInfo.hostname << ")" << pmm::NL;
	//Connect 2 remote PMM Sucker here
	boost::shared_ptr<apache::thrift::transport::TTransport> socket;
	boost::shared_ptr<apache::thrift::transport::TTransport> transport;
	boost::shared_ptr<apache::thrift::protocol::TProtocol> protocol;

	socket = boost::shared_ptr<apache::thrift::transport::TTransport>(new apache::thrift::transport::TSocket(suckerInfo.hostname, suckerInfo.port));
	transport = boost::shared_ptr<apache::thrift::transport::TTransport>(new apache::thrift::transport::TBufferedTransport(socket));
	protocol = boost::shared_ptr<apache::thrift::protocol::TProtocol>(new apache::thrift::protocol::TBinaryProtocol(transport));

	pmmrpc::PMMSuckerRPCClient *client = new pmmrpc::PMMSuckerRPCClient(protocol);
	try {
		transport->open();
		//Find all .db files and perform sync
		finddbAndSyncDBFiles("./fetchdb", client);
		transport->close();
	}
	catch(apache::thrift::TException &exx1){
		pmm::Log << "Unable to connect to " << suckerInfo.hostname << " RPC service: " << exx1.what() << pmm::NL;
		ret = false;
	}
	delete client;
	pmm::Log << "INFO: Initial sync feed of " << suckerInfo.suckerID << " (" << suckerInfo.hostname << ") completed!!!" << pmm::NL;
	return ret;
}
