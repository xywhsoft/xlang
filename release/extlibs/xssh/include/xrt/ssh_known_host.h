#ifndef XRT_SSH_KNOWN_HOST_H
#define XRT_SSH_KNOWN_HOST_H

#include <xrt/ssh_key_text.h>



#if defined(XSSH_FEATURE_KNOWN_HOST) && \
	!defined(XSSH_FEATURE_KEY_TEXT)
	#error "XSSH_FEATURE_KNOWN_HOST requires XSSH_FEATURE_KEY_TEXT"
#endif



#if defined(XSSH_FEATURE_KNOWN_HOST)

#define XSSH_DEFAULT_PORT 22u
#define XSSH_KNOWN_HOST_CERT_AUTHORITY "@cert-authority"
#define XSSH_KNOWN_HOST_REVOKED "@revoked"



/* 未知 marker 被保留，信任策略可以显式拒绝或扩展。 */
typedef enum xsshknownhostmarker {
	XSSH_KNOWN_HOST_MARKER_NONE = 0,
	XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY = 1,
	XSSH_KNOWN_HOST_MARKER_REVOKED = 2,
	XSSH_KNOWN_HOST_MARKER_UNKNOWN = 3
} xsshknownhostmarker;



/* 否定 pattern 优先于同一列表中的任意正向匹配。 */
typedef enum xsshknownhostmatch {
	XSSH_KNOWN_HOST_NO_MATCH = 0,
	XSSH_KNOWN_HOST_MATCH = 1,
	XSSH_KNOWN_HOST_NEGATED = 2
} xsshknownhostmatch;



/* known_hosts 行视图借用原始文本，密钥 blob 由调用方按 BlobSize 解码。 */
typedef struct xsshknownhostline {
	xstrview Marker;
	xsshknownhostmarker MarkerKind;
	xstrview Hosts;
	xstrview Algorithm;
	xstrview Base64;
	xstrview Comment;
	size_t BlobSize;
	bool Hashed;
} xsshknownhostline;



XRT_EXTERN_C_BEGIN



/* 解析 marker、host patterns、算法、Base64 和注释，不执行信任决策。 */
XRT_API xsshcode xrtSshKnownHostLineRead(
	xstrview Line,
	xsshknownhostline* pKnownHost
);



/* 解码并验证 known_hosts 行中的算法无关公钥 blob。 */
XRT_API xsshcode xrtSshKnownHostLineDecode(
	const xsshknownhostline* pKnownHost,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
);



/* 不分配解码缓冲，直接比较 known_hosts 行与完整原始公钥 blob。 */
XRT_API xsshcode xrtSshKnownHostLineKeyMatch(
	const xsshknownhostline* pKnownHost,
	xbytesview Blob,
	bool* pMatch
);



/*
	按 OpenSSH 规则匹配明文 host pattern 列表。
	Host 传未加方括号的主机名或地址；非 22 端口按 [host]:port 匹配。
*/
XRT_API xsshcode xrtSshKnownHostPatternsMatch(
	xstrview Patterns,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
);



/* 对已解析行执行明文 host pattern 匹配；hashed 行返回不支持。 */
XRT_API xsshcode xrtSshKnownHostLineMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
);



XRT_EXTERN_C_END

#endif

#endif
