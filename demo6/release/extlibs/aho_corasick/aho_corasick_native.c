#define XRT_MODULE_IO_BUFFER
#include "include/xrt_aho_corasick.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XAHO_EXPORT __declspec(dllexport)
#else
	#define XAHO_EXPORT
#endif



typedef struct xaho_native_result {
	xrtahoresult* Result;
	xrtaho* Automaton;
	xrtahomatch Match;
	bool Single;
} xaho_native_result;



typedef struct xaho_native_scan {
	xrtahostate* State;
	xstrview Text;
	str Owned;
	size_t Offset;
	xrtahomatch Match;
	bool Finished;
} xaho_native_scan;



typedef xaho_native_scan xaho_native_stream;



/* 返回当前结构化错误的消息。 */
XAHO_EXPORT const char* xac_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	return sMessage != NULL && sMessage[0] != 0 ?
		sMessage : "Aho-Corasick operation failed";
}



/* 创建 XLang 自动机句柄。 */
XAHO_EXPORT void* xac_create(
	int bIgnoreAsciiCase,
	uint64 iMaxPatterns,
	uint64 iMaxPatternBytes,
	uint64 iMaxTotalPatternBytes,
	uint64 iMaxNodes,
	uint64 iMaxMatches,
	uint64 iMaxInputBytes,
	uint64 iMaxOutputBytes
)
{
	xrtahooptions Options;

	xrtAhoOptionsInit(&Options);
	Options.bIgnoreAsciiCase = bIgnoreAsciiCase != 0;
	Options.iMaxPatterns = iMaxPatterns;
	Options.iMaxPatternBytes = iMaxPatternBytes;
	Options.iMaxTotalPatternBytes = iMaxTotalPatternBytes;
	Options.iMaxNodes = iMaxNodes;
	Options.iMaxMatches = iMaxMatches;
	Options.iMaxInputBytes = iMaxInputBytes;
	Options.iMaxOutputBytes = iMaxOutputBytes;
	return xrtAhoCreate(&Options);
}



/* 释放 XLang 自动机句柄。 */
XAHO_EXPORT void xac_destroy(void* pHandle)
{
	xrtAhoDestroy((xrtaho*)pHandle);
}



/* 添加明确长度模式。 */
XAHO_EXPORT int xac_add_pattern(
	void* pHandle,
	const char* sPattern,
	size_t iSize
)
{
	xstrview Pattern = { sPattern, iSize };

	return xrtAhoAdd((xrtaho*)pHandle, Pattern) ? 1 : 0;
}



/* 构建自动机。 */
XAHO_EXPORT int xac_build(void* pHandle)
{
	return xrtAhoBuild((xrtaho*)pHandle) ? 1 : 0;
}



/* 返回构建状态。 */
XAHO_EXPORT int xac_is_built(void* pHandle)
{
	return xrtAhoBuilt((const xrtaho*)pHandle) ? 1 : 0;
}



/* 返回模式数量，超出 XLang int 范围时失败。 */
XAHO_EXPORT int64 xac_pattern_count(void* pHandle)
{
	size_t iCount = xrtAhoPatternCount((const xrtaho*)pHandle);

	if ( iCount > (size_t)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick pattern count exceeds int range");
		return -1;
	}
	return (int64)iCount;
}



/* 返回指定模式的独立 Buffer。 */
XAHO_EXPORT void* xac_pattern(void* pHandle, int64 iIndex)
{
	xstrview Pattern;
	xbytesview Data;

	if ( iIndex < 0 ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick pattern index is out of range");
		return NULL;
	}
	Pattern = xrtAhoPattern((const xrtaho*)pHandle, (size_t)iIndex);
	if ( Pattern.Data == NULL ) { return NULL; }
	Data.Data = (const unsigned char*)Pattern.Data;
	Data.Size = Pattern.Size;
	return xrtBufferFrom(Data);
}



/* 创建普通结果包装。 */
static xaho_native_result* xac_wrap_result(xrtahoresult* pResult)
{
	xaho_native_result* pHandle;

	if ( pResult == NULL ) { return NULL; }
	pHandle = (xaho_native_result*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtAhoResultDestroy(pResult);
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_MEMORY, "Aho-Corasick result allocation failed");
		return NULL;
	}
	pHandle->Result = pResult;
	return pHandle;
}



/* 搜索全部匹配。 */
XAHO_EXPORT void* xac_search(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xstrview Text = { sText, iSize };

	return xac_wrap_result(xrtAhoSearch((xrtaho*)pHandle, Text));
}



/* 搜索左优先最长非重叠匹配。 */
XAHO_EXPORT void* xac_search_non_overlapping(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xstrview Text = { sText, iSize };

	return xac_wrap_result(
		xrtAhoSearchNonOverlapping((xrtaho*)pHandle, Text));
}



/* 查找第一个匹配，不创建完整结果数组。 */
XAHO_EXPORT void* xac_find(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xrtaho* pAutomaton = (xrtaho*)pHandle;
	xstrview Text = { sText, iSize };
	xaho_native_result* pResult;
	xrtahomatch Match;
	bool bFound;

	if ( !xrtAhoFind(pAutomaton, Text, &Match, &bFound) ) {
		return NULL;
	}
	if ( !bFound ) {
		pResult = (xaho_native_result*)xrtCalloc(1u, sizeof(*pResult));
		if ( pResult == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.aho_corasick",
				XRT_AHO_ERROR_MEMORY, "Aho-Corasick result allocation failed");
			return NULL;
		}
		pResult->Single = true;
		return pResult;
	}
	pResult = (xaho_native_result*)xrtCalloc(1u, sizeof(*pResult));
	if ( pResult == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_MEMORY, "Aho-Corasick result allocation failed");
		return NULL;
	}
	pResult->Match = Match;
	if ( xrtAhoRef(pAutomaton) == NULL ) {
		xrtFree(pResult);
		return NULL;
	}
	pResult->Automaton = pAutomaton;
	pResult->Single = true;
	return pResult;
}



/* 返回结果数量。 */
XAHO_EXPORT int64 xac_result_count(void* pHandle)
{
	xaho_native_result* pResult = (xaho_native_result*)pHandle;
	size_t iCount;

	if ( pResult == NULL ) { return 0; }
	iCount = pResult->Single ? (pResult->Automaton != NULL ? 1u : 0u) :
		xrtAhoResultCount(pResult->Result);
	if ( iCount > (size_t)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick result count exceeds int range");
		return -1;
	}
	return (int64)iCount;
}



/* 读取结果并统一单项、数组两种包装。 */
static bool xac_result_at(
	xaho_native_result* pResult,
	size_t iIndex,
	xrtahomatch* pMatch
)
{
	if ( pResult == NULL ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_ARGUMENT, "Aho-Corasick result is null");
		return false;
	}
	if ( pResult->Single ) {
		if ( pResult->Automaton == NULL || iIndex != 0u ) {
			xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
				XRT_AHO_ERROR_RANGE, "Aho-Corasick result index is out of range");
			return false;
		}
		*pMatch = pResult->Match;
		return true;
	}
	return xrtAhoResultAt(pResult->Result, iIndex, pMatch);
}



/* 返回包含匹配模式的独立 Buffer。 */
XAHO_EXPORT void* xac_result_pattern(void* pHandle, int64 iIndex)
{
	xrtahomatch Match;
	xbytesview Data;

	if ( iIndex < 0 || !xac_result_at((xaho_native_result*)pHandle,
		(size_t)iIndex, &Match) ) { return NULL; }
	Data.Data = (const unsigned char*)Match.Pattern.Data;
	Data.Size = Match.Pattern.Size;
	return xrtBufferFrom(Data);
}



/* 返回匹配开始位置。 */
XAHO_EXPORT int64 xac_result_start(void* pHandle, int64 iIndex)
{
	xrtahomatch Match;

	if ( iIndex < 0 || !xac_result_at((xaho_native_result*)pHandle,
		(size_t)iIndex, &Match) || Match.iStart > (uint64)INT64_MAX ) {
		return -1;
	}
	return (int64)Match.iStart;
}



/* 返回匹配结束位置。 */
XAHO_EXPORT int64 xac_result_end(void* pHandle, int64 iIndex)
{
	xrtahomatch Match;

	if ( iIndex < 0 || !xac_result_at((xaho_native_result*)pHandle,
		(size_t)iIndex, &Match) || Match.iEnd > (uint64)INT64_MAX ) {
		return -1;
	}
	return (int64)Match.iEnd;
}



/* 释放结果包装。 */
XAHO_EXPORT void xac_result_destroy(void* pHandle)
{
	xaho_native_result* pResult = (xaho_native_result*)pHandle;

	if ( pResult == NULL ) { return; }
	if ( pResult->Single ) {
		xrtAhoDestroy(pResult->Automaton);
	}
	else {
		xrtAhoResultDestroy(pResult->Result);
	}
	xrtFree(pResult);
}



/* 三态返回 contains，避免 false 与失败混淆。 */
XAHO_EXPORT int xac_contains(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xstrview Text = { sText, iSize };
	bool bContains;

	if ( !xrtAhoContains((xrtaho*)pHandle, Text, &bContains) ) { return -1; }
	return bContains ? 1 : 0;
}



/* 返回匹配数量，负数表示失败。 */
XAHO_EXPORT int64 xac_count(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xstrview Text = { sText, iSize };
	uint64 iCount;

	if ( !xrtAhoCount((xrtaho*)pHandle, Text, &iCount) ) { return -1; }
	if ( iCount > (uint64)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick match count exceeds int range");
		return -1;
	}
	return (int64)iCount;
}



/* 把核心返回的独立字符串交给 XLang Buffer。 */
static xbuffer* xac_take_string(str sText, size_t iSize)
{
	bytes pData = (bytes)sText;
	xbuffer* pBuffer;

	if ( sText == NULL ) { return NULL; }
	pBuffer = xrtBufferCreateTake(&pData, iSize, iSize + 1u);
	if ( pBuffer == NULL ) { xrtFree(sText); }
	return pBuffer;
}



/* 返回明确字节范围的 Buffer。 */
XAHO_EXPORT void* xac_text_range(
	const char* sText,
	size_t iTextSize,
	size_t iStart,
	size_t iCount
)
{
	xbytesview Data;

	if ( sText == NULL || iStart > iTextSize || iCount > (iTextSize - iStart) ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick text range is out of bounds");
		return NULL;
	}
	Data.Data = (const unsigned char*)sText + iStart;
	Data.Size = iCount;
	return xrtBufferFrom(Data);
}



/* 固定替换并返回 Buffer。 */
XAHO_EXPORT void* xac_replace(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	const char* sReplacement,
	size_t iReplacementSize
)
{
	xstrview Text = { sText, iTextSize };
	xstrview Replacement = { sReplacement, iReplacementSize };
	size_t iOutputSize;
	str sOutput;

	sOutput = xrtAhoReplace((xrtaho*)pHandle, Text,
		Replacement, &iOutputSize);
	return xac_take_string(sOutput, iOutputSize);
}



/* Unicode 标量遮盖并返回 Buffer。 */
XAHO_EXPORT void* xac_mask(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	const char* sMask,
	size_t iMaskSize
)
{
	xstrview Text = { sText, iTextSize };
	xstrview Mask = { sMask, iMaskSize };
	size_t iOutputSize;
	str sOutput;

	sOutput = xrtAhoMask((xrtaho*)pHandle, Text, Mask, &iOutputSize);
	return xac_take_string(sOutput, iOutputSize);
}



/* 高亮并返回 Buffer。 */
XAHO_EXPORT void* xac_highlight(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	const char* sOpen,
	size_t iOpenSize,
	const char* sClose,
	size_t iCloseSize
)
{
	xstrview Text = { sText, iTextSize };
	xstrview Open = { sOpen, iOpenSize };
	xstrview Close = { sClose, iCloseSize };
	size_t iOutputSize;
	str sOutput;

	sOutput = xrtAhoHighlight((xrtaho*)pHandle, Text,
		Open, Close, &iOutputSize);
	return xac_take_string(sOutput, iOutputSize);
}



/* 保存一个增量扫描匹配并主动暂停。 */
static bool xac_scan_capture(const xrtahomatch* pMatch, ptr pUserData)
{
	xaho_native_scan* pScan = (xaho_native_scan*)pUserData;

	pScan->Match = *pMatch;
	return false;
}



/* 创建借用 XLang 字符串的拉取式扫描包装。 */
XAHO_EXPORT void* xac_scan_begin(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xaho_native_scan* pScan;

	if ( sText == NULL && iSize != 0u ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_ARGUMENT, "Aho-Corasick text view is invalid");
		return NULL;
	}
	pScan = (xaho_native_scan*)xrtCalloc(1u, sizeof(*pScan));
	if ( pScan == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_MEMORY, "Aho-Corasick scan allocation failed");
		return NULL;
	}
	pScan->State = xrtAhoStateCreate((xrtaho*)pHandle);
	if ( pScan->State == NULL ) {
		xrtFree(pScan);
		return NULL;
	}
	pScan->Text.Data = sText;
	pScan->Text.Size = iSize;
	return pScan;
}



/* 拉取下一个匹配：1 为命中，0 为结束，-1 为失败。 */
XAHO_EXPORT int xac_scan_next(void* pHandle)
{
	xaho_native_scan* pScan = (xaho_native_scan*)pHandle;
	xstrview Chunk;
	size_t iConsumed = 0u;
	xrtahoscanstatus iStatus;

	if ( pScan == NULL ) { return -1; }
	if ( pScan->Finished ) { return 0; }
	Chunk.Data = pScan->Text.Size != 0u ?
		pScan->Text.Data + pScan->Offset : NULL;
	Chunk.Size = pScan->Text.Size - pScan->Offset;
	iStatus = xrtAhoStateFeed(pScan->State, Chunk,
		xac_scan_capture, pScan, &iConsumed);
	pScan->Offset += iConsumed;
	if ( iStatus == XRT_AHO_SCAN_ERROR ) { return -1; }
	if ( iStatus == XRT_AHO_SCAN_STOPPED ) { return 1; }
	xrtFree(pScan->Owned);
	pScan->Owned = NULL;
	pScan->Text.Data = NULL;
	pScan->Text.Size = 0u;
	pScan->Offset = 0u;
	pScan->Finished = true;
	return 0;
}



/* 返回当前扫描匹配开始位置。 */
XAHO_EXPORT int64 xac_scan_start(void* pHandle)
{
	xaho_native_scan* pScan = (xaho_native_scan*)pHandle;

	return pScan != NULL && pScan->Match.iStart <= (uint64)INT64_MAX ?
		(int64)pScan->Match.iStart : -1;
}



/* 返回当前扫描匹配结束位置。 */
XAHO_EXPORT int64 xac_scan_end_pos(void* pHandle)
{
	xaho_native_scan* pScan = (xaho_native_scan*)pHandle;

	return pScan != NULL && pScan->Match.iEnd <= (uint64)INT64_MAX ?
		(int64)pScan->Match.iEnd : -1;
}



/* 返回包含当前扫描模式的独立 Buffer。 */
XAHO_EXPORT void* xac_scan_pattern(void* pHandle)
{
	xaho_native_scan* pScan = (xaho_native_scan*)pHandle;
	xbytesview Data;

	if ( pScan == NULL ) { return NULL; }
	Data.Data = (const unsigned char*)pScan->Match.Pattern.Data;
	Data.Size = pScan->Match.Pattern.Size;
	return xrtBufferFrom(Data);
}



/* 释放拉取式扫描包装。 */
XAHO_EXPORT void xac_scan_end(void* pHandle)
{
	xaho_native_scan* pScan = (xaho_native_scan*)pHandle;

	if ( pScan == NULL ) { return; }
	xrtFree(pScan->Owned);
	xrtAhoStateDestroy(pScan->State);
	xrtFree(pScan);
}



/* 创建持有自动机引用的 XLang 增量扫描器。 */
XAHO_EXPORT void* xac_stream_create(void* pHandle)
{
	xaho_native_stream* pStream =
		(xaho_native_stream*)xrtCalloc(1u, sizeof(*pStream));

	if ( pStream == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_MEMORY, "Aho-Corasick stream allocation failed");
		return NULL;
	}
	pStream->State = xrtAhoStateCreate((xrtaho*)pHandle);
	if ( pStream->State == NULL ) {
		xrtFree(pStream);
		return NULL;
	}
	pStream->Finished = true;
	return pStream;
}



/* 复制一个新分块；上一个分块必须已经消费完成。 */
XAHO_EXPORT int xac_stream_feed(
	void* pHandle,
	const char* sChunk,
	size_t iSize
)
{
	xaho_native_stream* pStream = (xaho_native_stream*)pHandle;
	str sCopy = NULL;

	if ( pStream == NULL || (sChunk == NULL && iSize != 0u) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_ARGUMENT, "Aho-Corasick stream chunk is invalid");
		return 0;
	}
	if ( !pStream->Finished ) {
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_STATE, "Aho-Corasick stream chunk must be resumed first");
		return 0;
	}
	if ( iSize != 0u ) {
		if ( iSize == SIZE_MAX ) {
			xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
				XRT_AHO_ERROR_RANGE, "Aho-Corasick stream chunk is too large");
			return 0;
		}
		sCopy = (str)xrtMalloc(iSize + 1u);
		if ( sCopy == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.aho_corasick",
				XRT_AHO_ERROR_MEMORY, "Aho-Corasick stream chunk allocation failed");
			return 0;
		}
		memcpy(sCopy, sChunk, iSize);
		sCopy[iSize] = 0;
	}
	xrtFree(pStream->Owned);
	pStream->Owned = sCopy;
	pStream->Text.Data = sCopy;
	pStream->Text.Size = iSize;
	pStream->Offset = 0u;
	pStream->Finished = false;
	return 1;
}



/* 拉取当前分块的下一个结果。 */
XAHO_EXPORT int xac_stream_next(void* pHandle)
{
	return xac_scan_next(pHandle);
}



/* 返回当前增量流是否仍有未消费分块。 */
XAHO_EXPORT int xac_stream_pending(void* pHandle)
{
	xaho_native_stream* pStream = (xaho_native_stream*)pHandle;

	return pStream != NULL && !pStream->Finished ? 1 : 0;
}



/* 返回增量流累计字节偏移。 */
XAHO_EXPORT int64 xac_stream_offset(void* pHandle)
{
	xaho_native_stream* pStream = (xaho_native_stream*)pHandle;
	uint64 iOffset = pStream != NULL ? xrtAhoStateOffset(pStream->State) : 0u;

	if ( iOffset > (uint64)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick stream offset exceeds int range");
		return -1;
	}
	return (int64)iOffset;
}



/* 返回增量流累计匹配数。 */
XAHO_EXPORT int64 xac_stream_matches(void* pHandle)
{
	xaho_native_stream* pStream = (xaho_native_stream*)pHandle;
	uint64 iMatches = pStream != NULL ? xrtAhoStateMatches(pStream->State) : 0u;

	if ( iMatches > (uint64)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.aho_corasick",
			XRT_AHO_ERROR_RANGE, "Aho-Corasick stream match count exceeds int range");
		return -1;
	}
	return (int64)iMatches;
}



/* 返回当前增量流匹配开始位置。 */
XAHO_EXPORT int64 xac_stream_start(void* pHandle)
{
	return xac_scan_start(pHandle);
}



/* 返回当前增量流匹配结束位置。 */
XAHO_EXPORT int64 xac_stream_end_pos(void* pHandle)
{
	return xac_scan_end_pos(pHandle);
}



/* 返回当前增量流模式的独立 Buffer。 */
XAHO_EXPORT void* xac_stream_pattern(void* pHandle)
{
	return xac_scan_pattern(pHandle);
}



/* 重置增量流并丢弃未消费分块。 */
XAHO_EXPORT void xac_stream_reset(void* pHandle)
{
	xaho_native_stream* pStream = (xaho_native_stream*)pHandle;

	if ( pStream == NULL ) { return; }
	xrtFree(pStream->Owned);
	pStream->Owned = NULL;
	pStream->Text.Data = NULL;
	pStream->Text.Size = 0u;
	pStream->Offset = 0u;
	pStream->Finished = true;
	xrtAhoStateReset(pStream->State);
}



/* 释放增量流。 */
XAHO_EXPORT void xac_stream_destroy(void* pHandle)
{
	xac_scan_end(pHandle);
}
