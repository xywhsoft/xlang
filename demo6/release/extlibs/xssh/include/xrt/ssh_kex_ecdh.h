#ifndef XRT_SSH_KEX_ECDH_H
#define XRT_SSH_KEX_ECDH_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_KEX_ECDH) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_KEX_ECDH requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_KEX_ECDH)

#define XSSH_MSG_KEX_ECDH_INIT 30u
#define XSSH_MSG_KEX_ECDH_REPLY 31u



/* ECDH init 视图借用完整 payload。 */
typedef struct xsshecdhinit {
	xbytesview ClientPublic;
} xsshecdhinit;



/* ECDH reply 视图借用完整 payload。 */
typedef struct xsshecdhreply {
	xbytesview ServerHostKey;
	xbytesview ServerPublic;
	xbytesview Signature;
} xsshecdhreply;



XRT_EXTERN_C_BEGIN



/* 构建 SSH_MSG_KEX_ECDH_INIT payload。 */
XRT_API xsshcode xrtSshEcdhInitWrite(
	xsshwriter* pWriter,
	xbytesview ClientPublic
);



/* 严格解析完整 SSH_MSG_KEX_ECDH_INIT payload。 */
XRT_API xsshcode xrtSshEcdhInitRead(
	xbytesview Payload,
	xsshecdhinit* pMessage
);



/* 构建 SSH_MSG_KEX_ECDH_REPLY payload。 */
XRT_API xsshcode xrtSshEcdhReplyWrite(
	xsshwriter* pWriter,
	xbytesview ServerHostKey,
	xbytesview ServerPublic,
	xbytesview Signature
);



/* 严格解析完整 SSH_MSG_KEX_ECDH_REPLY payload。 */
XRT_API xsshcode xrtSshEcdhReplyRead(
	xbytesview Payload,
	xsshecdhreply* pMessage
);



XRT_EXTERN_C_END

#endif

#endif
