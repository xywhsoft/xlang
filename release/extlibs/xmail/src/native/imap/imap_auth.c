#include <xrt/imap_auth.h>

#include "../internal/xrt_imap_client.h"
#include "../internal/xrt_mail_auth.h"



#if defined(XMAIL_FEATURE_IMAP_AUTH)

typedef enum __ximapauthnext {
	__XIMAP_AUTH_ERROR = 0,
	__XIMAP_AUTH_CONTINUE,
	__XIMAP_AUTH_COMPLETE
} __ximapauthnext;



/* 设置稳定的 IMAP 认证错误。 */
static bool __xrtImapAuthError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_AUTH, sMessage);
	return false;
}



/* 读取到下一个 continuation 或 tagged completion。 */
static __ximapauthnext __xrtImapAuthNext(
	ximapclient* pClient,
	ximapstatus* pStatus,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	ximapevent Event;
	xmailnext Next;

	for ( ;; ) {
		Next = xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return __XIMAP_AUTH_ERROR;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return __XIMAP_AUTH_ERROR;
			}
			*pStatus = Final.Status;
			return __XIMAP_AUTH_COMPLETE;
		}
		if ( Event.HasLiteral ) {
			(void)__xrtImapAuthError(
				XERR_PROTOCOL,
				"unexpected IMAP literal during authentication"
			);
			return __XIMAP_AUTH_ERROR;
		}
		if ( (Event.Kind == XIMAP_EVENT_RESPONSE) &&
			(Event.Response.Kind == XIMAP_RESPONSE_CONTINUATION) ) {
			return __XIMAP_AUTH_CONTINUE;
		}
	}
}



/* 把最终认证状态转换为会话状态或稳定错误。 */
static bool __xrtImapAuthResult(
	ximapclient* pClient,
	ximapstatus Status
)
{
	if ( Status == XIMAP_STATUS_OK ) {
		return __xrtImapClientStateCommit(
			pClient,
			XIMAP_CLIENT_AUTHENTICATED
		);
	}
	return __xrtImapAuthError(
		Status == XIMAP_STATUS_NO ? XERR_PERMISSION : XERR_PROTOCOL,
		"IMAP authentication was rejected"
	);
}



/* 创建 `quoted-user SP quoted-secret` LOGIN 参数并保留精确长度。 */
static char* __xrtImapAuthLoginArguments(
	const ximapauthconfig* pConfig,
	size_t* pSize
)
{
	size_t iUsername;
	size_t iSecret;
	size_t iRequired;
	size_t iWritten;
	char* sArguments;

	if ( !xrtImapQuoteWrite(
		pConfig->Username,
		NULL,
		0,
		&iUsername
	) || !xrtImapQuoteWrite(
		pConfig->Secret,
		NULL,
		0,
		&iSecret
	) || !__xrtMailSizeAdd(iUsername, iSecret, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		(iRequired >= SIZE_MAX) ) {
		return NULL;
	}
	sArguments = (char*)xrtMalloc(iRequired + 1u);
	if ( sArguments == NULL ) {
		return NULL;
	}
	if ( !xrtImapQuoteWrite(
		pConfig->Username,
		sArguments,
		iRequired + 1u,
		&iWritten
	) || (iWritten != iUsername) ) {
		__xrtMailAuthFree(sArguments, iRequired + 1u);
		return NULL;
	}
	sArguments[iUsername] = ' ';
	if ( !xrtImapQuoteWrite(
		pConfig->Secret,
		sArguments + iUsername + 1u,
		iSecret + 1u,
		&iWritten
	) || (iWritten != iSecret) ) {
		__xrtMailAuthFree(sArguments, iRequired + 1u);
		return NULL;
	}
	*pSize = iRequired;
	return sArguments;
}



/* 执行传统 LOGIN 命令。 */
static bool __xrtImapAuthLogin(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sArguments;
	size_t iArguments;
	ximapstatus Status = XIMAP_STATUS_NONE;
	__ximapauthnext Next;
	bool bStarted;

	sArguments = __xrtImapAuthLoginArguments(pConfig, &iArguments);
	if ( sArguments == NULL ) {
		return false;
	}
	bStarted = xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("LOGIN"),
		(xstrview) { sArguments, iArguments },
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sArguments, iArguments + 1u);
	if ( !bStarted ) {
		return false;
	}
	Next = __xrtImapAuthNext(pClient, &Status, iDeadline, pCancel);
	if ( Next == __XIMAP_AUTH_CONTINUE ) {
		if ( !xrtImapClientContinue(
			pClient,
			XRT_STR_LITERAL("*"),
			iDeadline,
			pCancel
		) ) {
			return false;
		}
		(void)__xrtImapAuthNext(pClient, &Status, iDeadline, pCancel);
		return __xrtImapAuthError(
			XERR_PROTOCOL,
			"unexpected continuation for IMAP LOGIN"
		);
	}
	return (Next == __XIMAP_AUTH_COMPLETE) &&
		__xrtImapAuthResult(pClient, Status);
}



/* 创建带可选 SASL-IR 的 AUTHENTICATE 参数。 */
static char* __xrtImapAuthArguments(
	xstrview Mechanism,
	const char* sEncoded,
	size_t iEncoded,
	size_t* pSize
)
{
	char* sArguments;
	size_t iRequired;

	if ( !__xrtMailSizeAdd(Mechanism.Size, iEncoded, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		(iRequired >= SIZE_MAX) ) {
		return NULL;
	}
	sArguments = (char*)xrtMalloc(iRequired + 1u);
	if ( sArguments == NULL ) {
		return NULL;
	}
	memcpy(sArguments, Mechanism.Data, Mechanism.Size);
	sArguments[Mechanism.Size] = ' ';
	memcpy(sArguments + Mechanism.Size + 1u, sEncoded, iEncoded);
	sArguments[iRequired] = 0;
	*pSize = iRequired;
	return sArguments;
}



/* 执行 PLAIN、XOAUTH2 或 OAUTHBEARER challenge/response 交换。 */
static bool __xrtImapAuthSasl(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Mechanism = pConfig->Method == XIMAP_AUTH_PLAIN ?
		XRT_STR_LITERAL("PLAIN") :
		(pConfig->Method == XIMAP_AUTH_XOAUTH2 ?
		 XRT_STR_LITERAL("XOAUTH2") : XRT_STR_LITERAL("OAUTHBEARER"));
	xstrview AuthorizationId = pConfig->AuthorizationId.Size != 0 ?
		pConfig->AuthorizationId : pConfig->Username;
	char* sEncoded;
	char* sArguments = NULL;
	size_t iEncoded;
	size_t iArguments = 0;
	ximapstatus Status = XIMAP_STATUS_NONE;
	__ximapauthnext Next;
	bool bInitial;
	bool bSent;
	bool bFinalized = false;
	bool bStarted;

	if ( pConfig->Method == XIMAP_AUTH_PLAIN ) {
		sEncoded = __xrtMailAuthPlain(
			pConfig->AuthorizationId,
			pConfig->Username,
			pConfig->Secret,
			&iEncoded
		);
	} else if ( pConfig->Method == XIMAP_AUTH_XOAUTH2 ) {
		sEncoded = __xrtMailAuthXoauth2(
			pConfig->Username,
			pConfig->Secret,
			&iEncoded
		);
	} else {
		sEncoded = __xrtMailAuthOauthBearer(
			AuthorizationId,
			pConfig->Secret,
			&iEncoded
		);
	}
	if ( sEncoded == NULL ) {
		return false;
	}
	bInitial = pConfig->InitialResponse &&
		((xrtImapClientCapabilities(pClient) & XIMAP_CAP_SASL_IR) != 0);
	if ( bInitial ) {
		sArguments = __xrtImapAuthArguments(
			Mechanism,
			sEncoded,
			iEncoded,
			&iArguments
		);
		if ( sArguments == NULL ) {
			__xrtMailAuthFree(sEncoded, iEncoded + 1u);
			return false;
		}
	}
	bStarted = xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("AUTHENTICATE"),
		bInitial ? (xstrview) { sArguments, iArguments } : Mechanism,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sArguments, iArguments + (sArguments != NULL ? 1u : 0u));
	if ( !bStarted ) {
		__xrtMailAuthFree(sEncoded, iEncoded + 1u);
		return false;
	}
	bSent = bInitial;
	for ( ;; ) {
		Next = __xrtImapAuthNext(pClient, &Status, iDeadline, pCancel);
		if ( Next == __XIMAP_AUTH_ERROR ) {
			__xrtMailAuthFree(sEncoded, iEncoded + 1u);
			return false;
		}
		if ( Next == __XIMAP_AUTH_COMPLETE ) {
			break;
		}
		if ( !bSent ) {
			bStarted = xrtImapClientContinue(
				pClient,
				(xstrview) { sEncoded, iEncoded },
				iDeadline,
				pCancel
			);
			bSent = true;
		} else if ( (pConfig->Method == XIMAP_AUTH_XOAUTH2) &&
			!bFinalized ) {
			bStarted = xrtImapClientContinue(
				pClient,
				XRT_STR_LITERAL(""),
				iDeadline,
				pCancel
			);
			bFinalized = true;
		} else if ( (pConfig->Method == XIMAP_AUTH_OAUTHBEARER) &&
			!bFinalized ) {
			bStarted = xrtImapClientContinue(
				pClient,
				XRT_STR_LITERAL("AQ=="),
				iDeadline,
				pCancel
			);
			bFinalized = true;
		} else {
			bStarted = xrtImapClientContinue(
				pClient,
				XRT_STR_LITERAL("*"),
				iDeadline,
				pCancel
			);
			bFinalized = true;
		}
		if ( !bStarted ) {
			__xrtMailAuthFree(sEncoded, iEncoded + 1u);
			return false;
		}
	}
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	return __xrtImapAuthResult(pClient, Status);
}



/* 初始化 IMAP 认证配置。 */
XRT_API void xrtImapAuthConfigInit(ximapauthconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Method = XIMAP_AUTH_PLAIN;
	pConfig->InitialResponse = true;
}



/* 验证 IMAP 认证配置。 */
XRT_API bool xrtImapAuthConfigValid(const ximapauthconfig* pConfig)
{
	bool bBearer;
	size_t iQuoted;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Method < XIMAP_AUTH_LOGIN) ||
		(pConfig->Method > XIMAP_AUTH_OAUTHBEARER) ) {
		return __xrtImapAuthError(
			XERR_ARGUMENT,
			"invalid IMAP authentication method"
		);
	}
	bBearer = (pConfig->Method == XIMAP_AUTH_XOAUTH2) ||
		(pConfig->Method == XIMAP_AUTH_OAUTHBEARER);
	if ( !__xrtMailAuthFieldValid(pConfig->Username, bBearer) ||
		!__xrtMailAuthFieldValid(pConfig->Secret, bBearer) ||
		(pConfig->Username.Size == 0) || (pConfig->Secret.Size == 0) ||
		!__xrtMailAuthFieldValid(pConfig->AuthorizationId, bBearer) ||
		((pConfig->Method != XIMAP_AUTH_PLAIN) &&
		 (pConfig->Method != XIMAP_AUTH_OAUTHBEARER) &&
		 (pConfig->AuthorizationId.Size != 0)) ) {
		return __xrtImapAuthError(
			XERR_ARGUMENT,
			"invalid IMAP authentication credentials"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_LOGIN) &&
		(!xrtImapQuoteWrite(
			pConfig->Username,
			NULL,
			0,
			&iQuoted
		) || !xrtImapQuoteWrite(
			pConfig->Secret,
			NULL,
			0,
			&iQuoted
		)) ) {
		return false;
	}
	return true;
}



/* 执行选定的 IMAP 认证机制。 */
XRT_API bool xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	uint64 iCapability;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapAuthConfigValid(pConfig) ) {
		return false;
	}
	if ( xrtImapClientState(pClient) != XIMAP_CLIENT_NOT_AUTHENTICATED ) {
		return __xrtImapAuthError(
			XERR_STATE,
			"IMAP authentication requires NOT_AUTHENTICATED state"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_OAUTHBEARER) &&
		(xrtImapClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) ) {
		return __xrtImapAuthError(
			XERR_PERMISSION,
			"IMAP OAUTHBEARER requires TLS"
		);
	}
	if ( (xrtImapClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) &&
		!pConfig->AllowPlaintext ) {
		return __xrtImapAuthError(
			XERR_PERMISSION,
			"IMAP credentials require TLS or explicit plaintext opt-in"
		);
	}
	iCapability = xrtImapClientCapabilities(pClient);
	if ( pConfig->Method == XIMAP_AUTH_LOGIN ) {
		if ( (iCapability & XIMAP_CAP_LOGIN_DISABLED) != 0 ) {
			return __xrtImapAuthError(
				XERR_UNSUPPORTED,
				"IMAP server disabled the LOGIN command"
			);
		}
		return __xrtImapAuthLogin(pClient, pConfig, iDeadline, pCancel);
	}
	if ( (pConfig->Method == XIMAP_AUTH_PLAIN) &&
		((iCapability & XIMAP_CAP_AUTH_PLAIN) == 0) ) {
		return __xrtImapAuthError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise AUTH=PLAIN"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_XOAUTH2) &&
		((iCapability & XIMAP_CAP_AUTH_XOAUTH2) == 0) ) {
		return __xrtImapAuthError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise AUTH=XOAUTH2"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_OAUTHBEARER) &&
		((iCapability & XIMAP_CAP_AUTH_OAUTHBEARER) == 0) ) {
		return __xrtImapAuthError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise AUTH=OAUTHBEARER"
		);
	}
	return __xrtImapAuthSasl(pClient, pConfig, iDeadline, pCancel);
}

#endif
