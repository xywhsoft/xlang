#include <string.h>

#include <xrt/ssh_auth_password.h>



#if defined(XSSH_FEATURE_AUTH_PASSWORD)

/* 转换文本视图为原始字节视图。 */
static xbytesview xsshPasswordBytes(xstrview Text)
{
	xbytesview Value;

	Value.Data = (const unsigned char*)Text.Data;
	Value.Size = Text.Size;
	return Value;
}



/* 转换原始字节视图为文本视图。 */
static xstrview xsshPasswordText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 比较借用文本与固定协议文本。 */
static bool xsshPasswordTextEqual(xstrview Text, const char* sValue, size_t iSize)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 string 字段加入方法字段长度。 */
static xsshcode xsshPasswordAddString(xstrview Text, size_t* pSize)
{
	if ( (pSize == NULL) || !xrtUtf8Valid(Text, NULL) ||
		(Text.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pSize > (SIZE_MAX - 4u)) ||
		(Text.Size > (SIZE_MAX - *pSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize += 4u + Text.Size;
	return XSSH_OK;
}



/* 构建普通或更改密码请求，并在最后一次性发布 writer。 */
static xsshcode xsshPasswordWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password,
	xstrview NewPassword,
	bool bChange
)
{
	xbytesview arrInputs[3];
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = xsshPasswordBytes(User);
	arrInputs[1] = xsshPasswordBytes(Password);
	arrInputs[2] = xsshPasswordBytes(NewPassword);
	Code = xsshPasswordAddString(Password, &iFieldsSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( bChange ) {
		Code = xsshPasswordAddString(NewPassword, &iFieldsSize);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		3u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xrtSshAuthRequestWrite(
		&Writer,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteBool(&Writer, bChange) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(bChange &&
		 (xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK)) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入普通密码认证请求。 */
xsshcode xrtSshAuthPasswordWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password
)
{
	return xsshPasswordWrite(
		pWriter,
		User,
		Password,
		XRT_STR_LITERAL(""),
		false
	);
}



/* 写入旧密码与新密码认证请求。 */
xsshcode xrtSshAuthPasswordChangeWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password,
	xstrview NewPassword
)
{
	return xsshPasswordWrite(
		pWriter,
		User,
		Password,
		NewPassword,
		true
	);
}



/* 严格读取普通或更改密码请求。 */
xsshcode xrtSshAuthPasswordRead(
	xbytesview Payload,
	xsshauthpassword* pPassword
)
{
	xsshauthrequest Request;
	xsshauthpassword Password;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pPassword == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshPasswordTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshPasswordTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_PASSWORD,
		sizeof(XSSH_AUTH_METHOD_PASSWORD) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Password.User = Request.User;
	Code = xrtSshReadBool(&Reader, &Password.Change);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Password.Password = xsshPasswordText(Value);
	Password.NewPassword = XRT_STR_LITERAL("");
	if ( Password.Change ) {
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Password.NewPassword = xsshPasswordText(Value);
	}
	if ( !xrtUtf8Valid(Password.Password, NULL) ||
		!xrtUtf8Valid(Password.NewPassword, NULL) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPassword = Password;
	return XSSH_OK;
}



/* 写入服务端密码更改提示。 */
xsshcode xrtSshAuthPasswordPromptWrite(
	xsshwriter* pWriter,
	xstrview Prompt,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	arrInputs[0] = xsshPasswordBytes(Prompt);
	arrInputs[1] = xsshPasswordBytes(Language);
	Code = xsshPasswordAddString(Prompt, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshLanguageValid(Language) ||
		(Language.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (iTotal > (SIZE_MAX - 4u)) ||
		(Language.Size > (SIZE_MAX - iTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += 4u + Language.Size;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[0]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[1]
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取服务端密码更改提示。 */
xsshcode xrtSshAuthPasswordPromptRead(
	xbytesview Payload,
	xsshauthpasswordprompt* pPrompt
)
{
	xsshreader Reader;
	xsshauthpasswordprompt Prompt;
	xbytesview Value;
	uint8 iMessage;
	xsshcode Code;

	if ( (pPrompt == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Prompt.Prompt = xsshPasswordText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Prompt.Language = xsshPasswordText(Value);
	if ( !xrtUtf8Valid(Prompt.Prompt, NULL) ||
		!xrtSshLanguageValid(Prompt.Language) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPrompt = Prompt;
	return XSSH_OK;
}

#endif
