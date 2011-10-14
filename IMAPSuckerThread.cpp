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
#include "libetpan/libetpan.h"
#include <string.h>


#ifndef DEFAULT_MAX_MAIL_FETCHERS
#define DEFAULT_MAX_MAIL_FETCHERS 2
#endif
#ifndef DEFAULT_FETCH_RETRY_INTERVAL
#define DEFAULT_FETCH_RETRY_INTERVAL 60
#endif
#ifndef DEFAULT_MAX_IMAP_CONNECTION_TIME
#define DEFAULT_MAX_IMAP_CONNECTION_TIME 1200
#endif

namespace pmm {

	FetchedMailsCache fetchedMails;
	
	static bool etpanOperationFailed(int r)
	{
		if (r == MAILIMAP_NO_ERROR)
			return false;
		if (r == MAILIMAP_NO_ERROR_AUTHENTICATED)
			return false;
		if (r == MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
			return false;
		return true;
	}
	
	static char * get_msg_att_msg_content(struct mailimap_msg_att * msg_att, size_t * p_msg_size)
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
#ifdef DEBUG
			pmm::Log << "Got RAW message: " << item->att_data.att_static->att_data.att_body_section->sec_body_part << pmm::NL;
#endif
			return item->att_data.att_static->att_data.att_body_section->sec_body_part;
		}
		return NULL;
	}
	
	static char * get_msg_content(clist * fetch_result, size_t * p_msg_size)
	{
		clistiter * cur;
		
		/* for each message (there will be probably only on message) */
		for(cur = clist_begin(fetch_result) ; cur != NULL ; cur = clist_next(cur)) {
			struct mailimap_msg_att * msg_att;
			size_t msg_size;
			char * msg_content;
			
			msg_att = (struct mailimap_msg_att *)clist_content(cur);
			msg_content = get_msg_att_msg_content(msg_att, &msg_size);
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
	
	void IMAPSuckerThread::MailFetcher::fetch_msg(struct mailimap * imap, uint32_t uid, SharedQueue<NotificationPayload> *notificationQueue, const IMAPSuckerThread::IMAPFetchControl &imapFetch)
	{
		if (fetchedMails.entryExists(imapFetch.mailAccountInfo.email(), uid)) {
			//Do not fetch or even notify previously fetched e-mails
			return;
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
		//section = mailimap_section_new(NULL);
		clist *hdrsList = clist_new();
		clist_append(hdrsList, (void *)"FROM");
		clist_append(hdrsList, (void *)"SUBJECT");
		struct mailimap_header_list *hdrs = mailimap_header_list_new(hdrsList);
		section = mailimap_section_new_header_fields(hdrs);
		fetch_att = mailimap_fetch_att_new_body_peek_section(section);
		mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
		
		r = mailimap_uid_fetch(imap, set, fetch_type, &fetch_result);
		if(etpanOperationFailed(r)){
#warning Enqueue mail fetch for later
			IMAPFetchControl ifc = imapFetch;
			ifc.madeAttempts++;
			ifc.nextAttempt = time(NULL) + fetchRetryInterval;
			fetchQueue->add(ifc);
		}
		else{
			msg_content = get_msg_content(fetch_result, &msg_len);
			if (msg_content == NULL) {
				//fprintf(stderr, "no content\n");
				mailimap_fetch_list_free(fetch_result);
				return;
			}
			//Parse e-mail, retrieve FROM and SUBJECT
			
			for (size_t i = 0; i < imapFetch.mailAccountInfo.devTokens().size(); i++) {
				//Apply all processing rules before notifying
				NotificationPayload np(imapFetch.mailAccountInfo.devTokens()[i], msg_content, imapFetch.badgeCounter);
				notificationQueue->add(np);
				if(i == 0) fetchedMails.addEntry(imapFetch.mailAccountInfo.email(), uid);
			}
#warning TODO: insert message UID in databse so we know later that the message has been notified
			mailimap_fetch_list_free(fetch_result);
		}
	}
	
	IMAPSuckerThread::IMAPFetchControl::IMAPFetchControl(){
		madeAttempts = 0;
		nextAttempt = 0;
		badgeCounter = 0;
	}
	
	IMAPSuckerThread::IMAPFetchControl::IMAPFetchControl(const IMAPFetchControl &ifc){
		mailAccountInfo = ifc.mailAccountInfo;
		nextAttempt = ifc.nextAttempt;
		madeAttempts = ifc.madeAttempts;
		badgeCounter += ifc.badgeCounter;
	}
	
	
	IMAPSuckerThread::IMAPControl::IMAPControl(){
		failedLoginAttemptsCount = 0;
		idling = false;
		imap = NULL;
		startedOn = time(NULL);
	}
	
	IMAPSuckerThread::IMAPControl::~IMAPControl(){
		
	}
	
	IMAPSuckerThread::MailFetcher::MailFetcher(){
		availableMessages = 0;
	}
	
	void IMAPSuckerThread::MailFetcher::operator()(){
		pmm::Log << "DEBUG: IMAP MailFetcher warming up..." << pmm::NL;
		sleep(1);
		pmm::Log << "DEBUG: IMAP MailFetcher started!!!" << pmm::NL;
		while (true) {
			IMAPFetchControl imapFetch;
			while (fetchQueue->extractEntry(imapFetch)) {
				//try {
				if (imapFetch.madeAttempts > 0 && time(0) < imapFetch.nextAttempt) {
					if (fetchQueue->size() == 0) {
						usleep(10);
					}
					fetchQueue->add(imapFetch);
					break;
				}
#ifdef DEBUG
				pmm::Log << "DEBUG: IMAP MailFetcher: Fetching messages for: " << imapFetch.mailAccountInfo.email() << pmm::NL;
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
#warning TODO enqueue mail fetching for later
					imapFetch.madeAttempts++;
					imapFetch.nextAttempt = time_t(NULL) + fetchRetryInterval;
					fetchQueue->add(imapFetch);
#ifdef DEBUG
					pmm::Log << "CRITICAL: IMAP MailFetcher(" << (long)pthread_self() << ") Unable to connect to: " << imapFetch.mailAccountInfo.email() << ", response=" << imap->imap_response << " RE-SCHEDULING fetch!!!" << pmm::NL;
#endif				
				}
				else {
					result = mailimap_login(imap, imapFetch.mailAccountInfo.username().c_str(), imapFetch.mailAccountInfo.password().c_str());
					if(etpanOperationFailed(result)){
#warning TODO: Remember to report the user whenever we have too many login attempts
#ifdef DEBUG
						pmm::Log << "CRITICAL: IMAP MailFetcher: Unable to login to: " << imapFetch.mailAccountInfo.email() << ", response=" << imap->imap_response << pmm::NL;
#endif				
						imapFetch.madeAttempts++;
						imapFetch.nextAttempt = time_t(NULL) + fetchRetryInterval;
						fetchQueue->add(imapFetch);
					}
					else {
						result = mailimap_select(imap, "INBOX");
						if (etpanOperationFailed(result)) {
							imapFetch.madeAttempts++;
							imapFetch.nextAttempt = time_t(NULL) + fetchRetryInterval;
							fetchQueue->add(imapFetch);
#ifdef DEBUG
							pmm::Log << "CRITICAL: IMAP MailFetcher: Unable to select INBOX(" << imapFetch.mailAccountInfo.email() << ") etpan error=" << result << " response=" << imap->imap_response << pmm::NL;
#endif				
							continue;
						}
						clist *unseenMails = clist_new();
						struct mailimap_search_key *sKey = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UNSEEN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
						result = mailimap_uid_search(imap, (const char *)NULL, sKey, &unseenMails);
						if (etpanOperationFailed(result)) {
							throw GenericException("Can't find unseen messages, IMAP SEARCH failed miserably");
						}
#ifdef DEBUG
						pmm::Log << "DEBUG: MailFetcher: " << imapFetch.mailAccountInfo.email() << " SEARCH imap response=" << imap->imap_response << pmm::NL;
#endif
						imapFetch.badgeCounter = 0;
						for(clistiter * cur = clist_begin(unseenMails) ; cur != NULL ; cur = clist_next(cur)) {
							uint32_t uid;
							uid = *((uint32_t *)clist_content(cur));
#ifdef DEBUG
							pmm::Log << "DEBUG: IMAP MailFetcher " << imapFetch.mailAccountInfo.email() << " got UID=" << (int)uid << pmm::NL;
#endif
							imapFetch.badgeCounter++;
							fetch_msg(imap, uid, myNotificationQueue, imapFetch);
						}
						mailimap_search_result_free(unseenMails);	
						mailimap_logout(imap);
					}
					mailimap_close(imap);
				}
				/*} catch (...) {
				 fetchQueue->add(imapFetch);
				 throw ;
				 }*/
				mailimap_free(imap);
			}
			sleep(1);
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
	}
	IMAPSuckerThread::IMAPSuckerThread(size_t _maxMailFetchers){
		maxMailFetchers = _maxMailFetchers;
		mailFetchers = new MailFetcher[maxMailFetchers];
		//IMAPSuckerThread::fetchedMails.expireOldEntries();
	}
	IMAPSuckerThread::~IMAPSuckerThread(){
		delete [] mailFetchers;
	}
	
	void IMAPSuckerThread::closeConnection(const MailAccountInfo &m){
		if (mailboxControl[m.email()].isOpened) {
			struct mailimap *imap = imapControl[m.email()].imap;
			mailimap_close(imap);
			mailimap_free(imapControl[m.email()].imap);
			imapControl[m.email()].imap = NULL;
		}
		mailboxControl[m.email()].isOpened = false;
	}
	
	void IMAPSuckerThread::openConnection(const MailAccountInfo &m){
		int result;
		bool first_connection = false;
		for (size_t i = 0; i < maxMailFetchers; i++) {
			if (mailFetchers[i].isRunning == false) {
				mailFetchers[i].myNotificationQueue = notificationQueue;
				mailFetchers[i].fetchQueue = &imapFetchQueue;
				pmm::ThreadDispatcher::start(mailFetchers[i]);
				first_connection = true;
			}
		}
		if(imapControl[m.email()].imap == NULL) imapControl[m.email()].imap = mailimap_new(0, NULL);
		if (serverConnectAttempts.find(m.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[m.serverAddress()] = 0;
		if(first_connection) fetchMails(m);		
		if (m.useSSL()) {
			result = mailimap_ssl_connect(imapControl[m.email()].imap, m.serverAddress().c_str(), m.serverPort());
		}
		else {
			result = mailimap_socket_connect(imapControl[m.email()].imap, m.serverAddress().c_str(), m.serverPort());
		}
		if (etpanOperationFailed(result)) {
			serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
			if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
				//Max reconnect exceeded, notify user
#warning TODO: Find a better way to notify the user that we're unable to connect into their mail server
				std::stringstream errmsg;
				errmsg << "Unable to connect to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
#ifdef DEBUG
				pmm::Log << "IMAPSuckerThread(" << (long)pthread_self() << "): " << errmsg.str() << pmm::NL;
#endif
				for (size_t i = 0; m.devTokens().size(); i++) {
					NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str()));
					notificationQueue->add(msg);
				}
				serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
			}
			mailboxControl[m.email()].isOpened = false;
			mailimap_free(imapControl[m.email()].imap);
			imapControl[m.email()].imap = NULL;
#warning TODO: delay reconnections			
		}
		else {
			mailboxControl[m.email()].openedOn = time(NULL);
			//Proceed to login stage
			result = mailimap_login(imapControl[m.email()].imap, m.username().c_str(), m.password().c_str());
			if(etpanOperationFailed(result)){
				serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
				if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
					//Max reconnect exceeded, notify user
					std::stringstream errmsg;
#warning TODO: Find a better way to notify the user that we're unable to login into their mail account
					errmsg << "Unable to LOGIN to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
					for (size_t i = 0; m.devTokens().size(); i++) {
						NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str()));
						notificationQueue->add(msg);
					}
					serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
				}
				mailimap_free(imapControl[m.email()].imap);
				imapControl[m.email()].imap = NULL;
				mailboxControl[m.email()].isOpened = false;
			}
			else {
				//Start IMAP IDLE processing...
#ifdef DEBUG
				pmm::Log << "IMAPSuckerThread(" << (long)pthread_self() << "): Starting IMAP IDLE for " << m.email() << pmm::NL;
#endif
				//mailstream_debug = 1;
				result = mailimap_select(imapControl[m.email()].imap, "INBOX");
				if(etpanOperationFailed(result)){
					throw GenericException("Unable to select INBOX folder");
				}				
				if(!mailimap_has_idle(imapControl[m.email()].imap)){
					throw GenericException("Can't POLL non IDLE mailboxes.");
				}
				result = mailimap_idle(imapControl[m.email()].imap);
				if(etpanOperationFailed(result)){
					throw GenericException("Unable to start IDLE!!!");
				}
				//Report successfull login
				mailboxControl[m.email()].isOpened = true;
				mailboxControl[m.email()].openedOn = time(0x00);
				imapControl[m.email()].startedOn = time(NULL);
				//sleep(1);
#ifdef DEBUG
				pmm::Log << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " is being succesfully monitored!!!" << pmm::NL;
#endif
			}
		}
	}
	
	void IMAPSuckerThread::checkEmail(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
/*#ifdef DEBUG
			mout.lock();
			pmm::Log << "DEBUG: IMAPSuckerThread(" << (long)pthread_self() << ") checkMail " << theEmail << pmm::NL;
			mout.unlock();
#endif*/
			mailimap *imap = imapControl[m.email()].imap;
			if (imapControl[m.email()].startedOn + DEFAULT_MAX_IMAP_CONNECTION_TIME < time(NULL)) {
				//Think about closing and re-opening this connection!!!
				mailimap_idle_done(imap);
				mailimap_logout(imap);
				mailimap_close(imap);
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
			while (poll(&pelem, 1, 0) > 0) {
				char *response = mailimap_read_line(imap);
#ifdef DEBUG
				if(response != NULL){
					pmm::Log << "DEBUG: IMAPSuckerThread(" << (long)pthread_self() << ") IDLE GOT response=" << response << " for " << theEmail << pmm::NL;
				}
				else {
					pmm::Log << "DEBUG: IMAPSuckerThread(" << (long)pthread_self() << ") IDLE GOT response=NULL for " << theEmail << pmm::NL;
				}
#endif
				if (response != NULL && strlen(response) > 0 && strstr(response, "OK Still") == NULL) {
					//Compute how many recent we have
					//std::istringstream input(std::string(response).substr(2));
					//input >> recent;
					resetIdle = true;
					mailboxControl[theEmail].availableMessages += recent;
#ifdef DEBUG
					pmm::Log << "DEBUG: IMAPSuckerThread(" << (long)pthread_self() << ") IDLE GOT recent=" << recent << " for " << theEmail << pmm::NL;
#endif
					fetchMails(m);
				}
				if(response == NULL) break;
			}
			if (resetIdle) {
				int result = mailimap_idle_done(imap);
				if(etpanOperationFailed(result)) throw GenericException("Unable to send DONE to IMAP after IDLE.");
				result = mailimap_idle(imap);
				if(etpanOperationFailed(result)) throw GenericException("Unable to restart IDLE after DONE.");
			}
		}
	}
	
	void IMAPSuckerThread::fetchMails(const MailAccountInfo &m){
		//Find and schedule a fetching thread
		IMAPFetchControl ifc;
		ifc.mailAccountInfo = m;
		imapFetchQueue.add(ifc);
	}
}