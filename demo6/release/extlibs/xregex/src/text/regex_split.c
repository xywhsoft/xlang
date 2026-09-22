#include "../internal/xrt_regex.h"



#if defined(XREGEX_FEATURE_REGEX_SPLIT)

#define XRT_REGEX_SPLIT_FLAG_MASK \
	(XREGEX_SPLIT_CAPTURES | XREGEX_SPLIT_SKIP_EMPTY)



/* 拆分器只保存遍历位置；匹配和捕获缓存由 matcher 持有。 */
struct xregexsplitter {
	xregexmatcher* Matcher;
	xstrview Text;
	xregexsplitconfig Config;
	size_t Position;
	size_t Search;
	size_t Splits;
	size_t CaptureNext;
	bool SearchDone;
	bool Finished;
};



/* 返回输入视图内的合法子视图，空输入保持空指针。 */
static xstrview __xrtRegexSplitSlice(xstrview Text, size_t iBegin, size_t iSize)
{
	return (xstrview){ Text.Data != NULL ? Text.Data + iBegin : NULL, iSize };
}



/* 检查拆分配置的标志与保留字段。 */
static bool __xrtRegexSplitConfigValid(const xregexsplitconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Flags & ~XRT_REGEX_SPLIT_FLAG_MASK) != 0 ) {
		__xrtRegexError(
			XERR_ARGUMENT,
			XREGEX_ERROR_CONFIG,
			"split",
			"invalid regular expression split flags",
			false,
			0
		);
		return false;
	}
	for ( size_t i = 0; i < (sizeof(pConfig->Reserved) / sizeof(pConfig->Reserved[0])); i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtRegexError(
				XERR_ARGUMENT,
				XREGEX_ERROR_CONFIG,
				"split",
				"reserved regular expression split fields must be zero",
				false,
				0
			);
			return false;
		}
	}
	return true;
}



/* 把拆分器恢复到输入起点，同时复用已有 matcher 缓存。 */
static void __xrtRegexSplitterReset(xregexsplitter* pSplitter)
{
	pSplitter->Position = 0;
	pSplitter->Search = 0;
	pSplitter->Splits = 0;
	pSplitter->CaptureNext = 0;
	pSplitter->SearchDone = false;
	pSplitter->Finished = false;
}



/* 返回一个普通字段并按配置过滤空字段。 */
static bool __xrtRegexSplitterField(
	xregexsplitter* pSplitter,
	xregexsplitpart* pPart,
	size_t iBegin,
	size_t iEnd
)
{
	pPart->Text = __xrtRegexSplitSlice(
		pSplitter->Text,
		iBegin,
		iEnd - iBegin
	);
	pPart->Capture = XRT_NPOS;
	pPart->Matched = true;
	return ((pSplitter->Config.Flags & XREGEX_SPLIT_SKIP_EMPTY) == 0) ||
		(pPart->Text.Size != 0);
}



/* 返回当前分隔匹配的下一个捕获。 */
static xregexresult __xrtRegexSplitterCapture(
	xregexsplitter* pSplitter,
	xregexsplitpart* pPart
)
{
	size_t iCapture = pSplitter->CaptureNext;
	xregexcapture Capture;

	pSplitter->CaptureNext++;
	if ( pSplitter->CaptureNext >= xrtRegexCaptureCount(pSplitter->Matcher->Regex) ) {
		pSplitter->CaptureNext = 0;
	}
	if ( !xrtRegexMatcherCapture(pSplitter->Matcher, iCapture, &Capture) ) {
		return XREGEX_ERROR;
	}
	pPart->Text = Capture.Text;
	pPart->Capture = iCapture;
	pPart->Matched = Capture.Matched;
	if ( ((pSplitter->Config.Flags & XREGEX_SPLIT_SKIP_EMPTY) != 0) &&
		 (pPart->Text.Size == 0) ) {
		return XREGEX_NONE;
	}
	return XREGEX_MATCH;
}



/* 初始化不限制分隔次数且保留空字段的拆分配置。 */
XRT_API void xrtRegexSplitConfigInit(xregexsplitconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtRegexSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Limit = SIZE_MAX;
}



/* 创建借用输入的流式正则拆分器。 */
XRT_API xregexsplitter* xrtRegexSplitterCreate(
	xregex* pRegex,
	xstrview Text,
	const xregexsplitconfig* pConfig
)
{
	xregexsplitter* pSplitter;

	if ( (pRegex == NULL) || !__xrtRegexViewValid(Text) ||
		 !__xrtRegexSplitConfigValid(pConfig) ) {
		if ( pRegex == NULL ) {
			__xrtRegexSetInvalidArgument();
		}
		return NULL;
	}
	pSplitter = (xregexsplitter*)xrtCalloc(1, sizeof(*pSplitter));
	if ( pSplitter == NULL ) {
		return NULL;
	}
	pSplitter->Matcher = xrtRegexMatcherCreate(pRegex);
	if ( pSplitter->Matcher == NULL ) {
		xrtFree(pSplitter);
		return NULL;
	}
	pSplitter->Text = Text;
	pSplitter->Config = *pConfig;
	__xrtRegexSplitterReset(pSplitter);
	return pSplitter;
}



/* 释放拆分器、matcher 和持有的正则引用。 */
XRT_API void xrtRegexSplitterFree(xregexsplitter* pSplitter)
{
	if ( pSplitter == NULL ) {
		return;
	}
	xrtRegexMatcherFree(pSplitter->Matcher);
	xrtFree(pSplitter);
}



/* 返回下一字段或捕获，XREGEX_NONE 表示遍历结束。 */
XRT_API xregexresult xrtRegexSplitterNext(
	xregexsplitter* pSplitter,
	xregexsplitpart* pPart
)
{
	if ( (pSplitter == NULL) || (pPart == NULL) ) {
		__xrtRegexSetInvalidArgument();
		return XREGEX_ERROR;
	}
	for ( ;; ) {
		xregexcapture Delimiter;
		xregexresult Result;
		size_t iBegin;

		if ( pSplitter->CaptureNext != 0 ) {
			Result = __xrtRegexSplitterCapture(pSplitter, pPart);
			if ( Result != XREGEX_NONE ) {
				return Result;
			}
			continue;
		}
		if ( pSplitter->Finished ) {
			return XREGEX_NONE;
		}
		if ( pSplitter->SearchDone ||
			 (pSplitter->Splits >= pSplitter->Config.Limit) ) {
			iBegin = pSplitter->Position;
			pSplitter->Finished = true;
			if ( __xrtRegexSplitterField(
				pSplitter,
				pPart,
				iBegin,
				pSplitter->Text.Size
			) ) {
				return XREGEX_MATCH;
			}
			continue;
		}
		Result = xrtRegexMatcherFind(
			pSplitter->Matcher,
			pSplitter->Text,
			pSplitter->Search
		);
		if ( Result == XREGEX_ERROR ) {
			return Result;
		}
		if ( Result == XREGEX_NONE ) {
			pSplitter->SearchDone = true;
			continue;
		}
		if ( !xrtRegexMatcherCapture(pSplitter->Matcher, 0, &Delimiter) ) {
			return XREGEX_ERROR;
		}
		iBegin = pSplitter->Position;
		pSplitter->Position = Delimiter.Span.End;
		pSplitter->Splits++;
		if ( Delimiter.Span.Begin == Delimiter.Span.End ) {
			if ( Delimiter.Span.End == pSplitter->Text.Size ) {
				pSplitter->SearchDone = true;
			} else {
				pSplitter->Search = Delimiter.Span.End + __xrtRegexAdvance(
					pSplitter->Text,
					Delimiter.Span.End
				);
			}
		} else {
			pSplitter->Search = Delimiter.Span.End;
		}
		if ( ((pSplitter->Config.Flags & XREGEX_SPLIT_CAPTURES) != 0) &&
			 (xrtRegexCaptureCount(pSplitter->Matcher->Regex) > 1u) ) {
			pSplitter->CaptureNext = 1u;
		}
		if ( __xrtRegexSplitterField(
			pSplitter,
			pPart,
			iBegin,
			Delimiter.Span.Begin
		) ) {
			return XREGEX_MATCH;
		}
	}
}



/* 使用默认配置拆分并返回一个分配块内的零结尾字段。 */
XRT_API xstrlist* xrtRegexSplit(
	xregex* pRegex,
	xstrview Text
)
{
	xregexsplitconfig Config;
	xregexsplitter* pSplitter;
	xregexsplitpart Part;
	xregexresult Result;
	xstrlist* pList;
	size_t iCount = 0;
	size_t iDataSize = 0;
	size_t iIndex = 0;
	size_t iOffset = 0;

	xrtRegexSplitConfigInit(&Config);
	pSplitter = xrtRegexSplitterCreate(pRegex, Text, &Config);
	if ( pSplitter == NULL ) {
		return NULL;
	}
	while ( (Result = xrtRegexSplitterNext(pSplitter, &Part)) == XREGEX_MATCH ) {
		if ( (iCount == SIZE_MAX) || (Part.Text.Size == SIZE_MAX) ||
			 ((Part.Text.Size + 1u) > (SIZE_MAX - iDataSize)) ) {
			__xrtRegexSetSizeOverflow();
			xrtRegexSplitterFree(pSplitter);
			return NULL;
		}
		iCount++;
		iDataSize += Part.Text.Size + 1u;
	}
	if ( Result == XREGEX_ERROR ) {
		xrtRegexSplitterFree(pSplitter);
		return NULL;
	}
	pList = xrtStrListAlloc(iCount, iDataSize);
	if ( pList == NULL ) {
		xrtRegexSplitterFree(pSplitter);
		return NULL;
	}
	__xrtRegexSplitterReset(pSplitter);
	while ( (Result = xrtRegexSplitterNext(pSplitter, &Part)) == XREGEX_MATCH ) {
		if ( !xrtStrListWrite(pList, iIndex++, Part.Text, &iOffset) ) {
			Result = XREGEX_ERROR;
			break;
		}
	}
	xrtRegexSplitterFree(pSplitter);
	if ( Result == XREGEX_ERROR ) {
		xrtStrListFree(pList);
		return NULL;
	}
	return pList;
}

#undef XRT_REGEX_SPLIT_FLAG_MASK

#endif
