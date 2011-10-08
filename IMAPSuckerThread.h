//
//  IMAPSuckerThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_IMAPSuckerThread_h
#define PMM_Sucker_IMAPSuckerThread_h
#include "MailSuckerThread.h"
#include "SharedQueue.h"
#include <map>
#include "libetpan/libetpan.h"

namespace pmm {
	class IMAPSuckerThread : public MailSuckerThread {
	private:
		class IMAPControl {
		protected:
		public:
			struct mailimap *imap;
			bool idling;
			int failedLoginAttemptsCount;
			IMAPControl();
			~IMAPControl();
			IMAPControl(const IMAPControl &ic);
		};
		
		class MailFetcher : public GenericThread {
		private:
			MailAccountInfo mInfo;
			SharedQueue<NotificationPayload> *myNotificationQueue;
			int availableMessages;
		public:
			MailFetcher();
			void fetchAndReport(const MailAccountInfo &m, SharedQueue<NotificationPayload> *notifQueue, int recentMessages);
			void operator()();
		};
		
		std::map<std::string, IMAPControl> imapControl;
		MailFetcher *mailFetchers;
		size_t maxMailFetchers;
	protected:
		void closeConnection(const MailAccountInfo &m); //Override me
		void openConnection(const MailAccountInfo &m); //Override me
		void checkEmail(const MailAccountInfo &m); //Override me
		void fetchMails(const MailAccountInfo &m);
	public:		
		IMAPSuckerThread();
		IMAPSuckerThread(size_t _maxMailFetchers);
		virtual ~IMAPSuckerThread();
	};
}

#endif
