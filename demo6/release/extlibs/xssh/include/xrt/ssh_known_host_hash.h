#ifndef XRT_SSH_KNOWN_HOST_HASH_H
#define XRT_SSH_KNOWN_HOST_HASH_H

#include <xrt/ssh_known_host.h>



#if defined(XSSH_FEATURE_KNOWN_HOST_HASH) && \
	(!defined(XSSH_FEATURE_KNOWN_HOST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA1))
	#error "XSSH_FEATURE_KNOWN_HOST_HASH requires known-host and XRT SHA-1"
#endif



#if defined(XSSH_FEATURE_KNOWN_HOST_HASH)

#define XSSH_KNOWN_HOST_HASH_SIZE 20u



XRT_EXTERN_C_BEGIN



/* 计算 OpenSSH hashed-host 使用的 HMAC-SHA1；Salt 必须为 20 字节。 */
XRT_API xsshcode xrtSshKnownHostHash(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	void* pHash
);



/*
	生成 |1|salt|hash 文本；查询模式允许 sOutput 为 NULL、容量为零。
	实际写入容量必须包含末尾零字节，pOutputSize 不包含末尾零字节。
*/
XRT_API xsshcode xrtSshKnownHostHashWrite(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 常量时间校验一个完整 OpenSSH |1| hashed-host token。 */
XRT_API xsshcode xrtSshKnownHostHashMatch(
	xstrview HashedHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
);



/* 对已解析且 Hashed 的 known_hosts 行执行便利匹配。 */
XRT_API xsshcode xrtSshKnownHostLineHashMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
);



XRT_EXTERN_C_END

#endif

#endif
