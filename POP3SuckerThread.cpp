//
//  POP3SuckerThread.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 11/8/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#include "POP3SuckerThread.h"
#include "ThreadDispatcher.h"
#include "QuotaDB.h"
#include "SharedSet.h"

#ifndef DEFAULT_MAX_POP3_CONNECTION_TIME
#define DEFAULT_MAX_POP3_CONNECTION_TIME 500
#endif

#ifndef DEFAULT_MAX_POP3_FETCHER_THREADS
#define DEFAULT_MAX_POP3_FETCHER_THREADS 2
#endif

#ifndef DEFAULT_POP3_OLDEST_MESSAGE_INTERVAL
#define DEFAULT_POP3_OLDEST_MESSAGE_INTERVAL 500
#endif

#ifndef DEFAULT_POP3_MINIMUM_CHECK_INTERVAL
#define DEFAULT_POP3_MINIMUM_CHECK_INTERVAL 3
#endif

#ifndef DEFAULT_HOTMAIL_CHECK_INTERVAL
#define DEFAULT_HOTMAIL_CHECK_INTERVAL 360
#endif

#ifndef MAX_CHECK_INTERVAL
#define MAX_CHECK_INTERVAL 120
#endif

#ifndef DEFAULT_MAX_MSG_RETRIEVE
#define DEFAULT_MAX_MSG_RETRIEVE 200
#endif

#ifndef DEFAULT_MAX_HOTMAIL_THREADS
#define DEFAULT_MAX_HOTMAIL_THREADS 2
#endif

namespace pmm {
	MTLogger pop3Log;
	
	//Global POP3 fetching queue, any fetcher thread can poll accounts held here
	ExclusiveSharedQueue<POP3SuckerThread::POP3FetchItem> mainFetchQueue;
	SharedSet<std::string> busyEmailsSet;
	
	ExclusiveSharedQueue<POP3SuckerThread::POP3FetchItem> hotmailFetchQueue;
	SharedSet<std::string> busyHotmailsSet;
	
	POP3SuckerThread::POP3FetchItem::POP3FetchItem(){
		timestamp = time(0);
	}
	
	POP3SuckerThread::POP3FetchItem::POP3FetchItem(const POP3SuckerThread::POP3FetchItem &p){
		mailAccountInfo = p.mailAccountInfo;
		timestamp = p.timestamp;
	}
	
	POP3SuckerThread::POP3FetchItem::POP3FetchItem(const MailAccountInfo &m){
		mailAccountInfo = m;
		timestamp = time(0);
	}
	
	bool POP3SuckerThread::POP3FetchItem::operator==(const POP3FetchItem &p) const {
		if (mailAccountInfo == p.mailAccountInfo) {
			return true;
		}
		return false;
	}
	
	bool POP3SuckerThread::POP3FetchItem::operator<(const POP3FetchItem &p) const {
		if (mailAccountInfo.email().size() < p.mailAccountInfo.email().size()) {
			return true;
		}
		return false;
	}
	
	POP3SuckerThread::POP3FetcherThread::POP3FetcherThread(){
		//fetchQueue = NULL;
		notificationQueue = NULL;
		quotaUpdateVector = NULL;
		pmmStorageQueue = NULL;
		isForHotmail = false;
		mailstream_network_delay.tv_sec = 15;
	}
	
	POP3SuckerThread::POP3FetcherThread::~POP3FetcherThread(){
		
	}
	
	int POP3SuckerThread::POP3FetcherThread::fetchMessages(POP3FetchItem &pf){
		mailpop3 *pop3 = mailpop3_new(0, 0);
		int result;
		int messagesRetrieved = 0;
		if (pf.mailAccountInfo.useSSL()) {
			result = mailpop3_ssl_connect(pop3, pf.mailAccountInfo.serverAddress().c_str(), pf.mailAccountInfo.serverPort());
		}
		else {
			result = mailpop3_socket_connect(pop3, pf.mailAccountInfo.serverAddress().c_str(), pf.mailAccountInfo.serverPort());
		}
		if(etpanOperationFailed(result)){
			if (pop3->pop3_response == NULL) {
				pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << " can't connect to server " << pf.mailAccountInfo.serverAddress() << ", I will retry later." << pmm::NL;
			}
			else {
				pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << " can't connect to server " << pf.mailAccountInfo.serverAddress() << ": " << pop3->pop3_response << pmm::NL;
			}
		}
		else {
			result = mailpop3_login(pop3, pf.mailAccountInfo.username().c_str(), pf.mailAccountInfo.password().c_str());
			if(result != MAILPOP3_NO_ERROR){
				if(serverConnectAttempts.find(pf.mailAccountInfo.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = 0;
				int theVal = serverConnectAttempts[pf.mailAccountInfo.serverAddress()] + 1;
				serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = theVal;
				if (result == MAILPOP3_ERROR_BAD_PASSWORD) {
					if(theVal == 100){
						pop3Log << "CRITICAL: Password changed!!! " << pf.mailAccountInfo.email() << " can't login to server " << pf.mailAccountInfo.serverAddress() << ", account monitoring is being disabled!!!" << pmm::NL;
						pf.mailAccountInfo.isEnabled = false;
						std::vector<std::string> allTokens = pf.mailAccountInfo.devTokens();
						std::string msgX = "Can't login to mailbox ";
						msgX.append(pf.mailAccountInfo.email());
						msgX.append(", to continue receiving notifications please update your application settings.");
						emails2Disable->insert(pf.mailAccountInfo.email());
						for (size_t i = 0; i < allTokens.size(); i++) {
							NotificationPayload np(allTokens[i], msgX);
							np.isSystemNotification = true;
							notificationQueue->add(np);
						}					
						serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = 0;
					}
				}
				else {
					if (theVal % 1000 == 0) {
						//Notify the user that we might not be able to monitor this account
						std::vector<std::string> allTokens = pf.mailAccountInfo.devTokens();
						std::string msgX = "Push Me Mail Service:\nCan't login to mailbox ";
						msgX.append(pf.mailAccountInfo.email());
						msgX.append(", have you changed your password?");
						for (size_t i = 0; i < allTokens.size(); i++) {
							NotificationPayload np(allTokens[i], msgX);
							np.isSystemNotification = true;
							notificationQueue->add(np);
						}
					}
					else {
						if (pop3->pop3_response == NULL) {
							pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << " can't login to server " << pf.mailAccountInfo.serverAddress() << ", etpan code=" << result << ", I will retry later." << pmm::NL;
						}
						else {
							pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << " can't login to server " << pf.mailAccountInfo.serverAddress() << ": " << pop3->pop3_response << pmm::NL;
						}
					}
				}
			}
			else {
				carray *msgList;
				result = mailpop3_list(pop3, &msgList);
				if(result != MAILPOP3_NO_ERROR){
					if (result == MAILPOP3_ERROR_CANT_LIST) {
						pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << ": the server has denied mailbox listings" << pmm::NL;
					}
					else {
						if (pop3->pop3_response != NULL) pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << ": etpan code=" << result << " response: " << pop3->pop3_response << pmm::NL;
						else pop3Log << "Unable to retrieve messages for: " << pf.mailAccountInfo.email() << ": etpan code=" << result << pmm::NL;
					}
				}
				else {
					time_t largeT1;
					int max_retrieve = carray_count(msgList);
					if(max_retrieve > 1000){
						pop3Log << "WARNING: " << pf.mailAccountInfo.email() << " is a very large mailbox, it has " << max_retrieve << " messages, scanning it will take a while!!!" << pmm::NL;
						largeT1 = time(0);
					}
					if(isForHotmail) pop3Log << "HOTMAIL: Fetching messages for: " << pf.mailAccountInfo.email() << pmm::NL;
					else pop3Log << "Fetching messages for: " << pf.mailAccountInfo.email() << pmm::NL;
					//if(max_retrieve > DEFAULT_MAX_MSG_RETRIEVE) max_retrieve = DEFAULT_MAX_MSG_RETRIEVE;
					for (int i = 0; i < max_retrieve; i++) {
						//if(max_retrieve > 1000) mailpop3_noop(pop3);
						time_t now = time(0);
						struct mailpop3_msg_info *info = (struct mailpop3_msg_info *)carray_get(msgList, i);
						if (info == NULL) continue;
						//Verify for null here!!!
						if (info->msg_uidl == NULL) continue;
						if (!QuotaDB::have(pf.mailAccountInfo.email())) {
							pmm::Log << pf.mailAccountInfo.email() << " has ran out of quota in the middle of a POP3 mailbox poll!!!" << pmm::NL;
							break;
						}
						if (!fetchedMails.entryExists2(pf.mailAccountInfo.email(), info->msg_uidl)) {
							//Perform real message retrieval
							char *msgBuffer;
							size_t msgSize;
							MailMessage theMessage;
							theMessage.msgUid = info->msg_uidl;
							result = mailpop3_retr(pop3, info->msg_index, &msgBuffer, &msgSize);
							if(result != MAILPOP3_NO_ERROR){
								if(result == MAILPOP3_ERROR_STREAM){
									if(pop3->pop3_response == NULL) pop3Log << "CRITICAL: Unable to download message " << info->msg_uidl << " from " << pf.mailAccountInfo.email() << ": etpan code=" << result << ". Unable to perform I/O on stream, aborting scan, " << messagesRetrieved << "/" << max_retrieve << " retrieved." << pmm::NL;
									else pop3Log << "CRITICAL: Unable to download message " << info->msg_uidl << " from " << pf.mailAccountInfo.email() << ": etpan code=" << result << ". Unable to perform I/O on stream, aborting scan, " << messagesRetrieved << "/" << max_retrieve << " retrieved due to: " << pop3->pop3_response << pmm::NL;
									std::map<std::string, int> record;
									if(failedIOFetches.find(pf.mailAccountInfo.email()) == failedIOFetches.end()){
										record[info->msg_uidl] = 1;
										failedIOFetches[pf.mailAccountInfo.email()] = record;	
									}
									else {
										record = failedIOFetches[pf.mailAccountInfo.email()];
										record[info->msg_uidl]++;
										failedIOFetches[pf.mailAccountInfo.email()] = record;
									}
									if(record[info->msg_uidl] > 4){
										pop3Log << "CRITICAL: Too many attempts (" << record[info->msg_uidl] << ") to download message " << info->msg_uidl << " aborting :-(" << pmm::NL;
										fetchedMails.addEntry2(pf.mailAccountInfo.email(), info->msg_uidl);
										failedIOFetches[pf.mailAccountInfo.email()].erase(info->msg_uidl);
										if(failedIOFetches[pf.mailAccountInfo.email()].size() == 0) failedIOFetches.erase(pf.mailAccountInfo.email());
									}
									break;
								}
								else pop3Log << "Unable to download message " << info->msg_uidl << " from " << pf.mailAccountInfo.email() << ": etpan code=" << result << pmm::NL;
							}
							else {
								if(!MailMessage::parse(theMessage, msgBuffer, msgSize)){
									pmm::Log << "Unable to parse e-mail message !!!" << pmm::NL;
#warning Find a something better to do when a message can't be properly parsed!!!!
									mailpop3_retr_free(msgBuffer);
									continue;
								}
								mailpop3_retr_free(msgBuffer);
								if (startTimeMap.find(pf.mailAccountInfo.email()) == startTimeMap.end()) {
									startTimeMap[pf.mailAccountInfo.email()] = now - 300;
								}
#ifdef DEBUG_TIME_ELDERNESS
								pmm::Log << "Message is " << (theMessage.serverDate - startTimeMap[pf.mailAccountInfo.email()]) << " seconds old" << pmm::NL;
#endif

								if (theMessage.serverDate + 43200 < startTimeMap[pf.mailAccountInfo.email()]) {
									fetchedMails.addEntry2(pf.mailAccountInfo.email(), info->msg_uidl);
									pmm::Log << "Message to " << pf.mailAccountInfo.email() << " not notified because it is too old" << pmm::NL;
								}
								else {
									messagesRetrieved++;
									int cnt = cntRetrieved->get() + 1;
									cntRetrieved->set(cnt);
									theMessage.to = pf.mailAccountInfo.email();
									std::vector<std::string> myDevTokens = pf.mailAccountInfo.devTokens();
									for (size_t i = 0; i < myDevTokens.size(); i++) {
										//Apply all processing rules before notifying
										std::stringstream nMsg;
										std::string alertTone;
										PreferenceEngine::defaultAlertTone(alertTone, pf.mailAccountInfo.email()); //Here we retrieve the user alert tone
										nMsg << theMessage.from << "\n" << theMessage.subject;
										NotificationPayload np(myDevTokens[i], nMsg.str(), i + 1, alertTone);
										np.origMailMessage = theMessage;
										if (pf.mailAccountInfo.devel) {
											pmm::Log << "Using development notification queue for this message." << pmm::NL;
											develNotificationQueue->add(np);
										}
										else notificationQueue->add(np);
										if(i == 0){
											quotaUpdateVector->push_back(pf.mailAccountInfo.email());
											fetchedMails.addEntry2(pf.mailAccountInfo.email(), info->msg_uidl);
											if(!QuotaDB::decrease(pf.mailAccountInfo.email())){
												pop3Log << "ATTENTION: Account " << pf.mailAccountInfo.email() << " has ran out of quota!" << pmm::NL;
												std::stringstream msg;
												msg << "You have ran out of quota on " << pf.mailAccountInfo.email() << ", you may purchase more to keep receiving notifications.";
												pmm::NotificationPayload npi(myDevTokens[i], msg.str());
												npi.isSystemNotification = true;
												if (pf.mailAccountInfo.devel) {
													pmm::Log << "Using development notification queue for this message." << pmm::NL;
													develNotificationQueue->add(npi);
												}
												else notificationQueue->add(npi);
											}
											else {
												pmmStorageQueue->add(np);
											}
										}
									}
								}
							}
						}
					}
					if(max_retrieve > 1000){
						pop3Log << "WARNING: Scan of " << pf.mailAccountInfo.email() << " took: " << (int)(time(0) - largeT1) << " secs" << pmm::NL;
					}
				}
			}
			//mailpop3_quit(pop3);
		}
		mailpop3_free(pop3);
		return messagesRetrieved;
	}
	
	void POP3SuckerThread::POP3FetcherThread::operator()(){
		/*if (fetchQueue == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL fetchQueue.");
		}*/
		if (notificationQueue == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL notificationQueue.");
		}
		if (pmmStorageQueue == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL pmmStorageQueue.");
		}
		if (quotaUpdateVector == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL quotaUpdateVector.");
		}
		time(&startedOn);
		mainFetchQueue.name = "POP3MainFetchQueue";
		hotmailFetchQueue.name = "HotmailFetchQueue";
		while (true) {
			POP3FetchItem pf;
			bool gotSomething = false;
			if (isForHotmail) {
				while (hotmailFetchQueue.extractEntry(pf)) {
					if (busyHotmailsSet.contains(pf.mailAccountInfo.email())) {
						pop3Log << "WARNING: No need to monitor " << pf.mailAccountInfo.email() << " in this thread, another thread is taking care of it." << pmm::NL;
						continue;
					}
					//Fetch messages for Hotmail accounts here!!!
					busyHotmailsSet.insert(pf.mailAccountInfo.email());
					fetchMessages(pf);
					gotSomething = true;
					busyHotmailsSet.erase(pf.mailAccountInfo.email());
				}
			}
			else {
				//Fetch regular pop3 accounts here
				while (mainFetchQueue.extractEntry(pf)) {
					if (busyEmailsSet.contains(pf.mailAccountInfo.email())) {
						pop3Log << "WARNING: No need to monitor " << pf.mailAccountInfo.email() << " in this thread, another thread is taking care of it." << pmm::NL;
						continue;
					}
					//Fetch messages for account here!!!
					busyEmailsSet.insert(pf.mailAccountInfo.email());
					fetchMessages(pf);
					gotSomething = true;
					busyEmailsSet.erase(pf.mailAccountInfo.email());
				}
			}
			if(gotSomething) usleep(10);
			else usleep(1000);
		}
	}

	
	POP3SuckerThread::POP3Control::POP3Control(){
		pop3 = NULL;
		startedOn = time(NULL);
		lastCheck = startedOn;
		msgCount = 0;
		mailboxSize = 0;
		minimumCheckInterval = DEFAULT_POP3_MINIMUM_CHECK_INTERVAL;
	}
	
	POP3SuckerThread::POP3Control::~POP3Control(){

	}

	POP3SuckerThread::POP3Control::POP3Control(const POP3Control &pc){
		pop3 = pc.pop3;
		startedOn = pc.startedOn;
		msgCount = pc.msgCount;
		mailboxSize = pc.mailboxSize;
		minimumCheckInterval = pc.minimumCheckInterval;
		lastCheck = pc.lastCheck;
		startedOn = pc.startedOn;
	}
	
	POP3SuckerThread::POP3SuckerThread() {
		maxPOP3FetcherThreads = DEFAULT_MAX_POP3_FETCHER_THREADS;	
		pop3Fetcher = NULL;
		threadStartTime = time(0) - 900;
		iterationWaitMicroSeconds = 1000000;
		maxHotmailThreads = DEFAULT_MAX_HOTMAIL_THREADS;
	}
	
	POP3SuckerThread::POP3SuckerThread(size_t _maxMailFetchers){
		maxPOP3FetcherThreads = _maxMailFetchers;	
		pop3Fetcher = NULL;
		maxHotmailThreads = DEFAULT_MAX_HOTMAIL_THREADS;
	}
	
	POP3SuckerThread::~POP3SuckerThread(){
		
	}

	void POP3SuckerThread::initialize(){
		pmm::Log << "Initializing pop3 fetchers..." << pmm::NL;
		pop3Log << "Instantiating pop3 message fetching threads for the first time..." << pmm::NL;
		size_t l = maxPOP3FetcherThreads + maxHotmailThreads;
		pop3Fetcher = new POP3FetcherThread[l];
		for (size_t i = 0; i < l; i++) {
			pop3Fetcher[i].notificationQueue = notificationQueue;
			pop3Fetcher[i].pmmStorageQueue = pmmStorageQueue;
			pop3Fetcher[i].quotaUpdateVector = quotaUpdateVector;
			pop3Fetcher[i].develNotificationQueue = develNotificationQueue;
			pop3Fetcher[i].cntRetrieved = &cntRetrievedMessages;
		}
		for (size_t j = 0; j < maxHotmailThreads; j++) {
			pop3Fetcher[l - j - 1].isForHotmail = true;
		}
		for (size_t i = 0; i < l; i++) ThreadDispatcher::start(pop3Fetcher[i], 12 * 1024 * 1024);
	}
	
	void POP3SuckerThread::closeConnection(const MailAccountInfo &m){
		mailboxControl[m.email()].isOpened = false;
	}
	
	void POP3SuckerThread::openConnection(const MailAccountInfo &m){
		if (pop3Fetcher == NULL) {
			pop3Log << "Instantiating pop3 message fetching threads for the first time..." << pmm::NL;
			size_t l = maxPOP3FetcherThreads + maxHotmailThreads;
			pop3Fetcher = new POP3FetcherThread[l];
			for (size_t i = 0; i < l; i++) {
				pop3Fetcher[i].notificationQueue = notificationQueue;
				pop3Fetcher[i].pmmStorageQueue = pmmStorageQueue;
				pop3Fetcher[i].quotaUpdateVector = quotaUpdateVector;
				pop3Fetcher[i].develNotificationQueue = develNotificationQueue;
				pop3Fetcher[i].cntRetrieved = &cntRetrievedMessages;
				pop3Fetcher[i].emails2Disable = emails2Disable;
			}
			for (size_t j = 0; j < maxHotmailThreads; j++) {
				pop3Fetcher[l - j - 1].isForHotmail = true;
			}
			for (size_t i = 0; i < l; i++) ThreadDispatcher::start(pop3Fetcher[i], 12 * 1024 * 1024);
		}
		mailboxControl[m.email()].isOpened = true;
		mailboxControl[m.email()].openedOn = time(0x00);
		pop3Control[m.email()].startedOn = time(NULL);
#ifdef DEBUG
		pmm::pop3Log << m.email() << " is being succesfully monitored!!!" << pmm::NL;
#endif		
		if(m.email().find("@hotmail.") != m.email().npos){
			pop3Control[m.email()].minimumCheckInterval = DEFAULT_HOTMAIL_CHECK_INTERVAL;
		}
	}
	
	void POP3SuckerThread::checkEmail(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
			time_t currTime = time(NULL);
			//mailpop3 *pop3 = pop3Control[m.email()].pop3;
			if (pop3Control[m.email()].startedOn + DEFAULT_MAX_POP3_CONNECTION_TIME < currTime) {
				//Think about closing and re-opening this connection!!!
#ifdef DEBUG
				pmm::pop3Log << "Max connection time for account " << m.email() << " exceeded (" << DEFAULT_MAX_POP3_CONNECTION_TIME << " seconds) dropping connection!!!" << pmm::NL;
#endif
				//mailpop3_free(pop3);
				pop3Control[m.email()].pop3 = NULL;
				mailboxControl[theEmail].isOpened = false;
				return;
			}
			if(currTime - pop3Control[m.email()].lastCheck > pop3Control[m.email()].minimumCheckInterval){
				fetchMails(m);
				pop3Control[m.email()].lastCheck = currTime;
			}
		}
	}
	
	void POP3SuckerThread::fetchMails(const MailAccountInfo &m){
#ifdef DEBUG_POP3_RETRIEVAL
		pop3Log << "Retrieving new emails for: " << m.email() << "..." << pmm::NL;
#endif
		//Implement asynchronous retrieval code, tipically from a thread
		std::string email = m.email();
		if (email.find("@hotmail.") == email.npos || email.find("@live.") == email.npos) {
			//pop3Log << m.email() << " added to main fetch queue" << pmm::NL;
			mainFetchQueue.add(m);
		}
		else {
			hotmailFetchQueue.add(m);
		}

	}

}