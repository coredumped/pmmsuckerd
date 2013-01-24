//
//  ConcurrentTest.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 1/22/13.
//
//

#ifndef __PMM_Sucker__ConcurrentTest__
#define __PMM_Sucker__ConcurrentTest__
#include "GenericThread.h"
#include "Atomic.h"
#include "ObjectDatastore.h"
#include <iostream>

namespace pmmstore {
	class ConcurrentTest : public pmm::GenericThread {
	public:
		int succ;
		bool succeeded;
		pmm::ObjectDatastore *objStore;
		void operator()();
	};
}

#endif /* defined(__PMM_Sucker__ConcurrentTest__) */
