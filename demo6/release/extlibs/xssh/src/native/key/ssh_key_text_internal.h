#ifndef XSSH_KEY_TEXT_INTERNAL_H
#define XSSH_KEY_TEXT_INTERNAL_H

#include <xrt/ssh_wire.h>

#include <string.h>



/* OpenSSH 文本字段只把空格和水平制表符视为字段分隔。 */
static inline bool xsshKeyTextSpace(unsigned char iCharacter)
{
	return (iCharacter == (unsigned char)' ') ||
		(iCharacter == (unsigned char)'\t');
}



/* 去掉水平空白和一个行结束符，并拒绝嵌入行、NUL 与 DEL。 */
static inline xsshcode xsshKeyTextBounds(
	xstrview Line,
	size_t* pStart,
	size_t* pEnd
)
{
	size_t iStart = 0u;
	size_t iEnd = Line.Size;
	size_t i;

	if ( !xrtMemRangeValid(Line.Data, Line.Size) ||
		(pStart == NULL) || (pEnd == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	while ( (iStart < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iStart]
	) ) {
		++iStart;
	}
	while ( (iEnd > iStart) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iEnd - 1u]
	) ) {
		--iEnd;
	}
	if ( (iEnd > iStart) && (Line.Data[iEnd - 1u] == '\n') ) {
		--iEnd;
		if ( (iEnd > iStart) && (Line.Data[iEnd - 1u] == '\r') ) {
			--iEnd;
		}
	} else if ( (iEnd > iStart) && (Line.Data[iEnd - 1u] == '\r') ) {
		--iEnd;
	}
	while ( (iEnd > iStart) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iEnd - 1u]
	) ) {
		--iEnd;
	}
	if ( (iStart == iEnd) || (Line.Data[iStart] == '#') ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = iStart; i < iEnd; ++i ) {
		unsigned char iCharacter = (unsigned char)Line.Data[i];

		if ( (iCharacter == 0u) || (iCharacter == 0x7fu) ||
			(iCharacter == (unsigned char)'\r') ||
			(iCharacter == (unsigned char)'\n') ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	*pStart = iStart;
	*pEnd = iEnd;
	return XSSH_OK;
}



/* 读取一个允许双引号与反斜杠转义的 authorized_keys 字段。 */
static inline xsshcode xsshKeyTextToken(
	xstrview Line,
	size_t iEnd,
	size_t* pPosition,
	xstrview* pToken,
	bool* pPresent
)
{
	size_t i;
	size_t iStart;
	bool bQuoted = false;

	if ( (pPosition == NULL) || (pToken == NULL) ||
		(pPresent == NULL) || (*pPosition > iEnd) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	i = *pPosition;
	while ( (i < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[i]
	) ) {
		++i;
	}
	if ( i == iEnd ) {
		*pPresent = false;
		return XSSH_OK;
	}
	iStart = i;
	while ( i < iEnd ) {
		unsigned char iCharacter = (unsigned char)Line.Data[i];

		if ( !bQuoted && xsshKeyTextSpace(iCharacter) ) {
			break;
		}
		if ( iCharacter == (unsigned char)'"' ) {
			bQuoted = !bQuoted;
			++i;
			continue;
		}
		if ( bQuoted && (iCharacter == (unsigned char)'\\') ) {
			if ( (i + 1u) == iEnd ) {
				return XSSH_ERROR_PROTOCOL;
			}
			i += 2u;
			continue;
		}
		if ( (iCharacter < 0x20u) &&
			(iCharacter != (unsigned char)'\t') ) {
			return XSSH_ERROR_PROTOCOL;
		}
		++i;
	}
	if ( bQuoted || (i == iStart) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pToken->Data = Line.Data + iStart;
	pToken->Size = i - iStart;
	*pPosition = i;
	*pPresent = true;
	return XSSH_OK;
}



/* 快速筛选标准 Base64 token；规范尾位仍由 XRT Base64 严格校验。 */
static inline bool xsshKeyTextBase64Shape(xstrview Text)
{
	size_t iPadding = 0u;
	size_t i;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		(Text.Size < 4u) || ((Text.Size % 4u) != 0u) ) {
		return false;
	}
	if ( Text.Data[Text.Size - 1u] == '=' ) {
		++iPadding;
	}
	if ( Text.Data[Text.Size - 2u] == '=' ) {
		++iPadding;
	}
	for ( i = 0u; i < (Text.Size - iPadding); ++i ) {
		unsigned char iCharacter = (unsigned char)Text.Data[i];

		if ( !(((iCharacter >= (unsigned char)'A') &&
			(iCharacter <= (unsigned char)'Z')) ||
			((iCharacter >= (unsigned char)'a') &&
			 (iCharacter <= (unsigned char)'z')) ||
			((iCharacter >= (unsigned char)'0') &&
			 (iCharacter <= (unsigned char)'9')) ||
			(iCharacter == (unsigned char)'+') ||
			(iCharacter == (unsigned char)'/')) ) {
			return false;
		}
	}
	for ( ; i < Text.Size; ++i ) {
		if ( Text.Data[i] != '=' ) {
			return false;
		}
	}
	return true;
}



/* 返回已通过标准 Base64 校验的一个字符值，填充只用于尾组。 */
static inline uint32 xsshKeyTextBase64Value(unsigned char iCharacter)
{
	if ( (iCharacter >= (unsigned char)'A') &&
		(iCharacter <= (unsigned char)'Z') ) {
		return (uint32)(iCharacter - (unsigned char)'A');
	}
	if ( (iCharacter >= (unsigned char)'a') &&
		(iCharacter <= (unsigned char)'z') ) {
		return (uint32)(iCharacter - (unsigned char)'a') + 26u;
	}
	if ( (iCharacter >= (unsigned char)'0') &&
		(iCharacter <= (unsigned char)'9') ) {
		return (uint32)(iCharacter - (unsigned char)'0') + 52u;
	}
	return iCharacter == (unsigned char)'+' ? 62u :
		(iCharacter == (unsigned char)'/' ? 63u : 0u);
}



/* 从已验证 Base64 直接读取指定解码字节，不复制其余 blob。 */
static inline unsigned char xsshKeyTextBase64At(
	xstrview Text,
	size_t iIndex
)
{
	size_t iInput = (iIndex / 3u) * 4u;
	uint32 iValue =
		(xsshKeyTextBase64Value((unsigned char)Text.Data[iInput]) << 18u) |
		(xsshKeyTextBase64Value((unsigned char)Text.Data[iInput + 1u]) << 12u) |
		(xsshKeyTextBase64Value((unsigned char)Text.Data[iInput + 2u]) << 6u) |
		xsshKeyTextBase64Value((unsigned char)Text.Data[iInput + 3u]);

	switch ( iIndex % 3u ) {
		case 0u:
			return (unsigned char)(iValue >> 16u);
		case 1u:
			return (unsigned char)(iValue >> 8u);
		default:
			return (unsigned char)iValue;
	}
}



/* 校验 Base64 blob 的首个 SSH string 与文本算法完全一致。 */
static inline bool xsshKeyTextBase64AlgorithmEqual(
	xstrview Text,
	size_t iBlobSize,
	xstrview Algorithm
)
{
	uint32 iAlgorithmSize;
	size_t i;

	if ( (Algorithm.Size > UINT32_MAX) ||
		(Algorithm.Size > (SIZE_MAX - 4u)) ||
		(iBlobSize < (4u + Algorithm.Size)) ) {
		return false;
	}
	iAlgorithmSize = (uint32)Algorithm.Size;
	if ( (xsshKeyTextBase64At(Text, 0u) !=
		 (unsigned char)(iAlgorithmSize >> 24u)) ||
		(xsshKeyTextBase64At(Text, 1u) !=
		 (unsigned char)(iAlgorithmSize >> 16u)) ||
		(xsshKeyTextBase64At(Text, 2u) !=
		 (unsigned char)(iAlgorithmSize >> 8u)) ||
		(xsshKeyTextBase64At(Text, 3u) !=
		 (unsigned char)iAlgorithmSize) ) {
		return false;
	}
	for ( i = 0u; i < Algorithm.Size; ++i ) {
		if ( xsshKeyTextBase64At(Text, 4u + i) !=
			(unsigned char)Algorithm.Data[i] ) {
			return false;
		}
	}
	return true;
}



/* 比较两个借用文本视图。 */
static inline bool xsshKeyTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 逐组比较规范 Base64 与原始字节，不建立完整编码或解码副本。 */
static inline bool xsshKeyTextBase64Equal(
	xstrview Text,
	xbytesview Data
)
{
	static const char sAlphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t iGroups = Data.Size / 3u;
	size_t iRemain = Data.Size % 3u;
	size_t iTail = iRemain == 0u ? 0u : 4u;
	size_t iExpected;
	size_t i;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		!xrtMemRangeValid(Data.Data, Data.Size) ||
		(iGroups > ((SIZE_MAX - iTail) / 4u)) ) {
		return false;
	}
	iExpected = (iGroups * 4u) + iTail;
	if ( Text.Size != iExpected ) {
		return false;
	}
	for ( i = 0u; i < iGroups; ++i ) {
		size_t iInput = i * 3u;
		size_t iOutput = i * 4u;
		uint32 iValue = ((uint32)Data.Data[iInput] << 16u) |
			((uint32)Data.Data[iInput + 1u] << 8u) |
			(uint32)Data.Data[iInput + 2u];

		if ( (Text.Data[iOutput] != sAlphabet[(iValue >> 18u) & 0x3fu]) ||
			(Text.Data[iOutput + 1u] != sAlphabet[(iValue >> 12u) & 0x3fu]) ||
			(Text.Data[iOutput + 2u] != sAlphabet[(iValue >> 6u) & 0x3fu]) ||
			(Text.Data[iOutput + 3u] != sAlphabet[iValue & 0x3fu]) ) {
			return false;
		}
	}
	if ( iRemain != 0u ) {
		size_t iInput = iGroups * 3u;
		size_t iOutput = iGroups * 4u;
		uint32 iValue = (uint32)Data.Data[iInput] << 16u;

		if ( iRemain == 2u ) {
			iValue |= (uint32)Data.Data[iInput + 1u] << 8u;
		}
		if ( (Text.Data[iOutput] != sAlphabet[(iValue >> 18u) & 0x3fu]) ||
			(Text.Data[iOutput + 1u] != sAlphabet[(iValue >> 12u) & 0x3fu]) ||
			(Text.Data[iOutput + 2u] != (iRemain == 2u ?
			 sAlphabet[(iValue >> 6u) & 0x3fu] : '=')) ||
			(Text.Data[iOutput + 3u] != '=') ) {
			return false;
		}
	}
	return true;
}

#endif
