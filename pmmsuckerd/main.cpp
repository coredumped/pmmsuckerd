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
#include "ServerResponse.h"
#include "PMMSuckerSession.h"
#include "APNSNotificationThread.h"
#include "ThreadDispatcher.h"
#include "SharedQueue.h"
#include "NotificationPayload.h"
#include "IMAPSuckerThread.h"
#include "UtilityFunctions.h"
#ifndef DEFAULT_MAX_NOTIFICATION_THREADS
#define DEFAULT_MAX_NOTIFICATION_THREADS 2
#endif
#ifndef DEFAULT_MAX_POP3_POLLING_THREADS
#define DEFAULT_MAX_POP3_POLLING_THREADS 2
#endif
#ifndef DEFAULT_MAX_IMAP_POLLING_THREADS
#define DEFAULT_MAX_IMAP_POLLING_THREADS 2
#endif

void printHelpInfo();

int main (int argc, const char * argv[])
{
	std::string pmmServiceURL = DEFAULT_PMM_SERVICE_URL;
	size_t maxNotificationThreads = DEFAULT_MAX_NOTIFICATION_THREADS;
	size_t maxIMAPSuckerThreads = DEFAULT_MAX_IMAP_POLLING_THREADS;
	size_t maxPOP3SuckerThreads = DEFAULT_MAX_POP3_POLLING_THREADS;
	pmm::SharedQueue<pmm::NotificationPayload> notificationQueue;
	SSL_library_init();
	SSL_load_error_strings();
	for (int i = 1; 1 < argc; i++) {
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
	}
	pmm::SuckerSession session(pmmServiceURL);
	//1. Register to PMMService...
	try {
		session.register2PMM();
	} catch (pmm::ServerResponseException &se1) {
		if (se1.errorCode == pmm::PMM_ERROR_SUCKER_DENIED) {
			std::cerr << "Unable to register, permission denied." << std::endl;
			try{
				//Try to ask for membership automatically or report if a membership has already been asked
				session.reqMembership("Automated membership petition, please help!!!");
				std::cerr << "Membership request issued to pmm controller, try again later" << std::endl;
			}
			catch(pmm::ServerResponseException  &se2){ 
				std::cerr << "Failed to request membership automatically: " << se2.errorDescription << std::endl;
			}
		}
		else {
			std::cerr << "Unable to register: " << se1.errorDescription << std::endl;
		}
		return 1;
	}
	//Registration succeded, retrieve max
#ifdef DEBUG
	std::cout << "Initial registration succeded!!!" << std::endl;
#endif
	//2. Request accounts to poll
	std::vector<pmm::MailAccountInfo> emailAccounts;
	session.retrieveEmailAddresses(emailAccounts, true);
	//3. Save email accounts to local datastore, perform full database cleanup
#warning TODO: Save email accounts to local datastore, perform full database cleanup
	//4. Start APNS notification threads, validate remote devTokens
	pmm::APNSNotificationThread *notifThreads = new pmm::APNSNotificationThread[maxNotificationThreads];
	pmm::IMAPSuckerThread *imapSuckingThreads = new pmm::IMAPSuckerThread[maxIMAPSuckerThreads];
	for (size_t i = 0; i < maxNotificationThreads; i++) {
		//1. Initializa notification thread...
		//2. Start thread
		notifThreads[i].notificationQueue = &notificationQueue;
		notifThreads[i].setCertPath("/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem");
		notifThreads[i].setKeyPath("/Users/coredumped/Dropbox/iPhone and iPad Development Projects Documentation/PushMeMail/Push Me Mail Certs/development/pmm_devel.pem");
		pmm::ThreadDispatcher::start(notifThreads[i]);
	}
	std::vector<pmm::MailAccountInfo> imapAccounts, pop3Accounts;
	pmm::splitEmailAccounts(emailAccounts, imapAccounts, pop3Accounts);
	//5. Dispatch polling threads for imap
	for (size_t k = 0; k < imapAccounts.size(); k++) imapSuckingThreads[k % maxIMAPSuckerThreads].emailAccounts.push_back(imapAccounts[k]);
	for (size_t i = 0; i < maxIMAPSuckerThreads && imapAccounts.size() > 0; i++) {
		
		imapSuckingThreads[i].notificationQueue = &notificationQueue;
		pmm::ThreadDispatcher::start(imapSuckingThreads[i]);
	}

	//6. Dispatch polling threads for POP3
	for (size_t i = 0; i < maxPOP3SuckerThreads && pop3Accounts.size() > 0; i++) {
		for (size_t k = 0; k < emailAccounts.size(); k++) {
			if (k % (2 + i) == 0) {
				//pop3SuckingThreads[i].emailAccounts.push_back(emailAccounts[k]);
			}
		}
		//pmm::ThreadDispatcher::start(pop3SuckingThreads[i]);
	}
	//7. After registration time ends, close every connection, return to Step 1
	while (true) {
		//session.performAutoRegister();
		sleep(10);
	}
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
}
