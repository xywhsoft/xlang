#ifndef XRT_SSH_KNOWN_HOST_DB_H
#define XRT_SSH_KNOWN_HOST_DB_H

#include <xrt/ssh_known_host_hash.h>



#if defined(XSSH_FEATURE_KNOWN_HOST_DB) && \
	!defined(XSSH_FEATURE_KNOWN_HOST_HASH)
	#error "XSSH_FEATURE_KNOWN_HOST_DB requires SSH known-host hash support"
#endif



#if defined(XSSH_FEATURE_KNOWN_HOST_DB)

/* 严格模式把坏行和未知 marker 作为可定位的 INVALID 结果。 */
typedef enum xsshknownhostdbflag {
	XSSH_KNOWN_HOST_DB_STRICT = UINT32_C(0x00000001)
} xsshknownhostdbflag;



/* CA 表示存在候选 CA，仍需上层验证主机证书，不能直接视为已信任。 */
typedef enum xsshknownhosttrust {
	XSSH_KNOWN_HOST_TRUST_NEW = 0,
	XSSH_KNOWN_HOST_TRUST_MATCH = 1,
	XSSH_KNOWN_HOST_TRUST_CHANGED = 2,
	XSSH_KNOWN_HOST_TRUST_REVOKED = 3,
	XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY = 4,
	XSSH_KNOWN_HOST_TRUST_INVALID = 5
} xsshknownhosttrust;



/* 数据库游标借用完整文本；Position 和 LineNumber 只由迭代函数推进。 */
typedef struct xsshknownhostdb {
	xstrview Source;
	size_t Position;
	size_t LineNumber;
	uint32 Flags;
} xsshknownhostdb;



/* 条目及其字段都借用数据库文本；Valid 为 false 时仅 Source 和行号有效。 */
typedef struct xsshknownhostentry {
	xstrview Source;
	size_t LineNumber;
	bool Valid;
	xsshknownhostline KnownHost;
} xsshknownhostentry;



/* 常见信任判定返回决定性条目；NEW 时 Entry 清零。 */
typedef struct xsshknownhostcheck {
	xsshknownhosttrust Trust;
	xsshknownhostentry Entry;
} xsshknownhostcheck;



XRT_EXTERN_C_BEGIN



/* 初始化无分配 known_hosts 文本游标；空文本允许 Data 为 NULL。 */
XRT_API xsshcode xrtSshKnownHostDbInit(
	xsshknownhostdb* pDatabase,
	xstrview Source,
	uint32 iFlags
);



/*
	返回下一条记录；注释和空行总被跳过，非严格模式也跳过坏行。
	到达文本末尾返回 XSSH_NEED_MORE，并保持 pEntry 不变。
*/
XRT_API xsshcode xrtSshKnownHostDbNext(
	xsshknownhostdb* pDatabase,
	xsshknownhostentry* pEntry
);



/*
	用完整原始 host-key blob 扫描明文和 |1| 哈希记录，不分配密钥缓冲。
	REVOKED 优先于 MATCH，随后依次为 CA、CHANGED 和 NEW。
*/
XRT_API xsshcode xrtSshKnownHostDbCheck(
	xstrview Source,
	xstrview Host,
	uint32 iPort,
	xbytesview KeyBlob,
	uint32 iFlags,
	xsshknownhostcheck* pCheck
);



XRT_EXTERN_C_END

#endif

#endif
