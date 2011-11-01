//
//  DataTypes.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/31/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "DataTypes.h"

namespace pmm {
	QuotaIncreasePetition::QuotaIncreasePetition(){
		
	}
	
	QuotaIncreasePetition::QuotaIncreasePetition(const std::string &_email, int quota){
		emailAddress = _email;
		quotaValue = quota;
	}
	
	QuotaIncreasePetition::QuotaIncreasePetition(const QuotaIncreasePetition &q){
		emailAddress = q.emailAddress;
		quotaValue = q.quotaValue;
	}

}