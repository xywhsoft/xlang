#include <xrt/pop3_auth.h>

#include "../internal/xrt_mail_auth.h"
#include "../internal/xrt_pop3_client.h"



#if defined(XMAIL_FEATURE_POP3_AUTH)

typedef enum __xpop3authnext {
	__XPOP3_AUTH_ERROR = 0,
	__XPOP3_AUTH_CONTINUE,
	__XPOP3_AUTH_OK,
	__XPOP3_AUTH_REJECTED
} __xpop3authnext;



/* 设置稳定的 POP3 认证错误。 */
static bool __xrtPop3AuthError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_AUTH, sMessage);
	return false;
}



/* 判断线路是否以一个完整、不区分大小写的 POP3 状态词开头。 */
static bool __xrtPop3AuthStatus(xstrview Line, xstrview Status)
{
	return (Line.Size >= Status.Size) && __xrtMailAsciiEqualI(
		__xrtMailSlice(Line, 0, Status.Size),
		Status
	) && ((Line.Size == Status.Size) || (Line.Data[Status.Size] == ' '));
}



/* 读取 SASL continuation 或最终 POP3 状态，并稳定保存最终响应。 */
static __xpop3authnext __xrtPop3AuthNext(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;
	xpop3reply Reply;

	if ( !xrtPop3ClientLine(pClient, &Line, iDeadline, pCancel) ) {
		return __XPOP3_AUTH_ERROR;
	}
	if ( __xrtPop3AuthStatus(Line, XRT_STR_LITERAL("+OK")) ||
		__xrtPop3AuthStatus(Line, XRT_STR_LITERAL("-ERR")) ) {
		if ( !__xrtPop3ClientReplySave(pClient, Line, &Reply) ) {
			(void)__xrtPop3AuthError(
				XERR_PROTOCOL,
				"invalid POP3 AUTH final response"
			);
			(void)__xrtPop3ClientFail(pClient);
			return __XPOP3_AUTH_ERROR;
		}
		return Reply.Ok ? __XPOP3_AUTH_OK : __XPOP3_AUTH_REJECTED;
	}
	if ( (Line.Size != 0) && (Line.Data[0] == '+') &&
		((Line.Size == 1u) || (Line.Data[1] == ' ')) ) {
		return __XPOP3_AUTH_CONTINUE;
	}
	(void)__xrtPop3AuthError(
		XERR_PROTOCOL,
		"invalid POP3 AUTH continuation"
	);
	(void)__xrtPop3ClientFail(pClient);
	return __XPOP3_AUTH_ERROR;
}



/* 发送并清零一条包含传统 POP3 凭据的命令。 */
static bool __xrtPop3AuthCommand(
	xpop3client* pClient,
	xstrview Prefix,
	xstrview Credential,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sLine;
	size_t iSize;
	bool bSuccess;

	if ( !__xrtMailSizeAdd(Prefix.Size, Credential.Size, &iSize) ||
		(iSize > (XPOP3_COMMAND_MAX - 2u)) ) {
		return __xrtPop3AuthError(
			XERR_RANGE,
			"POP3 credential exceeds the command limit"
		);
	}
	sLine = (char*)xrtMalloc(iSize + 1u);
	if ( sLine == NULL ) {
		return false;
	}
	memcpy(sLine, Prefix.Data, Prefix.Size);
	memcpy(sLine + Prefix.Size, Credential.Data, Credential.Size);
	sLine[iSize] = 0;
	bSuccess = xrtPop3ClientSend(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sLine, iSize + 1u);
	return bSuccess && xrtPop3ClientReceive(
		pClient,
		pReply,
		iDeadline,
		pCancel
	);
}



/* 完成 USER/PASS 认证。 */
static bool __xrtPop3AuthUserPass(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !__xrtPop3AuthCommand(
		pClient,
		XRT_STR_LITERAL("USER "),
		pConfig->Username,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 username was rejected"
		);
	}
	if ( !__xrtPop3AuthCommand(
		pClient,
		XRT_STR_LITERAL("PASS "),
		pConfig->Secret,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 password was rejected"
		);
	}
	return __xrtPop3ClientAuthorize(pClient);
}



/* 返回配置对应的 SASL 机制名称和能力位。 */
static xstrview __xrtPop3AuthMechanism(
	xpop3authmethod Method,
	uint32* pCapability
)
{
	if ( Method == XPOP3_AUTH_PLAIN ) {
		*pCapability = XPOP3_SASL_PLAIN;
		return XRT_STR_LITERAL("PLAIN");
	}
	if ( Method == XPOP3_AUTH_XOAUTH2 ) {
		*pCapability = XPOP3_SASL_XOAUTH2;
		return XRT_STR_LITERAL("XOAUTH2");
	}
	*pCapability = XPOP3_SASL_OAUTHBEARER;
	return XRT_STR_LITERAL("OAUTHBEARER");
}



/* 创建配置机制的一次性 Base64 初始响应。 */
static char* __xrtPop3AuthResponse(
	const xpop3authconfig* pConfig,
	size_t* pEncodedSize
)
{
	if ( pConfig->Method == XPOP3_AUTH_PLAIN ) {
		return __xrtMailAuthPlain(
			pConfig->AuthorizationId,
			pConfig->Username,
			pConfig->Secret,
			pEncodedSize
		);
	}
	if ( pConfig->Method == XPOP3_AUTH_XOAUTH2 ) {
		return __xrtMailAuthXoauth2(
			pConfig->Username,
			pConfig->Secret,
			pEncodedSize
		);
	}
	return __xrtMailAuthOauthBearer(
		pConfig->AuthorizationId.Size != 0 ?
			pConfig->AuthorizationId : pConfig->Username,
		pConfig->Secret,
		pEncodedSize
	);
}



/* 发送 AUTH 命令，并在 255 字节预算允许时携带初始响应。 */
static bool __xrtPop3AuthStart(
	xpop3client* pClient,
	xstrview Mechanism,
	xstrview Encoded,
	bool bInitial,
	bool* pInitialUsed,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sLine[XPOP3_AUTH_COMMAND_MAX];
	size_t iSize = 5u + Mechanism.Size;
	bool bUseInitial = bInitial &&
		(Encoded.Size <= ((XPOP3_AUTH_COMMAND_MAX - 2u) - iSize - 1u));
	bool bSuccess;

	memcpy(sLine, "AUTH ", 5u);
	memcpy(sLine + 5u, Mechanism.Data, Mechanism.Size);
	if ( bUseInitial ) {
		sLine[iSize++] = ' ';
		memcpy(sLine + iSize, Encoded.Data, Encoded.Size);
		iSize += Encoded.Size;
	}
	bSuccess = xrtPop3ClientAuthLine(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	);
	xrtSecureZero(sLine, sizeof(sLine));
	*pInitialUsed = bUseInitial;
	return bSuccess;
}



/* 完成 PLAIN 或 bearer 的单响应 SASL 交换。 */
static bool __xrtPop3AuthSasl(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Mechanism;
	char* sEncoded;
	size_t iEncoded;
	uint32 iCapability;
	__xpop3authnext Next;
	bool bInitialUsed;
	bool bBearer;
	bool bSuccess;

	Mechanism = __xrtPop3AuthMechanism(pConfig->Method, &iCapability);
	if ( (xrtPop3ClientCapabilities(pClient) & XPOP3_CAP_SASL) == 0 ) {
		return __xrtPop3AuthError(
			XERR_UNSUPPORTED,
			"POP3 server did not advertise SASL"
		);
	}
	if ( (xrtPop3ClientSaslMechanisms(pClient) & iCapability) == 0 ) {
		return __xrtPop3AuthError(
			XERR_UNSUPPORTED,
			"POP3 server did not advertise the authentication mechanism"
		);
	}
	sEncoded = __xrtPop3AuthResponse(pConfig, &iEncoded);
	if ( sEncoded == NULL ) {
		return false;
	}
	if ( iEncoded > (XPOP3_AUTH_RESPONSE_MAX - 2u) ) {
		__xrtMailAuthFree(sEncoded, iEncoded + 1u);
		return __xrtPop3AuthError(
			XERR_RANGE,
			"POP3 authentication response exceeds the SASL limit"
		);
	}
	bSuccess = __xrtPop3AuthStart(
		pClient,
		Mechanism,
		(xstrview) { sEncoded, iEncoded },
		pConfig->InitialResponse,
		&bInitialUsed,
		iDeadline,
		pCancel
	);
	Next = bSuccess ? __xrtPop3AuthNext(
		pClient,
		iDeadline,
		pCancel
	) : __XPOP3_AUTH_ERROR;
	if ( (Next == __XPOP3_AUTH_CONTINUE) && !bInitialUsed ) {
		bSuccess = xrtPop3ClientAuthLine(
			pClient,
			(xstrview) { sEncoded, iEncoded },
			iDeadline,
			pCancel
		);
		Next = bSuccess ? __xrtPop3AuthNext(
			pClient,
			iDeadline,
			pCancel
		) : __XPOP3_AUTH_ERROR;
	}
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	if ( Next == __XPOP3_AUTH_ERROR ) {
		return false;
	}
	if ( Next == __XPOP3_AUTH_OK ) {
		return __xrtPop3ClientAuthorize(pClient);
	}
	if ( Next == __XPOP3_AUTH_REJECTED ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 authentication was rejected"
		);
	}
	bBearer = (pConfig->Method == XPOP3_AUTH_XOAUTH2) ||
		(pConfig->Method == XPOP3_AUTH_OAUTHBEARER);
	bSuccess = xrtPop3ClientAuthLine(
		pClient,
		bBearer ? XRT_STR_LITERAL("AQ==") : XRT_STR_LITERAL("*"),
		iDeadline,
		pCancel
	);
	Next = bSuccess ? __xrtPop3AuthNext(
		pClient,
		iDeadline,
		pCancel
	) : __XPOP3_AUTH_ERROR;
	if ( Next == __XPOP3_AUTH_ERROR ) {
		return false;
	}
	if ( (Next == __XPOP3_AUTH_OK) ||
		(Next == __XPOP3_AUTH_CONTINUE) ) {
		(void)__xrtPop3ClientFail(pClient);
		return __xrtPop3AuthError(
			XERR_PROTOCOL,
			"invalid POP3 AUTH completion after cancellation"
		);
	}
	return __xrtPop3AuthError(
		XERR_PERMISSION,
		bBearer ? "POP3 bearer authentication was rejected" :
			"POP3 authentication sent an unexpected extra challenge"
	);
}



/* 初始化 POP3 认证配置。 */
XRT_API void xrtPop3AuthConfigInit(xpop3authconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Method = XPOP3_AUTH_PLAIN;
	pConfig->InitialResponse = true;
}



/* 验证 POP3 认证配置。 */
XRT_API bool xrtPop3AuthConfigValid(const xpop3authconfig* pConfig)
{
	bool bBearer;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Method < XPOP3_AUTH_USER_PASS) ||
		(pConfig->Method > XPOP3_AUTH_OAUTHBEARER) ) {
		return __xrtPop3AuthError(
			XERR_ARGUMENT,
			"invalid POP3 authentication method"
		);
	}
	bBearer = (pConfig->Method == XPOP3_AUTH_XOAUTH2) ||
		(pConfig->Method == XPOP3_AUTH_OAUTHBEARER);
	if ( !__xrtMailAuthFieldValid(pConfig->Username, bBearer) ||
		!__xrtMailAuthFieldValid(pConfig->Secret, bBearer) ||
		(pConfig->Username.Size == 0) || (pConfig->Secret.Size == 0) ||
		!__xrtMailAuthFieldValid(pConfig->AuthorizationId, bBearer) ||
		((pConfig->Method != XPOP3_AUTH_PLAIN) &&
		 (pConfig->Method != XPOP3_AUTH_OAUTHBEARER) &&
		 (pConfig->AuthorizationId.Size != 0)) ) {
		return __xrtPop3AuthError(
			XERR_ARGUMENT,
			"invalid POP3 authentication credentials"
		);
	}
	return true;
}



/* 完成一次 POP3 认证。 */
XRT_API bool xrtPop3ClientAuth(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	bool bBearer;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3AuthConfigValid(pConfig) ) {
		return false;
	}
	if ( xrtPop3ClientState(pClient) != XPOP3_CLIENT_AUTHORIZATION ) {
		return __xrtPop3AuthError(
			XERR_STATE,
			"POP3 authentication requires AUTHORIZATION state"
		);
	}
	bBearer = (pConfig->Method == XPOP3_AUTH_XOAUTH2) ||
		(pConfig->Method == XPOP3_AUTH_OAUTHBEARER);
	if ( bBearer &&
		(xrtPop3ClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 bearer authentication requires TLS"
		);
	}
	if ( (xrtPop3ClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) &&
		!pConfig->AllowPlaintext ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 credentials require TLS or explicit plaintext opt-in"
		);
	}
	return pConfig->Method == XPOP3_AUTH_USER_PASS ?
		__xrtPop3AuthUserPass(
			pClient,
			pConfig,
			iDeadline,
			pCancel
		) : __xrtPop3AuthSasl(
			pClient,
			pConfig,
			iDeadline,
			pCancel
		);
}



/* 使用传统 USER/PASS 的轻量便利入口。 */
XRT_API bool xrtPop3ClientLogin(
	xpop3client* pClient,
	xstrview Username,
	xstrview Password,
	bool AllowPlaintext,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3authconfig Config;

	xrtPop3AuthConfigInit(&Config);
	Config.Method = XPOP3_AUTH_USER_PASS;
	Config.Username = Username;
	Config.Secret = Password;
	Config.AllowPlaintext = AllowPlaintext;
	return xrtPop3ClientAuth(
		pClient,
		&Config,
		iDeadline,
		pCancel
	);
}

#endif
