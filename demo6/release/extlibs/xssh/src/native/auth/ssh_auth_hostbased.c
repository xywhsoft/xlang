#include <string.h>

#include <xrt/ssh_auth_hostbased.h>



#if defined(XSSH_FEATURE_AUTH_HOSTBASED)

/* 比较两个借用算法名。 */
static bool xsshHostBasedAlgorithmEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较借用文本与固定协议文本。 */
static bool xsshHostBasedTextEqual(
	xstrview Text,
	const char* sValue,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 SSH string 的编码长度加入总长度。 */
static xsshcode xsshHostBasedAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ||
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



/* 校验 US-ASCII DNS 主机名和标签边界。 */
bool xrtSshAuthHostNameValid(xstrview HostName)
{
	size_t iLabel = 0u;
	size_t i;
	bool bRooted;

	if ( !xrtMemRangeValid(HostName.Data, HostName.Size) ||
		(HostName.Size == 0u) ) {
		return false;
	}
	bRooted = HostName.Data[HostName.Size - 1u] == '.';
	if ( HostName.Size > (bRooted ? XSSH_AUTH_HOST_NAME_MAX :
		(XSSH_AUTH_HOST_NAME_MAX - 1u)) ) {
		return false;
	}
	for ( i = 0u; i < HostName.Size; ++i ) {
		unsigned char iByte = (unsigned char)HostName.Data[i];

		if ( iByte == '.' ) {
			if ( (iLabel == 0u) || (iLabel > 63u) ||
				(HostName.Data[i - 1u] == '-') ) {
				return false;
			}
			iLabel = 0u;
			continue;
		}
		if ( !(((iByte >= 'a') && (iByte <= 'z')) ||
			((iByte >= 'A') && (iByte <= 'Z')) ||
			((iByte >= '0') && (iByte <= '9')) ||
			((iByte == '-') && (iLabel != 0u))) ) {
			return false;
		}
		++iLabel;
	}
	return bRooted || ((iLabel != 0u) && (iLabel <= 63u) &&
		(HostName.Data[HostName.Size - 1u] != '-'));
}



/* 校验算法、公钥、主机身份以及可选签名的关系。 */
static xsshcode xsshHostBasedValidate(
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature,
	bool bSignature
)
{
	xsshpublickey Key;
	xsshsignature ParsedSignature;
	xsshcode Code;

	if ( !xrtSshNameValid(Algorithm) ||
		!xrtSshAuthHostNameValid(HostName) ||
		!xrtUtf8Valid(ClientUser, NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPublicKeyRead(PublicKey, &Key);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	if ( !bSignature ) {
		return Signature.Size == 0u ? XSSH_OK : XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSignatureRead(Signature, &ParsedSignature);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	return xsshHostBasedAlgorithmEqual(
		Algorithm,
		ParsedSignature.Algorithm
	) ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 计算 hostbased 方法字段长度并完成全部字段校验。 */
static xsshcode xsshHostBasedFieldsSize(
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature,
	bool bSignature,
	size_t* pSize
)
{
	xbytesview arrValues[5];
	size_t iTotal = 0u;
	size_t iCount = bSignature ? 5u : 4u;
	size_t i;
	xsshcode Code;

	if ( pSize == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostBasedValidate(
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		Signature,
		bSignature
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	arrValues[0] = (xbytesview){
		(const unsigned char*)Algorithm.Data,
		Algorithm.Size
	};
	arrValues[1] = PublicKey;
	arrValues[2] = (xbytesview){
		(const unsigned char*)HostName.Data,
		HostName.Size
	};
	arrValues[3] = (xbytesview){
		(const unsigned char*)ClientUser.Data,
		ClientUser.Size
	};
	arrValues[4] = Signature;
	for ( i = 0u; i < iCount; ++i ) {
		Code = xsshHostBasedAddString(arrValues[i], &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	*pSize = iTotal;
	return XSSH_OK;
}



/* 写入已经完整预留过的 hostbased 请求字段。 */
static xsshcode xsshHostBasedBodyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature,
	bool bSignature
)
{
	xsshcode Code;

	Code = xrtSshAuthRequestWrite(
		pWriter,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_HOSTBASED),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteString(
		pWriter,
		(xbytesview){
			(const unsigned char*)Algorithm.Data,
			Algorithm.Size
		}
	) != XSSH_OK) || (xrtSshWriteString(
		pWriter,
		PublicKey
	) != XSSH_OK) || (xrtSshWriteString(
		pWriter,
		(xbytesview){
			(const unsigned char*)HostName.Data,
			HostName.Size
		}
	) != XSSH_OK) || (xrtSshWriteString(
		pWriter,
		(xbytesview){
			(const unsigned char*)ClientUser.Data,
			ClientUser.Size
		}
	) != XSSH_OK) || (bSignature &&
		(xrtSshWriteString(pWriter, Signature) != XSSH_OK)) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 写入完整 hostbased 认证请求。 */
xsshcode xrtSshAuthHostBasedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature
)
{
	xbytesview arrInputs[6];
	xsshwriter Writer;
	size_t iFieldsSize;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = (xbytesview){
		(const unsigned char*)User.Data,
		User.Size
	};
	arrInputs[1] = (xbytesview){
		(const unsigned char*)Algorithm.Data,
		Algorithm.Size
	};
	arrInputs[2] = PublicKey;
	arrInputs[3] = (xbytesview){
		(const unsigned char*)HostName.Data,
		HostName.Size
	};
	arrInputs[4] = (xbytesview){
		(const unsigned char*)ClientUser.Data,
		ClientUser.Size
	};
	arrInputs[5] = Signature;
	Code = xsshHostBasedFieldsSize(
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		Signature,
		true,
		&iFieldsSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_HOSTBASED),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 6u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xsshHostBasedBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		Signature,
		true
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取完整 hostbased 请求。 */
xsshcode xrtSshAuthHostBasedRead(
	xbytesview Payload,
	xsshauthhostbased* pHostBased
)
{
	xsshauthrequest Request;
	xsshauthhostbased HostBased;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pHostBased == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshHostBasedTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshHostBasedTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_HOSTBASED,
		sizeof(XSSH_AUTH_METHOD_HOSTBASED) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	HostBased.User = Request.User;
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	HostBased.Algorithm = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &HostBased.PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	HostBased.HostName = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	HostBased.ClientUser = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &HostBased.Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshHostBasedValidate(
		HostBased.Algorithm,
		HostBased.PublicKey,
		HostBased.HostName,
		HostBased.ClientUser,
		HostBased.Signature,
		true
	);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? XSSH_ERROR_PROTOCOL : Code;
	}
	*pHostBased = HostBased;
	return XSSH_OK;
}



/* 写入 session identifier 与不含签名字段的 hostbased 请求。 */
xsshcode xrtSshAuthHostBasedSignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser
)
{
	xbytesview arrInputs[6];
	xsshwriter Writer;
	size_t iFieldsSize;
	size_t iRequestSize;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = SessionId;
	arrInputs[1] = (xbytesview){
		(const unsigned char*)User.Data,
		User.Size
	};
	arrInputs[2] = (xbytesview){
		(const unsigned char*)Algorithm.Data,
		Algorithm.Size
	};
	arrInputs[3] = PublicKey;
	arrInputs[4] = (xbytesview){
		(const unsigned char*)HostName.Data,
		HostName.Size
	};
	arrInputs[5] = (xbytesview){
		(const unsigned char*)ClientUser.Data,
		ClientUser.Size
	};
	if ( SessionId.Size == 0u ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostBasedFieldsSize(
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		(xbytesview){ NULL, 0u },
		false,
		&iFieldsSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_HOSTBASED),
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
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 6u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteString(&Writer, SessionId) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshHostBasedBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}

#endif
