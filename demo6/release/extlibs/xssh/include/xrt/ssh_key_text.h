#ifndef XRT_SSH_KEY_TEXT_H
#define XRT_SSH_KEY_TEXT_H

#include <xrt/ssh_hostkey.h>



#if defined(XSSH_FEATURE_KEY_TEXT) && \
	(!defined(XSSH_FEATURE_HOSTKEY) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XSSH_FEATURE_KEY_TEXT requires SSH host-key and XRT Base64 support"
#endif



#if defined(XSSH_FEATURE_KEY_TEXT)

/* OpenSSH 公钥文本行视图借用原始行，BlobSize 给出解码缓冲需求。 */
typedef struct xsshopensshkeyline {
	xstrview Options;
	xstrview Algorithm;
	xstrview Base64;
	xstrview Comment;
	size_t BlobSize;
} xsshopensshkeyline;



XRT_EXTERN_C_BEGIN



/*
	解析 OpenSSH public-key/authorized_keys 行。
	识别带引号与转义的 options，但不解释具体 option；允许任意 SSH 算法名。
*/
XRT_API xsshcode xrtSshPublicKeyLineRead(
	xstrview Line,
	xsshopensshkeyline* pKeyLine
);



/*
	把文本行的 Base64 解码到调用方缓冲，并校验 blob 内算法与文本算法一致。
	协议校验失败时 pPublicKey 不变；pBlob 是解码工作区，成功解码后可能已写入。
*/
XRT_API xsshcode xrtSshPublicKeyLineDecode(
	const xsshopensshkeyline* pKeyLine,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
);



/* 不分配解码缓冲，直接比较文本行与完整原始公钥 blob。 */
XRT_API xsshcode xrtSshPublicKeyLineMatch(
	const xsshopensshkeyline* pKeyLine,
	xbytesview Blob,
	bool* pMatch
);



XRT_EXTERN_C_END

#endif

#endif
