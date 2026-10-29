#ifndef XRT_SSH_SESSION_STREAM_H
#define XRT_SSH_SESSION_STREAM_H

#include <xrt/ssh_session_reader.h>



#if defined(XSSH_FEATURE_SESSION_STREAM) && \
	!defined(XSSH_FEATURE_SESSION_READER)
	#error "XSSH_FEATURE_SESSION_STREAM requires the SSH session reader"
#endif



#if defined(XSSH_FEATURE_SESSION_STREAM)

typedef struct xsshsessionstream xsshsessionstream;



/* Stream 驱动只保留一个未决读事务；RETRY 表示内存恢复后可重试同一输入。 */
typedef enum xsshsessionstreamstate {
	XSSH_SESSION_STREAM_CREATED = 0,
	XSSH_SESSION_STREAM_OPEN = 1,
	XSSH_SESSION_STREAM_HOLD_IDENTIFICATION = 2,
	XSSH_SESSION_STREAM_HOLD_PACKET = 3,
	XSSH_SESSION_STREAM_RETRY = 4,
	XSSH_SESSION_STREAM_CLOSING = 5,
	XSSH_SESSION_STREAM_CLOSED = 6,
	XSSH_SESSION_STREAM_INVALID = 7
} xsshsessionstreamstate;



/* 接受立即提交，保留暂停读取并等待显式决定，中止会关闭 SSH 与 TCP。 */
typedef enum xsshsessionstreamdecision {
	XSSH_SESSION_STREAM_ACCEPT = 0,
	XSSH_SESSION_STREAM_HOLD = 1,
	XSSH_SESSION_STREAM_ABORT = 2
} xsshsessionstreamdecision;



/*
	全部回调都在 Stream 所属 Worker 串行执行，借用值只在对应事务提交或中止前有效。
	Action 负责协议选择，Identification 与 Packet 负责线路输入的提交决定。
*/
typedef struct xsshsessionstreamevents {
	void (*Open)(xsshsessionstream* pSession, ptr pData);
	void (*Action)(
		xsshsessionstream* pSession,
		xsshsessionaction Action,
		ptr pData
	);
	xsshsessionstreamdecision (*Identification)(
		xsshsessionstream* pSession,
		xstrview Version,
		ptr pData
	);
	xsshsessionstreamdecision (*Packet)(
		xsshsessionstream* pSession,
		const xsshsessiontcppacket* pPacket,
		ptr pData
	);
	void (*Rekey)(
		xsshsessionstream* pSession,
		xsshrekeydecision Decision,
		ptr pData
	);
	void (*Error)(
		xsshsessionstream* pSession,
		xsshcode Code,
		const xerror* pError,
		ptr pData
	);
	void (*End)(xsshsessionstream* pSession, ptr pData);
	void (*HighWater)(
		xsshsessionstream* pSession,
		size_t iQueued,
		ptr pData
	);
	void (*LowWater)(
		xsshsessionstream* pSession,
		size_t iQueued,
		ptr pData
	);
	void (*Drain)(xsshsessionstream* pSession, ptr pData);
	void (*Close)(
		xsshsessionstream* pSession,
		xnetresult Result,
		const xerror* pError,
		ptr pData
	);
} xsshsessionstreamevents;



/*
	驱动拥有 SSH TCP 会话与动态 Reader，借用 Stream、Worker 缓冲池、配置外部对象和用户数据。
	Packet 与 Version 仅保存 HOLD 事务的借用结果，不引入固定报文缓冲。
*/
struct xsshsessionstream {
	xsshsessiontcp Session;
	xsshsessionreader Reader;
	xsshsessiontcpconfig Config;
	xsshsessionstreamevents Events;
	xsshsessiontcppacket Packet;
	xnetstream* Stream;
	xnetbuf* Input;
	ptr UserData;
	xstrview Version;
	xsshsessionaction NotifiedAction;
	xsshsessionstreamstate State;
	bool SessionReady;
	bool Driving;
	bool DriveAgain;
	bool Paused;
	bool WritePaused;
	bool ReadEnded;
	uint32 Guard;
};



XRT_EXTERN_C_BEGIN



/* 复制会话配置和回调；此时不创建 Engine、Stream、会话动态块或后台任务。 */
XRT_API bool xrtSshSessionStreamInit(
	xsshsessionstream* pSession,
	const xsshsessiontcpconfig* pConfig,
	const xsshsessionstreamevents* pEvents,
	ptr pData
);



/* 只清理尚未附着或已经关闭的驱动；活动连接必须先中止并等待 Close。 */
XRT_API bool xrtSshSessionStreamClear(xsshsessionstream* pSession);



/*
	返回供 xrtNetStreamConnect 直接使用的稳定事件表，Stream data 必须是已初始化驱动。
	服务端可在 Accept 回调中改用 xrtSshSessionStreamAttach。
*/
XRT_API const xnetstreamevents* xrtSshSessionStreamNetEvents(void);



/* 在 Stream Worker 上接管事件；已打开 Stream 必须尚未积压输入。 */
XRT_API bool xrtSshSessionStreamAttach(
	xsshsessionstream* pSession,
	xnetstream* pStream
);



/* 返回驱动状态；无效对象返回独立 INVALID。 */
XRT_API xsshsessionstreamstate xrtSshSessionStreamState(
	const xsshsessionstream* pSession
);



/* 返回已经附着且尚未关闭的借用 Stream；连接建立前也可能非空。 */
XRT_API xnetstream* xrtSshSessionStreamTcp(xsshsessionstream* pSession);



/* 返回可直接驱动 KEX、认证和 connection 的底层 SSH TCP 会话。 */
XRT_API xsshsessiontcp* xrtSshSessionStreamSession(
	xsshsessionstream* pSession
);



/* 返回按需明文和主机公钥读取器，保留完整低层事务能力。 */
XRT_API xsshsessionreader* xrtSshSessionStreamReader(
	xsshsessionstream* pSession
);



/* 返回 HOLD 的 peer identification；其他状态返回空视图。 */
XRT_API xstrview xrtSshSessionStreamVersion(
	const xsshsessionstream* pSession
);



/* 返回 HOLD 的已认证 packet；其他状态返回空指针。 */
XRT_API const xsshsessiontcppacket* xrtSshSessionStreamPacket(
	const xsshsessionstream* pSession
);



/*
	在所属 Worker 上推进 Action、待提交输出和当前输入。
	NEED_MORE 与 SPACE 都保留连接；SPACE 进入 RETRY，释放内存后可再次调用。
*/
XRT_API xsshcode xrtSshSessionStreamDrive(xsshsessionstream* pSession);



/* 提交 HOLD 的 identification 或 packet，恢复读取并继续推进。 */
XRT_API xsshcode xrtSshSessionStreamAccept(xsshsessionstream* pSession);



/* 拒绝 HOLD 输入并异常关闭；返回底层读事务的中止结果。 */
XRT_API xsshcode xrtSshSessionStreamReject(xsshsessionstream* pSession);



/* 在所属 Worker 上回滚未进入 TCP 队列的输出、终止未决读取并请求异常关闭。 */
XRT_API bool xrtSshSessionStreamAbort(xsshsessionstream* pSession);



XRT_EXTERN_C_END

#endif

#endif
