//
//  GenericException.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/30/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "GenericException.h"

namespace pmm {
	
	GenericException::GenericException(){
		
	}
	
	GenericException::GenericException(const std::string &_errmsg){
		errmsg = _errmsg;
	}
	
	std::string GenericException::message(){
		return std::string(errmsg);
	}

}