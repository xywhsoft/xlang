#ifndef XRT_INTERNAL_MAIL_AUTH_H
#define XRT_INTERNAL_MAIL_AUTH_H

#include "xrt_mail.h"

#include <xrt/codec.h>



#if defined(XMAIL_FEATURE_SMTP_AUTH) || defined(XMAIL_FEATURE_POP3_AUTH) || \
	defined(XMAIL_FEATURE_IMAP_AUTH)

bool __xrtMailAuthFieldValid(xstrview Text, bool bRejectSoh);



void __xrtMailAuthFree(char* sText, size_t iSize);



char* __xrtMailAuthEncode(
	const void* pData,
	size_t iSize,
	size_t* pEncodedSize
);



char* __xrtMailAuthPlain(
	xstrview AuthorizationId,
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
);



char* __xrtMailAuthXoauth2(
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
);



char* __xrtMailAuthOauthBearer(
	xstrview AuthorizationId,
	xstrview Secret,
	size_t* pEncodedSize
);

#endif

#endif
