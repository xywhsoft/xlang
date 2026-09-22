#ifndef XRT_IMAP_APPEND_H
#define XRT_IMAP_APPEND_H

#include <xrt/imap_client.h>



#if defined(XMAIL_FEATURE_IMAP_APPEND) && \
	!defined(XMAIL_FEATURE_IMAP_CLIENT)
	#error "XMAIL_FEATURE_IMAP_APPEND requires IMAP client"
#endif



#if defined(XMAIL_FEATURE_IMAP_APPEND)

/* 自动模式优先遵守 APPENDLIMIT；显式模式允许调用方选择往返与吞吐。 */
typedef enum ximapliteralmode {
	XIMAP_LITERAL_AUTO = 0,
	XIMAP_LITERAL_SYNC,
	XIMAP_LITERAL_NONSYNC
} ximapliteralmode;



/* Flags 是可选括号列表，InternalDate 是可选未加引号日期文本。 */
typedef struct ximapappendconfig {
	xstrview Mailbox;
	xstrview Flags;
	xstrview InternalDate;
	size_t Size;
	ximapliteralmode Literal;
} ximapappendconfig;



/* UIDPLUS 结果只有在服务器返回合法 APPENDUID 时才标记 Present。 */
typedef struct ximapappendresult {
	uint64 UidValidity;
	uint64 Uid;
	bool Present;
} ximapappendresult;



XRT_EXTERN_C_BEGIN



/* 初始化同步策略为 AUTO，其余字段为空。 */
XRT_API void xrtImapAppendConfigInit(ximapappendconfig* pConfig);



/* 初始化无 APPENDUID 的空结果。 */
XRT_API void xrtImapAppendResultInit(ximapappendresult* pResult);



/*
	发送 APPEND 命令头并在同步 literal 模式下等待 continuation。
	成功后必须精确写入 Size 字节，再调用 AppendEnd。
*/
XRT_API bool xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 返回活动 APPEND literal 尚未写入的字节数。 */
XRT_API size_t xrtImapClientAppendRemaining(const ximapclient* pClient);



/* 零复制发送下一块 literal；超过声明长度时不发送任何字节。 */
XRT_API bool xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 结束 literal，消费命令响应并解析可选 APPENDUID。 */
XRT_API bool xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 对已经完整驻留内存的消息执行一次 APPEND。 */
XRT_API bool xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
