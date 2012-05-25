//
//  IMAPSuckerThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <map>
#include <time.h>
#include <sstream>
#include <poll.h>
#include "IMAPSuckerThread.h"
#include "ThreadDispatcher.h"
#include "MailMessage.h"
#include "libetpan/libetpan.h"
#include <string.h>
#include "QuotaDB.h"

#ifndef DEFAULT_MAX_MAIL_FETCHERS
#define DEFAULT_MAX_MAIL_FETCHERS 2
#endif
#ifndef DEFAULT_FETCH_RETRY_INTERVAL
#define DEFAULT_FETCH_RETRY_INTERVAL 60
#endif
#ifndef DEFAULT_MAX_IMAP_CONNECTION_TIME
#define DEFAULT_MAX_IMAP_CONNECTION_TIME 1200
#endif
#ifndef DEFAULT_MAX_IMAP_IDLE_CONNECTION_TIME
#define DEFAULT_MAX_IMAP_IDLE_CONNECTION_TIME 1200
#endif


namespace pmm {
	MTLogger imapLog;
	
	
	static char * get_msg_att_msg_content(struct mailimap_msg_att * msg_att, size_t * p_msg_size, MailMessage &tm)
	{
		clistiter * cur;
		/* iterate on each result of one given message */
		for(cur = clist_begin(msg_att->att_list) ; cur != NULL ; cur = clist_next(cur)) {			
			struct mailimap_msg_att_item * item = (struct mailimap_msg_att_item *)clist_content(cur);
			if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) {
				continue;
			}
			if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_BODY_SECTION) {
				continue;
			}
			* p_msg_size = item->att_data.att_static->att_data.att_body_section->sec_length;
			//MailMessage::parse(tm, item->att_data.att_static->att_data.att_body_section->sec_body_part);
			MailMessage::parse(tm, item->att_data.att_static->att_data.att_body_section->sec_body_part, item->att_data.att_static->att_data.att_body_section->sec_length);
			return item->att_data.att_static->att_data.att_body_section->sec_body_part;
		}
		return NULL;
	}
	
	static char * get_msg_content(clist * fetch_result, size_t * p_msg_size, MailMessage &tm)
	{
		clistiter * cur;
		
		/* for each message (there will be probably only on message) */
		for(cur = clist_begin(fetch_result) ; cur != NULL ; cur = clist_next(cur)) {
			struct mailimap_msg_att * msg_att;
			size_t msg_size;
			char * msg_content;
			
			msg_att = (struct mailimap_msg_att *)clist_content(cur);
			msg_content = get_msg_att_msg_content(msg_att, &msg_size, tm);
			if (msg_content == NULL) {
				continue;
			}
			
			* p_msg_size = msg_size;
			return msg_content;
		}
		
		return NULL;
	}
	
	static uint32_t get_uid(struct mailimap_msg_att * msg_att)
	{
		clistiter * cur;
		/* iterate on each result of one given message */
		for(cur = clist_begin(msg_att->att_list) ; cur != NULL ; cur = clist_next(cur)) {
			struct mailimap_msg_att_item * item;
			item = (struct mailimap_msg_att_item *)clist_content(cur);
			if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) {
				continue;
			}
			if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_UID) {
				continue;
			}
			return item->att_data.att_static->att_data.att_uid;
		}
		return 0;
	}
	
	bool IMAPSuckerThread::MailFetcher::fetch_msg(struct mailimap * imap, uint32_t uid, SharedQueue<NotificationPayload> *notificationQueue, const IMAPSuckerThread::IMAPFetchControl &imapFetch)
	{
		if (fetchedMails.entryExists2(imapFetch.mailAccountInfo.email(), uid)) {
			//Do not fetch or even notify previously fetched e-mails
			return false;
		}
		struct mailimap_set * set;
		struct mailimap_section * section;
		size_t msg_len;
		char * msg_content;
		struct mailimap_fetch_type * fetch_type;
		struct mailimap_fetch_att * fetch_att;
		int r;
		clist * fetch_result;
		
		set = mailimap_set_new_single(uid);
		fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
		section = mailimap_section_new(NULL);
		fetch_att = mailimap_fetch_att_new_body_peek_section(section);
		mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
		
		r = mailimap_uid_fetch(imap, set, fetch_type, &fetch_result);
		if(etpanOperationFailed(r)){
			IMAPFetchControl ifc = imapFetch;
			ifc.madeAttempts++;
			ifc.nextAttempt = time(NULL) + fetchRetryInterval;
			fetchQueue->add(ifc);
		}
		else{
			MailMessage theMessage;
			theMessage.to = imapFetch.mailAccountInfo.email();
			std::stringstream msgUid_s;
			msgUid_s << (int)uid;
			theMessage.msgUid = msgUid_s.str();
			//Parse e-mail, retrieve FROM and SUBJECT
			msg_content = get_msg_content(fetch_result, &msg_len, theMessage);
			if (msg_content == NULL) {
				mailimap_fetch_list_free(fetch_result);
				return false;
			}
			fetchedMails.addEntry2(imapFetch.mailAccountInfo.email(), uid);
			//Verify if theMessage is not too old, if it is then just discard it!!!
			if (theMessage.serverDate < threadStartTime) {
				imapLog << "Message for " << imapFetch.mailAccountInfo.email() << " is too old, not notifying it!!!" << pmm::NL;				
			}
			else {
				std::vector<std::string> myDevTokens = imapFetch.mailAccountInfo.devTokens();
				for (size_t i = 0; i < myDevTokens.size(); i++) {
					//Apply all processing rules before notifying
					std::string alertTone;
					std::stringstream nMsg;
					nMsg << theMessage.from << "\n" << theMessage.subject;
					
					PreferenceEngine::defaultAlertTone(alertTone, imapFetch.mailAccountInfo.email()); //Here we retrieve the user alert tone
					
					NotificationPayload np(myDevTokens[i], nMsg.str(), imapFetch.badgeCounter, alertTone);
					np.origMailMessage = theMessage;
					notificationQueue->add(np);
					if(i == 0){
						if (!QuotaDB::decrease(imapFetch.mailAccountInfo.email())) {
							imapLog << "ATTENTION: Account " << imapFetch.mailAccountInfo.email() << " has ran out of quota!!!" << pmm::NL;
							//std::stringstream msg;
							//msg << "You have ran out of quota on " << imapFetch.mailAccountInfo.email() << ", you may purchase more to keep receiving notifications.";
							//pmm::NotificationPayload npi(myDevTokens[i], msg.str());
							pmm::NoQuotaNotificationPayload npi(myDevTokens[i], imapFetch.mailAccountInfo.email());
							//npi.isSystemNotification = true;
							if (imapFetch.mailAccountInfo.devel) {
								pmm::Log << "Using development notification queue for this message." << pmm::NL;
								develNotificationQueue->add(npi);
							}
							else notificationQueue->add(npi);
						}
						quotaUpdateVector->push_back(imapFetch.mailAccountInfo.email());
						pmmStorageQueue->add(np);
					}
				}
			}			
			mailimap_fetch_list_free(fetch_result);
		}
		mailimap_fetch_att_free(fetch_att);
		mailimap_set_free(set);
		return true;
	}
	
	IMAPSuckerThread::IMAPFetchControl::IMAPFetchControl(){
		madeAttempts = 0;
		nextAttempt = 0;
		badgeCounter = 0;
		issuedDate = time(0);
	}
	
	IMAPSuckerThread::IMAPFetchControl::IMAPFetchControl(const IMAPFetchControl &ifc){
		mailAccountInfo = ifc.mailAccountInfo;
		nextAttempt = ifc.nextAttempt;
		madeAttempts = ifc.madeAttempts;
		badgeCounter += ifc.badgeCounter;
		issuedDate = ifc.issuedDate;
	}
	
	bool IMAPSuckerThread::IMAPFetchControl::operator==(const IMAPFetchControl &i) const {
		if (i.mailAccountInfo == mailAccountInfo) {
			return true;
		}
		return false;
	}
	
	bool IMAPSuckerThread::IMAPFetchControl::operator<(const IMAPFetchControl &i) const {
		if (i.mailAccountInfo.email().size() < i.mailAccountInfo.email().size()) {
			return true;
		}
		return false;
	}
	
	
	IMAPSuckerThread::IMAPControl::IMAPControl(){
		failedLoginAttemptsCount = 0;
		idling = false;
		imap = NULL;
		startedOn = time(NULL);
		maxIDLEConnectionTime = DEFAULT_MAX_IMAP_IDLE_CONNECTION_TIME;
		supportsIdle = true;
	}
	
	IMAPSuckerThread::IMAPControl::~IMAPControl(){
		
	}
	
	IMAPSuckerThread::MailFetcher::MailFetcher(){
		availableMessages = 0;
		pmmStorageQueue = NULL;
		threadStartTime = time(0) - 43200;
	}
	
	void IMAPSuckerThread::MailFetcher::operator()(){
		if (pmmStorageQueue == NULL) throw GenericException("Can't continue like this, the pmmStorageQueue is null!!!");
		pmm::imapLog << "DEBUG: IMAP MailFetcher warming up..." << pmm::NL;
		sleep(1);
		pmm::imapLog << "DEBUG: IMAP MailFetcher started!!!" << pmm::NL;
		fetchQueue->name = "IMAPFetchQueue";
		while (true) {
			IMAPFetchControl imapFetch;
			time_t startT = time(0);
			while (fetchQueue->extractEntry(imapFetch)) {
				if (busyEmails->contains(imapFetch.mailAccountInfo.email())) {
					if(startT % 600 == 0) imapLog << "WARNING: No need to monitor " << imapFetch.mailAccountInfo.email() << " in this thread, another thread is taking care of it." << pmm::NL;
					usleep(100);
					continue;
				}
				busyEmails->insert(imapFetch.mailAccountInfo.email());
				time_t rightNow = time(0);
				if (imapFetch.madeAttempts > 0 && rightNow < imapFetch.nextAttempt) {
					if (fetchQueue->size() == 0) {
						usleep(100);
					}
					fetchQueue->add(imapFetch);
				}
				else {
#ifdef DEBUG
					pmm::imapLog << "DEBUG: IMAP MailFetcher: Fetching messages for: " << imapFetch.mailAccountInfo.email() << pmm::NL;
#endif
					struct mailimap *imap = mailimap_new(0, NULL);
					int result;
					if (imapFetch.mailAccountInfo.useSSL()) {
						result = mailimap_ssl_connect(imap, imapFetch.mailAccountInfo.serverAddress().c_str(), imapFetch.mailAccountInfo.serverPort());
					}
					else {
						result = mailimap_socket_connect(imap, imapFetch.mailAccountInfo.serverAddress().c_str(), imapFetch.mailAccountInfo.serverPort());
					}
					if (etpanOperationFailed(result)) {
						imapFetch.madeAttempts++;
						imapFetch.nextAttempt = rightNow + fetchRetryInterval;
						fetchQueue->add(imapFetch);
#ifdef DEBUG
						if (imap->imap_response == 0) {
							pmm::imapLog << "CRITICAL: IMAP MailFetcher(" << (long)pthread_self() << ") Unable to connect to: " << imapFetch.mailAccountInfo.email() << ", RE-SCHEDULING fetch!!!" << pmm::NL;
						}
						else {
							pmm::imapLog << "CRITICAL: IMAP MailFetcher(" << (long)pthread_self() << ") Unable to connect to: " << imapFetch.mailAccountInfo.email() << ", response=" << imap->imap_response << " RE-SCHEDULING fetch!!!" << pmm::NL;
						}
#endif				
					}
					else {
						result = mailimap_login(imap, imapFetch.mailAccountInfo.username().c_str(), imapFetch.mailAccountInfo.password().c_str());
						if(etpanOperationFailed(result)){
#warning TODO: Remember to report the user whenever we have too many login attempts
#ifdef DEBUG
							if(imap->imap_response == 0) pmm::imapLog << "CRITICAL: IMAP MailFetcher: Unable to login to: " << imapFetch.mailAccountInfo.email() << ", response=" << result << pmm::NL;
							else pmm::imapLog << "CRITICAL: IMAP MailFetcher: Unable to login to: " << imapFetch.mailAccountInfo.email() << ", response=" << imap->imap_response << pmm::NL;
#endif				
							imapFetch.madeAttempts++;
							imapFetch.nextAttempt = rightNow + fetchRetryInterval;
							fetchQueue->add(imapFetch);
						}
						else {
							result = mailimap_select(imap, "INBOX");
							if (etpanOperationFailed(result)) {
								imapFetch.madeAttempts++;
								imapFetch.nextAttempt = time_t(NULL) + fetchRetryInterval;
								fetchQueue->add(imapFetch);
#ifdef DEBUG
								pmm::imapLog << "CRITICAL: IMAP MailFetcher: Unable to select INBOX(" << imapFetch.mailAccountInfo.email() << ") etpan error=" << result << " response=" << imap->imap_response << pmm::NL;
#endif				
							}
							else {
								clist *unseenMails = clist_new();
								struct mailimap_search_key *sKey = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UNSEEN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
								result = mailimap_uid_search(imap, (const char *)NULL, sKey, &unseenMails);
								if (etpanOperationFailed(result)) {
									if (imap->imap_response == 0) {
										imapLog << "CRITICAL: Can't find unseen messages, IMAP SEARCH failed miserably" << pmm::NL;
									}
								}
								else {
									imapFetch.madeAttempts = 0;
#ifdef DEBUG
									if(imap->imap_response != NULL) pmm::imapLog << "DEBUG: MailFetcher: " << imapFetch.mailAccountInfo.email() << " SEARCH imap response=" << imap->imap_response << pmm::NL;
#endif
									imapFetch.badgeCounter = 0;
									std::vector<uint32_t> uidSet;
									for(clistiter * cur = clist_begin(unseenMails) ; cur != NULL ; cur = clist_next(cur)) {
										if (!QuotaDB::have(imapFetch.mailAccountInfo.email())) {
											pmm::Log << imapFetch.mailAccountInfo.email() << " has ran out of quota in the middle of a IMAP mailbox poll!!!" << pmm::NL;
											break;
										}
										uint32_t uid;
										uid = *((uint32_t *)clist_content(cur));
#ifdef DEBUG_IMAP
										pmm::imapLog << "DEBUG: IMAP MailFetcher " << imapFetch.mailAccountInfo.email() << " got UID=" << (int)uid << pmm::NL;
#endif
										imapFetch.badgeCounter++;
										bool gotMessage = false;
										if (imapFetch.mailAccountInfo.devel) {
											//pmm::imapLog << "Using development notification queue..." << pmm::NL;
											gotMessage = fetch_msg(imap, uid, develNotificationQueue, imapFetch);
										}
										else {
											gotMessage = fetch_msg(imap, uid, myNotificationQueue, imapFetch);
										}
										uidSet.push_back(uid);
										if (gotMessage) {
											int cnt = cntRetrieved->get() + 1;
											cntRetrieved->set(cnt);
										}
									}
									//Remove old entries if the current time is a multiple of 60 seconds
									if (rightNow % 600 == 0) {
#ifdef DEBUG
										pmm::imapLog << "Removing old uid entries from fetch control database..." << pmm::NL;
#endif
										fetchedMails.removeEntriesNotInSet2(imapFetch.mailAccountInfo.email(), uidSet);
									}
									mailimap_search_result_free(unseenMails);
								}
							}
							mailimap_logout(imap);
						}
						mailimap_close(imap);
					}
					mailimap_free(imap);
				}
				busyEmails->erase(imapFetch.mailAccountInfo.email());
			}
			time_t endT = time(0);
			if(startT != endT) usleep(1000);
			else usleep(10000);
		}
	}
	
	
	IMAPSuckerThread::IMAPControl::IMAPControl(const IMAPControl &ic){
		failedLoginAttemptsCount = ic.failedLoginAttemptsCount;
		imap = ic.imap;
	}
	
	
	IMAPSuckerThread::IMAPSuckerThread(){
		maxMailFetchers = DEFAULT_MAX_MAIL_FETCHERS;
		mailFetchers = new MailFetcher[maxMailFetchers];
		//IMAPSuckerThread::fetchedMails.expireOldEntries();
		threadStartTime = time(0) - 43200;
	}
	IMAPSuckerThread::IMAPSuckerThread(size_t _maxMailFetchers){
		maxMailFetchers = _maxMailFetchers;
		mailFetchers = new MailFetcher[maxMailFetchers];
		//IMAPSuckerThread::fetchedMails.expireOldEntries();
		threadStartTime = time(0) - 43200;
	}
	IMAPSuckerThread::~IMAPSuckerThread(){
		delete [] mailFetchers;
	}
	
	void IMAPSuckerThread::closeConnection(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
			if (imapControl[theEmail].supportsIdle) {
				struct mailimap *imap = imapControl[theEmail].imap;
				mailimap_close(imap);
				mailimap_free(imapControl[theEmail].imap);
				imapControl[theEmail].imap = NULL;
			}
		}
		mailboxControl[theEmail].isOpened = false;
	}
	
	void IMAPSuckerThread::openConnection(const MailAccountInfo &m){
		int result;
		std::string theEmail = m.email();
		for (size_t i = 0; i < maxMailFetchers; i++) {
			if (mailFetchers[i].isRunning == false) {
				mailFetchers[i].myNotificationQueue = notificationQueue;
				mailFetchers[i].develNotificationQueue = develNotificationQueue;
				mailFetchers[i].fetchQueue = &imapFetchQueue;
				mailFetchers[i].quotaUpdateVector = quotaUpdateVector;
				mailFetchers[i].pmmStorageQueue = pmmStorageQueue;
				mailFetchers[i].threadStartTime = threadStartTime;
				mailFetchers[i].busyEmails = &busyEmails;
				mailFetchers[i].cntRetrieved = &cntRetrievedMessages;
				pmm::ThreadDispatcher::start(mailFetchers[i], 8 * 1024 * 1024);
			}
		}
		if(imapControl[theEmail].imap == NULL) imapControl[theEmail].imap = mailimap_new(0, NULL);
		if (serverConnectAttempts.find(m.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[m.serverAddress()] = 0;
		if (m.useSSL()) {
			result = mailimap_ssl_connect(imapControl[theEmail].imap, m.serverAddress().c_str(), m.serverPort());
		}
		else {
			result = mailimap_socket_connect(imapControl[theEmail].imap, m.serverAddress().c_str(), m.serverPort());
		}
		if (etpanOperationFailed(result)) {
			serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
			if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
				//Max reconnect exceeded, notify user
#warning TODO: Find a better way to notify the user that we are unable to connect into their mail server
				std::stringstream errmsg;
				errmsg << "Unable to connect to " << m.serverAddress() << " monitoring of " << theEmail << " has been stopped, we will retry later.";
#ifdef DEBUG
				pmm::imapLog << "IMAPSuckerThread(" << (long)pthread_self() << "): " << errmsg.str() << pmm::NL;
#endif
				/*std::vector<std::string> myDevTokens = m.devTokens();
				for (size_t i = 0; m.devTokens().size(); i++) {
					NotificationPayload msg(myDevTokens[i], errmsg.str());
					if (m.devel) {
						develNotificationQueue->add(msg);
					}
					else notificationQueue->add(msg);
				}*/
				serverConnectAttempts[m.serverAddress()] = 0;
				mailboxControl[theEmail].lastCheck = time(0) + 300;
#warning Add method for relinquishing email account monitoring
			}
			mailboxControl[theEmail].isOpened = false;
			mailimap_free(imapControl[theEmail].imap);
			imapControl[theEmail].imap = NULL;
#warning TODO: delay reconnections			
		}
		else {
			mailboxControl[theEmail].openedOn = time(NULL);
			//Proceed to login stage
			result = mailimap_login(imapControl[theEmail].imap, m.username().c_str(), m.password().c_str());
			if(etpanOperationFailed(result)){
				serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
				if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
					//Max reconnect exceeded, notify user
					if (mailboxControl[theEmail].lastCheck % 43200 == 0) {
						std::stringstream errmsg;
#warning TODO: Find a better way to notify the user that we are unable to login into their mail account
						errmsg << "Unable to LOGIN to " << m.serverAddress() << " monitoring of " << theEmail << " has been stopped, please reset your authentication information.";
/*						std::vector<std::string> myDevTokens = m.devTokens();
						for (size_t i = 0; myDevTokens.size(); i++) {
							NotificationPayload np(myDevTokens[i], errmsg.str());
							if (m.devel) {
								develNotificationQueue->add(np);
							}
							else notificationQueue->add(np);
						}*/
					}

#warning Add method for relinquishing email account monitoring
					mailboxControl[theEmail].lastCheck = time(0);
					mailboxControl[theEmail].isOpened = false;
				}
				mailimap_free(imapControl[theEmail].imap);
				imapControl[theEmail].imap = NULL;
				mailboxControl[theEmail].isOpened = false;
			}
			else {
				//Start IMAP IDLE processing...
				
				serverConnectAttempts[m.serverAddress()] = 0;
#ifdef DEBUG
				pmm::imapLog << "IMAPSuckerThread(" << (long)pthread_self() << "): Starting IMAP IDLE for " << theEmail << pmm::NL;
#endif
				//mailstream_debug = 1;
				result = mailimap_select(imapControl[theEmail].imap, "INBOX");
				if(etpanOperationFailed(result)){
					if (imapControl[theEmail].imap->imap_response == 0) {
						pmm::Log << "FATAL: Unable to select INBOX folder in account " << theEmail << pmm::NL;
					}
					else {
						pmm::Log << "FATAL: Unable to select INBOX folder in account " << theEmail << ": " << imapControl[theEmail].imap->imap_response <<  pmm::NL;
					}
					//throw GenericException("Unable to select INBOX folder");
				}	
				else {
					int idleEnabled;
					if (theEmail.compare("jinny27@nate.com") == 0) {
						idleEnabled = 0;
					}
					else idleEnabled = mailimap_has_idle(imapControl[theEmail].imap);
					if(!idleEnabled){
						imapLog << "WARNING: " << theEmail << " is not hosted in an IMAP IDLE environment." << pmm::NL;
						mailboxControl[theEmail].isOpened = true;
						mailboxControl[theEmail].openedOn = time(0x00);
						imapControl[theEmail].startedOn = time(NULL);
						mailimap_logout(imapControl[theEmail].imap);
						mailimap_close(imapControl[theEmail].imap);
						imapControl[theEmail].supportsIdle = false;
						return;
					}
					//Manually check mailbox in case any unnotified e-mail arrived before a call to IDLE
					fetchMails(m);
					imapControl[theEmail].supportsIdle = true;
					result = mailimap_idle(imapControl[theEmail].imap);
					if(result != MAILIMAP_NO_ERROR){
						if(imapControl[theEmail].imap->imap_response == 0){
							pmm::imapLog << "CRITICAL: Account " << m.email() << " said it was hosted at an IMAP IDLE capable server but we failed to monitor it!!!" << pmm::NL;
						}
						else {
							pmm::imapLog << "CRITICAL: Account " << m.email() << " said it was hosted at an IMAP IDLE capable server but we failed to monitor it: " << imapControl[theEmail].imap->imap_response << pmm::NL;
						}
						//throw GenericException("Unable to start IDLE!!!");
						mailboxControl[theEmail].isOpened = false;
						mailimap_logout(imapControl[theEmail].imap);
						mailimap_close(imapControl[theEmail].imap);
						mailimap_free(imapControl[theEmail].imap);
						imapControl[theEmail].imap = NULL;
					}
					else {
						//Report successfull login
						mailboxControl[theEmail].isOpened = true;
						mailboxControl[theEmail].openedOn = time(0x00);
						imapControl[theEmail].startedOn = time(NULL);
						//sleep(1);
#ifdef DEBUG
						pmm::imapLog << theEmail << " is being succesfully monitored!!!" << pmm::NL;
#endif
					}
				}
			}
		}
	}
	
	void IMAPSuckerThread::checkEmail(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
			if (!imapControl[theEmail].supportsIdle) {
				if(time(0) - mailboxControl[theEmail].lastCheck > 2){
					fetchMails(m);
				}
				return;
			}
			mailimap *imap = imapControl[theEmail].imap;
			if (imapControl[theEmail].startedOn + DEFAULT_MAX_IMAP_CONNECTION_TIME < time(NULL)) {
				//Think about closing and re-opening this connection!!!
#ifdef DEBUG
				pmm::imapLog << "Max connection time account for " << theEmail << " exceeded (" << DEFAULT_MAX_IMAP_CONNECTION_TIME << " seconds) dropping connection!!!" << pmm::NL;
#endif
				//mailimap_idle_done(imap);
				//mailimap_logout(imap);
				mailimap_close(imap);
				mailimap_free(imap);
				imapControl[theEmail].imap = NULL;
				mailboxControl[theEmail].isOpened = false;
				return;
			}
			mailstream_low *mlow = mailstream_get_low(imap->imap_stream);
			int fd = mailstream_low_get_fd(mlow);
			if(fd < 0) throw GenericException("Unable to get a valid FD, check libetpan compilation flags, make sure no cfnetwork support have been cooked in.");
			struct pollfd pelem;
			pelem.fd = fd;
			pelem.events = POLLIN;
			int recent = -1;
			bool resetIdle = false;
			int antiLoopCounter = 0;
			while (poll(&pelem, 1, 0) > 0) {
				char *response = mailimap_read_line(imap);
#ifdef DEBUG
				if(response != NULL){
					pmm::imapLog << "DEBUG: IMAPSuckerThread(" << (long)pthread_self() << ") IDLE GOT response=" << response << " for " << theEmail << pmm::NL;
				}
				else {
					pmm::imapLog << "DEBUG: IMAPSuckerThread(" << (long)pthread_self() << ") IDLE GOT response=NULL for " << theEmail << ", re-connecting..." << pmm::NL;
					if (imap->imap_stream) {
						mailstream_close(imap->imap_stream);
						imap->imap_stream = NULL;
					}
					mailimap_free(imap);
					imapControl[theEmail].imap = NULL;
					imapControl[theEmail].failedLoginAttemptsCount = 0;
					mailboxControl[theEmail].isOpened = false;
					return;
				}
#endif
				if (response != NULL && strlen(response) > 0 && strstr(response, "OK Still") == NULL) {
					if (strstr(response, "EXPUNGE") != NULL) {
#warning Expire old messages here
					}
					else {
						//Compute how many recent we have
						//std::istringstream input(std::string(response).substr(2));
						//input >> recent;
						resetIdle = true;
						mailboxControl[theEmail].availableMessages += recent;
#ifdef DEBUG
						pmm::imapLog << "DEBUG: IDLE GOT recent=" << recent << " for " << theEmail << pmm::NL;
#endif
						fetchMails(m);
					}
				}
				if(response == NULL) break;
				if(antiLoopCounter++ > 100) {
					pmm::imapLog << "PANIC: Got a looped IDLE status checker, attempting to send an IDLE reset to break free!!!" << pmm::NL;
					resetIdle = true;
					fetchMails(m);
					break;
				}
			}
			if (resetIdle) {
				int result = mailimap_idle_done(imap);
				if(result != MAILIMAP_NO_ERROR){
					pmm::imapLog << "Unable to send DONE to IMAP after IDLE for: " << theEmail << " disconnecting from monitoring, we will reconnect in the next cycle" << pmm::NL;
					mailimap_idle_done(imap);
					mailimap_logout(imap);
					mailimap_close(imap);
					mailimap_free(imap);
					imapControl[theEmail].imap = NULL;
					mailboxControl[theEmail].isOpened = false;
				}
				else {
					result = mailimap_idle(imap);
					if(result != MAILIMAP_NO_ERROR){
						//throw GenericException("Unable to restart IDLE after DONE.");
						if(imap->imap_response == NULL) pmm::imapLog << "CRITICAL: unable to reset IDLE, perhaps this is not the time to send such command?, disconnecting..." << pmm::NL;
						else pmm::imapLog << "CRITICAL: unable to reset IDLE: " << imap->imap_response << pmm::NL;
						mailimap_logout(imap);
						mailimap_close(imap);
						mailimap_free(imap);
						imapControl[theEmail].imap = NULL;
						mailboxControl[theEmail].isOpened = false;
					}
					else {
						fetchMails(m);
					}
				}
			}
		}
	}
	
	void IMAPSuckerThread::fetchMails(const MailAccountInfo &m){
		//Find and schedule a fetching thread
		IMAPFetchControl ifc;
		ifc.mailAccountInfo = m;
		ifc.issuedDate = time(0);
		imapFetchQueue.add(ifc);
	}
}