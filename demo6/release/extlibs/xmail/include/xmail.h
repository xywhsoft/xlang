#ifndef XMAIL_H
#define XMAIL_H

#include <xmail/features.h>
#include <xrt.h>
#include <xrt/mail.h>

#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) && \
	(!defined(XMAIL_FEATURE_MAIL_NET) || \
	 !defined(XRT_FEATURE_DEFLATE) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XMAIL_FEATURE_MAIL_NET_DEFLATE requires mail net, Deflate and Inflate"
#endif

#if defined(XMAIL_FEATURE_MAIL) && \
	(!defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
	 !defined(XMAIL_FEATURE_SMTP_SUBMIT) || \
	 !defined(XMAIL_FEATURE_SMTP_AUTH) || \
	 !defined(XMAIL_FEATURE_POP3_CLIENT_TLS) || \
	 !defined(XMAIL_FEATURE_POP3_AUTH) || \
	 !defined(XMAIL_FEATURE_POP3_MESSAGE) || \
	 !defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
	 !defined(XMAIL_FEATURE_IMAP_AUTH) || \
	 !defined(XMAIL_FEATURE_IMAP_BODY) || \
	 !defined(XMAIL_FEATURE_IMAP_MESSAGE) || \
	 !defined(XMAIL_FEATURE_IMAP_APPEND) || \
	 !defined(XMAIL_FEATURE_IMAP_COMPRESS))
	#error "XMAIL_FEATURE_MAIL requires every public mail capability"
#endif

#if defined(XMAIL_FEATURE_MAIL_CODEC)
	#include <xrt/mail_codec.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_HEADER)
	#include <xrt/mail_header.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_WORD)
	#include <xrt/mail_word.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_ADDRESS)
	#include <xrt/mail_address.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_DATE)
	#include <xrt/mail_date.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_ID)
	#include <xrt/mail_id.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_PARAM)
	#include <xrt/mail_param.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_MULTIPART)
	#include <xrt/mail_multipart.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_MESSAGE)
	#include <xrt/mail_message.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_TREE)
	#include <xrt/mail_tree.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_BUILD)
	#include <xrt/mail_build.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_COMPOSE)
	#include <xrt/mail_compose.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_WIRE)
	#include <xrt/mail_wire.h>
#endif

#if defined(XMAIL_FEATURE_SMTP)
	#include <xrt/smtp.h>
#endif

#if defined(XMAIL_FEATURE_POP3)
	#include <xrt/pop3.h>
#endif

#if defined(XMAIL_FEATURE_POP3_CLIENT) || \
	defined(XMAIL_FEATURE_POP3_CLIENT_TLS)
	#include <xrt/pop3_client.h>
#endif

#if defined(XMAIL_FEATURE_POP3_AUTH)
	#include <xrt/pop3_auth.h>
#endif

#if defined(XMAIL_FEATURE_POP3_MESSAGE)
	#include <xrt/pop3_message.h>
#endif

#if defined(XMAIL_FEATURE_IMAP)
	#include <xrt/imap.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_DATA)
	#include <xrt/imap_data.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_BODY)
	#include <xrt/imap_body.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_CLIENT) || \
	defined(XMAIL_FEATURE_IMAP_CLIENT_TLS)
	#include <xrt/imap_client.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_AUTH)
	#include <xrt/imap_auth.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_COMMAND)
	#include <xrt/imap_command.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_MESSAGE)
	#include <xrt/imap_message.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_APPEND)
	#include <xrt/imap_append.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_COMPRESS)
	#include <xrt/imap_compress.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_NET) || defined(XMAIL_FEATURE_MAIL_NET_TLS)
	#include <xrt/mail_net.h>
#endif

#if defined(XMAIL_FEATURE_SMTP_CLIENT) || \
	defined(XMAIL_FEATURE_SMTP_CLIENT_TLS)
	#include <xrt/smtp_client.h>
#endif

#if defined(XMAIL_FEATURE_SMTP_AUTH)
	#include <xrt/smtp_auth.h>
#endif

#if defined(XMAIL_FEATURE_SMTP_SUBMIT)
	#include <xrt/smtp_submit.h>
#endif

#endif
