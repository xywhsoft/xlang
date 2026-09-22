#ifndef XRT_SSH_PACKET_AES_GCM_H
#define XRT_SSH_PACKET_AES_GCM_H

#include <xrt/ssh_packet.h>
#include <xrt/crypto.h>



#if defined(XSSH_FEATURE_PACKET_AES_GCM) && \
	(!defined(XSSH_FEATURE_PACKET) || !defined(XRT_FEATURE_CRYPTO_AES_GCM))
	#error "XSSH_FEATURE_PACKET_AES_GCM requires packet and crypto_aes_gcm"
#endif



#if defined(XSSH_FEATURE_PACKET_AES_GCM)

#define XSSH_AES_GCM_BLOCK_SIZE 16u
#define XSSH_AES_GCM_IV_SIZE 12u
#define XSSH_AES_GCM_TAG_SIZE 16u
#define XSSH_AES_GCM_FIXED_IV_SIZE 4u



/* 每个方向独立持有一个状态；同一状态不得被多个线程并发推进。 */
typedef struct xsshaesgcm {
	xaesgcm Cipher;
	uint8 FixedIV[XSSH_AES_GCM_FIXED_IV_SIZE];
	uint64 Invocation;
	uint32 Guard;
} xsshaesgcm;



XRT_EXTERN_C_BEGIN



/* 计算 AES-GCM packet 的 padding 和 packet_length。 */
XRT_API xsshcode xrtSshAesGcmMeasure(
	size_t iPayloadSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
);



/* 用 AES-128/256 密钥和十二字节 initial IV 初始化单向 packet 状态。 */
XRT_API xsshcode xrtSshAesGcmInit(
	xsshaesgcm* pState,
	xbytesview Key,
	xbytesview InitialIV
);



/* 清除 AES-GCM 密钥、固定 IV 和 invocation counter。 */
XRT_API void xrtSshAesGcmClear(xsshaesgcm* pState);



/* 读取下一次 packet 使用的 invocation counter。 */
XRT_API xsshcode xrtSshAesGcmInvocation(
	const xsshaesgcm* pState,
	uint64* pInvocation
);



/* 原位构建并加密一个 AES-GCM packet；成功后才推进状态和序列号。 */
XRT_API xsshcode xrtSshAesGcmWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshaesgcm* pState,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 认证并解密一个 AES-GCM packet；payload 与 padding 借用 plain 缓冲。 */
XRT_API xsshcode xrtSshAesGcmRead(
	xsshreader* pReader,
	xsshaesgcm* pState,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
);



XRT_EXTERN_C_END

#endif

#endif
