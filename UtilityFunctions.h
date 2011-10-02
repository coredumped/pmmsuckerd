//
//  UtilityFunctions.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_UtilityFunctions_h
#define PMM_Sucker_UtilityFunctions_h
#include <string>

namespace pmm {

	void url_encode(std::string &theString);
	void devToken2Binary(std::string devTokenString, std::string &binaryDevToken);
}

#endif
