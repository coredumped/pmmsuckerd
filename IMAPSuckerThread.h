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
		std::map<std::string, IMAPControl> imapControl;
	protected:
		void closeConnection(const MailAccountInfo &m); //Override me
		void openConnection(const MailAccountInfo &m); //Override me
		void checkEmail(const MailAccountInfo &m); //Override me
	public:		
		IMAPSuckerThread();
		virtual ~IMAPSuckerThread();
	};
}

#endif
