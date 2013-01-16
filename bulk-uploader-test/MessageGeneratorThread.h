//
//  MessageGeneratorThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 1/15/13.
//
//

#ifndef __PMM_Sucker__MessageGeneratorThread__
#define __PMM_Sucker__MessageGeneratorThread__
#include <iostream>
#include "GenericThread.h"
#include "NotificationPayload.h"
#include "SharedQueue.h"

namespace pmmtest {
	class MessageGeneratorThread : public pmm::GenericThread {
	private:
	protected:
		int wait_microsecs;
	public:
		pmm::SharedQueue<pmm::NotificationPayload> *msgQueue;
		MessageGeneratorThread();
		MessageGeneratorThread(int min_generation_interval);
		void operator()();
	};
}



#endif /* defined(__PMM_Sucker__MessageGeneratorThread__) */
