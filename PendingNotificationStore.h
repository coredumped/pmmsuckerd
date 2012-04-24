//
//  PendingNotificationStore.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 4/17/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_PendingNotificationStore_h
#define PMM_Sucker_PendingNotificationStore_h
#include <inttypes.h>
#include "SharedQueue.h"
#include "NotificationPayload.h"

namespace pmm {
	class PendingNotificationStore {
	public:
		/** Saves every payload in the notification queue to a persistent store.
			@arg \c nQueue is the notification queue we want to save.
			@note Meant to be called in th event of an unhandled exception or a SIGSEGV */
		static void savePayloads(SharedQueue<NotificationPayload> *nQueue);
		
		/** Loads notification payloads from a persistent store to the current notification queue */
		static void loadPayloads(SharedQueue<NotificationPayload> *nQueue);
		
		static void saveSentPayload(const std::string &devToken, const std::string &payload, uint32_t _id);
		static void setSentPayloadErrorCode(uint32_t _id, int errorCode);
		static void eraseOldPayloads();
		static bool getDeviceTokenFromMessage(std::string &devToken, uint32_t _id);
	};
}

#endif
