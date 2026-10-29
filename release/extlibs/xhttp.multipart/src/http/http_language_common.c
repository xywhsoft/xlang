#include "../internal/xrt_http.h"



#if defined(XHTTP_FEATURE_HTTP_LANGUAGE_CORE)

/* 判断字节是否是 ASCII 字母。 */
static bool __xrtHttpLanguageAlpha(uint8 iByte)
{
	return ((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		((iByte >= (uint8)'a') && (iByte <= (uint8)'z'));
}



/* 判断字节是否是 ASCII 字母或十进制数字。 */
static bool __xrtHttpLanguageAlphaNum(uint8 iByte)
{
	return __xrtHttpLanguageAlpha(iByte) ||
		((iByte >= (uint8)'0') && (iByte <= (uint8)'9'));
}



/* 验证 RFC 4647 basic language range 使用的语言标签分段边界。 */
bool __xrtHttpLanguageTextValid(
	xstrview Text,
	bool bRange,
	bool bAllowEmpty,
	size_t* pSubtags
)
{
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		((pSubtags != NULL) &&
		 (!xrtMemRangeValid(pSubtags, sizeof(*pSubtags)) ||
		  xrtMemRangesOverlap(
			pSubtags, sizeof(*pSubtags), Text.Data, Text.Size
		  ))) ) {
		return false;
	}
	if ( Text.Size == 0 ) {
		if ( !bAllowEmpty ) {
			return false;
		}
		if ( pSubtags != NULL ) {
			*pSubtags = 0;
		}
		return true;
	}
	if ( bRange && (Text.Size == 1u) && (Text.Data[0] == '*') ) {
		if ( pSubtags != NULL ) {
			*pSubtags = 0;
		}
		return true;
	}

	while ( iOffset < Text.Size ) {
		size_t iBegin = iOffset;
		size_t iLength;

		while ( (iOffset < Text.Size) && (Text.Data[iOffset] != '-') ) {
			uint8 iByte = (uint8)Text.Data[iOffset];

			if ( ((iCount == 0) && !__xrtHttpLanguageAlpha(iByte)) ||
				((iCount != 0) && !__xrtHttpLanguageAlphaNum(iByte)) ) {
				return false;
			}
			iOffset++;
		}
		iLength = iOffset - iBegin;
		if ( (iLength == 0) || (iLength > 8u) ) {
			return false;
		}
		iCount++;
		if ( iOffset == Text.Size ) {
			break;
		}
		iOffset++;
		if ( iOffset == Text.Size ) {
			return false;
		}
	}

	if ( pSubtags != NULL ) {
		*pSubtags = iCount;
	}
	return true;
}

#endif
