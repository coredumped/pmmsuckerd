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

namespace pmm {
	class IMAPSuckerThread : public MailSuckerThread {
	private:
	protected:
	public:
		IMAPSuckerThread();
		virtual ~IMAPSuckerThread();
	};
}

#endif
