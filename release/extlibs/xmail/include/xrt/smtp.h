#ifndef XRT_SMTP_H
#define XRT_SMTP_H

#include <xrt/mail_wire.h>



#if defined(XMAIL_FEATURE_SMTP) && !defined(XMAIL_FEATURE_MAIL_WIRE)
	#error "XMAIL_FEATURE_SMTP requires XMAIL_FEATURE_MAIL_WIRE"
#endif



#if defined(XMAIL_FEATURE_SMTP)

#define XSMTP_REPLY_LINES_DEFAULT 100u
#define XSMTP_COMMAND_MAX 512u
#define XSMTP_AUTH_RESPONSE_MAX 12288u

#define XSMTP_CAP_PIPELINING UINT64_C(0x00000001)
#define XSMTP_CAP_SIZE UINT64_C(0x00000002)
#define XSMTP_CAP_STARTTLS UINT64_C(0x00000004)
#define XSMTP_CAP_AUTH_PLAIN UINT64_C(0x00000008)
#define XSMTP_CAP_AUTH_LOGIN UINT64_C(0x00000010)
#define XSMTP_CAP_AUTH_XOAUTH2 UINT64_C(0x00000020)
#define XSMTP_CAP_8BITMIME UINT64_C(0x00000040)
#define XSMTP_CAP_SMTPUTF8 UINT64_C(0x00000080)
#define XSMTP_CAP_DSN UINT64_C(0x00000100)
#define XSMTP_CAP_CHUNKING UINT64_C(0x00000200)
#define XSMTP_CAP_AUTH_OAUTHBEARER UINT64_C(0x00000400)
#define XSMTP_CAP_BINARYMIME UINT64_C(0x00000800)
#define XSMTP_CAP_ENHANCED_STATUS UINT64_C(0x00001000)



/* 单条 SMTP 响应行借用输入；Continued 为真表示后续仍有同码行。 */
typedef struct xsmtpreplyline {
	xstrview Source;
	xstrview Text;
	int Code;
	bool Continued;
} xsmtpreplyline;



/* 响应状态机验证多行响应代码、数量和唯一终止行。 */
typedef struct xsmtpreplyparser {
	int Code;
	size_t Lines;
	size_t MaxLines;
	bool Started;
	bool Done;
} xsmtpreplyparser;



/* EHLO 能力名称与参数均借用响应文本。 */
typedef struct xsmtpcapabilityview {
	xstrview Source;
	xstrview Name;
	xstrview Parameters;
} xsmtpcapabilityview;



XRT_EXTERN_C_BEGIN



/* 验证不含尖括号的 SMTP reverse-path 或 forward-path。 */
XRT_API bool xrtSmtpPathValid(xstrview Path, bool AllowEmpty);



/* 解析一条 `ddd-Text` 或 `ddd Text` SMTP 响应行。 */
XRT_API bool xrtSmtpReplyLineParse(
	xstrview Line,
	xsmtpreplyline* pReply
);



/* 初始化多行 SMTP 响应验证器；零限制使用默认值。 */
XRT_API bool xrtSmtpReplyParserInit(
	xsmtpreplyparser* pParser,
	size_t iMaxLines
);



/* 接受并发布一条响应行；解析器完成后不再接受输入。 */
XRT_API bool xrtSmtpReplyRead(
	xsmtpreplyparser* pParser,
	xstrview Line,
	xsmtpreplyline* pReply
);



/* 把 EHLO 文本拆成能力名称和可选参数。 */
XRT_API bool xrtSmtpCapabilityParse(
	xstrview Text,
	xsmtpcapabilityview* pCapability
);



/* 返回已知能力名称对应的位；未知扩展返回零。 */
XRT_API uint64 xrtSmtpCapability(xstrview Name);



/* 把一条能力及其参数合并到位集和 SIZE 上限。 */
XRT_API bool xrtSmtpCapabilityAdd(
	const xsmtpcapabilityview* pCapability,
	uint64* pCapabilities,
	uint64* pSizeLimit
);



/* 安全写出 `Verb [Arguments]\r\n`，拒绝控制字符和命令注入。 */
XRT_API bool xrtSmtpCommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 SMTP 命令行。 */
XRT_API str xrtSmtpCommand(
	xstrview Verb,
	xstrview Arguments,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
