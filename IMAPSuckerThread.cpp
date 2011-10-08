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
#include "libetpan/libetpan.h"

#ifndef DEFAULT_MAX_MAIL_FETCHERS
#define DEFAULT_MAX_MAIL_FETCHERS 2
#endif

namespace pmm {
	Mutex mout;
	
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
	
	static void fetch_msg(struct mailimap * imap, uint32_t uid, SharedQueue<NotificationPayload> *notificationQueue, const MailAccountInfo &m, int badgeNumber)
	{
		struct mailimap_set * set;
		struct mailimap_section * section;
		size_t msg_len;
		char * msg_content;
		struct mailimap_fetch_type * fetch_type;
		struct mailimap_fetch_att * fetch_att;
		int r;
		clist * fetch_result;
		
		/*snprintf(filename, sizeof(filename), "download/%u.eml", (unsigned int) uid);
		 r = stat(filename, &stat_info);
		 if (r == 0) {
		 // already cached
		 printf("%u is already fetched\n", (unsigned int) uid);
		 return;
		 }*/
		
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
		}
		else{
			//printf("fetch %u\n", (unsigned int) uid);
			
			msg_content = get_msg_content(fetch_result, &msg_len);
			if (msg_content == NULL) {
				//fprintf(stderr, "no content\n");
				mailimap_fetch_list_free(fetch_result);
				return;
			}
			//Parse e-mail, retrieve FROM and SUBJECT
			
			for (size_t i = 0; i < m.devTokens().size(); i++) {
				//Apply all processing rules before notifying
				NotificationPayload np(m.devTokens()[i], msg_content, badgeNumber);
			}
			printf("%u has been fetched\n", (unsigned int) uid);
			
#warning TODO: insert message UID in databse so we know later that the message has been notified
			mailimap_fetch_list_free(fetch_result);
		}
	}
	
	IMAPSuckerThread::IMAPControl::IMAPControl(){
		failedLoginAttemptsCount = 0;
		idling = false;
		imap = NULL;
	}
	
	IMAPSuckerThread::IMAPControl::~IMAPControl(){
		
	}
	
	IMAPSuckerThread::MailFetcher::MailFetcher(){
		
	}
	
	void IMAPSuckerThread::MailFetcher::fetchAndReport(const MailAccountInfo &m, SharedQueue<NotificationPayload> *notifQueue, int recentMessages){
		mInfo = m;
		myNotificationQueue = notifQueue;
		availableMessages = recentMessages;
	}
	
	void IMAPSuckerThread::MailFetcher::operator()(){
		if (mInfo.email().size() > 0) {
			struct mailimap *imap = mailimap_new(0, NULL);
			int result;
			if (mInfo.useSSL()) {
				result = mailimap_ssl_connect(imap, mInfo.serverAddress().c_str(), mInfo.serverPort());
			}
			else {
				result = mailimap_socket_connect(imap, mInfo.serverAddress().c_str(), mInfo.serverPort());
			}
			if (etpanOperationFailed(result)) {
#warning TODO enqueue mail fetching for later
			}
			else {
				clist * fetch_result;
				struct mailimap_set * set = mailimap_set_new_interval(1, 0); /* fetch in interval 1:* */
				struct mailimap_fetch_type * fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
				struct mailimap_fetch_att * fetch_att = mailimap_fetch_att_new_uid();
				mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);				
				result = mailimap_fetch(imap, set, fetch_type, &fetch_result);
				if (etpanOperationFailed(result)) {
#warning TODO: Enque email fetch
				}
				else{
					for(clistiter * cur = clist_begin(fetch_result) ; cur != NULL ; cur = clist_next(cur)) {
						struct mailimap_msg_att * msg_att;
						uint32_t uid;
						msg_att = (struct mailimap_msg_att *)clist_content(cur);
						uid = get_uid(msg_att);
						if (uid == 0)
							continue;
#warning TODO: Remember not to notify about previously notified messages
						fetch_msg(imap, uid, myNotificationQueue, mInfo, availableMessages);
					}
					
					mailimap_fetch_list_free(fetch_result);
				}
			}
			mInfo = MailAccountInfo();
		}
	}
	
	
	IMAPSuckerThread::IMAPControl::IMAPControl(const IMAPControl &ic){
		failedLoginAttemptsCount = ic.failedLoginAttemptsCount;
		imap = ic.imap;
	}
	
	
	IMAPSuckerThread::IMAPSuckerThread(){
		maxMailFetchers = DEFAULT_MAX_MAIL_FETCHERS;
		mailFetchers = new MailFetcher[maxMailFetchers];
	}
	IMAPSuckerThread::IMAPSuckerThread(size_t _maxMailFetchers){
		maxMailFetchers = _maxMailFetchers;
		mailFetchers = new MailFetcher[maxMailFetchers];
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
		if(imapControl[m.email()].imap == NULL) imapControl[m.email()].imap = mailimap_new(0, NULL);
		if (serverConnectAttempts.find(m.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[m.serverAddress()] = 0;
		
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
				for (size_t i = 0; m.devTokens().size(); i++) {
					NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str(), ++mailboxControl[m.email()].availableMessages));
					notificationQueue->add(msg);
				}
				serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
			}
			mailboxControl[m.email()].isOpened = false;
		}
		else {
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
						NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str(), ++mailboxControl[m.email()].availableMessages));
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
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): Starting IMAP IDLE for " << m.email() << std::endl;
				mout.unlock();
#endif
				mailstream_debug = 1;
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
				//sleep(1);
#ifdef DEBUG
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " is being succesfully monitored!!!" << std::endl;
				mout.unlock();
#endif
			}
		}
	}
	
	void IMAPSuckerThread::checkEmail(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
			mailimap *imap = imapControl[m.email()].imap;
			mailstream_low *mlow = mailstream_get_low(imap->imap_stream);
			int fd = mailstream_low_get_fd(mlow);
			struct pollfd pelem;
			pelem.fd = fd;
			pelem.events = POLLIN;
			int recent = -1;
			while (poll(&pelem, 1, 2) > 0) {
				char *response = mailimap_read_line(imap);
				if (strstr(response, "RECENT") != NULL) {
					//Compute how many recent we have
					int recent;
					std::istringstream input(std::string(response).substr(2));
					input >> recent;
					mailboxControl[theEmail].availableMessages += recent;
				}
			}
			if (recent >= 0) {
				mailimap_idle_done(imap);
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " POLL result is: 0x" << std::hex << pelem.revents << " recent=" << recent <<  std::endl;
				mout.unlock();
				std::stringstream msg;
				//Trigger mail fetching threads
				fetchMails(m);
				mailimap_idle(imap);
			}
		}
		else {
			openConnection(m);
		}
	}
	
	void IMAPSuckerThread::fetchMails(const MailAccountInfo &m){
		//Find and schedule a fetching thread
	}
}