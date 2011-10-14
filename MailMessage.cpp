//
//  MailMessage.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "MTLogger.h"
#include "libetpan/libetpan.h"
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
	
	void MailMessage::parse(MailMessage &m, const std::string &rawMessage){
		size_t indx;
		struct mailimf_message *result;
		mailimf_message_parse(rawMessage.c_str(), rawMessage.size(), &indx, &result);
		
		
		for (clistiter *iter = clist_begin(result->msg_fields->fld_list); iter != clist_end(result->msg_fields->fld_list); iter = iter->next) {
			struct mailimf_field *field = (struct mailimf_field *)clist_content(iter);
			pmm::Log << field->fld_type << pmm::NL;
			switch (field->fld_type) {
				case MAILIMF_FIELD_FROM:
				{
					clistiter *iter2 = clist_begin(field->fld_data.fld_from->frm_mb_list->mb_list);
					struct mailimf_mailbox *mbox = (struct mailimf_mailbox *)clist_content(iter2);
					if(mbox->mb_display_name == NULL) m.from = mbox->mb_addr_spec;
					else m.from = mbox->mb_display_name;
#ifdef DEBUG
					pmm::Log << "DEBUG: From=\"" << m.from << "\"" << pmm::NL;
#endif
				}
					break;
				case MAILIMF_FIELD_SUBJECT:
				{
					m.subject = field->fld_data.fld_subject->sbj_value;
#ifdef DEBUG
					pmm::Log << "DEBUG: Subject=\"" << m.subject << "\"" << pmm::NL;
#endif
				}
					break;
				default:
					break;
			}
			
		}
		mailimf_message_free(result);
	}
}
