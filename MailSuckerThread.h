//
//  MailSuckerThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_MailSuckerThread_h
#define PMM_Sucker_MailSuckerThread_h
#include "GenericThread.h"
#include "SharedVector.h"
#include "MailAccountInfo.h"

namespace pmm {
	class MailSuckerThread : public GenericThread {
	private:
	protected:

	public:
		SharedVector<MailAccountInfo> emailAccounts;

		MailSuckerThread();
		virtual ~MailSuckerThread();
	};
}


#endif
