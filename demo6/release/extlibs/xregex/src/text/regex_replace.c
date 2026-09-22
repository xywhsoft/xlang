#include "../internal/xrt_regex.h"



#if defined(XREGEX_FEATURE_REGEX_REPLACE)

/* 替换模板令牌要么借用一段字面量，要么引用一个捕获索引。 */
typedef struct xrt_regex_replace_token {
	xstrview Literal;
	size_t Capture;
	bool IsCapture;
} xrt_regex_replace_token;



/* 返回输入视图内的合法子视图，空输入保持空指针。 */
static xstrview __xrtRegexReplaceSlice(xstrview Text, size_t iBegin, size_t iSize)
{
	return (xstrview){ Text.Data != NULL ? Text.Data + iBegin : NULL, iSize };
}



/* 在替换失败时恢复构建器进入函数时的长度。 */
static void __xrtRegexReplaceRollback(xstrbuf* pOutput, size_t iSize)
{
	pOutput->Size = iSize;
	if ( pOutput->Data != NULL ) {
		pOutput->Data[iSize] = 0;
	}
}



/* 设置带模板字节位置的稳定替换错误。 */
static void __xrtRegexReplacementError(cstr sMessage, size_t iOffset)
{
	__xrtRegexError(
		XERR_VALUE,
		XREGEX_ERROR_REPLACEMENT,
		"replace",
		sMessage,
		true,
		iOffset
	);
}



/* 向可选令牌数组增加一个字面量。 */
static bool __xrtRegexReplacementLiteral(
	xrt_regex_replace_token* arrToken,
	size_t iCapacity,
	size_t* pCount,
	xstrview Literal
)
{
	if ( Literal.Size == 0 ) {
		return true;
	}
	if ( *pCount >= iCapacity ) {
		__xrtRegexSetInternal();
		return false;
	}
	if ( arrToken != NULL ) {
		arrToken[*pCount].Literal = Literal;
		arrToken[*pCount].Capture = 0;
		arrToken[*pCount].IsCapture = false;
	}
	(*pCount)++;
	return true;
}



/* 向可选令牌数组增加一个捕获引用。 */
static bool __xrtRegexReplacementCapture(
	xrt_regex_replace_token* arrToken,
	size_t iCapacity,
	size_t* pCount,
	size_t iCapture
)
{
	if ( *pCount >= iCapacity ) {
		__xrtRegexSetInternal();
		return false;
	}
	if ( arrToken != NULL ) {
		arrToken[*pCount].Literal = (xstrview){ NULL, 0 };
		arrToken[*pCount].Capture = iCapture;
		arrToken[*pCount].IsCapture = true;
	}
	(*pCount)++;
	return true;
}



/* 读取十进制捕获索引并拒绝溢出或不存在的槽。 */
static bool __xrtRegexReplacementIndex(
	const xregex* pRegex,
	xstrview Replacement,
	size_t iDollar,
	size_t* pPosition,
	size_t* pCapture
)
{
	size_t iPosition = *pPosition;
	size_t iCapture = 0;

	while ( (iPosition < Replacement.Size) &&
		 (Replacement.Data[iPosition] >= '0') &&
		 (Replacement.Data[iPosition] <= '9') ) {
		size_t iDigit = (size_t)(Replacement.Data[iPosition] - '0');

		if ( iCapture > ((SIZE_MAX - iDigit) / 10u) ) {
			__xrtRegexReplacementError("replacement capture index overflows", iDollar);
			return false;
		}
		iCapture = (iCapture * 10u) + iDigit;
		iPosition++;
	}
	if ( iCapture >= pRegex->CaptureCount ) {
		__xrtRegexReplacementError("replacement capture index is out of range", iDollar);
		return false;
	}
	*pPosition = iPosition;
	*pCapture = iCapture;
	return true;
}



/* 读取花括号捕获名并映射到稳定索引。 */
static bool __xrtRegexReplacementName(
	const xregex* pRegex,
	xstrview Replacement,
	size_t iDollar,
	size_t* pPosition,
	size_t* pCapture
)
{
	size_t iBegin = *pPosition;
	size_t iEnd = iBegin;
	xstrview Name;
	size_t iCapture;

	while ( (iEnd < Replacement.Size) && (Replacement.Data[iEnd] != '}') ) {
		iEnd++;
	}
	if ( (iEnd == iBegin) || (iEnd == Replacement.Size) ) {
		__xrtRegexReplacementError("replacement capture name is incomplete", iDollar);
		return false;
	}
	Name = __xrtRegexReplaceSlice(Replacement, iBegin, iEnd - iBegin);
	iCapture = xrtRegexCaptureIndex(pRegex, Name);
	if ( iCapture == XRT_NPOS ) {
		__xrtRegexReplacementError("replacement capture name was not found", iDollar);
		return false;
	}
	*pPosition = iEnd + 1u;
	*pCapture = iCapture;
	return true;
}



/* 解析并验证替换模板，可在首遍只计算令牌数。 */
static bool __xrtRegexReplacementParse(
	const xregex* pRegex,
	xstrview Replacement,
	xrt_regex_replace_token* arrToken,
	size_t iCapacity,
	size_t* pCount
)
{
	size_t iPosition = 0;
	size_t iLiteral = 0;
	size_t iCount = 0;

	while ( iPosition < Replacement.Size ) {
		size_t iDollar;
		size_t iCapture;

		if ( Replacement.Data[iPosition] != '$' ) {
			iPosition++;
			continue;
		}
		iDollar = iPosition;
		if ( !__xrtRegexReplacementLiteral(
			arrToken,
			iCapacity,
			&iCount,
			__xrtRegexReplaceSlice(Replacement, iLiteral, iPosition - iLiteral)
		) ) {
			return false;
		}
		iPosition++;
		if ( iPosition == Replacement.Size ) {
			__xrtRegexReplacementError(
				"replacement ends with an incomplete dollar token",
				iDollar
			);
			return false;
		}
		if ( Replacement.Data[iPosition] == '$' ) {
			if ( !__xrtRegexReplacementLiteral(
				arrToken,
				iCapacity,
				&iCount,
				__xrtRegexReplaceSlice(Replacement, iPosition, 1u)
			) ) {
				return false;
			}
			iPosition++;
		} else if ( Replacement.Data[iPosition] == '{' ) {
			iPosition++;
			if ( !__xrtRegexReplacementName(
				pRegex,
				Replacement,
				iDollar,
				&iPosition,
				&iCapture
			) || !__xrtRegexReplacementCapture(
				arrToken,
				iCapacity,
				&iCount,
				iCapture
			) ) {
				return false;
			}
		} else if ( (Replacement.Data[iPosition] >= '0') &&
				 (Replacement.Data[iPosition] <= '9') ) {
			if ( !__xrtRegexReplacementIndex(
				pRegex,
				Replacement,
				iDollar,
				&iPosition,
				&iCapture
			) || !__xrtRegexReplacementCapture(
				arrToken,
				iCapacity,
				&iCount,
				iCapture
			) ) {
				return false;
			}
		} else {
			__xrtRegexReplacementError(
				"replacement contains an unknown dollar token",
				iDollar
			);
			return false;
		}
		iLiteral = iPosition;
	}
	if ( !__xrtRegexReplacementLiteral(
		arrToken,
		iCapacity,
		&iCount,
		__xrtRegexReplaceSlice(
			Replacement,
			iLiteral,
			Replacement.Size - iLiteral
		)
	) ) {
		return false;
	}
	*pCount = iCount;
	return true;
}



/* 把当前匹配按预解析模板追加到构建器。 */
static bool __xrtRegexReplacementAppend(
	const xregexmatcher* pMatcher,
	const xrt_regex_replace_token* arrToken,
	size_t iTokenCount,
	xstrbuf* pOutput
)
{
	for ( size_t i = 0; i < iTokenCount; i++ ) {
		if ( arrToken[i].IsCapture ) {
			xregexcapture Capture;

			if ( !xrtRegexMatcherCapture(pMatcher, arrToken[i].Capture, &Capture) ) {
				return false;
			}
			if ( Capture.Matched && !xrtStrBufAppend(pOutput, Capture.Text) ) {
				return false;
			}
		} else if ( !xrtStrBufAppend(pOutput, arrToken[i].Literal) ) {
			return false;
		}
	}
	return true;
}



/* 在回调失败时保留 OOM，或用稳定回调错误包装原始原因。 */
static void __xrtRegexReplaceCallbackError(void)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	if ( (pCause != NULL) && (xrtErrorKind(pCause) == XERR_MEMORY) ) {
		xrtSetErrorTake(pCause);
		return;
	}
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_VALUE;
	Desc.Code = XREGEX_ERROR_CALLBACK;
	Desc.Domain = "xrt.regex";
	Desc.Operation = "replace_callback";
	Desc.Message = "regular expression replacement callback failed";
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCause);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 调用替换回调并隔离进入回调前遗留的线程错误。 */
static bool __xrtRegexReplacementCall(
	const xregexmatcher* pMatcher,
	xregexreplacefn pReplace,
	ptr pUserData,
	xstrbuf* pOutput
)
{
	xerror* pPrevious = xrtTakeError();
	size_t iSize = pOutput->Size;
	bool bResult = pReplace(pMatcher, pOutput, pUserData);

	if ( bResult && xrtStrBufValid(pOutput) && (pOutput->Size >= iSize) ) {
		xrtClearError();
		xrtSetErrorTake(pPrevious);
		return true;
	}
	xrtErrorFree(pPrevious);
	__xrtRegexReplaceCallbackError();
	return false;
}



/* 复制可能借用输出构建器的输入，防止增长后视图失效。 */
static bool __xrtRegexReplaceOwnAlias(
	const xstrbuf* pOutput,
	xstrview Source,
	xstrview* pStable,
	str* pOwned
)
{
	bool bAlias;
	size_t iOffset;

	*pStable = Source;
	*pOwned = NULL;
	if ( !xrtStrBufAlias(pOutput, Source, &bAlias, &iOffset) ) {
		return false;
	}
	(void)iOffset;
	if ( !bAlias ) {
		return true;
	}
	*pOwned = xrtStrDupView(Source);
	if ( *pOwned == NULL ) {
		return false;
	}
	pStable->Data = *pOwned;
	return true;
}



/* 执行模板或回调替换的共同遍历，并保证失败不产生部分输出。 */
static bool __xrtRegexReplaceRun(
	xregex* pRegex,
	xstrview Text,
	size_t iLimit,
	const xrt_regex_replace_token* arrToken,
	size_t iTokenCount,
	xregexreplacefn pReplace,
	ptr pUserData,
	xstrbuf* pOutput,
	size_t* pCount
)
{
	xregexmatcher* pMatcher;
	xregexcapture Match;
	xregexresult Result;
	size_t iOutputSize = pOutput->Size;
	size_t iCopied = 0;
	size_t iCount = 0;

	pMatcher = xrtRegexMatcherCreate(pRegex);
	if ( pMatcher == NULL ) {
		return false;
	}
	Result = iLimit != 0 ?
		xrtRegexMatcherFind(pMatcher, Text, 0) : XREGEX_NONE;
	while ( (Result == XREGEX_MATCH) && (iCount < iLimit) ) {
		if ( !xrtRegexMatcherCapture(pMatcher, 0, &Match) ||
			 !xrtStrBufAppend(
				pOutput,
				__xrtRegexReplaceSlice(
					Text,
					iCopied,
					Match.Span.Begin - iCopied
				)
			 ) ) {
			Result = XREGEX_ERROR;
			break;
		}
		if ( pReplace != NULL ) {
			if ( !__xrtRegexReplacementCall(pMatcher, pReplace, pUserData, pOutput) ) {
				Result = XREGEX_ERROR;
				break;
			}
		} else if ( !__xrtRegexReplacementAppend(
			pMatcher,
			arrToken,
			iTokenCount,
			pOutput
		) ) {
			Result = XREGEX_ERROR;
			break;
		}
		iCopied = Match.Span.End;
		iCount++;
		if ( iCount < iLimit ) {
			Result = xrtRegexMatcherNext(pMatcher);
		}
	}
	if ( (Result != XREGEX_ERROR) && !xrtStrBufAppend(
		pOutput,
		__xrtRegexReplaceSlice(Text, iCopied, Text.Size - iCopied)
	) ) {
		Result = XREGEX_ERROR;
	}
	xrtRegexMatcherFree(pMatcher);
	if ( Result == XREGEX_ERROR ) {
		__xrtRegexReplaceRollback(pOutput, iOutputSize);
		return false;
	}
	if ( pCount != NULL ) {
		*pCount = iCount;
	}
	return true;
}



/* 按模板替换至构建器，SIZE_MAX 表示不限制替换次数。 */
XRT_API bool xrtRegexReplaceTo(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement,
	size_t iLimit,
	xstrbuf* pOutput,
	size_t* pCount
)
{
	xrt_regex_replace_token* arrToken = NULL;
	xstrview StableText;
	xstrview StableReplacement;
	str sOwnedText = NULL;
	str sOwnedReplacement = NULL;
	size_t iTokenCount = 0;
	bool bResult = false;

	if ( (pRegex == NULL) || !__xrtRegexViewValid(Text) ||
		 !__xrtRegexViewValid(Replacement) || !xrtStrBufValid(pOutput) ) {
		if ( pRegex == NULL ) {
			__xrtRegexSetInvalidArgument();
		}
		return false;
	}
	if ( !__xrtRegexReplaceOwnAlias(pOutput, Text, &StableText, &sOwnedText) ||
		 !__xrtRegexReplaceOwnAlias(
			pOutput,
			Replacement,
			&StableReplacement,
			&sOwnedReplacement
		 ) ) {
		goto cleanup;
	}
	if ( !__xrtRegexReplacementParse(
		pRegex,
		StableReplacement,
		NULL,
		SIZE_MAX,
		&iTokenCount
	) ) {
		goto cleanup;
	}
	if ( iTokenCount > (SIZE_MAX / sizeof(*arrToken)) ) {
		__xrtRegexSetSizeOverflow();
		goto cleanup;
	}
	if ( iTokenCount != 0 ) {
		arrToken = (xrt_regex_replace_token*)xrtMalloc(
			iTokenCount * sizeof(*arrToken)
		);
		if ( arrToken == NULL ) {
			goto cleanup;
		}
		if ( !__xrtRegexReplacementParse(
			pRegex,
			StableReplacement,
			arrToken,
			iTokenCount,
			&iTokenCount
		) ) {
			goto cleanup;
		}
	}
	bResult = __xrtRegexReplaceRun(
		pRegex,
		StableText,
		iLimit,
		arrToken,
		iTokenCount,
		NULL,
		NULL,
		pOutput,
		pCount
	);

cleanup:
	xrtFree(arrToken);
	xrtFree(sOwnedReplacement);
	xrtFree(sOwnedText);
	return bResult;
}



/* 由回调生成每次替换内容。 */
XRT_API bool xrtRegexReplaceFuncTo(
	xregex* pRegex,
	xstrview Text,
	size_t iLimit,
	xregexreplacefn pReplace,
	ptr pUserData,
	xstrbuf* pOutput,
	size_t* pCount
)
{
	xstrview StableText;
	str sOwnedText = NULL;
	bool bResult;

	if ( (pRegex == NULL) || (pReplace == NULL) ||
		 !__xrtRegexViewValid(Text) || !xrtStrBufValid(pOutput) ) {
		if ( (pRegex == NULL) || (pReplace == NULL) ) {
			__xrtRegexSetInvalidArgument();
		}
		return false;
	}
	if ( !__xrtRegexReplaceOwnAlias(pOutput, Text, &StableText, &sOwnedText) ) {
		return false;
	}
	bResult = __xrtRegexReplaceRun(
		pRegex,
		StableText,
		iLimit,
		NULL,
		0,
		pReplace,
		pUserData,
		pOutput,
		pCount
	);
	xrtFree(sOwnedText);
	return bResult;
}



/* 使用指定上限替换并返回独立字符串。 */
static str __xrtRegexReplaceString(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement,
	size_t iLimit
)
{
	xstrbuf Output;

	xrtStrBufInit(&Output);
	if ( !xrtRegexReplaceTo(
		pRegex,
		Text,
		Replacement,
		iLimit,
		&Output,
		NULL
	) ) {
		xrtStrBufFree(&Output);
		return NULL;
	}
	return xrtStrBufTake(&Output);
}



/* 替换全部匹配并返回零结尾独立字符串。 */
XRT_API str xrtRegexReplace(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement
)
{
	return __xrtRegexReplaceString(pRegex, Text, Replacement, SIZE_MAX);
}



/* 只替换第一个匹配并返回零结尾独立字符串。 */
XRT_API str xrtRegexReplaceFirst(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement
)
{
	return __xrtRegexReplaceString(pRegex, Text, Replacement, 1u);
}

#endif
