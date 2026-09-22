#ifndef XRT_MAIL_HEADER_H
#define XRT_MAIL_HEADER_H

#include <xrt/mail.h>



#if defined(XMAIL_FEATURE_MAIL_HEADER)

/* 字段名称和值均借用原始报文，不要求零结尾。 */
typedef struct xmailheaderview {
	xstrview Name;
	xstrview Value;
} xmailheaderview;



/* 字段游标只保存原报文视图和下一字段位置。 */
typedef struct xmailheadercursor {
	xstrview Block;
	size_t Position;
	bool Done;
} xmailheadercursor;



XRT_EXTERN_C_BEGIN



/* 判断字段名是否只包含 RFC 5322 ftext 字节。 */
XRT_API bool xrtMailHeaderNameValid(xstrview Name);



/* 判断字段值是否只包含安全正文和合法 CRLF 折叠。 */
XRT_API bool xrtMailHeaderValueValid(xstrview Value);



/* 初始化零分配字段游标；Block 可以包含或省略末尾空行。 */
XRT_API bool xrtMailHeaderCursorInit(
	xmailheadercursor* pCursor,
	xstrview Block
);



/* 返回下一个字段的借用名称和原始折叠值。 */
XRT_API xmailnext xrtMailHeaderNext(
	xmailheadercursor* pCursor,
	xmailheaderview* pHeader
);



/* 展开一个字段值中的 CRLF + WSP，输出容量必须包含末尾零字节。 */
XRT_API bool xrtMailHeaderUnfoldWrite(
	xstrview Value,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的展开字段值。 */
XRT_API str xrtMailHeaderUnfold(xstrview Value, size_t* pOutputSize);



/*
	写出 `Name: Value\r\n` 并尽量在空白处折叠。
	零行宽使用 78，任何物理行都不会超过 998 字节。
*/
XRT_API bool xrtMailHeaderWrite(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的完整字段行。 */
XRT_API str xrtMailHeader(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
