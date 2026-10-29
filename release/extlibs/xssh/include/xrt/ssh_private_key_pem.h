#ifndef XRT_SSH_PRIVATE_KEY_PEM_H
#define XRT_SSH_PRIVATE_KEY_PEM_H

#include <xrt/ssh_private_key.h>
#include <xrt/pem.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM) && \
	(!defined(XSSH_FEATURE_PRIVATE_KEY) || !defined(XRT_FEATURE_PEM))
	#error "XSSH_FEATURE_PRIVATE_KEY_PEM requires SSH private-key and XRT PEM"
#endif



#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM)

#define XSSH_PRIVATE_KEY_PEM_LABEL "OPENSSH PRIVATE KEY"



XRT_EXTERN_C_BEGIN



/*
	查找并解码 OpenSSH 私钥 PEM 到调用方缓冲，再解析二进制容器。
	查询模式要求 pBinary 和 pPrivateKey 为 NULL、容量为零，只返回精确二进制长度。
*/
XRT_API xsshcode xrtSshPrivateKeyPemRead(
	xstrview Text,
	void* pBinary,
	size_t iCapacity,
	size_t* pBinarySize,
	xsshopensshprivatekey* pPrivateKey
);



XRT_EXTERN_C_END

#endif

#endif
