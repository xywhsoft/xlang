#ifndef XRT_IMAP_COMMAND_H
#define XRT_IMAP_COMMAND_H

#include <xrt/imap_client.h>



#if defined(XMAIL_FEATURE_IMAP_COMMAND) && \
	!defined(XMAIL_FEATURE_IMAP_CLIENT)
	#error "XMAIL_FEATURE_IMAP_COMMAND requires IMAP client"
#endif



#if defined(XMAIL_FEATURE_IMAP_COMMAND)

#define XIMAP_MAILBOX_EXISTS UINT32_C(0x00000001)
#define XIMAP_MAILBOX_RECENT UINT32_C(0x00000002)
#define XIMAP_MAILBOX_UNSEEN UINT32_C(0x00000004)
#define XIMAP_MAILBOX_UID_VALIDITY UINT32_C(0x00000008)
#define XIMAP_MAILBOX_UID_NEXT UINT32_C(0x00000010)
#define XIMAP_MAILBOX_HIGHEST_MODSEQ UINT32_C(0x00000020)
#define XIMAP_MAILBOX_ACCESS UINT32_C(0x00000040)



/* STORE 模式映射到 FLAGS、+FLAGS、-FLAGS 及其静默形式。 */
typedef enum ximapstoremode {
	XIMAP_STORE_SET = 0,
	XIMAP_STORE_SET_SILENT,
	XIMAP_STORE_ADD,
	XIMAP_STORE_ADD_SILENT,
	XIMAP_STORE_REMOVE,
	XIMAP_STORE_REMOVE_SILENT
} ximapstoremode;



/* SELECT/EXAMINE 摘要不拥有字符串，Present 区分缺失字段和零值。 */
typedef struct ximapmailboxinfo {
	uint64 Exists;
	uint64 Recent;
	uint64 Unseen;
	uint64 UidValidity;
	uint64 UidNext;
	uint64 HighestModSeq;
	uint32 Present;
	bool ReadOnly;
} ximapmailboxinfo;



XRT_EXTERN_C_BEGIN



/* 初始化空邮箱摘要。 */
XRT_API void xrtImapMailboxInfoInit(ximapmailboxinfo* pInfo);



/* 把一条 SELECT、EXAMINE 或未请求更新合并到邮箱摘要。 */
XRT_API xmailnext xrtImapMailboxInfoUpdate(
	const ximapresponseview* pResponse,
	ximapmailboxinfo* pInfo
);



/* 执行 NOOP 并消费命令期间的未请求响应。 */
XRT_API bool xrtImapClientNoop(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 选择可写邮箱，并返回零分配状态摘要。 */
XRT_API bool xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 以只读方式选择邮箱，并返回零分配状态摘要。 */
XRT_API bool xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 对当前选中邮箱执行 CHECK。 */
XRT_API bool xrtImapClientCheck(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 不执行隐式 EXPUNGE 地离开当前邮箱。 */
XRT_API bool xrtImapClientUnselect(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 执行 CLOSE，提交删除标记并离开当前邮箱。 */
XRT_API bool xrtImapClientCloseMailbox(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 创建、删除、重命名、订阅或取消订阅邮箱。 */
XRT_API bool xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始会返回数据的命令；结果继续通过 Next 和 ReadLiteral 流式读取。 */
XRT_API bool xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	xdeadline iDeadline,
	xcancel* pCancel
);

/* 空 UidSet 开始 EXPUNGE；非空集合开始 UID EXPUNGE。 */
XRT_API bool xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	xdeadline iDeadline,
	xcancel* pCancel
);

/* 开始 IDLE；收到 continuation 后可读取事件，结束时发送 DONE。 */
XRT_API bool xrtImapClientBeginIdle(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);

XRT_API bool xrtImapClientEndIdle(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
