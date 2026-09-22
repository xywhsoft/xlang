#ifndef XRT_MAIL_MULTIPART_H
#define XRT_MAIL_MULTIPART_H

#include <xrt/mail.h>
#include <xrt/mail_header.h>



#if defined(XMAIL_FEATURE_MAIL_MULTIPART) && !defined(XMAIL_FEATURE_MAIL_HEADER)
	#error "XMAIL_FEATURE_MAIL_MULTIPART requires XMAIL_FEATURE_MAIL_HEADER"
#endif



#if defined(XMAIL_FEATURE_MAIL_MULTIPART)

/* 零值使用库默认限制；SIZE_MAX 明确表示不限制 part 数量。 */
#define XMAIL_MULTIPART_PARTS_DEFAULT 1024u



/* multipart 构建标记直接对应第一段、后续段和关闭分隔线。 */
typedef enum xmailmultipartmark {
	XMAIL_MULTIPART_FIRST = 1,
	XMAIL_MULTIPART_NEXT,
	XMAIL_MULTIPART_CLOSE
} xmailmultipartmark;



/* part 视图借用原始正文，不复制字段或正文。 */
typedef struct xmailmultipartview {
	xstrview Source;
	xstrview Headers;
	xstrview Body;
} xmailmultipartview;



/* multipart 游标公开 preamble/epilogue，便于底层调用方保留完整语义。 */
typedef struct xmailmultipartcursor {
	xstrview Source;
	xstrview Boundary;
	xstrview Preamble;
	xstrview Epilogue;
	size_t Position;
	size_t Parts;
	size_t MaxParts;
	bool Closed;
	bool Done;
} xmailmultipartcursor;



XRT_EXTERN_C_BEGIN



/* 初始化严格 CRLF、严格行首 boundary 的零分配 multipart 游标。 */
XRT_API bool xrtMailMultipartCursorInit(
	xmailmultipartcursor* pCursor,
	xstrview Body,
	xstrview Boundary,
	size_t iMaxParts
);



/* 返回下一 part 的字段块和正文；关闭分隔线缺失时返回 ERROR。 */
XRT_API xmailnext xrtMailMultipartNext(
	xmailmultipartcursor* pCursor,
	xmailmultipartview* pPart
);



/* 写入可直接与字段和正文拼接发送的 multipart 分隔片段。 */
XRT_API bool xrtMailMultipartMarkWrite(
	xstrview Boundary,
	xmailmultipartmark Mark,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
