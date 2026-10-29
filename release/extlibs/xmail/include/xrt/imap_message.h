#ifndef XRT_IMAP_MESSAGE_H
#define XRT_IMAP_MESSAGE_H

#include <xrt/buffer.h>
#include <xrt/imap_command.h>
#include <xrt/imap_data.h>
#include <xrt/mail_tree.h>



#if defined(XMAIL_FEATURE_IMAP_MESSAGE) && \
	(!defined(XMAIL_FEATURE_IMAP_COMMAND) || \
	 !defined(XMAIL_FEATURE_IMAP_DATA) || \
	 !defined(XMAIL_FEATURE_MAIL_TREE) || \
	 !defined(XRT_FEATURE_BUFFER))
	#error "XMAIL_FEATURE_IMAP_MESSAGE requires IMAP command/data, mail tree and buffer"
#endif



#if defined(XMAIL_FEATURE_IMAP_MESSAGE)

#define XIMAP_MESSAGE_BYTES_DEFAULT XMAIL_TREE_SOURCE_BYTES_DEFAULT



XRT_EXTERN_C_BEGIN



/* 流式读取一封消息的 BODY section；空 Section 表示完整 RFC 消息。 */
XRT_API bool xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取 BODY section 并返回由 xrtFree 释放、末尾附零的连续字节。 */
XRT_API bytes xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取完整 BODY[] 并按 MIME 树预算解析；成功结果不依赖网络缓冲。 */
XRT_API bool xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
