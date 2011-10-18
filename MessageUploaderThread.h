//
//  MessageUploaderThread.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/18/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_MessageUploaderThread_h
#define PMM_Sucker_MessageUploaderThread_h
#include "GenericThread.h"
#include "NotificationPayload.h"
#include "SharedQueue.h"
#include "PMMSuckerSession.h"

namespace pmm {
	class MessageUploaderThread : public GenericThread {
	private:
	protected:
	public:
		SharedQueue<NotificationPayload> *pmmStorageQueue;
		SuckerSession *session;
		MessageUploaderThread();
		virtual ~MessageUploaderThread();
		void operator()();
	};
}

#endif
