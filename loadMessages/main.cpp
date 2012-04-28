//
//  main.cpp
//  loadMessages
//
//  Created by Juan Guerrero on 4/26/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <sqlite3.h>
#include<vector>
#include <sstream>
#include "PMMSuckerSession.h"
#include "MTLogger.h"

static bool getEmailAddress(std::string &_email, const std::string &devToken, const 	std::vector<pmm::MailAccountInfo> &emailAccounts){
	for (size_t i = 0; i < emailAccounts.size(); i++) {
		pmm::MailAccountInfo m = emailAccounts[i];
		std::vector<std::string> tokens = m.devTokens();
		for (size_t j = 0; j < tokens.size(); j++) {
			if (devToken.compare(tokens[j]) == 0) {
				_email = m.email();
				return true;
			}
		}
	}
	return false;
}

int main(int argc, const char * argv[])
{
	if(argc <= 1){
		std::cerr << "At least you must provide a datafile..." << std::endl;
		return 1;
	}
	pmm::Log.open("uploader.log");
	pmm::SuckerSession pmmSession;
	pmmSession.myID = "a5be5a486204462ac21a243e3861f24";
	std::vector<pmm::MailAccountInfo> emailAccounts;
	pmmSession.retrieveEmailAddresses(emailAccounts);

	sqlite3 *db = 0;
	int errCode = sqlite3_open(argv[1], &db);
	if(errCode != SQLITE_OK){
		std::cerr << "Unable to open database " << argv[1] << ": " << sqlite3_errmsg(db) << std::endl;
		return 1;
	}
	std::stringstream sqlCmd;
	sqlite3_stmt *statement;
	char *szTail;
	sqlCmd << "SELECT id,devtoken,message,tstamp FROM sent_notifications order by tstamp asc";
	errCode = sqlite3_prepare_v2(db, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&szTail);
	if(errCode != SQLITE_OK){
		std::cerr << "Unable to execute " << sqlCmd.str() << " due to: " << sqlite3_errmsg(db) << std::endl;
		return 1;
	}
	std::vector<int> ids2remove;
	while (sqlite3_step(statement) == SQLITE_ROW) {
		char *devToken = (char *)sqlite3_column_text(statement, 1);
		std::string email;
		if(getEmailAddress(email, devToken, emailAccounts)){
			std::string json = (char *)sqlite3_column_text(statement, 2);
			size_t pos = json.find("\",\"sound\":");
			if(pos == json.npos){
				std::cerr << "Unable to parse: " << json << std::endl;
				return 1;
			}
			std::string message = json.substr(17, pos - 17);
			
			pmm::NotificationPayload np(devToken, message);
			pmm::MailMessage mail;
			mail.to = email;
			pos = message.find("\\n");
			mail.from = message.substr(0, pos);
			mail.subject = message.substr(pos + 2);
			mail.msgUid = "0";
			mail.serverDate = (time_t)sqlite3_column_int(statement, 3);
			mail.dateOfArrival = (time_t)sqlite3_column_int(statement, 3);
			std::cout << "from: " << mail.from << std::endl;
			std::cout << "subject: " << mail.subject << std::endl;
			np.origMailMessage = mail;
			std::cout << "Sending message to " << mail.to << "... ";
			std::cout.flush();
			pmmSession.uploadNotificationMessage(np);
			std::cout << "done" << std::endl;
			ids2remove.push_back(sqlite3_column_int(statement, 0));
		}
	}
	sqlite3_finalize(statement);
	std::cout << ids2remove.size() << " messages uploaded" << std::endl;
	
	sqlite3_close(db);
    return 0;
}

