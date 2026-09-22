#ifndef XRT_SSH_CONNECTION_MESSAGE_H
#define XRT_SSH_CONNECTION_MESSAGE_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_CONNECTION_MESSAGE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_CONNECTION_MESSAGE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_CONNECTION_MESSAGE)

#define XSSH_MSG_GLOBAL_REQUEST 80u
#define XSSH_MSG_REQUEST_SUCCESS 81u
#define XSSH_MSG_REQUEST_FAILURE 82u



/* 全局请求借用完整 payload，未知类型字段保持原始字节。 */
typedef struct xsshglobalrequest {
	xstrview Name;
	bool WantReply;
	xbytesview Fields;
} xsshglobalrequest;



XRT_EXTERN_C_BEGIN



/* 写入或严格读取可扩展全局请求。 */
XRT_API xsshcode xrtSshGlobalRequestWrite(
	xsshwriter* pWriter,
	xstrview Name,
	bool bWantReply,
	xbytesview Fields
);
XRT_API xsshcode xrtSshGlobalRequestRead(
	xbytesview Payload,
	xsshglobalrequest* pRequest
);



/* 写入或严格读取带任意请求专用数据的成功响应。 */
XRT_API xsshcode xrtSshGlobalSuccessWrite(
	xsshwriter* pWriter,
	xbytesview Fields
);
XRT_API xsshcode xrtSshGlobalSuccessRead(
	xbytesview Payload,
	xbytesview* pFields
);



/* 写入或严格读取无字段失败响应。 */
XRT_API xsshcode xrtSshGlobalFailureWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshGlobalFailureRead(xbytesview Payload);



XRT_EXTERN_C_END

#endif

#endif
