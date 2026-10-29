#ifndef XRT_INTERNAL_IMAP_CLIENT_H
#define XRT_INTERNAL_IMAP_CLIENT_H

#include "xrt_mail.h"

#if defined(XMAIL_FEATURE_IMAP_COMPRESS)
	#include <xrt/compress.h>
#endif



#if defined(XMAIL_FEATURE_IMAP_CLIENT)

bool __xrtImapClientStateCommit(
	ximapclient* pClient,
	ximapclientstate State
);



bool __xrtImapClientProtocolFail(
	ximapclient* pClient,
	cstr sMessage
);



bool __xrtImapClientAppendStart(ximapclient* pClient, size_t iSize);



size_t __xrtImapClientAppendRemaining(const ximapclient* pClient);



bool __xrtImapClientAppendEnd(ximapclient* pClient);



bool __xrtImapClientIdleStart(ximapclient* pClient);



bool __xrtImapClientIdleEnd(ximapclient* pClient);



#if defined(XMAIL_FEATURE_IMAP_COMPRESS)
bool __xrtImapClientCompressStart(
	ximapclient* pClient,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
);



bool __xrtImapClientCompressed(const ximapclient* pClient);
#endif



#endif

#endif
