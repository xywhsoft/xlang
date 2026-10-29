#include <string.h>

#include <xrt/ssh_auth_publickey.h>



#if defined(XSSH_FEATURE_AUTH_PUBLICKEY)

/* 转换文本视图为原始字节视图。 */
static xbytesview xsshPublicKeyBytes(xstrview Text)
{
	xbytesview Value;

	Value.Data = (const unsigned char*)Text.Data;
	Value.Size = Text.Size;
	return Value;
}



/* 转换原始字节视图为文本视图。 */
static xstrview xsshPublicKeyText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 比较两个借用算法名。 */
static bool xsshPublicKeyAlgorithmEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较借用文本与固定协议文本。 */
static bool xsshPublicKeyTextEqual(
	xstrview Text,
	const char* sValue,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 string 字段加入方法字段长度。 */
static xsshcode xsshPublicKeyAddString(xbytesview Value, size_t* pSize)
{
	if ( (pSize == NULL) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pSize > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验算法、公钥 blob 以及可选签名 blob 的关系。 */
static xsshcode xsshPublicKeyValidate(
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature,
	bool bSignatureField
)
{
	xsshpublickey Key;
	xsshsignature ParsedSignature;
	xsshcode Code;

	if ( !xrtSshNameValid(Algorithm) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPublicKeyRead(PublicKey, &Key);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	if ( !bSignatureField ) {
		return Signature.Size == 0u ? XSSH_OK : XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSignatureRead(Signature, &ParsedSignature);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	return xsshPublicKeyAlgorithmEqual(
		Algorithm,
		ParsedSignature.Algorithm
	) ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 写入已经完整预留过的 publickey 请求字段。 */
static xsshcode xsshPublicKeyBodyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature,
	bool bHasSignature,
	bool bSignatureField
)
{
	xsshcode Code;

	Code = xrtSshAuthRequestWrite(
		pWriter,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteBool(pWriter, bHasSignature) != XSSH_OK) ||
		(xrtSshWriteString(
			pWriter,
			xsshPublicKeyBytes(Algorithm)
		) != XSSH_OK) || (xrtSshWriteString(
			pWriter,
			PublicKey
		) != XSSH_OK) || (bSignatureField &&
		 (xrtSshWriteString(pWriter, Signature) != XSSH_OK)) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 构建 probe、带签名请求或签名原文中的请求部分。 */
static xsshcode xsshPublicKeyRequestWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature,
	bool bHasSignature,
	bool bSignatureField
)
{
	xbytesview arrInputs[4];
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = xsshPublicKeyBytes(User);
	arrInputs[1] = xsshPublicKeyBytes(Algorithm);
	arrInputs[2] = PublicKey;
	arrInputs[3] = Signature;
	Code = xsshPublicKeyValidate(
		Algorithm,
		PublicKey,
		Signature,
		bSignatureField
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshPublicKeyAddString(
		arrInputs[1],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshPublicKeyAddString(
		PublicKey,
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( bSignatureField ) {
		Code = xsshPublicKeyAddString(Signature, &iFieldsSize);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY),
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
		4u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xsshPublicKeyBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		Signature,
		bHasSignature,
		bSignatureField
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入无签名 publickey 探测。 */
xsshcode xrtSshAuthPublicKeyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
)
{
	return xsshPublicKeyRequestWrite(
		pWriter,
		User,
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		false,
		false
	);
}



/* 写入带签名 publickey 请求。 */
xsshcode xrtSshAuthPublicKeySignedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature
)
{
	return xsshPublicKeyRequestWrite(
		pWriter,
		User,
		Algorithm,
		PublicKey,
		Signature,
		true,
		true
	);
}



/* 严格读取 publickey 探测或带签名请求。 */
xsshcode xrtSshAuthPublicKeyRead(
	xbytesview Payload,
	xsshauthpublickey* pPublicKey
)
{
	xsshauthrequest Request;
	xsshauthpublickey PublicKey;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pPublicKey == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshPublicKeyTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshPublicKeyTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_PUBLICKEY,
		sizeof(XSSH_AUTH_METHOD_PUBLICKEY) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	PublicKey.User = Request.User;
	Code = xrtSshReadBool(&Reader, &PublicKey.HasSignature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PublicKey.Algorithm = xsshPublicKeyText(Value);
	Code = xrtSshReadString(&Reader, &PublicKey.PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PublicKey.Signature = (xbytesview){ NULL, 0u };
	if ( PublicKey.HasSignature ) {
		Code = xrtSshReadString(&Reader, &PublicKey.Signature);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshPublicKeyValidate(
		PublicKey.Algorithm,
		PublicKey.PublicKey,
		PublicKey.Signature,
		PublicKey.HasSignature
	);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? XSSH_ERROR_PROTOCOL : Code;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}



/* 写入 session identifier 与不含签名字段的已签名请求。 */
xsshcode xrtSshAuthPublicKeySignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
)
{
	xbytesview arrInputs[4];
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	size_t iRequestSize;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = SessionId;
	arrInputs[1] = xsshPublicKeyBytes(User);
	arrInputs[2] = xsshPublicKeyBytes(Algorithm);
	arrInputs[3] = PublicKey;
	if ( SessionId.Size == 0u ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPublicKeyValidate(
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshPublicKeyAddString(
		arrInputs[2],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshPublicKeyAddString(
		PublicKey,
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY),
		iFieldsSize,
		&iRequestSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (SessionId.Size > UINT32_MAX) ||
		(iRequestSize > (SIZE_MAX - 4u)) ||
		(SessionId.Size > (SIZE_MAX - iRequestSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 4u + SessionId.Size + iRequestSize;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		4u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteString(&Writer, SessionId) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshPublicKeyBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		true,
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入服务端 publickey 探测成功响应。 */
xsshcode xrtSshAuthPublicKeyOkWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview PublicKey
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	arrInputs[0] = xsshPublicKeyBytes(Algorithm);
	arrInputs[1] = PublicKey;
	Code = xsshPublicKeyValidate(
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshPublicKeyAddString(
		arrInputs[0],
		&iTotal
	)) != XSSH_OK) || ((Code = xsshPublicKeyAddString(
		PublicKey,
		&iTotal
	)) != XSSH_OK) ) {
		return Code;
	}
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
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_PK_OK) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, PublicKey) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取服务端 publickey 探测成功响应。 */
xsshcode xrtSshAuthPublicKeyOkRead(
	xbytesview Payload,
	xsshauthpublickeyok* pPublicKey
)
{
	xsshreader Reader;
	xsshauthpublickeyok PublicKey;
	xbytesview Value;
	uint8 iMessage;
	xsshcode Code;

	if ( (pPublicKey == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_PK_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PublicKey.Algorithm = xsshPublicKeyText(Value);
	Code = xrtSshReadString(&Reader, &PublicKey.PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshPublicKeyValidate(
		PublicKey.Algorithm,
		PublicKey.PublicKey,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? XSSH_ERROR_PROTOCOL : Code;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}

#endif
