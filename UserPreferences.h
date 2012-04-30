//
//  UserPreferences.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 2/20/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_UserPreferences_h
#define PMM_Sucker_UserPreferences_h
#include <string>
#include <sqlite3.h>
#include "SharedQueue.h"
#include "GenericThread.h"

namespace pmm {
	struct PreferenceQueueItem {
		std::string emailAccount;
		std::string settingDomain;
		std::string settingKey;
		std::string settingValue;
	};
	
	class PreferenceEngine : public GenericThread {
	private:
	protected:
		sqlite3 *db;
	public:
		SharedQueue<PreferenceQueueItem> *preferenceQueue;
		PreferenceEngine();
		~PreferenceEngine();
		
		static void defaultAlertTone(std::string &alertTonePath, const std::string &emailAccount);
		void userPreferenceGet(std::map<std::string, std::string> &preferenceMap, const std::string &emailAccount, const std::string &domain = "user");
		
		void destroyPreferences(const std::string &emailAccount);
		void operator()();
	};
}


#endif
