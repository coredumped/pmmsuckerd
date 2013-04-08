//
//  PMMSuckerInfi.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 3/26/13.
//
//

#ifndef __PMM_Sucker__PMMSuckerInfo__
#define __PMM_Sucker__PMMSuckerInfo__

#include <iostream>

namespace pmm {
	class PMMSuckerInfo {
	public:
		std::string suckerID;
		std::string hostname;
		bool allowsIMAP;
		bool allowsPOP3;
		int port;
		std::string secret;
		
		PMMSuckerInfo();
	};
	
	extern const std::string DefaultPMMSuckerSecret;
}

#endif /* defined(__PMM_Sucker__PMMSuckerInfo__) */
