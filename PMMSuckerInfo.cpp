//
//  PMMSuckerInfi.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 3/26/13.
//
//

#include "PMMSuckerInfo.h"
#ifndef DEFAULT_PMMSUCKER_IPC_SECRET
#define DEFAULT_PMMSUCKER_IPC_SECRET "6479eee5c1b7b014cbe3971616d04d10"
#endif

namespace pmm {
	
	extern const std::string DefaultPMMSuckerSecret = DEFAULT_PMMSUCKER_IPC_SECRET;
	
	PMMSuckerInfo::PMMSuckerInfo() {
		port = -1;
		allowsPOP3 = true;
		allowsIMAP = true;
		secret = DEFAULT_PMMSUCKER_IPC_SECRET;
	}
	
}
