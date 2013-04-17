/*
 * Implements direct pmmsuckerd commands and distributed
 * descentralized collaboration between suckers
 */

namespace cpp pmmrpc

struct Command {
	1:string name;
	2:map<string, string> parameter;
}

struct FetchDBItem {
	1:i32 timestamp;
	2:string email;
	3:string uid;
}

struct NotificationPayload {
	1:string devtoken;
	2:string message;
	3:string sound;
	4:i32 badge;
}

struct MailAccountInfo {
	1:string email;
	2:string mailboxType;
	3:string username;
	4:string password;
	5:string serverAddress;
	6:i32 serverPort;
	7:bool useSSL;
	8:list<string> devTokens;
	9:bool isEnabled;
	10:i32 quota;
	11:bool devel;
}

exception GenericException {
	1:i32 errorCode;
	2:string errorMessage;
}

exception FetchDBUnableToPutItemException {
	1:FetchDBItem item;
	2:string errorMessage;
}

service PMMSuckerRPC {
	//Information retrieval
	list<string> getAllEmailAccounts() throws (1:GenericException ex1);

	//FetchDB interface
	bool fetchDBPutItem(1:string email, 2:string uid) throws (1:FetchDBUnableToPutItemException ex1, 2:GenericException ex2);
	void fetchDBPutItemAsync(1:string email, 2:string uid) throws (1:FetchDBUnableToPutItemException ex1, 2:GenericException ex2);
	void fetchDBInitialSyncPutItemAsync(1:string email, 2:string uidBatch, 3:string delim) throws (1:FetchDBUnableToPutItemException ex1, 2:GenericException ex2);
	list<FetchDBItem> fetchDBGetItems(1:string email) throws (1:GenericException ex1);

	//Helpers for push notifications
	bool notificationPayloadPush() throws (1:GenericException ex1);
	
	//Remote command processing
	bool commandSubmit(1:Command cmd);
	
	//E-mail account polling manipulation
	bool emailAccountRegister(1:MailAccountInfo m) throws (1:GenericException ex1);
	bool emailAccountUnregister(1:string email) throws (1:GenericException ex1);
}
