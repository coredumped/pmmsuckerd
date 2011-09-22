//
//  JSONParserException.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/22/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include "JSONParseException.h"

namespace pmm {
	JSONParseException::JSONParseException(){
		
	}
	
	JSONParseException::JSONParseException(const std::string &_errorText){
		_description = _errorText;
	}
	
	std::string JSONParseException::description(){
		return std::string(_description);
	}

}