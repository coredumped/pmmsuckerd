//
//  JSONParseException.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/21/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef PMM_Sucker_JSONParseException_h
#define PMM_Sucker_JSONParseException_h
#include<string>

namespace pmm {
	class JSONParseException {
	private:
		std::string _description;
	protected:
	public:
		JSONParseException();
		JSONParseException(const std::string &_errorText);
		std::string description();
	};
}

#endif
