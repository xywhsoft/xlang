#ifndef XRT_SSH_CHANNEL_MESSAGE_H
#define XRT_SSH_CHANNEL_MESSAGE_H

#include <xrt/charset.h>
#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_CHANNEL_MESSAGE) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_UNICODE))
	#error "XSSH_FEATURE_CHANNEL_MESSAGE requires XSSH_FEATURE_WIRE and XRT_FEATURE_UNICODE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_MESSAGE)

#define XSSH_MSG_CHANNEL_OPEN 90u
#define XSSH_MSG_CHANNEL_OPEN_CONFIRMATION 91u
#define XSSH_MSG_CHANNEL_OPEN_FAILURE 92u
#define XSSH_MSG_CHANNEL_WINDOW_ADJUST 93u
#define XSSH_MSG_CHANNEL_DATA 94u
#define XSSH_MSG_CHANNEL_EXTENDED_DATA 95u
#define XSSH_MSG_CHANNEL_EOF 96u
#define XSSH_MSG_CHANNEL_CLOSE 97u
#define XSSH_MSG_CHANNEL_REQUEST 98u
#define XSSH_MSG_CHANNEL_SUCCESS 99u
#define XSSH_MSG_CHANNEL_FAILURE 100u

#define XSSH_CHANNEL_OPEN_ADMINISTRATIVELY_PROHIBITED 1u
#define XSSH_CHANNEL_OPEN_CONNECT_FAILED 2u
#define XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE 3u
#define XSSH_CHANNEL_OPEN_RESOURCE_SHORTAGE 4u

#define XSSH_CHANNEL_EXTENDED_DATA_STDERR 1u



/* Channel open 借用类型专用字段，不限制扩展 channel 类型。 */
typedef struct xsshchannelopen {
	xstrview Type;
	uint32 Sender;
	uint32 Window;
	uint32 MaxPacket;
	xbytesview Fields;
} xsshchannelopen;



/* Channel open confirmation 借用类型专用确认字段。 */
typedef struct xsshchannelconfirmation {
	uint32 Recipient;
	uint32 Sender;
	uint32 Window;
	uint32 MaxPacket;
	xbytesview Fields;
} xsshchannelconfirmation;



/* Channel open failure 借用 UTF-8 描述和 ASCII language tag。 */
typedef struct xsshchannelopenfailure {
	uint32 Recipient;
	uint32 Reason;
	xstrview Description;
	xstrview Language;
} xsshchannelopenfailure;



/* Window adjust 保留完整 uint32 增量。 */
typedef struct xsshchanneladjust {
	uint32 Recipient;
	uint32 Bytes;
} xsshchanneladjust;



/* 普通 channel data 借用二进制 string 内容。 */
typedef struct xsshchanneldata {
	uint32 Recipient;
	xbytesview Data;
} xsshchanneldata;



/* Extended data 保留未知类型码与二进制内容。 */
typedef struct xsshchannelextendeddata {
	uint32 Recipient;
	uint32 Type;
	xbytesview Data;
} xsshchannelextendeddata;



/* Channel request 借用未知请求的全部专用字段。 */
typedef struct xsshchannelrequest {
	uint32 Recipient;
	xstrview Type;
	bool WantReply;
	xbytesview Fields;
} xsshchannelrequest;



XRT_EXTERN_C_BEGIN



/* 写入或读取可扩展 channel open。 */
XRT_API xsshcode xrtSshChannelOpenWrite(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
);
XRT_API xsshcode xrtSshChannelOpenRead(
	xbytesview Payload,
	xsshchannelopen* pOpen
);



/* 写入或读取带类型专用字段的 channel open confirmation。 */
XRT_API xsshcode xrtSshChannelOpenConfirmationWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
);
XRT_API xsshcode xrtSshChannelOpenConfirmationRead(
	xbytesview Payload,
	xsshchannelconfirmation* pConfirmation
);



/* 写入或读取 channel open failure。 */
XRT_API xsshcode xrtSshChannelOpenFailureWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iReason,
	xstrview Description,
	xstrview Language
);
XRT_API xsshcode xrtSshChannelOpenFailureRead(
	xbytesview Payload,
	xsshchannelopenfailure* pFailure
);



/* 写入或读取 channel window adjust。 */
XRT_API xsshcode xrtSshChannelWindowAdjustWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iBytes
);
XRT_API xsshcode xrtSshChannelWindowAdjustRead(
	xbytesview Payload,
	xsshchanneladjust* pAdjust
);



/* 写入或读取普通 channel data。 */
XRT_API xsshcode xrtSshChannelDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xbytesview Data
);
XRT_API xsshcode xrtSshChannelDataRead(
	xbytesview Payload,
	xsshchanneldata* pData
);



/* 写入或读取带类型码的 channel extended data。 */
XRT_API xsshcode xrtSshChannelExtendedDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iType,
	xbytesview Data
);
XRT_API xsshcode xrtSshChannelExtendedDataRead(
	xbytesview Payload,
	xsshchannelextendeddata* pData
);



/* 写入或严格读取 channel EOF。 */
XRT_API xsshcode xrtSshChannelEofWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelEofRead(
	xbytesview Payload,
	uint32* pRecipient
);



/* 写入或严格读取 channel close。 */
XRT_API xsshcode xrtSshChannelCloseWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelCloseRead(
	xbytesview Payload,
	uint32* pRecipient
);



/* 写入或读取保留未知专用字段的 channel request。 */
XRT_API xsshcode xrtSshChannelRequestWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	xbytesview Fields
);
XRT_API xsshcode xrtSshChannelRequestRead(
	xbytesview Payload,
	xsshchannelrequest* pRequest
);



/* 写入或严格读取 channel request success。 */
XRT_API xsshcode xrtSshChannelSuccessWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelSuccessRead(
	xbytesview Payload,
	uint32* pRecipient
);



/* 写入或严格读取 channel request failure。 */
XRT_API xsshcode xrtSshChannelFailureWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelFailureRead(
	xbytesview Payload,
	uint32* pRecipient
);



XRT_EXTERN_C_END

#endif

#endif
