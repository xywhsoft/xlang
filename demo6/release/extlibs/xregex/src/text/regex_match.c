#include "../internal/xrt_regex.h"



#if defined(XREGEX_FEATURE_REGEX_MATCH)

/* 清除上一轮匹配状态但保留可复用执行缓存。 */
static void __xrtRegexMatcherClear(xregexmatcher* pMatcher, xstrview Text)
{
	pMatcher->Text = Text;
	pMatcher->HasText = true;
	pMatcher->HasMatch = false;
	pMatcher->CanNext = false;
}



/* 执行一次搜索并刷新 matcher 的借用结果。 */
xregexresult __xrtRegexMatcherFind(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart,
	bool bCanNext
)
{
	static const char sEmpty[] = "";
	xregexresult Result;
	int iEngine;

	if ( (pMatcher == NULL) || !__xrtRegexViewValid(Text) || (iStart > Text.Size) ) {
		if ( (pMatcher == NULL) || (iStart > Text.Size) ) {
			__xrtRegexSetInvalidArgument();
		}
		return XREGEX_ERROR;
	}
	__xrtRegexMatcherClear(pMatcher, Text);
	iEngine = bbre_xrt_context_match(
		pMatcher->Context,
		Text.Data != NULL ? Text.Data : sEmpty,
		Text.Size,
		iStart,
		pMatcher->Captures,
		pMatcher->CaptureMatched,
		(unsigned int)pMatcher->Regex->CaptureCount
	);
	Result = __xrtRegexResult(iEngine, "match");
	if ( Result == XREGEX_MATCH ) {
		pMatcher->HasMatch = true;
		pMatcher->CanNext = bCanNext;
	}
	return Result;
}



/* 返回从当前位置开始应跳过的一个 UTF-8 标量字节数。 */
size_t __xrtRegexAdvance(xstrview Text, size_t iPosition)
{
	uint32 iScalar;
	size_t iRead;
	xutfstatus Status;

	Status = xrtUtf8Decode(
		(xstrview){ Text.Data + iPosition, Text.Size - iPosition },
		&iScalar,
		&iRead
	);
	(void)iScalar;
	return (Status == XUTF_OK) && (iRead != 0) ? iRead : 1u;
}



/* 为一个不可变编译对象创建可复用 matcher。 */
XRT_API xregexmatcher* xrtRegexMatcherCreate(xregex* pRegex)
{
	bbre_alloc Alloc;
	xregexmatcher* pMatcher;
	size_t iCaptureBytes;
	size_t iMatchedBytes;

	if ( pRegex == NULL ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( (pRegex->CaptureCount > UINT_MAX) ||
		 (pRegex->CaptureCount > (SIZE_MAX / sizeof(bbre_span))) ||
		 (pRegex->CaptureCount > (SIZE_MAX / sizeof(unsigned int))) ) {
		__xrtRegexSetSizeOverflow();
		return NULL;
	}
	iCaptureBytes = pRegex->CaptureCount * sizeof(bbre_span);
	iMatchedBytes = pRegex->CaptureCount * sizeof(unsigned int);
	if ( (iMatchedBytes > (SIZE_MAX - sizeof(*pMatcher))) ||
		 (iCaptureBytes > (SIZE_MAX - sizeof(*pMatcher) - iMatchedBytes)) ) {
		__xrtRegexSetSizeOverflow();
		return NULL;
	}
	pMatcher = (xregexmatcher*)xrtCalloc(
		1,
		sizeof(*pMatcher) + iCaptureBytes + iMatchedBytes
	);
	if ( pMatcher == NULL ) {
		return NULL;
	}
	pMatcher->Captures = (bbre_span*)(pMatcher + 1);
	pMatcher->CaptureMatched = (unsigned int*)(
		(unsigned char*)pMatcher->Captures + iCaptureBytes
	);
	pMatcher->Regex = xrtRegexRef(pRegex);
	if ( pMatcher->Regex == NULL ) {
		xrtFree(pMatcher);
		return NULL;
	}
	Alloc.user = NULL;
	Alloc.cb = __xrtRegexAlloc;
	if ( bbre_xrt_context_init_regex(&pMatcher->Context, pRegex->Engine, &Alloc) != 0 ) {
		if ( xrtGetError() == NULL ) {
			__xrtRegexSetOutOfMemory();
		}
		xrtRegexMatcherFree(pMatcher);
		return NULL;
	}
	return pMatcher;
}



/* 释放 matcher、执行缓存和持有的编译对象引用。 */
XRT_API void xrtRegexMatcherFree(xregexmatcher* pMatcher)
{
	if ( pMatcher == NULL ) {
		return;
	}
	bbre_xrt_context_destroy(pMatcher->Context);
	xrtRegexRelease(pMatcher->Regex);
	xrtFree(pMatcher);
}



/* 从字节位置开始搜索首个匹配。 */
XRT_API xregexresult xrtRegexMatcherFind(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart
)
{
	return __xrtRegexMatcherFind(pMatcher, Text, iStart, true);
}



/* 要求首个匹配恰好从指定字节位置开始。 */
XRT_API xregexresult xrtRegexMatcherAt(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart
)
{
	xregexresult Result = __xrtRegexMatcherFind(pMatcher, Text, iStart, false);

	if ( (Result == XREGEX_MATCH) &&
		 (pMatcher->Captures[0].begin != iStart) ) {
		pMatcher->HasMatch = false;
		return XREGEX_NONE;
	}
	return Result;
}



/* 要求表达式覆盖完整输入。 */
XRT_API xregexresult xrtRegexMatcherFull(
	xregexmatcher* pMatcher,
	xstrview Text
)
{
	static const char sEmpty[] = "";
	xregexresult Result;
	int iEngine;

	if ( (pMatcher == NULL) || !__xrtRegexViewValid(Text) ) {
		if ( pMatcher == NULL ) {
			__xrtRegexSetInvalidArgument();
		}
		return XREGEX_ERROR;
	}
	__xrtRegexMatcherClear(pMatcher, Text);
	iEngine = bbre_xrt_context_full_match(
		pMatcher->Context,
		Text.Data != NULL ? Text.Data : sEmpty,
		Text.Size,
		pMatcher->Captures,
		pMatcher->CaptureMatched,
		(unsigned int)pMatcher->Regex->CaptureCount
	);
	Result = __xrtRegexResult(iEngine, "full_match");
	if ( Result == XREGEX_MATCH ) {
		pMatcher->HasMatch = true;
	}
	return Result;
}



/* 继续查找下一项并保证空匹配能够向前推进。 */
XRT_API xregexresult xrtRegexMatcherNext(xregexmatcher* pMatcher)
{
	size_t iPosition;

	if ( (pMatcher == NULL) || !pMatcher->HasText ) {
		__xrtRegexSetInvalidState();
		return XREGEX_ERROR;
	}
	if ( !pMatcher->HasMatch || !pMatcher->CanNext ) {
		return XREGEX_NONE;
	}
	iPosition = pMatcher->Captures[0].end;
	if ( pMatcher->Captures[0].begin == pMatcher->Captures[0].end ) {
		if ( iPosition == pMatcher->Text.Size ) {
			pMatcher->HasMatch = false;
			pMatcher->CanNext = false;
			return XREGEX_NONE;
		}
		iPosition += __xrtRegexAdvance(pMatcher->Text, iPosition);
	}
	return __xrtRegexMatcherFind(pMatcher, pMatcher->Text, iPosition, true);
}



/* 返回 matcher 当前是否持有一次成功匹配。 */
XRT_API bool xrtRegexMatcherMatched(const xregexmatcher* pMatcher)
{
	if ( pMatcher == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	return pMatcher->HasMatch;
}



/* 返回当前输入的借用视图。 */
XRT_API xstrview xrtRegexMatcherText(const xregexmatcher* pMatcher)
{
	if ( pMatcher == NULL ) {
		__xrtRegexSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return pMatcher->Text;
}



/* 返回指定捕获的参与状态、绝对字节范围和借用文本。 */
XRT_API bool xrtRegexMatcherCapture(
	const xregexmatcher* pMatcher,
	size_t iIndex,
	xregexcapture* pCapture
)
{
	if ( (pMatcher == NULL) || (pCapture == NULL) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( iIndex >= pMatcher->Regex->CaptureCount ) {
		__xrtRegexSetRange();
		return false;
	}
	if ( !pMatcher->HasMatch ) {
		__xrtRegexSetInvalidState();
		return false;
	}
	pCapture->Matched = pMatcher->CaptureMatched[iIndex] != 0;
	pCapture->Span.Begin = pMatcher->Captures[iIndex].begin;
	pCapture->Span.End = pMatcher->Captures[iIndex].end;
	if ( pCapture->Matched ) {
		pCapture->Text.Data = pMatcher->Text.Data + pCapture->Span.Begin;
		pCapture->Text.Size = pCapture->Span.End - pCapture->Span.Begin;
	} else {
		pCapture->Text.Data = NULL;
		pCapture->Text.Size = 0;
	}
	return true;
}



/* 按名称返回当前捕获。 */
XRT_API bool xrtRegexMatcherCaptureNamed(
	const xregexmatcher* pMatcher,
	xstrview Name,
	xregexcapture* pCapture
)
{
	size_t iIndex;

	if ( pMatcher == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( !__xrtRegexViewValid(Name) ) {
		return false;
	}
	iIndex = xrtRegexCaptureIndex(pMatcher->Regex, Name);
	if ( iIndex == XRT_NPOS ) {
		__xrtRegexSetRange();
		return false;
	}
	return xrtRegexMatcherCapture(pMatcher, iIndex, pCapture);
}



/* 使用临时 matcher 执行指定操作。 */
static xregexresult __xrtRegexTest(
	xregex* pRegex,
	xstrview Text,
	bool bFull
)
{
	xregexmatcher* pMatcher = xrtRegexMatcherCreate(pRegex);
	xregexresult Result;

	if ( pMatcher == NULL ) {
		return XREGEX_ERROR;
	}
	Result = bFull ?
		xrtRegexMatcherFull(pMatcher, Text) :
		xrtRegexMatcherFind(pMatcher, Text, 0);
	xrtRegexMatcherFree(pMatcher);
	return Result;
}



/* 使用临时 matcher 搜索编译表达式。 */
XRT_API xregexresult xrtRegexTest(
	xregex* pRegex,
	xstrview Text
)
{
	return __xrtRegexTest(pRegex, Text, false);
}



/* 使用临时 matcher 检查编译表达式是否覆盖完整输入。 */
XRT_API xregexresult xrtRegexFullTest(
	xregex* pRegex,
	xstrview Text
)
{
	return __xrtRegexTest(pRegex, Text, true);
}



/* 编译表达式并使用临时 matcher 执行指定操作。 */
static xregexresult __xrtRegexMatch(
	xstrview Pattern,
	xstrview Text,
	bool bFull
)
{
	xregex* pRegex = xrtRegexCompile(Pattern);
	xregexresult Result;

	if ( pRegex == NULL ) {
		return XREGEX_ERROR;
	}
	Result = __xrtRegexTest(pRegex, Text, bFull);
	xrtRegexRelease(pRegex);
	return Result;
}



/* 编译默认表达式并执行一次搜索。 */
XRT_API xregexresult xrtRegexMatch(
	xstrview Pattern,
	xstrview Text
)
{
	return __xrtRegexMatch(Pattern, Text, false);
}



/* 编译默认表达式并执行一次完整匹配。 */
XRT_API xregexresult xrtRegexFullMatch(
	xstrview Pattern,
	xstrview Text
)
{
	return __xrtRegexMatch(Pattern, Text, true);
}

#endif
