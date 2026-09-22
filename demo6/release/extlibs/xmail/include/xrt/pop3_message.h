#ifndef XRT_POP3_MESSAGE_H
#define XRT_POP3_MESSAGE_H

#include <xrt/buffer.h>
#include <xrt/mail_tree.h>
#include <xrt/pop3_client.h>



#if defined(XMAIL_FEATURE_POP3_MESSAGE) && \
	(!defined(XMAIL_FEATURE_POP3_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_TREE) || \
	 !defined(XRT_FEATURE_BUFFER))
	#error "XMAIL_FEATURE_POP3_MESSAGE requires POP3 client, mail tree and buffer"
#endif



#if defined(XMAIL_FEATURE_POP3_MESSAGE)

#define XPOP3_MESSAGE_BYTES_DEFAULT XMAIL_TREE_SOURCE_BYTES_DEFAULT



XRT_EXTERN_C_BEGIN



/* 流式读取完整邮件并恢复每一行的 CRLF；零预算使用默认上限。 */
XRT_API bool xrtPop3ClientRetrWrite(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 流式读取字段和指定数量正文行，并恢复每一行的 CRLF。 */
XRT_API bool xrtPop3ClientTopWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取完整邮件并返回由 xrtFree 释放、末尾附零的连续字节。 */
XRT_API bytes xrtPop3ClientRetrBytes(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取 TOP 结果并返回由 xrtFree 释放、末尾附零的连续字节。 */
XRT_API bytes xrtPop3ClientTopBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 按 MIME 树预算读取并解析完整邮件；成功结果不依赖网络缓冲。 */
XRT_API bool xrtPop3ClientRetrTree(
	xpop3client* pClient,
	uint64 iMessage,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
