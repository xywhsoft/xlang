#ifndef XRT_POP3_H
#define XRT_POP3_H

#include <xrt/mail_wire.h>



#if defined(XMAIL_FEATURE_POP3) && !defined(XMAIL_FEATURE_MAIL_WIRE)
	#error "XMAIL_FEATURE_POP3 requires XMAIL_FEATURE_MAIL_WIRE"
#endif



#if defined(XMAIL_FEATURE_POP3)

#define XPOP3_COMMAND_MAX 512u
#define XPOP3_AUTH_COMMAND_MAX 255u
#define XPOP3_AUTH_RESPONSE_MAX 12288u

#define XPOP3_CAP_TOP UINT32_C(0x00000001)
#define XPOP3_CAP_USER UINT32_C(0x00000002)
#define XPOP3_CAP_SASL UINT32_C(0x00000004)
#define XPOP3_CAP_RESP_CODES UINT32_C(0x00000008)
#define XPOP3_CAP_LOGIN_DELAY UINT32_C(0x00000010)
#define XPOP3_CAP_PIPELINING UINT32_C(0x00000020)
#define XPOP3_CAP_EXPIRE UINT32_C(0x00000040)
#define XPOP3_CAP_UIDL UINT32_C(0x00000080)
#define XPOP3_CAP_IMPLEMENTATION UINT32_C(0x00000100)
#define XPOP3_CAP_STLS UINT32_C(0x00000200)

#define XPOP3_SASL_PLAIN UINT32_C(0x00000001)
#define XPOP3_SASL_XOAUTH2 UINT32_C(0x00000002)
#define XPOP3_SASL_OAUTHBEARER UINT32_C(0x00000004)



/* POP3 状态行借用输入，Text 不包含状态指示符和其后的空白。 */
typedef struct xpop3replyview {
	xstrview Source;
	xstrview Text;
	bool Ok;
} xpop3replyview;



/* STAT 响应使用 64 位消息数量和总字节数。 */
typedef struct xpop3stat {
	uint64 Messages;
	uint64 Bytes;
} xpop3stat;



/* LIST 行包含 1 起始消息序号和字节数。 */
typedef struct xpop3listview {
	uint64 Message;
	uint64 Bytes;
} xpop3listview;



/* UIDL 行的唯一 ID 借用输入行。 */
typedef struct xpop3uidlview {
	uint64 Message;
	xstrview Id;
} xpop3uidlview;



/* CAPA 行的名称和参数均借用输入。 */
typedef struct xpop3capabilityview {
	xstrview Source;
	xstrview Name;
	xstrview Parameters;
} xpop3capabilityview;



XRT_EXTERN_C_BEGIN



/* 解析 `+OK` 或 `-ERR` 状态行。 */
XRT_API bool xrtPop3ReplyParse(xstrview Line, xpop3replyview* pReply);



/* 解析成功 STAT 响应中的消息数量和总字节数。 */
XRT_API bool xrtPop3StatParse(xstrview Line, xpop3stat* pStat);



/* 解析多行 LIST 响应中的一项。 */
XRT_API bool xrtPop3ListParse(xstrview Line, xpop3listview* pItem);



/* 解析多行 UIDL 响应中的一项。 */
XRT_API bool xrtPop3UidlParse(xstrview Line, xpop3uidlview* pItem);



/* 解析一条 CAPA 能力行。 */
XRT_API bool xrtPop3CapabilityParse(
	xstrview Line,
	xpop3capabilityview* pCapability
);



/* 返回已知 POP3 能力名称的稳定标记，未知扩展返回零。 */
XRT_API uint32 xrtPop3Capability(xstrview Name);



/* 返回内置 SASL 机制名称的稳定标记，未知机制返回零。 */
XRT_API uint32 xrtPop3SaslMechanism(xstrview Name);



/* 安全写出 `Verb [Arguments]\r\n`，拒绝控制字符和超长命令。 */
XRT_API bool xrtPop3CommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 POP3 命令行。 */
XRT_API str xrtPop3Command(
	xstrview Verb,
	xstrview Arguments,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
