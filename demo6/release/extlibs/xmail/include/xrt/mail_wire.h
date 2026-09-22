#ifndef XRT_MAIL_WIRE_H
#define XRT_MAIL_WIRE_H

#include <xrt/mail.h>



#if defined(XMAIL_FEATURE_MAIL_WIRE)

/* 零行限制使用 64 KiB；SIZE_MAX 明确表示不限制。 */
#define XMAIL_WIRE_LINE_DEFAULT (64u * 1024u)



/* 增量 dot writer 保留跨片段 CRLF 和行首状态，不持有输入。 */
typedef struct xmaildotwriter {
	bool LineStart;
	bool PendingCr;
	bool Finished;
} xmaildotwriter;



XRT_EXTERN_C_BEGIN



/*
	从增量输入读取一条严格 CRLF 行；ITEM 返回不含 CRLF 的借用视图，
	END 表示数据尚不完整，ERROR 表示裸换行或超限。
*/
XRT_API xmailnext xrtMailLineRead(
	xstrview Data,
	size_t iMaxLine,
	xstrview* pLine,
	size_t* pConsumed
);



/* 对一条已去除 CRLF 的 dot-transparent 行去转义；单点终止行返回 END。 */
XRT_API xmailnext xrtMailDotLine(xstrview Line, xstrview* pData);



/* 初始化可接收任意分块边界的增量 dot writer。 */
XRT_API bool xrtMailDotWriterInit(xmaildotwriter* pWriter);



/* 校验严格 CRLF 并向 sink 写出一个 dot-transparent 输入片段。 */
XRT_API bool xrtMailDotWriterWrite(
	xmaildotwriter* pWriter,
	xbytesview Data,
	xmailwriteproc pWrite,
	ptr pUserData
);



/* 补足最后一行并写出 SMTP/POP3 点终止行。 */
XRT_API bool xrtMailDotWriterFinish(
	xmaildotwriter* pWriter,
	xmailwriteproc pWrite,
	ptr pUserData
);



/*
	按 SMTP/POP3 dot transparency 写出数据；Terminate 会补足 CRLF 并追加
	终止行。输入允许最后一行不完整，但其他换行必须是严格 CRLF。
*/
XRT_API bool xrtMailDotWrite(
	xstrview Data,
	bool Terminate,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 dot-transparent 字节。 */
XRT_API bytes xrtMailDot(
	xstrview Data,
	bool Terminate,
	size_t* pOutputSize
);



/* 解码完整的 dot-transparent 行块；可要求最后一行必须是终止行。 */
XRT_API bool xrtMailDotDecodeWrite(
	xstrview Data,
	bool RequireTerminator,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的去转义字节。 */
XRT_API bytes xrtMailDotDecode(
	xstrview Data,
	bool RequireTerminator,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
