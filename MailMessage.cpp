//
//  MailMessage.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "MailMessage.h"
#include "MTLogger.h"
#include "libetpan/mailmime.h"

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

	void MailMessage::parse(MailMessage &m, const std::string &rawMessage){
		size_t indx;
		struct mailmime *result;
		mailmime_parse(rawMessage.c_str(), rawMessage.size(), &indx, &result);
		

		for (clistiter *iter = clist_begin(result->mm_mime_fields->fld_list); iter != clist_end(result->mm_mime_fields->fld_list); iter = iter->next) {
			struct mailmime_field *field = (struct mailmime_field *)clist_content(iter);
			pmm::Log << field->fld_data.fld_id << pmm::NL;
		}
		
		mailmime_free(result);
	}
}
