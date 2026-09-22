#include "../internal/xrt_regex.h"



#if defined(XREGEX_FEATURE_REGEX_CORE)

/* 判断一个字节是否具有正则表达式语法含义。 */
static bool __xrtRegexEscapeByte(unsigned char iByte)
{
	return (iByte == (unsigned char)'\\') || (iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'+') || (iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'?') || (iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') || (iByte == (unsigned char)'|') ||
		(iByte == (unsigned char)'[') || (iByte == (unsigned char)']') ||
		(iByte == (unsigned char)'{') || (iByte == (unsigned char)'}') ||
		(iByte == (unsigned char)'^') || (iByte == (unsigned char)'$');
}



/* 校验输入并计算正则字面量的精确输出长度。 */
static bool __xrtRegexEscapeEncodedSize(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired = Text.Size;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( __xrtRegexEscapeByte((unsigned char)Text.Data[i]) ) {
			if ( iRequired == SIZE_MAX ) {
				__xrtRegexSetSizeOverflow();
				return false;
			}
			iRequired++;
		}
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtRegexSetSizeOverflow();
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 在不重叠的缓冲区中从前向后写出转义结果。 */
static void __xrtRegexEscapeForward(
	xstrview Text,
	char* sOutput,
	size_t iOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( __xrtRegexEscapeByte(iByte) ) {
			sOutput[iOutput++] = '\\';
		}
		sOutput[iOutput++] = (char)iByte;
	}
	sOutput[iOutputSize] = 0;
}



/* 从后向前写出结果，使输入和输出可以从同一地址开始。 */
static void __xrtRegexEscapeBackward(
	xstrview Text,
	char* sOutput,
	size_t iOutputSize
)
{
	size_t iInput = Text.Size;
	size_t iOutput = iOutputSize;

	sOutput[iOutputSize] = 0;
	while ( iInput != 0 ) {
		unsigned char iByte = (unsigned char)Text.Data[--iInput];

		sOutput[--iOutput] = (char)iByte;
		if ( __xrtRegexEscapeByte(iByte) ) {
			sOutput[--iOutput] = '\\';
		}
	}
}



/* 返回字面量文本转义为正则表达式后的精确长度。 */
XRT_API bool xrtRegexEscapeSize(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired;

	if ( !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( !__xrtRegexEscapeEncodedSize(Text, &iRequired) ) {
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 将字面量文本转义到调用方缓冲区。 */
XRT_API bool xrtRegexEscapeWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( !__xrtRegexEscapeEncodedSize(Text, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		 )) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtRegexSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) && ((const void*)sOutput != (const void*)Text.Data) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( (const void*)sOutput == (const void*)Text.Data ) {
		__xrtRegexEscapeBackward(Text, sOutput, iRequired);
	} else {
		__xrtRegexEscapeForward(Text, sOutput, iRequired);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的零结尾正则字面量。 */
XRT_API str xrtRegexEscape(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || ((pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	)) ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtRegexEscapeEncodedSize(Text, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtRegexEscapeForward(Text, sOutput, iRequired);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}

#endif
