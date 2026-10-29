#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_CORE)

/* 把 uint64 写成不带末尾零字节的十进制文本。 */
size_t __xrtMailUint64Write(char* sOutput, uint64 iValue)
{
	char sReverse[20];
	size_t iSize = 0;

	do {
		sReverse[iSize++] = (char)('0' + (iValue % UINT64_C(10)));
		iValue /= UINT64_C(10);
	} while ( iValue != 0 );
	for ( size_t i = 0; i < iSize; i++ ) {
		sOutput[i] = sReverse[iSize - i - 1u];
	}
	return iSize;
}



/* 计算换行规范化后的精确字节数。 */
static bool __xrtMailCrlfSize(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		size_t iAdd = 1;

		if ( Text.Data[i] == '\r' ) {
			iAdd = 2;
			if ( ((i + 1u) < Text.Size) && (Text.Data[i + 1u] == '\n') ) {
				i++;
			}
		} else if ( Text.Data[i] == '\n' ) {
			iAdd = 2;
		}
		if ( !__xrtMailSizeAdd(iRequired, iAdd, &iRequired) ) {
			return false;
		}
	}
	*pOutputSize = iRequired;
	return true;
}



/* 把换行写为唯一的 CRLF 表示。 */
static void __xrtMailCrlfBody(xstrview Text, char* sOutput, size_t iOutputSize)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		char iByte = Text.Data[i];

		if ( iByte == '\r' ) {
			sOutput[iOutput++] = '\r';
			sOutput[iOutput++] = '\n';
			if ( ((i + 1u) < Text.Size) && (Text.Data[i + 1u] == '\n') ) {
				i++;
			}
		} else if ( iByte == '\n' ) {
			sOutput[iOutput++] = '\r';
			sOutput[iOutput++] = '\n';
		} else {
			sOutput[iOutput++] = iByte;
		}
	}
	sOutput[iOutputSize] = 0;
}



/* 把任意常见换行规范为 CRLF。 */
XRT_API bool xrtMailCrlfWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		sOutput,
		iCapacity
	)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailCrlfSize(Text, &iRequired) ) {
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailCrlfBody(Text, sOutput, iRequired);
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 CRLF 规范文本。 */
XRT_API str xrtMailCrlf(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtMailViewValid(Text) || !__xrtMailCrlfSize(Text, &iRequired) ) {
		if ( !__xrtMailViewValid(Text) ) {
			__xrtMailSetInvalidArgument();
		}
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtMailCrlfBody(Text, sOutput, iRequired);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 判断字节是否属于 MIME bcharsnospace。 */
static bool __xrtMailBoundaryByte(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'\'') || (iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') || (iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)'_') || (iByte == (unsigned char)',') ||
		(iByte == (unsigned char)'-') || (iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'/') || (iByte == (unsigned char)':') ||
		(iByte == (unsigned char)'=') || (iByte == (unsigned char)'?');
}



/* 验证 MIME boundary。 */
XRT_API bool xrtMailBoundaryValid(xstrview Boundary)
{
	if ( !__xrtMailViewValid(Boundary) || (Boundary.Size == 0) ||
		 (Boundary.Size > XMAIL_BOUNDARY_MAX) ||
		 (Boundary.Data[Boundary.Size - 1u] == ' ') ) {
		return false;
	}
	for ( size_t i = 0; i < Boundary.Size; i++ ) {
		if ( (Boundary.Data[i] != ' ') &&
			 !__xrtMailBoundaryByte((unsigned char)Boundary.Data[i]) ) {
			return false;
		}
	}
	return true;
}

#endif
