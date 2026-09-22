#ifndef XRT_IMAP_COMPRESS_H
#define XRT_IMAP_COMPRESS_H

#include <xrt/compress.h>
#include <xrt/imap_client.h>



#if defined(XMAIL_FEATURE_IMAP_COMPRESS) && \
	(!defined(XMAIL_FEATURE_IMAP_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || \
	 !defined(XRT_FEATURE_DEFLATE) || !defined(XRT_FEATURE_INFLATE))
	#error "XMAIL_FEATURE_IMAP_COMPRESS requires IMAP client and raw Deflate"
#endif



#if defined(XMAIL_FEATURE_IMAP_COMPRESS)

/* IMAP COMPRESS 使用 raw DEFLATE；级别、策略和窗口可按负载调整。 */
typedef struct ximapcompressconfig {
	int32 Level;
	xdeflatestrategy Strategy;
	uint8 WindowBits;
} ximapcompressconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 XRT 默认压缩级别、策略和 32 KiB 窗口。 */
XRT_API void xrtImapCompressConfigInit(ximapcompressconfig* pConfig);



/* 验证 raw DEFLATE 编码与解码配置可以同时创建。 */
XRT_API bool xrtImapCompressConfigValid(
	const ximapcompressconfig* pConfig
);



/* 返回当前会话是否已经成功启用 IMAP COMPRESS。 */
XRT_API bool xrtImapClientCompressed(const ximapclient* pClient);



/* 协商 COMPRESS DEFLATE；只有 tagged OK 后才切换双向传输。 */
XRT_API bool xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
