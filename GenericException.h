//
//  GenericException.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/30/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_GenericException_h
#define PMM_Sucker_GenericException_h
#include <string>

namespace pmm {
	
	class GenericException {
	private:
		std::string errmsg;
	public:
		GenericException();
		GenericException(const std::string &_errmsg);
		std::string message();
	};
}

#endif
