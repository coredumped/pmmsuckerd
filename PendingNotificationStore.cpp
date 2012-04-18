//
//  PendingNotificationStore.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 4/17/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include "PendingNotificationStore.h"
#include "UtilityFunctions.h"
#include "GenericException.h"
#include <iostream>
#include <sqlite3.h>
#ifndef DEFAULT_PENDING_NOTIFICATION_DATAFILE
#define DEFAULT_PENDING_NOTIFICATION_DATAFILE "pending_queue.db"
#endif
#ifndef DEFAULT_PENDING_NOTIFICATION_TABLE
#define DEFAULT_PENDING_NOTIFICATION_TABLE "pending_queue"
#endif

namespace pmm {

	static const char *qTable = DEFAULT_PENDING_NOTIFICATION_TABLE;
	
	static sqlite3 *connect2NotifDB(){
		sqlite3 *qDB = NULL;
		if(sqlite3_open(DEFAULT_PENDING_NOTIFICATION_DATAFILE, &qDB) != SQLITE_OK){
			pmm::Log << "Unable to open " << DEFAULT_PENDING_NOTIFICATION_DATAFILE << " datafile: " << sqlite3_errmsg(qDB) << pmm::NL;
			throw GenericException(sqlite3_errmsg(qDB));
		}
		//Create preferences table if it does not exists
		if(!pmm::tableExists(qDB, qTable)){
			std::stringstream createCmd;
			char *errmsg;
			createCmd << "CREATE TABLE " << qTable << " (";
			createCmd << "id		INTEGER PRIMARY KEY,";
			createCmd << "devtoken	TEXT,";
			createCmd << "message	TEXT,";
			createCmd << "sound		TEXT,";
			createCmd << "badge		INTEGER";
			createCmd << ")";
			if(sqlite3_exec(qDB, createCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK){
				pmm::Log << "Unable to create pending notifications table: " << errmsg << pmm::NL;
				throw GenericException(errmsg);
			}
		}
		return qDB;
	}
	
	void PendingNotificationStore::savePayloads(SharedQueue<NotificationPayload> *nQueue){
		sqlite3 *db = connect2NotifDB();
		NotificationPayload payload;
		int i = 1000;
		while (nQueue->extractEntry(payload)) {
			std::stringstream insCmd;
			char *errmsg;
			insCmd << "INSERT INTO " << qTable << " (id, devtoken, message, sound, badge) VALUES (";
			insCmd << (i++) << ", '" << payload.deviceToken() << "', ";
			insCmd << "'" << payload.message() << "', '" << payload.soundName() << "', " << payload.badge() << ")";
			if (sqlite3_exec(db, insCmd.str().c_str(), 0, 0, &errmsg) != SQLITE_OK) {
				pmm::Log << "Unable to add notification payload to persistent queue(" << insCmd.str() << "): " << errmsg << pmm::NL;
			}
		}
		sqlite3_close(db);
	}
	
	void PendingNotificationStore::loadPayloads(SharedQueue<NotificationPayload> *nQueue){
		sqlite3 *db = connect2NotifDB();
		std::stringstream sqlCmd;
		sqlite3_stmt *statement;
		char *szTail;
		sqlCmd << "SELECT devtoken,message,sound,badge FROM " << qTable;
		if(sqlite3_prepare_v2(db, sqlCmd.str().c_str(), (int)sqlCmd.str().size(), &statement, (const char **)&szTail) == SQLITE_OK){
			while (sqlite3_step(statement) == SQLITE_ROW) {
				const char *tok = (const char *)sqlite3_column_text(statement, 0);
				const char *msg = (const char *)sqlite3_column_text(statement, 1);
				const char *snd = (const char *)sqlite3_column_text(statement, 2);
				int badge = sqlite3_column_int(statement, 3);
				NotificationPayload payload(tok, msg, badge, snd);
				nQueue->add(payload);
			}
			sqlite3_finalize(statement);
		}
		else {
			const char *errmsg = sqlite3_errmsg(db);
			pmm::Log << "Unable to retrieve notification payloads from last crash or app shutdown (" << sqlCmd.str() << ") due to: " << errmsg << pmm::NL;
		}
		sqlite3_close(db);
		//Remove datafile
		unlink(DEFAULT_PENDING_NOTIFICATION_DATAFILE);
	}
	
}