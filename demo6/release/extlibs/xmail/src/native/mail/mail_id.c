#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_ID)

#define __XMAIL_MESSAGE_ID_RANDOM 16u
#define __XMAIL_BOUNDARY_RANDOM 18u
#define __XMAIL_BOUNDARY_PREFIX "=_xmail_"
#define __XMAIL_BOUNDARY_PREFIX_SIZE 8u



/* 发布标识符语法错误。 */
static bool __xrtMailIdError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, sMessage);
	return false;
}



/* 去掉标识符外侧线性空白。 */
static xstrview __xrtMailIdTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 验证 Message-ID 的 dot-atom-text。 */
static bool __xrtMailIdDotAtom(xstrview Text, bool bUtf8)
{
	bool bDot = true;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( iByte == (unsigned char)'.' ) {
			if ( bDot ) {
				return false;
			}
			bDot = true;
			continue;
		}
		if ( (iByte < 128u) ? !__xrtMailAtext(iByte) : !bUtf8 ) {
			return false;
		}
		bDot = false;
	}
	return !bDot;
}



/* 验证 Message-ID 的 no-fold-literal。 */
static bool __xrtMailIdLiteral(xstrview Text, bool bUtf8)
{
	bool bEscape = false;

	if ( (Text.Size < 3u) || (Text.Data[0] != '[') ||
		 (Text.Data[Text.Size - 1u] != ']') ) {
		return false;
	}
	for ( size_t i = 1u; (i + 1u) < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( bEscape ) {
			bEscape = false;
			continue;
		}
		if ( iByte == (unsigned char)'\\' ) {
			bEscape = true;
			continue;
		}
		if ( (iByte == (unsigned char)'[') || (iByte == (unsigned char)']') ||
			 (iByte < 33u) || ((iByte > 126u) && !bUtf8) ) {
			return false;
		}
	}
	return !bEscape;
}



/* 验证 Message-ID 右部。 */
static bool __xrtMailIdRight(xstrview Right, bool bUtf8)
{
	return __xrtMailIdDotAtom(Right, bUtf8) ||
		__xrtMailIdLiteral(Right, bUtf8);
}



/* 解析完整 Message-ID。 */
XRT_API bool xrtMailMessageIdParse(
	xstrview Text,
	uint32 iFlags,
	xmailmessageidview* pMessageId
)
{
	xstrview Source;
	xstrview Inside;
	xstrview Left;
	xstrview Right;
	xmailmessageidview Result;
	size_t iAt = XRT_NPOS;
	bool bUtf8 = (iFlags & (uint32)XMAIL_ID_UTF8) != 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pMessageId, sizeof(*pMessageId)) ||
		 ((iFlags & ~(uint32)XMAIL_ID_UTF8) != 0) ||
		 xrtMemRangesOverlap(pMessageId, sizeof(*pMessageId), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Source = __xrtMailIdTrim(Text);
	if ( (Source.Size < 5u) || (Source.Data[0] != '<') ||
		 (Source.Data[Source.Size - 1u] != '>') ) {
		return __xrtMailIdError("mail Message-ID requires angle brackets");
	}
	Inside = __xrtMailView(Source.Data + 1u, Source.Size - 2u);
	if ( bUtf8 && !xrtUtf8Valid(Inside, NULL) ) {
		return __xrtMailIdError("mail Message-ID contains invalid UTF-8");
	}
	for ( size_t i = 0; i < Inside.Size; i++ ) {
		if ( Inside.Data[i] == '@' ) {
			iAt = i;
			break;
		}
	}
	if ( iAt == XRT_NPOS ) {
		return __xrtMailIdError("mail Message-ID has no at sign");
	}
	Left = __xrtMailView(Inside.Data, iAt);
	Right = __xrtMailView(Inside.Data + iAt + 1u, Inside.Size - iAt - 1u);
	if ( !__xrtMailIdDotAtom(Left, bUtf8) || !__xrtMailIdRight(Right, bUtf8) ) {
		return __xrtMailIdError("mail Message-ID has an invalid id-left or id-right");
	}
	Result.Source = Source;
	Result.Left = Left;
	Result.Right = Right;
	*pMessageId = Result;
	return true;
}



/* 把随机字节直接写成大写十六进制。 */
static void __xrtMailIdHex(
	const unsigned char* pRandom,
	size_t iRandomSize,
	char* sOutput
)
{
	for ( size_t i = 0; i < iRandomSize; i++ ) {
		sOutput[i * 2u] = __xrtMailHex((unsigned char)(pRandom[i] >> 4u));
		sOutput[(i * 2u) + 1u] = __xrtMailHex(pRandom[i]);
	}
}



/* 使用安全随机源写入 Message-ID。 */
XRT_API bool xrtMailMessageIdWrite(
	xstrview Right,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	unsigned char arrRandom[__XMAIL_MESSAGE_ID_RANDOM];
	size_t iRequired;

	if ( !__xrtMailViewValid(Right) || !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Right.Data, Right.Size) ||
		 ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity) ||
		   xrtMemRangesOverlap(sOutput, iCapacity, Right.Data, Right.Size))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailIdRight(Right, false) ) {
		return __xrtMailIdError("mail Message-ID id-right is invalid");
	}
	if ( !__xrtMailSizeAdd(Right.Size, 35u, &iRequired) ) {
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( !xrtSecureRandom(arrRandom, sizeof(arrRandom)) ) {
		return false;
	}
	sOutput[0] = '<';
	__xrtMailIdHex(arrRandom, sizeof(arrRandom), sOutput + 1u);
	sOutput[33u] = '@';
	memcpy(sOutput + 34u, Right.Data, Right.Size);
	sOutput[34u + Right.Size] = '>';
	sOutput[iRequired] = 0;
	memset(arrRandom, 0, sizeof(arrRandom));
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 Message-ID。 */
XRT_API str xrtMailMessageId(xstrview Right, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailMessageIdWrite(Right, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailMessageIdWrite(Right, sOutput, iRequired + 1u, &iRequired) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 使用安全随机源写入 MIME boundary。 */
XRT_API bool xrtMailBoundaryWrite(
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	unsigned char arrRandom[__XMAIL_BOUNDARY_RANDOM];
	size_t iRequired = __XMAIL_BOUNDARY_PREFIX_SIZE +
		(__XMAIL_BOUNDARY_RANDOM * 2u);

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( !xrtSecureRandom(arrRandom, sizeof(arrRandom)) ) {
		return false;
	}
	memcpy(sOutput, __XMAIL_BOUNDARY_PREFIX, __XMAIL_BOUNDARY_PREFIX_SIZE);
	__xrtMailIdHex(
		arrRandom,
		sizeof(arrRandom),
		sOutput + __XMAIL_BOUNDARY_PREFIX_SIZE
	);
	sOutput[iRequired] = 0;
	memset(arrRandom, 0, sizeof(arrRandom));
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 MIME boundary。 */
XRT_API str xrtMailBoundary(size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailBoundaryWrite(NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailBoundaryWrite(sOutput, iRequired + 1u, &iRequired) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}

#endif
