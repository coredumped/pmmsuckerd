//
//  MailMessage.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "MailMessage.h"

namespace pmm {
	MailMessage::MailMessage(){
		
	}
	
	MailMessage::MailMessage(const std::string &_from, const std::string &_subject){
		from = _from;
		subject = _subject;
	}
	
	MailMessage::MailMessage(const MailMessage &m){
		from = m.from;
		subject = m.subject;
	}

}
