#include <string.h>

#include <xrt/ssh_auth_message.h>



#if defined(XSSH_FEATURE_AUTH_MESSAGE)

/* 转换文本视图为原始字节视图。 */
static xbytesview xsshAuthBytes(xstrview Text)
{
	xbytesview Value;

	Value.Data = (const unsigned char*)Text.Data;
	Value.Size = Text.Size;
	return Value;
}



/* 转换原始字节视图为不要求零结尾的文本视图。 */
static xstrview xsshAuthText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 判断两个文本视图是否完全相同。 */
static bool xsshAuthTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 将一个 SSH string 的编码长度加入总长度。 */
static xsshcode xsshAuthAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		((Value.Data == NULL) && (Value.Size != 0u)) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验整条输出及其输入重叠，再返回可原子提交的 writer 副本。 */
static xsshcode xsshAuthPrepare(
	xsshwriter* pWriter,
	size_t iTotal,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pCopy
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (pWriter == NULL) || (pCopy == NULL) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		pInputs,
		iInputCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	*pCopy = Writer;
	return XSSH_OK;
}



/* 初始化 reader 并消费指定 USERAUTH 消息号。 */
static xsshcode xsshAuthReader(
	xbytesview Payload,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pReader == NULL) || !xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 计算通用认证请求及其原始方法字段的总长度。 */
xsshcode xrtSshAuthRequestSize(
	xstrview User,
	xstrview Service,
	xstrview Method,
	size_t iFieldsSize,
	size_t* pSize
)
{
	xbytesview Values[3];
	size_t iTotal = 1u;
	xsshcode Code;

	Values[0] = xsshAuthBytes(User);
	Values[1] = xsshAuthBytes(Service);
	Values[2] = xsshAuthBytes(Method);
	if ( (pSize == NULL) || !xrtUtf8Valid(User, NULL) ||
		!xrtSshNameValid(Service) || !xrtSshNameValid(Method) ||
		xrtMemRangesOverlap(pSize, sizeof(*pSize), User.Data, User.Size) ||
		xrtMemRangesOverlap(pSize, sizeof(*pSize), Service.Data, Service.Size) ||
		xrtMemRangesOverlap(pSize, sizeof(*pSize), Method.Data, Method.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshAuthAddString(Values[0], &iTotal)) != XSSH_OK) ||
		((Code = xsshAuthAddString(Values[1], &iTotal)) != XSSH_OK) ||
		((Code = xsshAuthAddString(Values[2], &iTotal)) != XSSH_OK) ) {
		return Code;
	}
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize = iTotal + iFieldsSize;
	return XSSH_OK;
}



/* 写入可保留未知方法字段的通用认证请求。 */
xsshcode xrtSshAuthRequestWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Service,
	xstrview Method,
	xbytesview Fields
)
{
	xbytesview arrInputs[4];
	xsshwriter Writer;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = xsshAuthBytes(User);
	arrInputs[1] = xsshAuthBytes(Service);
	arrInputs[2] = xsshAuthBytes(Method);
	arrInputs[3] = Fields;
	Code = xrtSshAuthRequestSize(
		User,
		Service,
		Method,
		Fields.Size,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshAuthPrepare(pWriter, iTotal, arrInputs, 4u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_REQUEST) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Fields) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取通用认证请求并借用方法专用字段。 */
xsshcode xrtSshAuthRequestRead(
	xbytesview Payload,
	xsshauthrequest* pRequest
)
{
	xsshreader Reader;
	xsshauthrequest Request;
	xbytesview Value;
	xsshcode Code;

	if ( pRequest == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_REQUEST, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.User = xsshAuthText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.Service = xsshAuthText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.Method = xsshAuthText(Value);
	if ( !xrtUtf8Valid(Request.User, NULL) ||
		!xrtSshNameValid(Request.Service) ||
		!xrtSshNameValid(Request.Method) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&Request.Fields
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pRequest = Request;
	return XSSH_OK;
}



/* 写入标准 ssh-connection none 探测。 */
xsshcode xrtSshAuthNoneWrite(xsshwriter* pWriter, xstrview User)
{
	return xrtSshAuthRequestWrite(
		pWriter,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_NONE),
		(xbytesview){ NULL, 0u }
	);
}



/* 严格读取标准 ssh-connection none 探测。 */
xsshcode xrtSshAuthNoneRead(xbytesview Payload, xstrview* pUser)
{
	xsshauthrequest Request;
	xsshcode Code;

	if ( pUser == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshAuthTextEqual(
		Request.Service,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION)
	) || !xsshAuthTextEqual(
		Request.Method,
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_NONE)
	) || (Request.Fields.Size != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pUser = Request.User;
	return XSSH_OK;
}



/* 写入认证失败及后续可用方法。 */
xsshcode xrtSshAuthFailureWrite(
	xsshwriter* pWriter,
	xstrview Methods,
	bool bPartialSuccess
)
{
	xbytesview Value = xsshAuthBytes(Methods);
	xsshwriter Writer;
	size_t iTotal = 2u;
	xsshcode Code;

	if ( !xrtSshNameListValid(Methods) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthAddString(Value, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshAuthPrepare(pWriter, iTotal, &Value, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_FAILURE) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Value) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bPartialSuccess) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取认证失败及后续可用方法。 */
xsshcode xrtSshAuthFailureRead(
	xbytesview Payload,
	xsshauthfailure* pFailure
)
{
	xsshreader Reader;
	xsshauthfailure Failure;
	xbytesview Methods;
	xsshcode Code;

	if ( pFailure == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_FAILURE, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Methods);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Failure.Methods = xsshAuthText(Methods);
	if ( !xrtSshNameListValid(Failure.Methods) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBool(&Reader, &Failure.PartialSuccess);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pFailure = Failure;
	return XSSH_OK;
}



/* 写入无字段认证成功消息。 */
xsshcode xrtSshAuthSuccessWrite(xsshwriter* pWriter)
{
	xsshwriter Writer;
	xsshcode Code;

	Code = xsshAuthPrepare(pWriter, 1u, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_SUCCESS) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取无字段认证成功消息。 */
xsshcode xrtSshAuthSuccessRead(xbytesview Payload)
{
	xsshreader Reader;
	xsshcode Code;

	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_SUCCESS, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshReaderRemaining(&Reader) == 0u ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 写入 UTF-8 认证横幅和可选 ASCII language tag。 */
xsshcode xrtSshAuthBannerWrite(
	xsshwriter* pWriter,
	xstrview Message,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	arrInputs[0] = xsshAuthBytes(Message);
	arrInputs[1] = xsshAuthBytes(Language);
	if ( !xrtUtf8Valid(Message, NULL) ||
		!xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshAuthAddString(arrInputs[0], &iTotal)) != XSSH_OK) ||
		((Code = xsshAuthAddString(arrInputs[1], &iTotal)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshAuthPrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_BANNER) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 UTF-8 认证横幅和 ASCII language tag。 */
xsshcode xrtSshAuthBannerRead(
	xbytesview Payload,
	xsshauthbanner* pBanner
)
{
	xsshreader Reader;
	xsshauthbanner Banner;
	xbytesview Value;
	xsshcode Code;

	if ( pBanner == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_BANNER, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Banner.Message = xsshAuthText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Banner.Language = xsshAuthText(Value);
	if ( !xrtUtf8Valid(Banner.Message, NULL) ||
		!xrtSshLanguageValid(Banner.Language) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pBanner = Banner;
	return XSSH_OK;
}

#endif
