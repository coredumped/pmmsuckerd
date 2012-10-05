//
//  POP3SuckerThread.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 11/8/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_POP3SuckerThread_h
#define PMM_Sucker_POP3SuckerThread_h
#include "MailSuckerThread.h"
#include "FetchedMailsCache.h"
#include "UserPreferences.h"
#include "libetpan/libetpan.h"
#include "ExclusiveSharedQueue.h"
#include <map>

namespace pmm {
	class POP3SuckerThread : public MailSuckerThread {
	public:
		class POP3FetchItem {
		public:
			MailAccountInfo mailAccountInfo;
			time_t timestamp;
			POP3FetchItem();
			POP3FetchItem(const POP3FetchItem &p);
			POP3FetchItem(const MailAccountInfo &m);
			bool operator==(const POP3FetchItem &p) const;
			bool operator<(const POP3FetchItem &p) const;
		};
	private:
		class POP3Control {
		public:
			struct mailpop3 *pop3;
			time_t startedOn;
			size_t msgCount;
			size_t mailboxSize;
			time_t lastCheck;
			int minimumCheckInterval;

			POP3Control();
			~POP3Control();
			POP3Control(const POP3Control &pc);
		};
		
		class POP3FetcherThread : public GenericThread {
		private:
			FetchedMailsCache fetchedMails;
		protected:
			std::map<std::string, std::map<std::string, int> > failedIOFetches;
			int fetchMessages(POP3FetchItem &pf);
		public:
			bool isForHotmail;
			time_t mailboxScanTimeout;
			time_t startedOn;
			std::map<std::string, time_t> startTimeMap;
			SharedQueue<NotificationPayload> *notificationQueue;
			SharedQueue<NotificationPayload> *develNotificationQueue;
			SharedQueue<NotificationPayload> *pmmStorageQueue;
			SharedVector<std::string> *quotaUpdateVector;
			std::map<std::string, int> serverConnectAttempts;
			AtomicVar<int> *cntRetrieved;
			AtomicVar<int> *cntBytesDownloaded;
			AtomicVar<int> *cntFailedLoginAttempts;
			SharedSet<std::string> *emails2Disable;
			POP3FetcherThread();
			~POP3FetcherThread();
			void operator()();
		};
		
		size_t maxPOP3FetcherThreads;
		size_t maxHotmailThreads;
		POP3FetcherThread *pop3Fetcher;
		std::map<std::string, POP3Control> pop3Control;
	protected:
		void closeConnection(const MailAccountInfo &m); //Override me
		void openConnection(const MailAccountInfo &m); //Override me
		void checkEmail(const MailAccountInfo &m); //Override me
		void fetchMails(const MailAccountInfo &m);
	public:
		POP3SuckerThread();
		POP3SuckerThread(size_t _maxMailFetchers);
		virtual ~POP3SuckerThread();
		
		virtual void initialize();
	};
	

	extern MTLogger pop3Log;
}

#endif
