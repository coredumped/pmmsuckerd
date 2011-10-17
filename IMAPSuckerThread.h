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
#include "FetchedMailsCache.h"
#include <map>
#include "libetpan/libetpan.h"

namespace pmm {
	
	
	class IMAPSuckerThread : public MailSuckerThread {
	private:

		class IMAPFetchControl {
		public:
			MailAccountInfo mailAccountInfo;
			time_t nextAttempt;
			int madeAttempts;
			int badgeCounter;
			IMAPFetchControl();
			IMAPFetchControl(const IMAPFetchControl &ifc);
		};

		class IMAPControl {
		protected:
		public:
			struct mailimap *imap;
			bool idling;
			int failedLoginAttemptsCount;
			time_t startedOn;
			IMAPControl();
			~IMAPControl();
			IMAPControl(const IMAPControl &ic);
		};
		
		class MailFetcher : public GenericThread {
		private:
			int dispatched;
			//MailAccountInfo mInfo;
			int availableMessages;
			int fetchRetryInterval;
			void fetch_msg(struct mailimap * imap, uint32_t uid, SharedQueue<NotificationPayload> *notificationQueue, const IMAPSuckerThread::IMAPFetchControl &m);
		public:
			SharedQueue<IMAPFetchControl> *fetchQueue;
			SharedQueue<NotificationPayload> *myNotificationQueue;
			SharedVector<std::string> *quotaUpdateVector;
			MailFetcher();
			void operator()();
		};
		
		std::map<std::string, IMAPControl> imapControl;
		SharedQueue<IMAPFetchControl> imapFetchQueue;
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
	
	extern FetchedMailsCache fetchedMails;
	extern MTLogger imapLog;
}

#endif
