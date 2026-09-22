#ifndef XRT_SSH_TRANSPORT_MESSAGE_H
#define XRT_SSH_TRANSPORT_MESSAGE_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_TRANSPORT_MESSAGE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE)

#define XSSH_MSG_DISCONNECT 1u
#define XSSH_MSG_IGNORE 2u
#define XSSH_MSG_UNIMPLEMENTED 3u
#define XSSH_MSG_DEBUG 4u
#define XSSH_MSG_SERVICE_REQUEST 5u
#define XSSH_MSG_SERVICE_ACCEPT 6u
#define XSSH_MSG_EXT_INFO 7u
#define XSSH_MSG_NEWCOMPRESS 8u
#define XSSH_MSG_NEWKEYS 21u



/* RFC 4253 disconnect reason code。 */
typedef enum xsshdisconnectreason {
	XSSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT = 1,
	XSSH_DISCONNECT_PROTOCOL_ERROR = 2,
	XSSH_DISCONNECT_KEY_EXCHANGE_FAILED = 3,
	XSSH_DISCONNECT_RESERVED = 4,
	XSSH_DISCONNECT_MAC_ERROR = 5,
	XSSH_DISCONNECT_COMPRESSION_ERROR = 6,
	XSSH_DISCONNECT_SERVICE_NOT_AVAILABLE = 7,
	XSSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED = 8,
	XSSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE = 9,
	XSSH_DISCONNECT_CONNECTION_LOST = 10,
	XSSH_DISCONNECT_BY_APPLICATION = 11,
	XSSH_DISCONNECT_TOO_MANY_CONNECTIONS = 12,
	XSSH_DISCONNECT_AUTH_CANCELLED_BY_USER = 13,
	XSSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE = 14,
	XSSH_DISCONNECT_ILLEGAL_USER_NAME = 15
} xsshdisconnectreason;



/* Disconnect 视图借用完整 payload。 */
typedef struct xsshdisconnect {
	uint32 Reason;
	xstrview Description;
	xstrview Language;
} xsshdisconnect;



/* Ignore 视图借用完整 payload。 */
typedef struct xsshignore {
	xbytesview Data;
} xsshignore;



/* Debug 视图借用完整 payload。 */
typedef struct xsshdebug {
	bool AlwaysDisplay;
	xstrview Message;
	xstrview Language;
} xsshdebug;



/* Service 视图借用完整 payload。 */
typedef struct xsshservice {
	xstrview Name;
} xsshservice;



/* 单个 EXT_INFO 扩展视图借用完整 payload。 */
typedef struct xsshextension {
	xstrview Name;
	xbytesview Value;
} xsshextension;



/* EXT_INFO 迭代器已在初始化时严格验证全部字段。 */
typedef struct xsshextinfo {
	uint32 Count;
	uint32 Index;
	xsshreader Reader;
} xsshextinfo;



XRT_EXTERN_C_BEGIN



/* 读取 payload 的消息号，不推进任何外部状态。 */
XRT_API xsshcode xrtSshMessageType(xbytesview Payload, uint8* pMessage);



/* 写入或严格读取无字段 SSH_MSG_NEWKEYS。 */
XRT_API xsshcode xrtSshNewKeysWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshNewKeysRead(xbytesview Payload);



/* 写入或严格读取 SSH_MSG_DISCONNECT。 */
XRT_API xsshcode xrtSshDisconnectWrite(
	xsshwriter* pWriter,
	uint32 iReason,
	xstrview Description,
	xstrview Language
);
XRT_API xsshcode xrtSshDisconnectRead(
	xbytesview Payload,
	xsshdisconnect* pMessage
);



/* 写入或严格读取 SSH_MSG_IGNORE。 */
XRT_API xsshcode xrtSshIgnoreWrite(
	xsshwriter* pWriter,
	xbytesview Data
);
XRT_API xsshcode xrtSshIgnoreRead(
	xbytesview Payload,
	xsshignore* pMessage
);



/* 写入或严格读取 SSH_MSG_UNIMPLEMENTED 的拒绝序列号。 */
XRT_API xsshcode xrtSshUnimplementedWrite(
	xsshwriter* pWriter,
	uint32 iSequence
);
XRT_API xsshcode xrtSshUnimplementedRead(
	xbytesview Payload,
	uint32* pSequence
);



/* 写入或严格读取 SSH_MSG_DEBUG。 */
XRT_API xsshcode xrtSshDebugWrite(
	xsshwriter* pWriter,
	bool bAlwaysDisplay,
	xstrview Message,
	xstrview Language
);
XRT_API xsshcode xrtSshDebugRead(
	xbytesview Payload,
	xsshdebug* pMessage
);



/* 写入或严格读取 service request/accept。 */
XRT_API xsshcode xrtSshServiceRequestWrite(
	xsshwriter* pWriter,
	xstrview Service
);
XRT_API xsshcode xrtSshServiceRequestRead(
	xbytesview Payload,
	xsshservice* pMessage
);
XRT_API xsshcode xrtSshServiceAcceptWrite(
	xsshwriter* pWriter,
	xstrview Service
);
XRT_API xsshcode xrtSshServiceAcceptRead(
	xbytesview Payload,
	xsshservice* pMessage
);



/* 写入、验证并迭代 RFC 8308 SSH_MSG_EXT_INFO。 */
XRT_API xsshcode xrtSshExtInfoWrite(
	xsshwriter* pWriter,
	const xsshextension* pExtensions,
	size_t iCount
);
XRT_API xsshcode xrtSshExtInfoRead(
	xbytesview Payload,
	xsshextinfo* pExtInfo
);
XRT_API bool xrtSshExtInfoNext(
	xsshextinfo* pExtInfo,
	xsshextension* pExtension
);



/* 写入或严格读取 RFC 8308 SSH_MSG_NEWCOMPRESS。 */
XRT_API xsshcode xrtSshNewCompressWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshNewCompressRead(xbytesview Payload);



XRT_EXTERN_C_END

#endif

#endif
