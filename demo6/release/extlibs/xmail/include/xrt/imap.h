#ifndef XRT_IMAP_H
#define XRT_IMAP_H

#include <xrt/mail_wire.h>



#if defined(XMAIL_FEATURE_IMAP) && !defined(XMAIL_FEATURE_MAIL_WIRE)
	#error "XMAIL_FEATURE_IMAP requires XMAIL_FEATURE_MAIL_WIRE"
#endif



#if defined(XMAIL_FEATURE_IMAP)

#define XIMAP_COMMAND_LINE_DEFAULT (64u * 1024u)

#define XIMAP_CAP_IMAP4REV1 UINT64_C(0x00000001)
#define XIMAP_CAP_IMAP4REV2 UINT64_C(0x00000002)
#define XIMAP_CAP_STARTTLS UINT64_C(0x00000004)
#define XIMAP_CAP_AUTH_PLAIN UINT64_C(0x00000008)
#define XIMAP_CAP_AUTH_XOAUTH2 UINT64_C(0x00000010)
#define XIMAP_CAP_IDLE UINT64_C(0x00000020)
#define XIMAP_CAP_UIDPLUS UINT64_C(0x00000040)
#define XIMAP_CAP_MOVE UINT64_C(0x00000080)
#define XIMAP_CAP_NAMESPACE UINT64_C(0x00000100)
#define XIMAP_CAP_ENABLE UINT64_C(0x00000200)
#define XIMAP_CAP_UTF8_ACCEPT UINT64_C(0x00000400)
#define XIMAP_CAP_CONDSTORE UINT64_C(0x00000800)
#define XIMAP_CAP_QRESYNC UINT64_C(0x00001000)
#define XIMAP_CAP_LITERAL_PLUS UINT64_C(0x00002000)
#define XIMAP_CAP_SASL_IR UINT64_C(0x00004000)
#define XIMAP_CAP_BINARY UINT64_C(0x00008000)
#define XIMAP_CAP_LOGIN_DISABLED UINT64_C(0x00010000)
#define XIMAP_CAP_AUTH_OAUTHBEARER UINT64_C(0x00020000)
#define XIMAP_CAP_UNSELECT UINT64_C(0x00040000)
#define XIMAP_CAP_ESEARCH UINT64_C(0x00080000)
#define XIMAP_CAP_LIST_EXTENDED UINT64_C(0x00100000)
#define XIMAP_CAP_SPECIAL_USE UINT64_C(0x00200000)
#define XIMAP_CAP_SORT UINT64_C(0x00400000)
#define XIMAP_CAP_THREAD_REFERENCES UINT64_C(0x00800000)
#define XIMAP_CAP_QUOTA UINT64_C(0x01000000)
#define XIMAP_CAP_ACL UINT64_C(0x02000000)
#define XIMAP_CAP_METADATA UINT64_C(0x04000000)
#define XIMAP_CAP_NOTIFY UINT64_C(0x08000000)
#define XIMAP_CAP_COMPRESS_DEFLATE UINT64_C(0x10000000)
#define XIMAP_CAP_APPENDLIMIT UINT64_C(0x20000000)
#define XIMAP_CAP_LITERAL_MINUS UINT64_C(0x40000000)



typedef enum ximapresponsekind {
	XIMAP_RESPONSE_TAGGED = 1,
	XIMAP_RESPONSE_UNTAGGED,
	XIMAP_RESPONSE_CONTINUATION
} ximapresponsekind;



typedef enum ximapstatus {
	XIMAP_STATUS_NONE = 0,
	XIMAP_STATUS_OK,
	XIMAP_STATUS_NO,
	XIMAP_STATUS_BAD,
	XIMAP_STATUS_PREAUTH,
	XIMAP_STATUS_BYE
} ximapstatus;



/* IMAP 响应视图借用输入；Text 是状态后的文本或完整非状态 untagged 内容。 */
typedef struct ximapresponseview {
	xstrview Source;
	xstrview Tag;
	xstrview Text;
	ximapresponsekind Kind;
	ximapstatus Status;
} ximapresponseview;



/* literal 标记位于行尾，支持同步、LITERAL+ 和 binary literal。 */
typedef struct ximapliteralview {
	xstrview Source;
	size_t Size;
	bool NonSynchronizing;
	bool Binary;
} ximapliteralview;



/* 响应码视图借用状态后的文本；Text 是右方括号后的说明文本。 */
typedef struct ximapcodeview {
	xstrview Source;
	xstrview Name;
	xstrview Arguments;
	xstrview Text;
} ximapcodeview;



/* 数字响应视图借用非标记响应 Text，例如 `23 FETCH (...)`。 */
typedef struct ximapnumberview {
	xstrview Source;
	xstrview Name;
	xstrview Text;
	uint64 Number;
} ximapnumberview;



/* 空白分隔 atom 游标用于 CAPABILITY、SEARCH 等简单响应。 */
typedef struct ximapatomcursor {
	xstrview Text;
	size_t Position;
	bool Done;
} ximapatomcursor;



XRT_EXTERN_C_BEGIN



/* 判断文本是否可以作为 IMAP atom 使用。 */
XRT_API bool xrtImapAtomValid(xstrview Atom);



/* 验证序号集合、UID 集合或 SEARCHRES 的 `$` 引用。 */
XRT_API bool xrtImapSequenceSetValid(xstrview Set);



/* 解析 tagged、untagged 或 continuation 响应行。 */
XRT_API bool xrtImapResponseParse(
	xstrview Line,
	ximapresponseview* pResponse
);



/* 查找行尾 literal 标记；无标记返回 END，合法标记返回 ITEM。 */
XRT_API xmailnext xrtImapLiteralParse(
	xstrview Line,
	ximapliteralview* pLiteral
);



/* 解析文本开头的 `[code arguments]`；没有响应码返回 END。 */
XRT_API xmailnext xrtImapCodeParse(
	xstrview Text,
	ximapcodeview* pCode
);



/* 解析 `number name [text]`；不是数字响应返回 END。 */
XRT_API xmailnext xrtImapNumberParse(
	xstrview Text,
	ximapnumberview* pNumber
);



/* 初始化空白分隔 atom 游标。 */
XRT_API bool xrtImapAtomCursorInit(
	ximapatomcursor* pCursor,
	xstrview Text
);



/* 返回下一 atom；控制字符或 IMAP atom-specials 返回错误。 */
XRT_API xmailnext xrtImapAtomNext(
	ximapatomcursor* pCursor,
	xstrview* pAtom
);



/* 返回已知 IMAP capability 的稳定标记，未知扩展返回零。 */
XRT_API uint64 xrtImapCapability(xstrview Capability);



/* 写出转义后的 IMAP quoted string，容量包含末尾零字节。 */
XRT_API bool xrtImapQuoteWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 IMAP quoted string。 */
XRT_API str xrtImapQuote(xstrview Text, size_t* pOutputSize);



/* 安全写出 `Tag Command [Arguments]\r\n`。 */
XRT_API bool xrtImapCommandWrite(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 IMAP 命令行。 */
XRT_API str xrtImapCommand(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
