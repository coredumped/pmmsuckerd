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
	
	static void getMIMEData(struct mailmime_data * data, std::stringstream &outputStream)
	{
		if (data->dt_type == MAILMIME_DATA_TEXT) {
			switch (data->dt_encoding) {
				case MAILMIME_MECHANISM_BASE64:
				{
					//Decode base64 encoding
					char *decodedMsg;
					size_t decodedSize, indx = 0;
					int r = mailmime_base64_body_parse(data->dt_data.dt_text.dt_data, data->dt_data.dt_text.dt_length, &indx, &decodedMsg, &decodedSize);
					if (r == MAILIMF_NO_ERROR) {
						outputStream.write(decodedMsg, decodedSize);
						mailmime_decoded_part_free(decodedMsg);
					}
				}
					break;
				default:
					outputStream.write(data->dt_data.dt_text.dt_data, data->dt_data.dt_text.dt_length);
					break;
			}
		}
	}

	static void getMIMEMsgBody(struct mailmime * mime, std::stringstream &outputStream)
	{
		clistiter * cur;		
		switch (mime->mm_type) {
			case MAILMIME_SINGLE:
				getMIMEData(mime->mm_data.mm_single, outputStream);
				break;
			case MAILMIME_MULTIPLE:
				for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ; cur != NULL ; cur = clist_next(cur)) {
					getMIMEMsgBody((struct mailmime *)clist_content(cur), outputStream);
				}
				break;
				
			case MAILMIME_MESSAGE:
				if (mime->mm_data.mm_message.mm_fields) {					
					if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
						getMIMEMsgBody(mime->mm_data.mm_message.mm_msg_mime, outputStream);
					}
					break;
				}
		}
	}

	
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
		m.dateOfArrival = time(0);
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
					struct tm tDate = { 0 };
					memset(&tDate, 0, sizeof(tm));
					tDate.tm_year = origDate->dt_year - 1900;
					tDate.tm_mon = origDate->dt_month - 1;
					tDate.tm_mday = origDate->dt_day;
					tDate.tm_hour = origDate->dt_hour;
					tDate.tm_min = origDate->dt_min;
					tDate.tm_sec = origDate->dt_sec;
					tDate.tm_gmtoff = origDate->dt_zone;
					m.dateOfArrival = mktime(&tDate);
#ifdef DEBUG
					time_t _crTime = time(0);
					pmm::Log << "DEBUG: Computed dateOfArrival=" << m.dateOfArrival << " currTstamp=" << _crTime << " diff=" << (m.dateOfArrival - _crTime) << pmm::NL;
#endif
					gotTime = true;
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
				}
					break;
				default:
					break;
			}
		}
		if (m.subject.size() == 0) {
#ifdef DEBUG
			pmm::Log << "DEBUG: Computing subject from: " << pmm::NL;
			pmm::Log << rawMessage << pmm::NL;
#endif
			std::stringstream msgBody;
			getMIMEMsgBody(result, msgBody);
			std::string theBody = msgBody.str();
			if(theBody.size() > 0 && theBody.size() < 256){
				m.subject.assign(theBody.c_str(), theBody.size());
			}
			else {
				m.subject.assign(theBody.c_str(), 256);
			}
			while (m.subject[0] == '\r' || m.subject[0] == '\n') {
				m.subject = m.subject.substr(1);
			}
		}
#ifdef USE_IMF
		mailimf_message_free(result);
#else
		mailmime_free(result);
#endif
	}
}
