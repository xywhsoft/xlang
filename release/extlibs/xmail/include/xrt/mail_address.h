#ifndef XRT_MAIL_ADDRESS_H
#define XRT_MAIL_ADDRESS_H

#include <xrt/mail_word.h>



#if defined(XMAIL_FEATURE_MAIL_ADDRESS) && !defined(XMAIL_FEATURE_MAIL_WORD)
	#error "XMAIL_FEATURE_MAIL_ADDRESS requires XMAIL_FEATURE_MAIL_WORD"
#endif



#if defined(XMAIL_FEATURE_MAIL_ADDRESS)

/* 注释采用迭代扫描，但仍限制恶意输入的嵌套深度。 */
#define XMAIL_ADDRESS_COMMENT_DEPTH 16u



/* 地址列表游标显式报告组边界，避免丢失列表结构。 */
typedef enum xmailaddresskind {
	XMAIL_ADDRESS_MAILBOX = 1,
	XMAIL_ADDRESS_GROUP_BEGIN,
	XMAIL_ADDRESS_GROUP_END
} xmailaddresskind;



/* 默认只接受 ASCII addr-spec；SMTPUTF8 必须由调用方显式开启。 */
typedef enum xmailaddressflag {
	XMAIL_ADDRESS_DEFAULT = 0,
	XMAIL_ADDRESS_SMTPUTF8 = UINT32_C(0x00000001)
} xmailaddressflag;



/* 所有字段均借用原始列表；Name 保留引号、注释或编码词形式。 */
typedef struct xmailaddressview {
	xmailaddresskind Kind;
	xstrview Source;
	xstrview Name;
	xstrview Address;
	xstrview Local;
	xstrview Domain;
} xmailaddressview;



/* 常用地址列表项借用显示名和 addr-spec，供构建接口直接批量写出。 */
typedef struct xmailaddress {
	xstrview Name;
	xstrview Address;
} xmailaddress;



/* 地址列表游标可跨 group 边界增量推进，不持有堆内存。 */
typedef struct xmailaddresscursor {
	xstrview Text;
	size_t Position;
	uint32 Flags;
	bool InGroup;
	bool Done;
} xmailaddresscursor;



XRT_EXTERN_C_BEGIN



/* 初始化严格地址列表游标。 */
XRT_API bool xrtMailAddressCursorInit(
	xmailaddresscursor* pCursor,
	xstrview Text,
	uint32 iFlags
);



/* 返回下一个 mailbox、group begin 或 group end 借用视图。 */
XRT_API xmailnext xrtMailAddressNext(
	xmailaddresscursor* pCursor,
	xmailaddressview* pAddress
);



/* 验证一个完整 addr-spec，可选返回拆分后的 local-part 与 domain。 */
XRT_API bool xrtMailAddressValid(
	xstrview Address,
	uint32 iFlags,
	xstrview* pLocal,
	xstrview* pDomain
);



/*
	写出常用的 display-name <addr-spec>；空显示名只写 addr-spec。
	非 ASCII 显示名使用指定的 RFC 2047 编码，实际容量必须包含末尾零。
*/
XRT_API bool xrtMailAddressWrite(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的规范 mailbox 文本。 */
XRT_API str xrtMailAddress(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
);



/* 写出逗号和空格分隔的规范 mailbox 列表；空数组写出空文本。 */
XRT_API bool xrtMailAddressListWrite(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的规范 mailbox 列表。 */
XRT_API str xrtMailAddressList(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
