#include "../internal/xrt_mail_auth.h"
#include "../internal/xrt_smtp_client.h"



#if defined(XMAIL_FEATURE_SMTP_AUTH)

/* 判断认证字段是否具有有效的借用地址范围。 */
/* 设置稳定的 SMTP 认证错误。 */
static bool __xrtSmtpAuthError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_AUTH, sMessage);
	return false;
}



/* 发送一行敏感文本并在发送返回后立即清零临时副本。 */
static bool __xrtSmtpAuthSend(
	xsmtpclient* pClient,
	xstrview Prefix,
	xstrview Encoded,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sLine;
	size_t iSize;
	bool bSuccess;

	if ( !__xrtMailSizeAdd(Prefix.Size, Encoded.Size, &iSize) ||
		(iSize > (Prefix.Size != 0 ?
		 (XSMTP_COMMAND_MAX - 2u) : XSMTP_AUTH_RESPONSE_MAX)) ) {
		return __xrtSmtpAuthError(
			XERR_RANGE,
			"SMTP authentication response exceeds the command limit"
		);
	}
	sLine = (char*)xrtMalloc(iSize + 1u);
	if ( sLine == NULL ) {
		return false;
	}
	if ( Prefix.Size != 0 ) {
		memcpy(sLine, Prefix.Data, Prefix.Size);
	}
	if ( Encoded.Size != 0 ) {
		memcpy(sLine + Prefix.Size, Encoded.Data, Encoded.Size);
	}
	sLine[iSize] = 0;
	bSuccess = Prefix.Size != 0 ? xrtSmtpClientSend(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	) : xrtSmtpClientAuthLine(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sLine, iSize + 1u);
	return bSuccess && xrtSmtpClientReceive(
		pClient,
		pReply,
		iDeadline,
		pCancel
	);
}



/* 把认证终态响应转换为稳定错误。 */
static bool __xrtSmtpAuthResult(const xsmtpreply* pReply)
{
	if ( pReply->Code == 235 ) {
		return true;
	}
	return __xrtSmtpAuthError(
		((pReply->Code >= 400) && (pReply->Code <= 599)) ?
			XERR_PERMISSION : XERR_PROTOCOL,
		"SMTP authentication was rejected"
	);
}



/* 按配置和 SMTP 行长限制决定初始响应或单独 challenge。 */
static bool __xrtSmtpAuthExchange(
	xsmtpclient* pClient,
	xstrview Mechanism,
	char* sEncoded,
	size_t iEncoded,
	bool bInitial,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sPrefix[32];
	size_t iPrefix;

	if ( iEncoded > XSMTP_AUTH_RESPONSE_MAX ) {
		return __xrtSmtpAuthError(
			XERR_RANGE,
			"SMTP authentication response exceeds the SASL limit"
		);
	}
	if ( bInitial &&
		(Mechanism.Size <= (XSMTP_COMMAND_MAX - 8u)) &&
		(iEncoded <= ((XSMTP_COMMAND_MAX - 8u) - Mechanism.Size)) ) {
		iPrefix = 5u + Mechanism.Size + 1u;
		if ( iPrefix > sizeof(sPrefix) ) {
			return __xrtSmtpAuthError(
				XERR_INTERNAL,
				"SMTP authentication mechanism prefix overflow"
			);
		}
		memcpy(sPrefix, "AUTH ", 5u);
		memcpy(sPrefix + 5u, Mechanism.Data, Mechanism.Size);
		sPrefix[iPrefix - 1u] = ' ';
		return __xrtSmtpAuthSend(
			pClient,
			(xstrview) { sPrefix, iPrefix },
			(xstrview) { sEncoded, iEncoded },
			pReply,
			iDeadline,
			pCancel
		);
	}
	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("AUTH"),
		Mechanism,
		pReply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( pReply->Code != 334 ) {
		return true;
	}
	return __xrtSmtpAuthSend(
		pClient,
		XRT_STR_LITERAL(""),
		(xstrview) { sEncoded, iEncoded },
		pReply,
		iDeadline,
		pCancel
	);
}



/* 执行 SASL PLAIN 认证。 */
static bool __xrtSmtpAuthPlain(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sEncoded;
	size_t iEncoded;
	xsmtpreply Reply;
	bool bSuccess;

	sEncoded = __xrtMailAuthPlain(
		pConfig->AuthorizationId,
		pConfig->Username,
		pConfig->Secret,
		&iEncoded
	);
	if ( sEncoded == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthExchange(
		pClient,
		XRT_STR_LITERAL("PLAIN"),
		sEncoded,
		iEncoded,
		pConfig->InitialResponse,
		&Reply,
		iDeadline,
		pCancel
	);
	if ( bSuccess && (Reply.Code == 334) ) {
		bSuccess = __xrtSmtpAuthSend(
			pClient,
			XRT_STR_LITERAL(""),
			(xstrview) { sEncoded, iEncoded },
			&Reply,
			iDeadline,
			pCancel
		);
	}
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	return bSuccess && __xrtSmtpAuthResult(&Reply);
}



/* 执行传统 AUTH LOGIN challenge 交换。 */
static bool __xrtSmtpAuthLogin(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sUsername;
	char* sSecret;
	size_t iUsername;
	size_t iSecret;
	xsmtpreply Reply;
	bool bSuccess;

	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("AUTH"),
		XRT_STR_LITERAL("LOGIN"),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 334 ) {
		return __xrtSmtpAuthResult(&Reply);
	}
	sUsername = __xrtMailAuthEncode(
		pConfig->Username.Data,
		pConfig->Username.Size,
		&iUsername
	);
	if ( sUsername == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthSend(
		pClient,
		XRT_STR_LITERAL(""),
		(xstrview) { sUsername, iUsername },
		&Reply,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sUsername, iUsername + 1u);
	if ( !bSuccess || (Reply.Code != 334) ) {
		return bSuccess ? __xrtSmtpAuthResult(&Reply) : false;
	}
	sSecret = __xrtMailAuthEncode(
		pConfig->Secret.Data,
		pConfig->Secret.Size,
		&iSecret
	);
	if ( sSecret == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthSend(
		pClient,
		XRT_STR_LITERAL(""),
		(xstrview) { sSecret, iSecret },
		&Reply,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sSecret, iSecret + 1u);
	return bSuccess && __xrtSmtpAuthResult(&Reply);
}



/* 执行 XOAUTH2 或 OAUTHBEARER 初始响应交换。 */
static bool __xrtSmtpAuthBearer(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview AuthorizationId = pConfig->AuthorizationId.Size != 0 ?
		pConfig->AuthorizationId : pConfig->Username;
	xstrview Mechanism = pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
		XRT_STR_LITERAL("XOAUTH2") : XRT_STR_LITERAL("OAUTHBEARER");
	char* sEncoded;
	size_t iEncoded;
	xsmtpreply Reply;
	bool bSuccess;

	sEncoded = pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
		__xrtMailAuthXoauth2(
			pConfig->Username,
			pConfig->Secret,
			&iEncoded
		) : __xrtMailAuthOauthBearer(
			AuthorizationId,
			pConfig->Secret,
			&iEncoded
		);
	if ( sEncoded == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthExchange(
		pClient,
		Mechanism,
		sEncoded,
		iEncoded,
		pConfig->InitialResponse,
		&Reply,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	if ( bSuccess && (Reply.Code == 334) ) {
		bSuccess = xrtSmtpClientSend(
			pClient,
			pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
				XRT_STR_LITERAL("") : XRT_STR_LITERAL("AQ=="),
			iDeadline,
			pCancel
		) && xrtSmtpClientReceive(
			pClient,
			&Reply,
			iDeadline,
			pCancel
		);
	}
	return bSuccess && __xrtSmtpAuthResult(&Reply);
}



/* 初始化 SMTP 认证配置。 */
XRT_API void xrtSmtpAuthConfigInit(xsmtpauthconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Method = XSMTP_AUTH_PLAIN;
	pConfig->InitialResponse = true;
}



/* 验证 SMTP 认证配置。 */
XRT_API bool xrtSmtpAuthConfigValid(const xsmtpauthconfig* pConfig)
{
	bool bBearer;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Method < XSMTP_AUTH_PLAIN) ||
		(pConfig->Method > XSMTP_AUTH_OAUTHBEARER) ) {
		return __xrtSmtpAuthError(
			XERR_ARGUMENT,
			"invalid SMTP authentication method"
		);
	}
	bBearer = (pConfig->Method == XSMTP_AUTH_XOAUTH2) ||
		(pConfig->Method == XSMTP_AUTH_OAUTHBEARER);
	if ( !__xrtMailAuthFieldValid(pConfig->Username, bBearer) ||
		!__xrtMailAuthFieldValid(pConfig->Secret, bBearer) ||
		(pConfig->Username.Size == 0) || (pConfig->Secret.Size == 0) ||
		!__xrtMailAuthFieldValid(pConfig->AuthorizationId, bBearer) ||
		((pConfig->Method != XSMTP_AUTH_PLAIN) &&
		 (pConfig->Method != XSMTP_AUTH_OAUTHBEARER) &&
		 (pConfig->AuthorizationId.Size != 0)) ) {
		return __xrtSmtpAuthError(
			XERR_ARGUMENT,
			"invalid SMTP authentication credentials"
		);
	}
	return true;
}



/* 完成一次 SMTP 认证。 */
XRT_API bool xrtSmtpClientAuth(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	uint64 iCapability;
	bool bSuccess;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtSmtpAuthConfigValid(pConfig) ) {
		return false;
	}
	if ( xrtSmtpClientState(pClient) != XSMTP_CLIENT_READY ) {
		return __xrtSmtpAuthError(
			XERR_STATE,
			"SMTP authentication requires READY state"
		);
	}
	if ( xrtSmtpClientAuthenticated(pClient) ) {
		return __xrtSmtpAuthError(
			XERR_STATE,
			"SMTP session is already authenticated"
		);
	}
	if ( (pConfig->Method == XSMTP_AUTH_OAUTHBEARER) &&
		(xrtSmtpClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) ) {
		return __xrtSmtpAuthError(
			XERR_PERMISSION,
			"SMTP OAUTHBEARER requires TLS"
		);
	}
	if ( (xrtSmtpClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) &&
		!pConfig->AllowPlaintext ) {
		return __xrtSmtpAuthError(
			XERR_PERMISSION,
			"SMTP credentials require TLS or explicit plaintext opt-in"
		);
	}
	iCapability = pConfig->Method == XSMTP_AUTH_PLAIN ?
		XSMTP_CAP_AUTH_PLAIN :
		(pConfig->Method == XSMTP_AUTH_LOGIN ? XSMTP_CAP_AUTH_LOGIN :
		 (pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
		  XSMTP_CAP_AUTH_XOAUTH2 : XSMTP_CAP_AUTH_OAUTHBEARER));
	if ( (xrtSmtpClientCapabilities(pClient) & iCapability) == 0 ) {
		return __xrtSmtpAuthError(
			XERR_UNSUPPORTED,
			"SMTP server did not advertise the authentication mechanism"
		);
	}
	if ( pConfig->Method == XSMTP_AUTH_PLAIN ) {
		bSuccess = __xrtSmtpAuthPlain(pClient, pConfig, iDeadline, pCancel);
	} else if ( pConfig->Method == XSMTP_AUTH_LOGIN ) {
		bSuccess = __xrtSmtpAuthLogin(pClient, pConfig, iDeadline, pCancel);
	} else {
		bSuccess = __xrtSmtpAuthBearer(pClient, pConfig, iDeadline, pCancel);
	}
	if ( bSuccess ) {
		__xrtSmtpClientAuthComplete(pClient);
	}
	return bSuccess;
}

#endif
