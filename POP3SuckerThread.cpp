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

#ifndef DEFAULT_MAX_POP3_CONNECTION_TIME
#define DEFAULT_MAX_POP3_CONNECTION_TIME 500
#endif

#ifndef DEFAULT_MAX_POP3_FETCHER_THREADS
#define DEFAULT_MAX_POP3_FETCHER_THREADS 2
#endif

#ifndef DEFAULT_POP3_OLDEST_MESSAGE_INTERVAL
#define DEFAULT_POP3_OLDEST_MESSAGE_INTERVAL 43200
#endif

#ifndef DEFAULT_POP3_MINIMUM_CHECK_INTERVAL
#define DEFAULT_POP3_MINIMUM_CHECK_INTERVAL 5
#endif

#ifndef DEFAULT_HOTMAIL_CHECK_INTERVAL
#define DEFAULT_HOTMAIL_CHECK_INTERVAL 30
#endif

#ifndef MAX_CHECK_INTERVAL
#define MAX_CHECK_INTERVAL 120
#endif

namespace pmm {
	MTLogger pop3Log;
	
	POP3SuckerThread::POP3FetcherThread::POP3FetcherThread(){
		fetchQueue = NULL;
		notificationQueue = NULL;
		quotaUpdateVector = NULL;
		pmmStorageQueue = NULL;
	}
	
	POP3SuckerThread::POP3FetcherThread::~POP3FetcherThread(){
		
	}
	
	void POP3SuckerThread::POP3FetcherThread::operator()(){
		if (fetchQueue == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL fetchQueue.");
		}
		if (notificationQueue == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL notificationQueue.");
		}
		if (pmmStorageQueue == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL pmmStorageQueue.");
		}
		if (quotaUpdateVector == NULL) {
			throw GenericException("Unable to start a POP3 message fetching thread with a NULL quotaUpdateVector.");
		}

		while (true) {
			MailAccountInfo m;
			while (fetchQueue->extractEntry(m)) {
				//Fetch messages for account here!!!
				mailpop3 *pop3 = mailpop3_new(0, 0);
				int result;
				if (m.useSSL()) {
					result = mailpop3_ssl_connect(pop3, m.serverAddress().c_str(), m.serverPort());
				}
				else {
					result = mailpop3_socket_connect(pop3, m.serverAddress().c_str(), m.serverPort());
				}
				if(etpanOperationFailed(result)){
					pop3Log << "Unable to retreive messages for: " << m.email() << " can't connect to server " << m.serverAddress() << ", I will retry later." << pmm::NL;
					mailpop3_free(pop3);
					sleep(1);
					break;
				}
				else {
					result = mailpop3_login(pop3, m.username().c_str(), m.password().c_str());
					if(etpanOperationFailed(result)){
						pop3Log << "Unable to retreive messages for: " << m.email() << " can't login to server " << m.serverAddress() << ", I will retry later." << pmm::NL;
						sleep(1);						
					}
					else {
						carray *msgList;
						result = mailpop3_list(pop3, &msgList);
						if(etpanOperationFailed(result)){
							pop3Log << "Unable to retreive messages for: " << m.email() << ": etpan code=" << result << pmm::NL;
							sleep(1);							
						}
						else {
							for (int i = 0; i < carray_count(msgList); i++) {
								struct mailpop3_msg_info *info = (struct mailpop3_msg_info *)carray_get(msgList, i);
								if (!fetchedMails.entryExists(m.email(), info->msg_uidl)) {
									//Perform real message retrieval
									char *msgBuffer;
									size_t msgSize;
									MailMessage theMessage;
									theMessage.msgUid = info->msg_uidl;
									result = mailpop3_retr(pop3, info->msg_index, &msgBuffer, &msgSize);
									if(etpanOperationFailed(result)){
										pop3Log << "Unable to download message " << info->msg_uidl << " from " << m.email() << ": etpan code=" << result << pmm::NL;
									}
									else {
										MailMessage::parse(theMessage, std::string(msgBuffer, msgSize));
										mailpop3_retr_free(msgBuffer);
										if (abs(time(NULL) - theMessage.dateOfArrival) > DEFAULT_POP3_OLDEST_MESSAGE_INTERVAL) {
											fetchedMails.addEntry(m.email(), info->msg_uidl);
										}
										else {
											for (size_t i = 0; i < m.devTokens().size(); i++) {
												//Apply all processing rules before notifying
												std::stringstream nMsg;
												nMsg << theMessage.from << "\n" << theMessage.subject;
												NotificationPayload np(m.devTokens()[i], nMsg.str(), i + 1);
												np.origMailMessage = theMessage;
												notificationQueue->add(np);
												if(i == 0){
													fetchedMails.addEntry(m.email(), info->msg_uidl);
													quotaUpdateVector->push_back(m.email());
													pmmStorageQueue->add(np);
												}
											}
										}
									}
								}
							}
						}
					}
					mailpop3_quit(pop3);
				}
				mailpop3_free(pop3);
			}
			usleep(250000);
		}
	}

	
	POP3SuckerThread::POP3Control::POP3Control(){
		pop3 = NULL;
		startedOn = time(NULL);
		lastCheck = time(NULL);
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
	}
	
	POP3SuckerThread::POP3SuckerThread(){
		maxPOP3FetcherThreads = DEFAULT_MAX_POP3_FETCHER_THREADS;	
		pop3Fetcher = NULL;
	}
	
	POP3SuckerThread::POP3SuckerThread(size_t _maxMailFetchers){
		
	}
	
	POP3SuckerThread::~POP3SuckerThread(){
		
	}

	
	void POP3SuckerThread::closeConnection(const MailAccountInfo &m){
		/*if (mailboxControl[m.email()].isOpened) {
			mailpop3_free(pop3Control[m.email()].pop3);
			pop3Control[m.email()].pop3 = NULL;
		}*/
		mailboxControl[m.email()].isOpened = false;
	}
	
	void POP3SuckerThread::openConnection(const MailAccountInfo &m){
		if (pop3Fetcher == NULL) {
			pop3Log << "Instantiating pop3 message fetching threads for the first time..." << pmm::NL;
			pop3Fetcher = new POP3FetcherThread[maxPOP3FetcherThreads];
			for (size_t i = 0; i < maxPOP3FetcherThreads; i++) {
				pop3Fetcher[i].fetchQueue = &fetchQueue;
				pop3Fetcher[i].notificationQueue = notificationQueue;
				pop3Fetcher[i].pmmStorageQueue = pmmStorageQueue;
				pop3Fetcher[i].quotaUpdateVector = quotaUpdateVector;
				ThreadDispatcher::start(pop3Fetcher[i], 8 * 1024 * 1024);
			}
		}
		mailboxControl[m.email()].isOpened = true;
		mailboxControl[m.email()].openedOn = time(0x00);
		pop3Control[m.email()].startedOn = time(NULL);
#ifdef DEBUG
		pmm::pop3Log << m.email() << " is being succesfully monitored!!!" << pmm::NL;
#endif		
		if(m.email().find("hotmail.com") != m.email().npos){
			pop3Control[m.email()].minimumCheckInterval = DEFAULT_HOTMAIL_CHECK_INTERVAL;
		}
		/*
		int result;
		if(pop3Control[m.email()].pop3 == NULL){ 
			pop3Control[m.email()].pop3 = mailpop3_new(0, NULL);
		}
		if (serverConnectAttempts.find(m.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[m.serverAddress()] = 0;
		if (m.useSSL()) {
			result = mailpop3_ssl_connect(pop3Control[m.email()].pop3, m.serverAddress().c_str(), m.serverPort());
		}
		else {
			result = mailpop3_socket_connect(pop3Control[m.email()].pop3, m.serverAddress().c_str(), m.serverPort());
		}
		if(etpanOperationFailed(result)){
			serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
			if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
				//Max reconnect exceeded, notify user
#warning TODO: Find a better way to notify the user that we are unable to connect into their mail server
				std::stringstream errmsg;
				errmsg << "Unable to connect to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
				pmm::pop3Log << errmsg.str() << pmm::NL;
				for (size_t i = 0; m.devTokens().size(); i++) {
					NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str()));
					notificationQueue->add(msg);
				}
				serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
			}
			mailboxControl[m.email()].isOpened = false;
			mailpop3_free(pop3Control[m.email()].pop3);
			pop3Control[m.email()].pop3 = NULL;
#warning TODO: delay reconnections			
		}
		else {
			mailboxControl[m.email()].openedOn = time(NULL);
			//Proceed to login stage
			result = mailpop3_login(pop3Control[m.email()].pop3, m.username().c_str(), m.password().c_str());
			if(etpanOperationFailed(result)){
				serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
				if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
					//Max reconnect exceeded, notify user
					std::stringstream errmsg;
#warning TODO: Find a better way to notify the user that we are unable to login into their mail account
					errmsg << "Unable to LOGIN to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped, please reset your authentication information.";
					for (size_t i = 0; m.devTokens().size(); i++) {
						NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str()));
						notificationQueue->add(msg);
					}
					serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
				}
				mailpop3_free(pop3Control[m.email()].pop3);
				pop3Control[m.email()].pop3 = NULL;
				mailboxControl[m.email()].isOpened = false;
			}
			else {
				//Report successfull login
				mailboxControl[m.email()].isOpened = true;
				mailboxControl[m.email()].openedOn = time(0x00);
				pop3Control[m.email()].startedOn = time(NULL);
#ifdef DEBUG
				pmm::pop3Log << m.email() << " is being succesfully monitored!!!" << pmm::NL;
#endif
			}
		}
		 */
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
				/*carray *msgList = NULL;
				int r = mailpop3_list(pop3, &msgList);
				if (etpanOperationFailed(r)) {
					if(pop3->pop3_response != NULL) pop3Log << "Unable to perform LIST on " << theEmail << ": " << pop3->pop3_response << ", will try again in the next cycle" << pmm::NL;
					else pop3Log << "Unable to perform LIST on " << theEmail << ", will try again in the next cycle" << pmm::NL;
				}
				else {
					//Got e-mail list
					if(!fetchedMails.hasAllThesePOP3Entries(theEmail, msgList)){
						fetchMails(m);	
					}
					//Reset mailpop3 list info
					for(int i = 0; i < carray_count(msgList); i++){
						struct mailpop3_msg_info *msg = (struct mailpop3_msg_info *)carray_get(msgList, i);
						if (msg->msg_uidl != NULL)
							free(msg->msg_uidl);
						free(msg);
					}
					carray_free(msgList);
					pop3Control[m.email()].pop3->pop3_msg_tab = NULL;
					pop3Control[m.email()].lastCheck = currTime;
				}*/
				fetchMails(m);
			}
		}
	}
	
	void POP3SuckerThread::fetchMails(const MailAccountInfo &m){
#ifdef DEBUG
		pop3Log << "Retrieving new emails for: " << m.email() << "..." << pmm::NL;
#endif
		//Implement asynchronous retrieval code, tipically from a thread
		fetchQueue.add(m);
	}

}