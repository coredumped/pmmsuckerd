//
//  GenericThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_GenericThread_h
#define PMM_Sucker_GenericThread_h
#include"Mutex.h"
#include<string>

namespace pmm {
	class GenericThread {
	private:
	protected:
	public:
		/** Tells wheter the thread is still running */
		pmm::AtomicFlag isRunning;
		GenericThread();
		virtual ~GenericThread();
		/** This method is called before your thread starts running */
		virtual void onStart();
		/** Override this method with the code you want to execute concurrently. */
		virtual void operator()();
		/** This method is called after your thread has finished executing */
		virtual void onFinish();
	};
	
}

#endif
