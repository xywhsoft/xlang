#include "../internal/xrt_mail_auth.h"



#if defined(XMAIL_FEATURE_SMTP_AUTH) || defined(XMAIL_FEATURE_POP3_AUTH) || \
	defined(XMAIL_FEATURE_IMAP_AUTH)

/* 验证认证字段的地址和机制专属分隔符。 */
bool __xrtMailAuthFieldValid(xstrview Text, bool bRejectSoh)
{
	if ( !__xrtMailViewValid(Text) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ||
			(bRejectSoh && (iByte == 1u)) ) {
			return false;
		}
	}
	return true;
}



/* 清零并释放凭据或编码后的临时缓冲。 */
void __xrtMailAuthFree(char* sText, size_t iSize)
{
	if ( sText == NULL ) {
		return;
	}
	xrtSecureZero(sText, iSize);
	xrtFree(sText);
}



/* 创建规范 Base64 凭据文本。 */
char* __xrtMailAuthEncode(
	const void* pData,
	size_t iSize,
	size_t* pEncodedSize
)
{
	char* sEncoded;
	size_t iEncoded;

	if ( !xrtMemRangeValid(pData, iSize) ||
		!xrtMemRangeValid(pEncodedSize, sizeof(*pEncodedSize)) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !xrtBase64Encode(
		pData,
		iSize,
		NULL,
		0,
		&iEncoded,
		NULL
	) || (iEncoded >= SIZE_MAX) ) {
		return NULL;
	}
	sEncoded = (char*)xrtMalloc(iEncoded + 1u);
	if ( sEncoded == NULL ) {
		return NULL;
	}
	if ( !xrtBase64Encode(
		pData,
		iSize,
		sEncoded,
		iEncoded + 1u,
		&iEncoded,
		NULL
	) ) {
		__xrtMailAuthFree(sEncoded, iEncoded + 1u);
		return NULL;
	}
	*pEncodedSize = iEncoded;
	return sEncoded;
}



/* 创建 SASL PLAIN 的 Base64 初始响应。 */
char* __xrtMailAuthPlain(
	xstrview AuthorizationId,
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
)
{
	char* sPlain;
	char* sEncoded;
	size_t iPlain;
	size_t iOffset = 0;

	if ( !__xrtMailSizeAdd(AuthorizationId.Size, Username.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, Secret.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 2u, &iPlain) ) {
		return NULL;
	}
	sPlain = (char*)xrtMalloc(iPlain);
	if ( sPlain == NULL ) {
		return NULL;
	}
	if ( AuthorizationId.Size != 0 ) {
		memcpy(sPlain, AuthorizationId.Data, AuthorizationId.Size);
		iOffset = AuthorizationId.Size;
	}
	sPlain[iOffset++] = 0;
	memcpy(sPlain + iOffset, Username.Data, Username.Size);
	iOffset += Username.Size;
	sPlain[iOffset++] = 0;
	memcpy(sPlain + iOffset, Secret.Data, Secret.Size);
	sEncoded = __xrtMailAuthEncode(sPlain, iPlain, pEncodedSize);
	__xrtMailAuthFree(sPlain, iPlain);
	return sEncoded;
}



/* 创建 XOAUTH2 bearer 的 Base64 初始响应。 */
char* __xrtMailAuthXoauth2(
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
)
{
	static const char sUser[] = "user=";
	static const char sBearer[] = "\x01" "auth=Bearer ";
	char* sPlain;
	char* sEncoded;
	size_t iPlain = sizeof(sUser) - 1u;
	size_t iOffset = 0;

	if ( !__xrtMailSizeAdd(iPlain, Username.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, sizeof(sBearer) - 1u, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, Secret.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 2u, &iPlain) ) {
		return NULL;
	}
	sPlain = (char*)xrtMalloc(iPlain);
	if ( sPlain == NULL ) {
		return NULL;
	}
	memcpy(sPlain + iOffset, sUser, sizeof(sUser) - 1u);
	iOffset += sizeof(sUser) - 1u;
	memcpy(sPlain + iOffset, Username.Data, Username.Size);
	iOffset += Username.Size;
	memcpy(sPlain + iOffset, sBearer, sizeof(sBearer) - 1u);
	iOffset += sizeof(sBearer) - 1u;
	memcpy(sPlain + iOffset, Secret.Data, Secret.Size);
	iOffset += Secret.Size;
	sPlain[iOffset++] = 1;
	sPlain[iOffset] = 1;
	sEncoded = __xrtMailAuthEncode(sPlain, iPlain, pEncodedSize);
	__xrtMailAuthFree(sPlain, iPlain);
	return sEncoded;
}



/* 计算并写出 GS2 authzid，其中逗号和等号使用规范转义。 */
static bool __xrtMailAuthGs2Id(
	xstrview AuthorizationId,
	char* sOutput,
	size_t* pSize
)
{
	size_t iRequired = 0;
	size_t iOffset = 0;

	for ( size_t i = 0; i < AuthorizationId.Size; i++ ) {
		size_t iByteSize = (AuthorizationId.Data[i] == ',') ||
			(AuthorizationId.Data[i] == '=') ? 3u : 1u;

		if ( !__xrtMailSizeAdd(iRequired, iByteSize, &iRequired) ) {
			return false;
		}
	}
	if ( sOutput != NULL ) {
		for ( size_t i = 0; i < AuthorizationId.Size; i++ ) {
			if ( AuthorizationId.Data[i] == ',' ) {
				memcpy(sOutput + iOffset, "=2C", 3u);
				iOffset += 3u;
			} else if ( AuthorizationId.Data[i] == '=' ) {
				memcpy(sOutput + iOffset, "=3D", 3u);
				iOffset += 3u;
			} else {
				sOutput[iOffset++] = AuthorizationId.Data[i];
			}
		}
	}
	*pSize = iRequired;
	return true;
}



/* 创建 RFC 7628 OAUTHBEARER 的 Base64 初始响应。 */
char* __xrtMailAuthOauthBearer(
	xstrview AuthorizationId,
	xstrview Secret,
	size_t* pEncodedSize
)
{
	static const char sHeader[] = "n,a=";
	static const char sBearer[] = "\x01" "auth=Bearer ";
	char* sPlain;
	char* sEncoded;
	size_t iAuthzid;
	size_t iPlain = sizeof(sHeader) - 1u;
	size_t iOffset = 0;

	if ( !__xrtMailAuthGs2Id(AuthorizationId, NULL, &iAuthzid) ||
		!__xrtMailSizeAdd(iPlain, iAuthzid, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 1u, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, sizeof(sBearer) - 1u, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, Secret.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 2u, &iPlain) ) {
		return NULL;
	}
	sPlain = (char*)xrtMalloc(iPlain);
	if ( sPlain == NULL ) {
		return NULL;
	}
	memcpy(sPlain + iOffset, sHeader, sizeof(sHeader) - 1u);
	iOffset += sizeof(sHeader) - 1u;
	if ( !__xrtMailAuthGs2Id(
		AuthorizationId,
		sPlain + iOffset,
		&iAuthzid
	) ) {
		__xrtMailAuthFree(sPlain, iPlain);
		return NULL;
	}
	iOffset += iAuthzid;
	sPlain[iOffset++] = ',';
	memcpy(sPlain + iOffset, sBearer, sizeof(sBearer) - 1u);
	iOffset += sizeof(sBearer) - 1u;
	memcpy(sPlain + iOffset, Secret.Data, Secret.Size);
	iOffset += Secret.Size;
	sPlain[iOffset++] = 1;
	sPlain[iOffset] = 1;
	sEncoded = __xrtMailAuthEncode(sPlain, iPlain, pEncodedSize);
	__xrtMailAuthFree(sPlain, iPlain);
	return sEncoded;
}

#endif
