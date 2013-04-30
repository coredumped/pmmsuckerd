//
//  DataTypes.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/31/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_DataTypes_h
#define PMM_Sucker_DataTypes_h
#include <string>

namespace pmm {

	class QuotaIncreasePetition {
		public:
		std::string emailAddress;
		int quotaValue;
		time_t creationTime;
		QuotaIncreasePetition();
		QuotaIncreasePetition(const std::string &_email, int quota);
		QuotaIncreasePetition(const QuotaIncreasePetition &q);
	};
}

#endif
