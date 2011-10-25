//
//  MailMessage.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#ifdef __linux__
#include <string.h>
#endif
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
		to = m.to;
		dateOfArrival = m.dateOfArrival;
		msgUid = m.msgUid;
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
		bool gotTime = false;
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
					size_t s1pos; 
					if ((s1pos = m.from.find("=?")) != m.from.npos) {
						//Encoded with RFC 2047, let's decode this damn thing!!!
						size_t indx2 = 0;
						char *newFrom;
						//Find source encoding
						size_t s2pos;
						if ((s2pos = m.from.find_first_of("?", s1pos + 2)) != m.from.npos) {
							std::string sourceEncoding = m.from.substr(s1pos + 2, s2pos - s1pos - 2);
							mailmime_encoded_phrase_parse(sourceEncoding.c_str(), m.from.c_str(), m.from.size(), &indx2, sourceEncoding.c_str(), &newFrom);
							m.from = newFrom;
							free(newFrom);
						}
					}

#ifdef DEBUG
					pmm::Log << "DEBUG: From=\"" << m.from << "\"" << pmm::NL;
#endif
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
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
						if ((s2pos = m.subject.find_first_of("?", s1pos + 2)) != m.subject.npos) {
							std::string sourceEncoding = m.subject.substr(s1pos + 2, s2pos - s1pos - 2);
#warning Find out what are we going to do with Languages that use encodings different than utf-8
							mailmime_encoded_phrase_parse(sourceEncoding.c_str(), m.subject.c_str(), m.subject.size(), &indx2, sourceEncoding.c_str(), &newSubject);
							m.subject = newSubject;
							free(newSubject);
						}
					}
#ifdef DEBUG
					pmm::Log << "DEBUG: Subject=\"" << m.subject << "\"" << pmm::NL;
#endif
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
				}
					break;
				case MAILIMF_FIELD_ORIG_DATE:
				{
					struct mailimf_date_time *origDate = field->fld_data.fld_orig_date->dt_date_time;
					struct tm tDate;
					memset(&tDate, 0, sizeof(tm));
					tDate.tm_year = origDate->dt_year;
					tDate.tm_mon = origDate->dt_month;
					tDate.tm_mday = origDate->dt_day;
					tDate.tm_hour = origDate->dt_hour;
					tDate.tm_min = origDate->dt_min;
					tDate.tm_sec = origDate->dt_sec;
					tDate.tm_gmtoff = origDate->dt_zone;
					m.dateOfArrival = mktime(&tDate);
					gotTime = true;
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
				}
					break;
				default:
					break;
			}
		}
		if (m.subject.size() == 0) {
			//Retrieve the first 256 bytes of the body message
			size_t bIndx = 0;
			char *msgBody;
			size_t msgBodyLength;
			mailmime_base64_body_parse(rawMessage.c_str(), rawMessage.size(), &bIndx, &msgBody, &msgBodyLength);
			if (msgBodyLength <= 0) {
				m.subject = "(No Subject)";
			}
			else if (msgBodyLength < 256) {
				m.subject.assign(msgBody, msgBodyLength);
			}
			else {
				m.subject.assign(msgBody, 256);
			}
			if(msgBodyLength > 0) free(msgBody);
		}
#ifdef USE_IMF
		mailimf_message_free(result);
#else
		mailmime_free(result);
#endif
	}
}
