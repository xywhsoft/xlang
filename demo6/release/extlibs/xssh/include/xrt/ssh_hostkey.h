#ifndef XRT_SSH_HOSTKEY_H
#define XRT_SSH_HOSTKEY_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_HOSTKEY) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_HOSTKEY requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_HOSTKEY)

#define XSSH_ED25519_PUBLIC_SIZE 32u
#define XSSH_ED25519_SIGNATURE_SIZE 64u
#define XSSH_HOSTKEY_ED25519 "ssh-ed25519"



/* 公钥视图借用完整 key blob；Parameters 保留算法字段后的原始编码。 */
typedef struct xsshpublickey {
	xstrview Algorithm;
	xbytesview Parameters;
} xsshpublickey;



/* 签名视图借用完整 signature blob。 */
typedef struct xsshsignature {
	xstrview Algorithm;
	xbytesview Signature;
} xsshsignature;



XRT_EXTERN_C_BEGIN



/* 读取公钥算法和其余算法专用字段，不解释 Parameters。 */
XRT_API xsshcode xrtSshPublicKeyRead(
	xbytesview Blob,
	xsshpublickey* pPublicKey
);



/* 严格读取由算法名和签名字节组成的 SSH signature blob。 */
XRT_API xsshcode xrtSshSignatureRead(
	xbytesview Blob,
	xsshsignature* pSignature
);



/* 写入算法无关的 SSH signature blob。 */
XRT_API xsshcode xrtSshSignatureWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview Signature
);



/* 严格读取 ssh-ed25519 公钥并返回 32 字节借用视图。 */
XRT_API xsshcode xrtSshEd25519PublicKeyRead(
	xbytesview Blob,
	xbytesview* pPublicKey
);



/* 写入规范 ssh-ed25519 公钥 blob。 */
XRT_API xsshcode xrtSshEd25519PublicKeyWrite(
	xsshwriter* pWriter,
	xbytesview PublicKey
);



/* 严格读取 ssh-ed25519 签名并返回 64 字节借用视图。 */
XRT_API xsshcode xrtSshEd25519SignatureRead(
	xbytesview Blob,
	xbytesview* pSignature
);



/* 写入规范 ssh-ed25519 signature blob。 */
XRT_API xsshcode xrtSshEd25519SignatureWrite(
	xsshwriter* pWriter,
	xbytesview Signature
);



XRT_EXTERN_C_END

#endif

#endif
