//
//  MailMessage.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#include "MTLogger.h"
#include "libetpan/libetpan.h"
#include "MailMessage.h"
#include "UtilityFunctions.h"

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
		size_t indx = 0;
		struct mailimf_fields *fields;
#ifdef USE_IMF
		struct mailimf_message *result;
		mailimf_message_parse(rawMessage.c_str(), rawMessage.size(), &indx, &result);
		fields = result->msg_fields->fld_list;
#else
		struct mailmime *result;
		int retCode = mailmime_parse(rawMessage.c_str(), rawMessage.size(), &indx, &result);
		fields = result->mm_data.mm_message.mm_fields;
#endif
		m.from = "";
		m.subject = "";
		for (clistiter *iter = clist_begin(fields->fld_list); iter != clist_end(fields->fld_list); iter = iter->next) {
			struct mailimf_field *field = (struct mailimf_field *)clist_content(iter);
			pmm::Log << field->fld_type << pmm::NL;
			switch (field->fld_type) {
				case MAILIMF_FIELD_FROM:
				{
					clistiter *iter2 = clist_begin(field->fld_data.fld_from->frm_mb_list->mb_list);
					struct mailimf_mailbox *mbox = (struct mailimf_mailbox *)clist_content(iter2);
					if(mbox->mb_display_name == NULL) m.from = mbox->mb_addr_spec;
					else{
						m.from = mbox->mb_display_name;
					}
#ifdef DEBUG
					pmm::Log << "DEBUG: From=\"" << m.from << "\"" << pmm::NL;
#endif
					if (m.from.size() > 0 && m.subject.size() > 0) break;
				}
					break;
				case MAILIMF_FIELD_SUBJECT:
				{
					m.subject = field->fld_data.fld_subject->sbj_value;
					size_t s1pos; 
					if ((s1pos = m.subject.find("=?")) != m.subject.npos) {
						//Encoded with RFC 2047, let's decode this damn thing!!!
						size_t indx2 = 0;
						char *newSubject;
						//Find source encoding
						size_t s2pos;
						if ((s2pos = m.subject.find("?", s1pos + 1)) != m.subject.npos) {
							std::string sourceEncoding = m.subject.substr(s1pos + 2, s2pos - s1pos - 3);
#warning Find out what are we going to do with Languages that use encodings different than utf-8
							mailmime_encoded_phrase_parse(sourceEncoding.c_str(), m.subject.c_str(), m.subject.size(), &indx2, sourceEncoding.c_str(), &newSubject);
							m.subject = newSubject;
							free(newSubject);
						}
					}
#ifdef DEBUG
					pmm::Log << "DEBUG: Subject=\"" << m.subject << "\"" << pmm::NL;
#endif
					if (m.from.size() > 0 && m.subject.size() > 0) break;
				}
					break;
				default:
					break;
			}
			
		}
#ifdef USE_IMF
		mailimf_message_free(result);
#else
		mailmime_free(result);
#endif
	}
}
