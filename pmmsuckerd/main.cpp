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
#ifndef DEFAULT_MAX_NOTIFICATION_THREADS
#define DEFAULT_MAX_NOTIFICATION_THREADS 2
#endif
#ifndef DEFAULT_MAX_POP3_POLLING_THREADS
#define DEFAULT_MAX_POP3_POLLING_THREADS 2
#endif
#ifndef DEFAULT_MAX_IMAP_POLLING_THREADS
#define DEFAULT_MAX_IMAP_POLLING_THREADS 2
#endif
#ifndef DEFAULT_MAX_MESSAGE_UPLOADER_THREADS
#define DEFAULT_MAX_MESSAGE_UPLOADER_THREADS 4
#endif
#ifndef DEFAULT_SSL_CERTIFICATE_PATH
#define DEFAULT_SSL_CERTIFICATE_PATH "/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem"
#endif
#ifndef DEFAULT_SS_PRIVATE_KEY_PATH
#define DEFAULT_SS_PRIVATE_KEY_PATH "/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem"
#endif
#ifndef DEFAULT_LOGFILE
#define DEFAULT_LOGFILE "pmmsuckerd.log"
#endif
#ifndef DEFAULT_THREAD_STACK_SIZE
#define DEFAULT_THREAD_STACK_SIZE 8388608
#endif
#ifndef DEFAULT_COMMAND_POLLING_INTERVAL
#define DEFAULT_COMMAND_POLLING_INTERVAL 60
#endif

void printHelpInfo();
pmm::SuckerSession *globalSession;
void emergencyUnregister();

void disableAccountsWithExceededQuota(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &accounts);
void updateAccountQuotas(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, int> &quotaInfo);
void updateAccountProperties(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo);
void addNewEmailAccount(pmm::SuckerSession &session, pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, size_t *assignationIndex, const std::string &emailAccount);
//void updateMailAccountQuota(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &mailAccountInfo, pmm::SharedQueue<pmm::NotificationPayload> *notificationQueue);


int main (int argc, const char * argv[])
{
	std::string pmmServiceURL = DEFAULT_PMM_SERVICE_URL;
	std::string logFilePath = DEFAULT_LOGFILE;
	size_t maxNotificationThreads = DEFAULT_MAX_NOTIFICATION_THREADS;
	size_t maxIMAPSuckerThreads = DEFAULT_MAX_IMAP_POLLING_THREADS;
	size_t maxPOP3SuckerThreads = DEFAULT_MAX_POP3_POLLING_THREADS;
	size_t maxMessageUploaderThreads = DEFAULT_MAX_MESSAGE_UPLOADER_THREADS;
	size_t threadStackSize = DEFAULT_THREAD_STACK_SIZE;
	std::string sslCertificatePath = DEFAULT_SSL_CERTIFICATE_PATH;
	std::string sslPrivateKeyPath = DEFAULT_SS_PRIVATE_KEY_PATH;
	int commandPollingInterval = DEFAULT_COMMAND_POLLING_INTERVAL;
	pmm::SharedQueue<pmm::NotificationPayload> notificationQueue;
	pmm::SharedVector<std::string> quotaUpdateVector;
	pmm::SharedQueue<pmm::NotificationPayload> pmmStorageQueue;
	pmm::SharedQueue<pmm::QuotaIncreasePetition> quotaIncreaseQueue;
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
		else if(arg.compare("--log") == 0 && (i + 1) < argc){
			logFilePath = argv[++i];
		}
		else if(arg.compare("--command-polling-interval") == 0 && (i + 1) < argc){
			std::stringstream input(argv[++i]);
			input >> commandPollingInterval;			
		}
	}
	pmm::Log.open(logFilePath);
	pmm::CacheLog.open("mailcache.log");
	pmm::CacheLog.setTag("FetchedMailsCache");
	pmm::APNSLog.open("apns.log");
	pmm::APNSLog.setTag("APNSNotificationThread");
	pmm::imapLog.open("imap-fetch.log");
	pmm::imapLog.setTag("IMAPSuckerThread");
	pmm::pop3Log.open("pop3-fetch.log");
	pmm::pop3Log.setTag("POP3SuckerThread");
	pmm::SuckerSession session(pmmServiceURL);
	//1. Register to PMMService...
	try {
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
	//Registration succeded, retrieve max
#ifdef DEBUG
	std::cout << "Initial registration succeded!!!" << std::endl;
#endif
	globalSession = &session;
	std::set_terminate(emergencyUnregister);
	//2. Request accounts to poll
	std::vector<pmm::MailAccountInfo> emailAccounts;
	session.retrieveEmailAddresses(emailAccounts);
	//3. Save email accounts to local datastore, perform full database cleanup
	//4. Start APNS notification threads, validate remote devTokens
	pmm::APNSNotificationThread *notifThreads = new pmm::APNSNotificationThread[maxNotificationThreads];
	pmm::MessageUploaderThread *msgUploaderThreads = new pmm::MessageUploaderThread[maxMessageUploaderThreads];
	pmm::IMAPSuckerThread *imapSuckingThreads = new pmm::IMAPSuckerThread[maxIMAPSuckerThreads];
	pmm::POP3SuckerThread *pop3SuckingThreads = new pmm::POP3SuckerThread[maxPOP3SuckerThreads];
	for (size_t i = 0; i < maxNotificationThreads; i++) {
		//1. Initializa notification thread...
		//2. Start thread
		notifThreads[i].notificationQueue = &notificationQueue;
		notifThreads[i].setCertPath(sslCertificatePath);
		notifThreads[i].setKeyPath(sslPrivateKeyPath);
		pmm::ThreadDispatcher::start(notifThreads[i], threadStackSize);
		sleep(1);
	}
	for (size_t i = 0; i < maxMessageUploaderThreads; i++) {
		msgUploaderThreads[i].session = &session;
		msgUploaderThreads[i].pmmStorageQueue = &pmmStorageQueue;
		pmm::ThreadDispatcher::start(msgUploaderThreads[i], threadStackSize);
	}
	std::vector<pmm::MailAccountInfo> imapAccounts, pop3Accounts;
	pmm::splitEmailAccounts(emailAccounts, imapAccounts, pop3Accounts);
	//5. Dispatch polling threads for imap
	for (size_t k = 0; k < imapAccounts.size(); k++){
		imapSuckingThreads[imapAssignationIndex++].emailAccounts.push_back(imapAccounts[k]);
		if (imapAssignationIndex >= maxIMAPSuckerThreads) {
			imapAssignationIndex = 0;
		}
	}
	for (size_t i = 0; i < maxIMAPSuckerThreads; i++) {
		imapSuckingThreads[i].notificationQueue = &notificationQueue;
		imapSuckingThreads[i].quotaUpdateVector = &quotaUpdateVector;
		imapSuckingThreads[i].pmmStorageQueue = &pmmStorageQueue;
		imapSuckingThreads[i].quotaIncreaseQueue = &quotaIncreaseQueue;
		pmm::ThreadDispatcher::start(imapSuckingThreads[i], threadStackSize);
		sleep(1);
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
		pmm::ThreadDispatcher::start(pop3SuckingThreads[i], threadStackSize);
		sleep(1);
	}
	//7. After registration time ends, close every connection, return to Step 1
	int tic = 1;
	std::map<std::string, int> quotas;
	bool keepRunning = true;
	while (keepRunning) {
		if (tic % 10 == 0) {
			//Process quota updates if any
			if (quotaUpdateVector.size() > 0) {
				quotaUpdateVector.beginCriticalSection();
				for (size_t i = 0; i < quotaUpdateVector.unlockedSize(); i++) {
					if (quotas.find(quotaUpdateVector.atUnlocked(i)) == quotas.end()) {
						quotas[quotaUpdateVector.atUnlocked(i)] = 0;
					}
					quotas[quotaUpdateVector.atUnlocked(i)] = quotas[quotaUpdateVector.atUnlocked(i)] + 1;
				}
				quotaUpdateVector.unlockedClear();
				quotaUpdateVector.endCriticalSection();
				//Report quota changes to pmm service.
				try{
					if(session.reportQuotas(quotas)){
						updateAccountQuotas(imapSuckingThreads, maxIMAPSuckerThreads, quotas);
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
		if(tic % commandPollingInterval == 0){ //Server commands processing
			try{
				std::vector< std::map<std::string, std::map<std::string, std::string> > > tasksToRun;
				int nTasks = session.getPendingTasks(tasksToRun);
				for (int i = 0 ; i < nTasks; i++) {
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
					}
					else if (command.compare(pmm::Commands::mailAccountQuotaChanged) == 0){
						pmm::QuotaIncreasePetition p;
						p.emailAddress = parameters["email"];
						std::stringstream input(parameters["quota"]);
						input >> p.quotaValue;
						quotaIncreaseQueue.add(p);
					}
					else if (command.compare(pmm::Commands::newMailAccountRegistered) == 0){
						if (parameters["mailboxType"].compare("IMAP") == 0) {
							addNewEmailAccount(session, imapSuckingThreads, maxIMAPSuckerThreads, &imapAssignationIndex, parameters["email"]);
						}
						else {
							addNewEmailAccount(session, pop3SuckingThreads, maxPOP3SuckerThreads, &popAssignationIndex, parameters["email"]);
						}
					}
					else {
						pmm::Log << "CRITICAL: Unknown command received from central controller: " << command << pmm::NL;
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
	std::cerr << "Triggering emergency unregister, some unhandled exception ocurred :-(" << std::endl;
	globalSession->unregisterFromPMM();
	abort();
}

void disableAccountsWithExceededQuota(pmm::MailSuckerThread *mailSuckerThreads, size_t nElems, std::map<std::string, std::string> &accounts){
	//Disable email accounts that require it
	for (size_t k = 0; k < nElems; k++) {
		mailSuckerThreads[k].emailAccounts.beginCriticalSection();
		for (std::map<std::string, std::string>::iterator iter2 = accounts.begin(); iter2 != accounts.end(); iter2++) {
			for (size_t l = 0; l < mailSuckerThreads[k].emailAccounts.unlockedSize(); l++) {
				if (mailSuckerThreads[k].emailAccounts.atUnlocked(l).email().compare(iter2->second) == 0) {
					mailSuckerThreads[k].emailAccounts.atUnlocked(l).quota = 0;
					mailSuckerThreads[k].emailAccounts.atUnlocked(l).isEnabled = false;
#ifdef DEBUG
					pmm::Log << "disableAccountsWithExceededQuota: disabling monitoring for: " << mailSuckerThreads[k].emailAccounts.atUnlocked(l).email() << pmm::NL;
#endif
				}
			}
		}	
		mailSuckerThreads[k].emailAccounts.endCriticalSection();
	}
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
		pmm::Log << "Adding " << emailAccount << " to " << m.mailboxType() << " monitoring." << pmm::NL;
	}
#ifdef DEBUG
	else {
		pmm::Log << "WARNING: No information returned from app engine regarding " << emailAccount << ", perhaps it is being monitored by another sucker?" << pmm::NL;
	}
#endif
}











