#ifndef XRT_MAIL_TREE_H
#define XRT_MAIL_TREE_H

#include <xrt/mail_message.h>
#include <xrt/mail_multipart.h>
#include <xrt/mail_param.h>



#if defined(XMAIL_FEATURE_MAIL_TREE) && \
	(!defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
	 !defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	 !defined(XMAIL_FEATURE_MAIL_PARAM))
	#error "XMAIL_FEATURE_MAIL_TREE requires message, multipart and param"
#endif



#if defined(XMAIL_FEATURE_MAIL_TREE)

/* 零限制使用适合不可信互联网邮件的保守预算。 */
#define XMAIL_TREE_DEPTH_DEFAULT 32u
#define XMAIL_TREE_DEPTH_MAX 128u
#define XMAIL_TREE_PARTS_DEFAULT 4096u
#define XMAIL_TREE_SOURCE_BYTES_DEFAULT (64u * 1024u * 1024u)
#define XMAIL_TREE_DECODED_BYTES_DEFAULT (128u * 1024u * 1024u)



/* 兼容标记必须显式启用，默认解析保持严格。 */
typedef enum xmailtreeflag {
	XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER = UINT32_C(0x00000001),
	XMAIL_TREE_RELAXED_QP = UINT32_C(0x00000002)
} xmailtreeflag;



/* 所有限制作用于整棵树；字段限制作用于每一个 MIME entity。 */
typedef struct xmailtreelimits {
	size_t MaxDepth;
	size_t MaxParts;
	size_t MaxSourceBytes;
	size_t MaxDecodedBytes;
	size_t MaxHeaderBytes;
	size_t MaxHeaders;
	uint32 Flags;
} xmailtreelimits;



typedef struct xmailpart xmailpart;



/*
	MIME part 的全部视图由所属 xmailtree 持有。
	Data 是解码后的叶子正文；未知传输编码获准保留时 Decoded 为 false。
*/
struct xmailpart {
	xmailmessageview Message;
	xmailmediatypeview ContentType;
	xmaildispositionview Disposition;
	xstrview FileName;
	xstrview ContentId;
	xstrview Preamble;
	xstrview Epilogue;
	xbytesview Data;
	xmailpart* Children;
	size_t ChildCount;
	xmailtransfer Transfer;
	bool Attachment;
	bool Inline;
	bool Decoded;
	bool Embedded;
};



/* Source、Root 及其所有后代统一由 Storage 持有。 */
typedef struct xmailtree {
	xstrview Source;
	xmailpart* Root;
	size_t PartCount;
	size_t DecodedBytes;
	ptr Storage;
} xmailtree;



XRT_EXTERN_C_BEGIN



/* 使用默认预算初始化 MIME 树限制。 */
XRT_API void xrtMailTreeLimitsInit(xmailtreelimits* pLimits);



/* 验证 MIME 树限制；零字段按默认预算解释。 */
XRT_API bool xrtMailTreeLimitsValid(const xmailtreelimits* pLimits);



/* 复制并解析完整 RFC 消息；成功结果不再依赖输入缓冲。 */
XRT_API bool xrtMailTreeParse(
	xstrview Source,
	const xmailtreelimits* pLimits,
	xmailtree* pTree
);



/* 释放整棵 MIME 树；允许传入 NULL 或已清零结构。 */
XRT_API void xrtMailTreeFree(xmailtree* pTree);



XRT_EXTERN_C_END

#endif

#endif
