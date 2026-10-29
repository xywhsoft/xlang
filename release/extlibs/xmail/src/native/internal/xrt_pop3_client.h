#ifndef XRT_INTERNAL_POP3_CLIENT_H
#define XRT_INTERNAL_POP3_CLIENT_H

#include "xrt_mail_net.h"



#if defined(XMAIL_FEATURE_POP3_CLIENT)

/* POP3 认证扩展只共享状态对象，不复制客户端实现。 */
struct xpop3client {
	__xmailtransport Transport;
	xpop3clientstate State;
	xpop3clientstate ReturnState;
	uint32 Capabilities;
	uint32 SaslMechanisms;
	__xmailtext Reply;
};



bool __xrtPop3ClientReplySave(
	xpop3client* pClient,
	xstrview Line,
	xpop3reply* pReply
);



bool __xrtPop3ClientFail(xpop3client* pClient);



bool __xrtPop3ClientAuthorize(xpop3client* pClient);

#endif

#endif
