#ifndef XRT_SSH_PRIVATE_KEY_H
#define XRT_SSH_PRIVATE_KEY_H

#include <xrt/ssh_hostkey.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY) && !defined(XSSH_FEATURE_HOSTKEY)
	#error "XSSH_FEATURE_PRIVATE_KEY requires SSH host-key support"
#endif



#if defined(XSSH_FEATURE_PRIVATE_KEY)

#define XSSH_PRIVATE_KEY_MAGIC "openssh-key-v1"
#define XSSH_PRIVATE_KEY_MAGIC_SIZE 15u
#define XSSH_PRIVATE_KEY_NONE "none"



/* 容器及全部字段借用完整 openssh-key-v1 二进制输入。 */
typedef struct xsshopensshprivatekey {
	xbytesview Blob;
	xstrview Cipher;
	xstrview Kdf;
	xbytesview KdfOptions;
	uint32 KeyCount;
	xbytesview PublicKeys;
	xbytesview PrivateList;
} xsshopensshprivatekey;



/* 公钥游标借用容器中的连续 SSH string 序列。 */
typedef struct xsshprivatekeypublics {
	xsshreader Reader;
	uint32 Remaining;
} xsshprivatekeypublics;



XRT_EXTERN_C_BEGIN



/* 解析二进制 openssh-key-v1 容器，并预验证全部公开公钥 blob。 */
XRT_API xsshcode xrtSshPrivateKeyRead(
	xbytesview Blob,
	xsshopensshprivatekey* pPrivateKey
);



/* 判断容器是否需要外部 cipher/KDF 层先解密 PrivateList。 */
XRT_API xsshcode xrtSshPrivateKeyIsEncrypted(
	const xsshopensshprivatekey* pPrivateKey,
	bool* pEncrypted
);



/* 初始化容器公开公钥游标。 */
XRT_API xsshcode xrtSshPrivateKeyPublicsInit(
	const xsshopensshprivatekey* pPrivateKey,
	xsshprivatekeypublics* pPublics
);



/* 返回下一把公开公钥 blob；遍历完成返回 XSSH_NEED_MORE。 */
XRT_API xsshcode xrtSshPrivateKeyPublicsNext(
	xsshprivatekeypublics* pPublics,
	xbytesview* pPublicKey
);



XRT_EXTERN_C_END

#endif

#endif
