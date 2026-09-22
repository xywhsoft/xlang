#ifndef XRT_SSH_KEXINIT_H
#define XRT_SSH_KEXINIT_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_KEXINIT) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_KEXINIT requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_KEXINIT)

#define XSSH_MSG_KEXINIT 20u
#define XSSH_KEX_COOKIE_SIZE 16u

#define XSSH_KEX_DEFAULT \
	"curve25519-sha256,curve25519-sha256@libssh.org"
#define XSSH_KEX_EXT_INFO_CLIENT "ext-info-c"
#define XSSH_KEX_EXT_INFO_SERVER "ext-info-s"
#define XSSH_KEX_STRICT_CLIENT "kex-strict-c"
#define XSSH_KEX_STRICT_SERVER "kex-strict-s"
#define XSSH_KEX_STRICT_CLIENT_PRE_STANDARD \
	"kex-strict-c-v00@openssh.com"
#define XSSH_KEX_STRICT_SERVER_PRE_STANDARD \
	"kex-strict-s-v00@openssh.com"
#define XSSH_KEX_CLIENT_INITIAL_DEFAULT \
	XSSH_KEX_DEFAULT "," XSSH_KEX_EXT_INFO_CLIENT "," \
	XSSH_KEX_STRICT_CLIENT "," XSSH_KEX_STRICT_CLIENT_PRE_STANDARD
#define XSSH_KEX_SERVER_INITIAL_DEFAULT \
	XSSH_KEX_DEFAULT "," XSSH_KEX_EXT_INFO_SERVER "," \
	XSSH_KEX_STRICT_SERVER "," XSSH_KEX_STRICT_SERVER_PRE_STANDARD
#define XSSH_HOSTKEY_DEFAULT "ssh-ed25519"
#define XSSH_CIPHER_DEFAULT \
	"aes128-gcm@openssh.com,aes256-gcm@openssh.com"
#define XSSH_MAC_DEFAULT "hmac-sha2-256,hmac-sha2-512"
#define XSSH_COMPRESSION_DEFAULT "none"



/* Endpoint 角色在整个 SSH 会话及其重协商期间保持不变。 */
typedef enum xsshrole {
	XSSH_ROLE_CLIENT = 0,
	XSSH_ROLE_SERVER = 1
} xsshrole;



/* KEXINIT 构建参数；Data 为 NULL 的列表使用 xssh 默认值。 */
typedef struct xsshkexinitconfig {
	xsshrole Role;
	bool Initial;
	xstrview KexAlgorithms;
	xstrview ServerHostKeyAlgorithms;
	xstrview EncryptionClientToServer;
	xstrview EncryptionServerToClient;
	xstrview MacClientToServer;
	xstrview MacServerToClient;
	xstrview CompressionClientToServer;
	xstrview CompressionServerToClient;
	xstrview LanguagesClientToServer;
	xstrview LanguagesServerToClient;
	bool FirstKexPacketFollows;
} xsshkexinitconfig;



/* KEXINIT 借用输入 payload；Cookie 和所有列表都不复制。 */
typedef struct xsshkexinit {
	xbytesview Cookie;
	xstrview KexAlgorithms;
	xstrview ServerHostKeyAlgorithms;
	xstrview EncryptionClientToServer;
	xstrview EncryptionServerToClient;
	xstrview MacClientToServer;
	xstrview MacServerToClient;
	xstrview CompressionClientToServer;
	xstrview CompressionServerToClient;
	xstrview LanguagesClientToServer;
	xstrview LanguagesServerToClient;
	bool FirstKexPacketFollows;
} xsshkexinit;



/* 协商结果中的视图借用 client KEXINIT payload。 */
typedef struct xsshkexnegotiation {
	xstrview KexAlgorithm;
	xstrview ServerHostKeyAlgorithm;
	xstrview CipherClientToServer;
	xstrview CipherServerToClient;
	xstrview MacClientToServer;
	xstrview MacServerToClient;
	xstrview CompressionClientToServer;
	xstrview CompressionServerToClient;
} xsshkexnegotiation;



/* 首次 KEX 的扩展方向与 strict-kex 协商结果。 */
typedef struct xsshkexfeatures {
	bool AcceptExtInfo;
	bool SendExtInfo;
	bool Strict;
} xsshkexfeatures;



XRT_EXTERN_C_BEGIN



/* 按 endpoint 角色和首次/重协商阶段初始化安全默认清单。 */
XRT_API bool xrtSshKexInitConfigInit(
	xsshkexinitconfig* pConfig,
	xsshrole Role,
	bool bInitial
);



/* 使用调用方提供的十六字节 cookie 构建完整 KEXINIT payload。 */
XRT_API xsshcode xrtSshKexInitWrite(
	xsshwriter* pWriter,
	xbytesview Cookie,
	const xsshkexinitconfig* pConfig
);



/* 解析一个完整 KEXINIT payload，并拒绝保留字段或尾随数据。 */
XRT_API xsshcode xrtSshKexInitRead(
	xbytesview Payload,
	xsshkexinit* pKexInit
);



/* 按 RFC 4253 客户端优先级协商双向算法。 */
XRT_API xsshcode xrtSshKexNegotiate(
	const xsshkexinit* pClient,
	const xsshkexinit* pServer,
	xsshkexnegotiation* pNegotiation
);



/* 根据双方 KEXINIT 判定 EXT_INFO 方向和 strict-kex 是否启用。 */
XRT_API xsshcode xrtSshKexFeatures(
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	xsshrole Role,
	bool bInitial,
	xsshkexfeatures* pFeatures
);



/* 判断已知 cipher 是否自带认证而不消费协商出的 MAC。 */
XRT_API bool xrtSshCipherIsAead(xstrview Cipher);



/* 判断 peer 的 first_kex_packet_follows 猜测包是否必须丢弃。 */
XRT_API xsshcode xrtSshKexGuessSkip(
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	bool* pSkip
);



XRT_EXTERN_C_END

#endif

#endif
