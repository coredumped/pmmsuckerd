//
//  PendingNotificationStore.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 4/17/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_PendingNotificationStore_h
#define PMM_Sucker_PendingNotificationStore_h
#include "SharedQueue.h"
#include "NotificationPayload.h"

namespace pmm {
	class PendingNotificationStore {
	public:
		static void savePayloads(SharedQueue<NotificationPayload> *nQueue);
		static void loadPayloads(SharedQueue<NotificationPayload> *nQueue);
		
		static void saveSentPayload(const std::string &devToken, const std::string &payload, uint32_t _id);
		static void setSentPayloadErrorCode(uint32_t _id, int errorCode);
		static void eraseOldPayloads();
	};
}

#endif
