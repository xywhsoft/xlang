#include <xrt/imap_compress.h>

#include "../internal/xrt_imap_client.h"
#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP_COMPRESS)

/* 创建稳定的 IMAP COMPRESS 错误。 */
static bool __xrtImapCompressError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 从公开配置生成 raw Deflate 双向配置。 */
static bool __xrtImapCompressConfigs(
	const ximapcompressconfig* pConfig,
	xdeflateconfig* pDeflate,
	xinflateconfig* pInflate
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xrtMemRangeValid(pDeflate, sizeof(*pDeflate)) ||
		!xrtMemRangeValid(pInflate, sizeof(*pInflate)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	xrtDeflateConfigInit(pDeflate);
	pDeflate->Format = XDEFLATE_RAW;
	pDeflate->Level = pConfig->Level;
	pDeflate->Strategy = pConfig->Strategy;
	pDeflate->WindowBits = pConfig->WindowBits;
	xrtInflateConfigInit(pInflate);
	pInflate->Format = XINFLATE_RAW;
	pInflate->WindowBits = pConfig->WindowBits;
	return xrtDeflateConfigValid(pDeflate) &&
		xrtInflateConfigValid(pInflate);
}



/* 初始化 IMAP 压缩配置。 */
XRT_API void xrtImapCompressConfigInit(ximapcompressconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	pConfig->Level = XDEFLATE_LEVEL_DEFAULT;
	pConfig->Strategy = XDEFLATE_STRATEGY_DEFAULT;
	pConfig->WindowBits = XDEFLATE_WINDOW_MAX;
}



/* 验证 IMAP raw DEFLATE 双向配置。 */
XRT_API bool xrtImapCompressConfigValid(
	const ximapcompressconfig* pConfig
)
{
	xdeflateconfig Deflate;
	xinflateconfig Inflate;

	return __xrtImapCompressConfigs(pConfig, &Deflate, &Inflate);
}



/* 返回当前 IMAP 会话压缩状态。 */
XRT_API bool xrtImapClientCompressed(const ximapclient* pClient)
{
	return __xrtImapClientCompressed(pClient);
}



/* 协商并切换 IMAP COMPRESS=DEFLATE。 */
XRT_API bool xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xdeflateconfig Deflate;
	xinflateconfig Inflate;
	ximapclientstate State;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapCompressConfigs(pConfig, &Deflate, &Inflate) ) {
		return false;
	}
	State = xrtImapClientState(pClient);
	if ( (State != XIMAP_CLIENT_AUTHENTICATED) &&
		(State != XIMAP_CLIENT_SELECTED) ) {
		return __xrtImapCompressError(
			XERR_STATE,
			"IMAP COMPRESS requires an authenticated session"
		);
	}
	if ( xrtImapClientCompressed(pClient) ) {
		return __xrtImapCompressError(
			XERR_STATE,
			"IMAP compression is already active"
		);
	}
	if ( (xrtImapClientCapabilities(pClient) &
		XIMAP_CAP_COMPRESS_DEFLATE) == 0 ) {
		return __xrtImapCompressError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise COMPRESS=DEFLATE"
		);
	}
	if ( !xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("COMPRESS"),
		XRT_STR_LITERAL("DEFLATE"),
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			if ( Final.Status != XIMAP_STATUS_OK ) {
				return __xrtImapCompressError(
					XERR_UNSUPPORTED,
					"IMAP COMPRESS was rejected"
				);
			}
			return __xrtImapClientCompressStart(
				pClient,
				&Deflate,
				&Inflate
			);
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"IMAP COMPRESS returned an unexpected literal"
			);
		}
	}
}

#endif
