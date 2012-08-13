//
//  POP3SuckerThread.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 11/8/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#include <signal.h>
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
#define DEFAULT_MAX_MSG_RETRIEVE 5
#endif

#ifndef DEFAULT_MAX_HOTMAIL_THREADS
#define DEFAULT_MAX_HOTMAIL_THREADS 2
#endif

#ifndef DEFAULT_YAHOO_CHECK_INTERVAL
#define DEFAULT_YAHOO_CHECK_INTERVAL 15
#endif

#ifndef DEFAULT_MAX_YAHOO_FETCH_TIMEOUT
#define DEFAULT_MAX_YAHOO_FETCH_TIMEOUT 15
#endif

namespace pmm {
	MTLogger pop3Log;
	
	static const char *yahooServiceNotPermittedErrorMessage = "[AUTH] Access to this service is not permitted.";
	
	//Global POP3 fetching queue, any fetcher thread can poll accounts held here
	ExclusiveSharedQueue<POP3SuckerThread::POP3FetchItem> mainFetchQueue;
	SharedSet<std::string> busyEmailsSet;
	SharedSet<std::string> yahooAccountsToBan;
	
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
		emails2Disable = NULL;
		mailstream_network_delay.tv_sec = 15;
	}
	
	POP3SuckerThread::POP3FetcherThread::~POP3FetcherThread(){
		
	}
	
	int POP3SuckerThread::POP3FetcherThread::fetchMessages(POP3FetchItem &pf){
		mailpop3 *pop3 = mailpop3_new(0, 0);
		if(pop3 == NULL){ pop3Log << "PANIC: Unable to create POP3 handle!!!" << pmm::NL; return false; }
		int result;
		int messagesRetrieved = 0;
		time_t fetchT0 = time(NULL);
		bool isYahooAccount = false;
		if (pf.mailAccountInfo.serverAddress().find(".yahoo.") != pf.mailAccountInfo.serverAddress().npos) isYahooAccount = true;
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
			int theVal = serverConnectAttempts[pf.mailAccountInfo.serverAddress()] + 1;
			serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = theVal;
			if (theVal % 100 == 0) {
				messagesRetrieved = -3;
			}
		}
		else {
			result = mailpop3_login(pop3, pf.mailAccountInfo.username().c_str(), pf.mailAccountInfo.password().c_str());
			if(result != MAILPOP3_NO_ERROR){
				if(serverConnectAttempts.find(pf.mailAccountInfo.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = 0;
				int theVal = serverConnectAttempts[pf.mailAccountInfo.serverAddress()] + 1;
				serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = theVal;
				if (result == MAILPOP3_ERROR_BAD_PASSWORD) {
					std::string errorMsg;
					if (pop3->pop3_response != NULL) {
						errorMsg = pop3->pop3_response;
						std::string email = pf.mailAccountInfo.email();
						if (errorMsg.find(yahooServiceNotPermittedErrorMessage) != errorMsg.npos && email.find("@yahoo.") != email.npos) {
							pmm::pop3Log << "PANIC: " << pf.mailAccountInfo.email() << " requires a paid Yahoo Plus! account, unable to monitor!!!" << pmm::NL;
							mailpop3_free(pop3);
							return -2;
						}
						else {
							pmm::pop3Log << "CRITICAL: Password failed(" << theVal << ") for " << pf.mailAccountInfo.email() << ", server: " << pf.mailAccountInfo.serverAddress() <<", due to: " << pop3->pop3_response << pmm::NL;
							if (isYahooAccount && errorMsg.find("(error 999)") != errorMsg.npos) {
								//Return -1 so the caller method knows that the fetch must be delayed somehow
								mailpop3_free(pop3);
								return -1;
							}
						}
					}
					if(theVal % 100 == 0){
						pop3Log << "CRITICAL: Password changed!!! " << pf.mailAccountInfo.email() << " can't login to server " << pf.mailAccountInfo.serverAddress() << ", account monitoring is being disabled!!!" << pmm::NL;
						//pf.mailAccountInfo.isEnabled = false; //This piece of code does nothing!!!
						if(emails2Disable != NULL) emails2Disable->insert(pf.mailAccountInfo.email());
						std::vector<std::string> allTokens = pf.mailAccountInfo.devTokens();
						std::string msgX = pf.mailAccountInfo.email();
						if (errorMsg.size() > 0) {
							msgX.append(": ");
							msgX.append(errorMsg);
						}
						else msgX.append("authentication failed: check your app settings!!!");
						for (size_t i = 0; i < allTokens.size(); i++) {
							NotificationPayload np(allTokens[i], msgX);
							np.isSystemNotification = true;
							notificationQueue->add(np);
						}					
						serverConnectAttempts[pf.mailAccountInfo.serverAddress()] = 0;
						messagesRetrieved = -3;
					}
				}
				else {
					if (theVal % 100 == 0) {
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
						return -2;
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
#ifdef POP3_DEBUG
					if(isForHotmail) pop3Log << "HOTMAIL: Fetching messages for: " << pf.mailAccountInfo.email() << pmm::NL;
					else pop3Log << "Fetching messages for: " << pf.mailAccountInfo.email() << pmm::NL;
#endif
					//if(max_retrieve > DEFAULT_MAX_MSG_RETRIEVE) max_retrieve = DEFAULT_MAX_MSG_RETRIEVE;
					int maxRetrievalIterations = DEFAULT_MAX_MSG_RETRIEVE;

					for (int i = 0; i < max_retrieve; i++) {
						time_t now = time(0);
						if(isYahooAccount && messagesRetrieved > maxRetrievalIterations) break;
						if(isYahooAccount && now - fetchT0 > DEFAULT_MAX_YAHOO_FETCH_TIMEOUT) break;
						struct mailpop3_msg_info *info = (struct mailpop3_msg_info *)carray_get(msgList, i);
						if (info == NULL) continue;
						//Verify for null here!!!
						if (info->msg_uidl == NULL) continue;
						if (!QuotaDB::have(pf.mailAccountInfo.email())) {
							pmm::pop3Log << pf.mailAccountInfo.email() << " has ran out of quota in the middle of a POP3 mailbox poll!!!" << pmm::NL;
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
								else if (result == MAILPOP3_ERROR_NO_SUCH_MESSAGE) {
									if(pop3->pop3_response == NULL) pop3Log << "PANIC: Unable to download non-existent message " << info->msg_uidl << " (" << pf.mailAccountInfo.email() << ")" << pmm::NL;
									else pop3Log << "PANIC: Unable to download non-existent message " << info->msg_uidl << " (" << pf.mailAccountInfo.email() << "): " << pop3->pop3_response << pmm::NL;
									fetchedMails.addEntry2(pf.mailAccountInfo.email(), info->msg_uidl);
									/*if (isYahooAccount){
										//Since the socket might already be closed there is no socket leaking here
										//However everything in the pop3 structure might still be leaking, this
										//must be solved in the future somehow
										//return messagesRetrieved;
										break;
									}*/
								}
								else{
									pop3Log << "Unable to download message " << info->msg_uidl << " from " << pf.mailAccountInfo.email() << ": etpan code=" << result << pmm::NL;	
								}
							}
							else {
								if(!MailMessage::parse(theMessage, msgBuffer, msgSize)){
									pmm::pop3Log << "Unable to parse e-mail message !!!" << pmm::NL;
#warning Find a something better to do when a message can't be properly parsed!!!!
									mailpop3_retr_free(msgBuffer);
									continue;
								}
								mailpop3_retr_free(msgBuffer);
								if (startTimeMap.find(pf.mailAccountInfo.email()) == startTimeMap.end()) {
									startTimeMap[pf.mailAccountInfo.email()] = now - 300;
								}
#ifdef DEBUG_TIME_ELDERNESS
								pmm::pop3Log << "Message is " << (theMessage.serverDate - startTimeMap[pf.mailAccountInfo.email()]) << " seconds old" << pmm::NL;
#endif

								if (theMessage.serverDate + 43200 < startTimeMap[pf.mailAccountInfo.email()]) {
									fetchedMails.addEntry2(pf.mailAccountInfo.email(), info->msg_uidl);
									pmm::pop3Log << "Message to " << pf.mailAccountInfo.email() << " not notified because it is too old" << pmm::NL;
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
											pmm::pop3Log << "Using development notification queue for this message." << pmm::NL;
											develNotificationQueue->add(np);
										}
										else notificationQueue->add(np);
										if(i == 0){
											quotaUpdateVector->push_back(pf.mailAccountInfo.email());
											fetchedMails.addEntry2(pf.mailAccountInfo.email(), info->msg_uidl);
											if(!QuotaDB::decrease(pf.mailAccountInfo.email())){
												pop3Log << "ATTENTION: Account " << pf.mailAccountInfo.email() << " has ran out of quota!" << pmm::NL;
												//std::stringstream msg;
												//msg << "You have ran out of quota on " << pf.mailAccountInfo.email() << ", you may purchase more to keep receiving notifications.";
												//pmm::NotificationPayload npi(myDevTokens[i], msg.str());
												
												pmm::NoQuotaNotificationPayload npi(myDevTokens[i], pf.mailAccountInfo.email());
												//npi.isSystemNotification = true;
												if (pf.mailAccountInfo.devel) {
													pmm::pop3Log << "Using development notification queue for this message." << pmm::NL;
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
					fetchedMails.closeConnection(pf.mailAccountInfo.email());
				}
				mailpop3_quit(pop3);
			}
			//mailpop3_quit(pop3);
			/*if (pop3->pop3_stream != NULL) {
				mailstream_close(pop3->pop3_stream);
			}*/
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
		std::set<std::string> delayedAccounts;
		std::map<std::string, time_t> nextCheck;
		sigset_t bSignal;
		sigemptyset(&bSignal);
		sigaddset(&bSignal, SIGPIPE);
		if(pthread_sigmask(SIG_BLOCK, &bSignal, NULL) != 0){
			pmm::Log << "WARNING: Unable to block SIGPIPE !" << pmm::NL;
		}
		while (true) {
			POP3FetchItem pf;
			bool gotSomething = false;
			if (isForHotmail) {
				while (hotmailFetchQueue.extractEntry(pf)) {
					if (busyHotmailsSet.contains(pf.mailAccountInfo.email())) {
						pop3Log << "WARNING: No need to monitor " << pf.mailAccountInfo.email() << " in this thread, another thread is taking care of it." << pmm::NL;
						continue;
					}
					time_t now = time(0);
					//Fetch messages for Hotmail accounts here!!!
					busyHotmailsSet.insert(pf.mailAccountInfo.email());
					int n = fetchMessages(pf);
					gotSomething = true;
					if (n == -3) {
						pop3Log << "CRITICAL: account " << pf.mailAccountInfo.email() << " won't be polled again until 1 hour have passed" << pmm::NL;
						nextCheck[pf.mailAccountInfo.email()] = now + 3600;
						delayedAccounts.insert(pf.mailAccountInfo.email());
					}
					busyHotmailsSet.erase(pf.mailAccountInfo.email());
				}
			}
			else {
				//Fetch regular pop3 accounts here
				while (mainFetchQueue.extractEntry(pf)) {
					time_t now = time(0);
					std::string lastRemovedFromDelayed;
					if (busyEmailsSet.contains(pf.mailAccountInfo.email())) {
						pop3Log << "WARNING: No need to monitor " << pf.mailAccountInfo.email() << " in this thread, another thread is taking care of it." << pmm::NL;
						continue;
					}
					if (delayedAccounts.find(pf.mailAccountInfo.email()) != delayedAccounts.end()) {
						if (nextCheck[pf.mailAccountInfo.email()] < now) {
							delayedAccounts.erase(pf.mailAccountInfo.email());
							nextCheck.erase(pf.mailAccountInfo.email());
							pop3Log << "NOTICE: removing " << pf.mailAccountInfo.email() << " from the delayed accounts fetch" << pmm::NL;
							lastRemovedFromDelayed = pf.mailAccountInfo.email();
						}
						else continue;
					}
					//Fetch messages for account here!!!
					busyEmailsSet.insert(pf.mailAccountInfo.email());
					int n = fetchMessages(pf);
					if(pf.mailAccountInfo.serverAddress().find(".yahoo.") != pf.mailAccountInfo.serverAddress().npos){
						if(n == -1){
							//Perform delay here
							if(lastRemovedFromDelayed.compare(pf.mailAccountInfo.email()) == 0){
								pop3Log << "CRITICAL: Yahoo temporarily blocked (error 999) " << pf.mailAccountInfo.email() << " not polling for 24 hrs :-(" << pmm::NL;
								nextCheck[pf.mailAccountInfo.email()] = now + 86700;
							}
							else nextCheck[pf.mailAccountInfo.email()] = now + 15 * 60;
							delayedAccounts.insert(pf.mailAccountInfo.email());
							pop3Log << "WARNING: performing delayed fetch on: " << pf.mailAccountInfo.email() << pmm::NL;
						}
						else if (n == -2){
							//This is intense now, we need to tell this guy that we can't monitor his mailbox because he/she doesn't have a paid Yahoo account :-(
							std::stringstream msg;
							msg << "Sorry, Yahoo Plus! is required for this app to push e-mail notifications from " << pf.mailAccountInfo.email();
							std::vector<std::string> devTokens = pf.mailAccountInfo.devTokens();
							for (size_t i = 0; i < devTokens.size(); i++) {
								pmm::NotificationPayload np(devTokens[i], msg.str());
								notificationQueue->add(np);
							}
							//Now we add the e-mail address to the global banned list of yahoo addresses
							yahooAccountsToBan.insert(pf.mailAccountInfo.email());
						}	
					}
					if (n == -3) {
						pop3Log << "CRITICAL: account " << pf.mailAccountInfo.email() << " won't be polled again until 1 hour have passed" << pmm::NL;
						nextCheck[pf.mailAccountInfo.email()] = now + 3600;
						delayedAccounts.insert(pf.mailAccountInfo.email());
					}
					gotSomething = true;
					busyEmailsSet.erase(pf.mailAccountInfo.email());
				}
			}
			if(gotSomething) usleep(1000);
			else usleep(10000);
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
				pop3Fetcher[i].emails2Disable = &this->emails2Disable;
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
		if(m.serverAddress().find(".live.") != m.serverAddress().npos){
			pop3Control[m.email()].minimumCheckInterval = DEFAULT_HOTMAIL_CHECK_INTERVAL;
		}
		else if(m.serverAddress().find(".yahoo.") != m.serverAddress().npos){
			pop3Control[m.email()].minimumCheckInterval = DEFAULT_YAHOO_CHECK_INTERVAL;
		}

	}
	
	void POP3SuckerThread::checkEmail(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
			time_t currTime = time(NULL);
			//mailpop3 *pop3 = pop3Control[m.email()].pop3;
			if (pop3Control[m.email()].startedOn + DEFAULT_MAX_POP3_CONNECTION_TIME < currTime) {
				//Think about closing and re-opening this connection!!!
/*#ifdef DEBUG
				pmm::pop3Log << "Max connection time for account " << m.email() << " exceeded (" << DEFAULT_MAX_POP3_CONNECTION_TIME << " seconds) dropping connection!!!" << pmm::NL;
#endif*/
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
		if (yahooAccountsToBan.contains(email)) {
			rmAccountQueue->add(email);
			yahooAccountsToBan.erase(email);
			return;
		}
		if (email.find("@hotmail.") == email.npos || email.find("@live.") == email.npos) {
			//pop3Log << m.email() << " added to main fetch queue" << pmm::NL;
			mainFetchQueue.add(m);
		}
		else {
			hotmailFetchQueue.add(m);
		}

	}

}