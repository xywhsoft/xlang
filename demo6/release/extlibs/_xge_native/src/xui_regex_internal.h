#ifndef XUI_REGEX_INTERNAL_H
#define XUI_REGEX_INTERNAL_H

#include "xregex.h"

#include <string.h>



/* XUI 复用 matcher，避免搜索循环为每次尝试重复分配执行缓存。 */
typedef struct xui_regex_internal_t {
	xregex* Regex;
	xregexmatcher* Matcher;
} xui_regex_internal_t;



/* 编译表达式并创建可复用 matcher。 */
static inline xui_regex_internal_t* __xuiRegexCreate(
	const char* sPattern,
	size_t iPatternSize,
	uint32_t iFlags
)
{
	xui_regex_internal_t* pRegex;
	xregexconfig Config;
	xstrview Pattern;

	if ( (sPattern == NULL) && (iPatternSize != 0u) ) return NULL;
	pRegex = (xui_regex_internal_t*)xrtMalloc(sizeof(*pRegex));
	if ( pRegex == NULL ) return NULL;
	memset(pRegex, 0, sizeof(*pRegex));
	xrtRegexConfigInit(&Config);
	Config.Flags = iFlags & (
		XREGEX_IGNORE_CASE |
		XREGEX_MULTILINE |
		XREGEX_DOT_ALL |
		XREGEX_UNGREEDY
	);
	Pattern.Data = sPattern;
	Pattern.Size = iPatternSize;
	pRegex->Regex = xrtRegexCompileConfig(Pattern, &Config);
	if ( pRegex->Regex != NULL ) {
		pRegex->Matcher = xrtRegexMatcherCreate(pRegex->Regex);
	}
	if ( pRegex->Matcher == NULL ) {
		if ( pRegex->Regex != NULL ) xrtRegexRelease(pRegex->Regex);
		xrtFree(pRegex);
		return NULL;
	}
	return pRegex;
}



/* 释放 matcher 与编译对象。 */
static inline void __xuiRegexDestroy(xui_regex_internal_t* pRegex)
{
	if ( pRegex == NULL ) return;
	if ( pRegex->Matcher != NULL ) xrtRegexMatcherFree(pRegex->Matcher);
	if ( pRegex->Regex != NULL ) xrtRegexRelease(pRegex->Regex);
	xrtFree(pRegex);
}



/* 从指定字节位置搜索，并返回完整匹配的绝对范围。 */
static inline int __xuiRegexFindAt(
	xui_regex_internal_t* pRegex,
	const char* sText,
	size_t iTextSize,
	size_t iStart,
	xregexspan* pSpan
)
{
	xregexcapture Capture;
	xstrview Text;
	xregexresult Result;

	if ( (pRegex == NULL) || (pSpan == NULL) ||
	     ((sText == NULL) && (iTextSize != 0u)) || (iStart > iTextSize) ) return -1;
	Text.Data = sText;
	Text.Size = iTextSize;
	Result = xrtRegexMatcherFind(pRegex->Matcher, Text, iStart);
	if ( Result != XREGEX_MATCH ) return (int)Result;
	if ( !xrtRegexMatcherCapture(pRegex->Matcher, 0u, &Capture) || !Capture.Matched ) return -1;
	*pSpan = Capture.Span;
	return 1;
}



/* 返回包含完整匹配在内的捕获数量。 */
static inline size_t __xuiRegexCaptureCount(const xui_regex_internal_t* pRegex)
{
	return (pRegex != NULL) ? xrtRegexCaptureCount(pRegex->Regex) : 0u;
}



/* 读取当前匹配的捕获，并换算为相对完整匹配起点的范围。 */
static inline int __xuiRegexCaptures(
	xui_regex_internal_t* pRegex,
	xregexspan* pSpans,
	uint32_t* pMatched,
	size_t iCount
)
{
	xregexcapture Whole;
	xregexcapture Capture;
	size_t i;

	if ( (pRegex == NULL) || ((pSpans == NULL) && (iCount != 0u)) ||
	     ((pMatched == NULL) && (iCount != 0u)) ) return -1;
	if ( !xrtRegexMatcherCapture(pRegex->Matcher, 0u, &Whole) || !Whole.Matched ) return 0;
	for ( i = 0u; i < iCount; i++ ) {
		pMatched[i] = 0u;
		pSpans[i].Begin = 0u;
		pSpans[i].End = 0u;
		if ( xrtRegexMatcherCapture(pRegex->Matcher, i, &Capture) && Capture.Matched ) {
			pMatched[i] = 1u;
			pSpans[i].Begin = Capture.Span.Begin - Whole.Span.Begin;
			pSpans[i].End = Capture.Span.End - Whole.Span.Begin;
		}
	}
	return 1;
}

#endif
