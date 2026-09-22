#ifndef XRT_SSH_SESSION_READER_H
#define XRT_SSH_SESSION_READER_H

#include <xrt/ssh_session_tcp.h>



#if defined(XSSH_FEATURE_SESSION_READER) && \
	!defined(XSSH_FEATURE_SESSION_TCP)
	#error "XSSH_FEATURE_SESSION_READER requires the SSH TCP session"
#endif



#if defined(XSSH_FEATURE_SESSION_READER)

/* Reader 只允许一个动态 packet 读取事务。 */
typedef enum xsshsessionreaderstate {
	XSSH_SESSION_READER_IDLE = 0,
	XSSH_SESSION_READER_HOST_KEY = 1,
	XSSH_SESSION_READER_RETRY = 2,
	XSSH_SESSION_READER_READY = 3,
	XSSH_SESSION_READER_INVALID = 4
} xsshsessionreaderstate;



/*
	Reader 借用一个 TCP 会话，拥有按实际报文申请的明文工作区和稳定主机公钥。
	它不拥有 Stream、输入、会话、缓冲池、等待、任务或 channel 数据。
*/
typedef struct xsshsessionreader {
	xnetbuf Plain;
	xnetbuf HostKey;
	xsshsessiontcp* Session;
	xnetbuf* Input;
	xnetwspan PlainSpan;
	xnetwspan HostKeySpan;
	xsshpacketneed Need;
	size_t HostKeyOldSize;
	size_t HostKeySize;
	xsshsessionreaderstate State;
	uint32 Guard;
} xsshsessionreader;



XRT_EXTERN_C_BEGIN



/* 使用与 TCP 会话相同的 Worker 缓冲池初始化空动态读取器。 */
XRT_API bool xrtSshSessionReaderInit(
	xsshsessionreader* pReader,
	xnetbufpool* pPool,
	xsshsessiontcp* pSession
);



/* 中止未决读取并释放动态块；不会清理借用的 TCP 会话。 */
XRT_API void xrtSshSessionReaderClear(xsshsessionreader* pReader);



/* 返回读取器绑定的 TCP 会话；无效对象返回空。 */
XRT_API xsshsessiontcp* xrtSshSessionReaderSession(
	xsshsessionreader* pReader
);



/* 返回只读 TCP 会话；无效对象返回空。 */
XRT_API const xsshsessiontcp* xrtSshSessionReaderSessionConst(
	const xsshsessionreader* pReader
);



/* 返回空闲、空间重试或已准备状态；无效对象返回独立 INVALID。 */
XRT_API xsshsessionreaderstate xrtSshSessionReaderState(
	const xsshsessionreader* pReader
);



/*
	按 transport 探测结果解析下一 packet；明文包零分配，加密包按需申请工作区。
	客户端 ECDH_REPLY 会在同一未消费输入上自动按精确长度扩容并重试主机公钥复制。
*/
XRT_API xsshcode xrtSshSessionReaderPrepare(
	xsshsessionreader* pReader,
	xnetbuf* pInput,
	uint64 iNowMs,
	xsshsessiontcppacket* pPacket
);



/* 提交已接受 packet，释放临时明文并发布本轮主机公钥存储。 */
XRT_API xsshcode xrtSshSessionReaderCommit(
	xsshsessionreader* pReader,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 拒绝当前 packet、消费其线路前缀并终止绑定会话。 */
XRT_API xsshcode xrtSshSessionReaderAbort(xsshsessionreader* pReader);



/* 返回当前或最近一轮 KEX 已验签主机公钥的稳定借用视图。 */
XRT_API xbytesview xrtSshSessionReaderHostKey(
	const xsshsessionreader* pReader
);



XRT_EXTERN_C_END

#endif

#endif
