#define XRT_MODULE_ATOMIC
#define XRT_MODULE_UNICODE

#include "include/xrt_aho_corasick.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>



#define XAHO_NONE UINT32_MAX
#define XAHO_ERROR_DOMAIN "xlang.extlib.aho_corasick"



typedef struct xahonode {
	uint32 iBuildEdge;
	uint32 iTransition;
	uint32 iTransitionCount;
	uint32 iFail;
	uint32 iOutputLink;
	uint32 iOutput;
} xahonode;



typedef struct xahobuildedge {
	uint32 iNext;
	uint32 iNode;
	uint8 iByte;
} xahobuildedge;



typedef struct xahotransition {
	uint32 iNode;
	uint8 iByte;
} xahotransition;



typedef struct xahooutput {
	uint32 iPattern;
	uint32 iNext;
} xahooutput;



typedef struct xahopattern {
	str Data;
	size_t Size;
} xahopattern;



typedef struct xahomatchentry {
	uint32 iPattern;
	uint64 iStart;
	uint64 iEnd;
} xahomatchentry;



struct xrtaho {
	xatomic32 References;
	xrtahooptions Options;
	xahonode* Nodes;
	size_t NodeCount;
	size_t NodeCapacity;
	xahobuildedge* BuildEdges;
	size_t BuildEdgeCount;
	size_t BuildEdgeCapacity;
	xahotransition* Transitions;
	size_t TransitionCount;
	xahooutput* Outputs;
	size_t OutputCount;
	size_t OutputCapacity;
	xahopattern* Patterns;
	size_t PatternCount;
	size_t PatternCapacity;
	uint64 iTotalPatternBytes;
	size_t iMaxPatternBytes;
	bool Built;
};



struct xrtahoresult {
	xrtaho* Automaton;
	xahomatchentry* Items;
	size_t Count;
};



struct xrtahostate {
	xrtaho* Automaton;
	uint32 iNode;
	uint32 iEmitNode;
	uint32 iEmitOutput;
	uint64 iOffset;
	uint64 iMatches;
};



typedef struct xahomatchvector {
	xahomatchentry* Items;
	size_t Count;
	size_t Capacity;
	bool Failed;
} xahomatchvector;



typedef struct xahofindcontext {
	xahomatchentry Match;
	bool Found;
} xahofindcontext;



typedef struct xahointervalcursor {
	const xrtahoresult* Result;
	size_t Index;
} xahointervalcursor;



#if defined(XRT_AHO_TESTING)
static size_t g_xahoFailAllocation;



/* 测试构建只使用的单次故障注入入口。 */
void xrtAhoTestFailAfter(size_t iAllocation)
{
	g_xahoFailAllocation = iAllocation;
}



/* 判断本次模块内分配是否需要失败。 */
static bool xaho_test_allocation(void)
{
	if ( g_xahoFailAllocation == 0u ) { return false; }
	g_xahoFailAllocation -= 1u;
	return g_xahoFailAllocation == 0u;
}



/* 测试模式的模块内分配。 */
static ptr xaho_alloc(size_t iSize)
{
	return xaho_test_allocation() ? NULL : xrtMalloc(iSize);
}



/* 测试模式的模块内清零分配。 */
static ptr xaho_calloc(size_t iCount, size_t iSize)
{
	return xaho_test_allocation() ? NULL : xrtCalloc(iCount, iSize);
}



/* 测试模式的模块内重分配。 */
static ptr xaho_realloc(ptr pMemory, size_t iSize)
{
	return xaho_test_allocation() ? NULL : xrtRealloc(pMemory, iSize);
}
#else
	#define xaho_alloc(iSize) xrtMalloc(iSize)
	#define xaho_calloc(iCount, iSize) xrtCalloc((iCount), (iSize))
	#define xaho_realloc(pMemory, iSize) xrtRealloc((pMemory), (iSize))
#endif



/* 设置模块错误并返回 false，便于保持失败分支简洁。 */
static bool xaho_fail(xerrkind Kind, int iCode, cstr sMessage)
{
	xrtSetErrorInfo(Kind, XAHO_ERROR_DOMAIN, iCode, sMessage);
	return false;
}



/* 设置内存错误。 */
static bool xaho_memory(void)
{
	return xaho_fail(XERR_MEMORY, XRT_AHO_ERROR_MEMORY,
		"Aho-Corasick memory allocation failed");
}



/* 判断零值表示无限的 uint64 限制。 */
static bool xaho_over_limit(uint64 iLimit, uint64 iValue)
{
	return (iLimit != 0u) && (iValue > iLimit);
}



/* 向统计字节数加入数组容量，溢出时饱和为 UINT64_MAX。 */
static void xaho_info_bytes(
	uint64* pTotal,
	size_t iCount,
	size_t iItemSize
)
{
	uint64 iBytes;

	if ( *pTotal == UINT64_MAX ) { return; }
	if ( iCount > (size_t)(UINT64_MAX / iItemSize) ) {
		*pTotal = UINT64_MAX;
		return;
	}
	iBytes = (uint64)iCount * (uint64)iItemSize;
	if ( *pTotal > (UINT64_MAX - iBytes) ) {
		*pTotal = UINT64_MAX;
		return;
	}
	*pTotal += iBytes;
}



/* 安全计算 size_t 加法。 */
static bool xaho_size_add(size_t iLeft, size_t iRight, size_t* pResult)
{
	if ( iLeft > (SIZE_MAX - iRight) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick size overflow");
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 安全计算 size_t 乘法。 */
static bool xaho_size_multiply(size_t iLeft, size_t iRight, size_t* pResult)
{
	if ( (iLeft != 0u) && (iRight > (SIZE_MAX / iLeft)) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick size overflow");
	}
	*pResult = iLeft * iRight;
	return true;
}



/* 安全计算 uint64 加法。 */
static bool xaho_u64_add(uint64 iLeft, uint64 iRight, uint64* pResult)
{
	if ( iLeft > (UINT64_MAX - iRight) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick offset overflow");
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 验证字符串视图。 */
static bool xaho_view_valid(xstrview Text)
{
	return (Text.Size == 0u) || (Text.Data != NULL);
}



/* 以几何策略扩展数组，失败时保留原数组。 */
static bool xaho_reserve(
	ptr* ppData,
	size_t* pCapacity,
	size_t iRequired,
	size_t iItemSize
)
{
	size_t iCapacity;
	size_t iBytes;
	ptr pData;

	if ( iRequired <= *pCapacity ) { return true; }
	iCapacity = *pCapacity != 0u ? *pCapacity : 8u;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity *= 2u;
	}
	if ( !xaho_size_multiply(iCapacity, iItemSize, &iBytes) ) { return false; }
	pData = xaho_realloc(*ppData, iBytes);
	if ( pData == NULL ) { return xaho_memory(); }
	*ppData = pData;
	*pCapacity = iCapacity;
	return true;
}



/* 初始化一个尚未连接的节点。 */
static void xaho_node_init(xahonode* pNode)
{
	memset(pNode, 0, sizeof(*pNode));
	pNode->iBuildEdge = XAHO_NONE;
	pNode->iFail = 0u;
	pNode->iOutputLink = XAHO_NONE;
	pNode->iOutput = XAHO_NONE;
}



/* 按构建期链表查找一个字节转移。 */
static uint32 xaho_build_transition(
	const xrtaho* pAutomaton,
	uint32 iNode,
	uint8 iByte
)
{
	uint32 iEdge = pAutomaton->Nodes[iNode].iBuildEdge;

	while ( iEdge != XAHO_NONE ) {
		const xahobuildedge* pEdge = &pAutomaton->BuildEdges[iEdge];
		if ( pEdge->iByte == iByte ) { return pEdge->iNode; }
		iEdge = pEdge->iNext;
	}
	return XAHO_NONE;
}



/* 按构建后的有序稀疏表查找一个字节转移。 */
static uint32 xaho_transition(
	const xrtaho* pAutomaton,
	uint32 iNode,
	uint8 iByte
)
{
	const xahonode* pNode = &pAutomaton->Nodes[iNode];
	size_t iLeft = pNode->iTransition;
	size_t iRight = iLeft + pNode->iTransitionCount;

	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2u);
		const xahotransition* pTransition =
			&pAutomaton->Transitions[iMiddle];
		if ( pTransition->iByte < iByte ) {
			iLeft = iMiddle + 1u;
		}
		else {
			iRight = iMiddle;
		}
	}
	if ( iLeft < ((size_t)pNode->iTransition + pNode->iTransitionCount) &&
		pAutomaton->Transitions[iLeft].iByte == iByte ) {
		return pAutomaton->Transitions[iLeft].iNode;
	}
	return XAHO_NONE;
}



/* qsort 使用的单节点转移比较器。 */
static int xaho_transition_compare(const void* pLeft, const void* pRight)
{
	const xahotransition* pFirst = (const xahotransition*)pLeft;
	const xahotransition* pSecond = (const xahotransition*)pRight;

	if ( pFirst->iByte < pSecond->iByte ) { return -1; }
	if ( pFirst->iByte > pSecond->iByte ) { return 1; }
	return 0;
}



/* 对 ASCII 字母进行大小写折叠。 */
static uint8 xaho_fold(const xrtaho* pAutomaton, uint8 iByte)
{
	if ( pAutomaton->Options.bIgnoreAsciiCase &&
		iByte >= (uint8)'A' && iByte <= (uint8)'Z' ) {
		return (uint8)(iByte + ((uint8)'a' - (uint8)'A'));
	}
	return iByte;
}



/* 释放自动机的实际存储。 */
static void xaho_destroy_storage(xrtaho* pAutomaton)
{
	size_t i;

	for ( i = 0u; i < pAutomaton->PatternCount; ++i ) {
		xrtFree(pAutomaton->Patterns[i].Data);
	}
	xrtFree(pAutomaton->Patterns);
	xrtFree(pAutomaton->Outputs);
	xrtFree(pAutomaton->Transitions);
	xrtFree(pAutomaton->BuildEdges);
	xrtFree(pAutomaton->Nodes);
	xrtFree(pAutomaton);
}



/* 验证自动机可执行搜索。 */
static bool xaho_search_ready(xrtaho* pAutomaton, xstrview Text)
{
	if ( pAutomaton == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick automaton is null");
	}
	if ( !xaho_view_valid(Text) ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick text view is invalid");
	}
	if ( !pAutomaton->Built ) {
		return xaho_fail(XERR_STATE, XRT_AHO_ERROR_STATE,
			"Aho-Corasick automaton must be built before searching");
	}
	if ( xaho_over_limit(pAutomaton->Options.iMaxInputBytes,
		(uint64)Text.Size) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick input limit exceeded");
	}
	return true;
}



/* 把内部匹配转换为公开结构。 */
static void xaho_match_public(
	const xrtaho* pAutomaton,
	const xahomatchentry* pEntry,
	xrtahomatch* pMatch
)
{
	const xahopattern* pPattern = &pAutomaton->Patterns[pEntry->iPattern];

	pMatch->iPattern = pEntry->iPattern;
	pMatch->iStart = pEntry->iStart;
	pMatch->iEnd = pEntry->iEnd;
	pMatch->Pattern.Data = pPattern->Data;
	pMatch->Pattern.Size = pPattern->Size;
}



/* 初始化不持有额外引用的栈上扫描状态。 */
static void xaho_state_init(xrtahostate* pState, xrtaho* pAutomaton)
{
	memset(pState, 0, sizeof(*pState));
	pState->Automaton = pAutomaton;
	pState->iEmitNode = XAHO_NONE;
	pState->iEmitOutput = XAHO_NONE;
}



/* 比较普通搜索的稳定排序规则。 */
static int xaho_match_compare(const void* pLeft, const void* pRight)
{
	const xahomatchentry* pFirst = (const xahomatchentry*)pLeft;
	const xahomatchentry* pSecond = (const xahomatchentry*)pRight;

	if ( pFirst->iStart < pSecond->iStart ) { return -1; }
	if ( pFirst->iStart > pSecond->iStart ) { return 1; }
	if ( pFirst->iEnd < pSecond->iEnd ) { return -1; }
	if ( pFirst->iEnd > pSecond->iEnd ) { return 1; }
	if ( pFirst->iPattern < pSecond->iPattern ) { return -1; }
	if ( pFirst->iPattern > pSecond->iPattern ) { return 1; }
	return 0;
}



/* 比较左优先最长非重叠选择规则。 */
static int xaho_longest_compare(const void* pLeft, const void* pRight)
{
	const xahomatchentry* pFirst = (const xahomatchentry*)pLeft;
	const xahomatchentry* pSecond = (const xahomatchentry*)pRight;

	if ( pFirst->iStart < pSecond->iStart ) { return -1; }
	if ( pFirst->iStart > pSecond->iStart ) { return 1; }
	if ( pFirst->iEnd > pSecond->iEnd ) { return -1; }
	if ( pFirst->iEnd < pSecond->iEnd ) { return 1; }
	if ( pFirst->iPattern < pSecond->iPattern ) { return -1; }
	if ( pFirst->iPattern > pSecond->iPattern ) { return 1; }
	return 0;
}



/* 收集搜索结果。 */
static bool xaho_collect_match(const xrtahomatch* pMatch, ptr pUserData)
{
	xahomatchvector* pVector = (xahomatchvector*)pUserData;
	size_t iRequired;
	xahomatchentry* pItem;

	if ( !xaho_size_add(pVector->Count, 1u, &iRequired) ||
		!xaho_reserve((ptr*)&pVector->Items, &pVector->Capacity,
			iRequired, sizeof(*pVector->Items)) ) {
		pVector->Failed = true;
		return false;
	}
	pItem = &pVector->Items[pVector->Count++];
	pItem->iPattern = (uint32)pMatch->iPattern;
	pItem->iStart = pMatch->iStart;
	pItem->iEnd = pMatch->iEnd;
	return true;
}



/* 记录排序规则下最靠前的匹配。 */
static bool xaho_find_match(const xrtahomatch* pMatch, ptr pUserData)
{
	xahofindcontext* pContext = (xahofindcontext*)pUserData;
	xahomatchentry Current;

	Current.iPattern = (uint32)pMatch->iPattern;
	Current.iStart = pMatch->iStart;
	Current.iEnd = pMatch->iEnd;
	if ( !pContext->Found ||
		xaho_match_compare(&Current, &pContext->Match) < 0 ) {
		pContext->Match = Current;
		pContext->Found = true;
	}
	return true;
}



/* 命中一个结果后立即停止。 */
static bool xaho_stop_match(const xrtahomatch* pMatch, ptr pUserData)
{
	bool* pFound = (bool*)pUserData;

	(void)pMatch;
	*pFound = true;
	return false;
}



/* 检查输出字节预算，不分配或修改缓冲区。 */
bool xrtAhoCheckOutputSize(const xrtaho* pAutomaton, size_t iSize)
{
	if ( pAutomaton == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick automaton is null");
	}
	if ( xaho_over_limit(pAutomaton->Options.iMaxOutputBytes, (uint64)iSize) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick output limit exceeded");
	}
	return true;
}


/* 校验预算后分配结果，失败时不发布部分文本。 */
static str xaho_output_create(
	const xrtaho* pAutomaton,
	size_t iSize,
	size_t* pOutputSize
)
{
	str sResult;
	size_t iCapacity;

	if ( !xrtAhoCheckOutputSize(pAutomaton, iSize) ) { return NULL; }
	if ( !xaho_size_add(iSize, 1u, &iCapacity) ) { return NULL; }
	sResult = (str)xaho_alloc(iCapacity);
	if ( sResult == NULL ) {
		xaho_memory();
		return NULL;
	}
	sResult[iSize] = 0;
	if ( pOutputSize != NULL ) { *pOutputSize = iSize; }
	return sResult;
}



/* 复制视图并同时执行模块输出限制。 */
static str xaho_copy_output(
	const xrtaho* pAutomaton,
	xstrview Text,
	size_t* pOutputSize
)
{
	str sResult = xaho_output_create(pAutomaton, Text.Size, pOutputSize);

	if ( sResult == NULL ) { return NULL; }
	if ( Text.Size != 0u ) { memcpy(sResult, Text.Data, Text.Size); }
	return sResult;
}



/* 读取下一个严格重叠合并区间。 */
static bool xaho_interval_next(
	xahointervalcursor* pCursor,
	uint64* pStart,
	uint64* pEnd
)
{
	const xrtahoresult* pResult = pCursor->Result;
	const xahomatchentry* pItem;

	if ( pCursor->Index >= pResult->Count ) { return false; }
	pItem = &pResult->Items[pCursor->Index++];
	*pStart = pItem->iStart;
	*pEnd = pItem->iEnd;
	while ( pCursor->Index < pResult->Count ) {
		pItem = &pResult->Items[pCursor->Index];
		if ( pItem->iStart >= *pEnd ) { break; }
		if ( pItem->iEnd > *pEnd ) { *pEnd = pItem->iEnd; }
		pCursor->Index += 1u;
	}
	return true;
}



/* 初始化默认配置。 */
void xrtAhoOptionsInit(xrtahooptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = (uint32)sizeof(*pOptions);
	pOptions->iVersion = XRT_AHO_OPTIONS_VERSION;
}



/* 初始化统计结构。 */
void xrtAhoInfoInit(xrtahoinfo* pInfo)
{
	if ( pInfo == NULL ) { return; }
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = (uint32)sizeof(*pInfo);
	pInfo->iVersion = XRT_AHO_INFO_VERSION;
}



/* 创建空自动机。 */
xrtaho* xrtAhoCreate(const xrtahooptions* pOptions)
{
	xrtahooptions Options;
	xrtaho* pAutomaton;

	xrtAhoOptionsInit(&Options);
	if ( pOptions != NULL ) {
		if ( pOptions->iSize < sizeof(Options) ||
			pOptions->iVersion != XRT_AHO_OPTIONS_VERSION ) {
			xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
				"Aho-Corasick options are invalid");
			return NULL;
		}
		Options = *pOptions;
	}
	pAutomaton = (xrtaho*)xaho_calloc(1u, sizeof(*pAutomaton));
	if ( pAutomaton == NULL ) {
		xaho_memory();
		return NULL;
	}
	pAutomaton->Options = Options;
	xrtAtomic32Init(&pAutomaton->References, 1u);
	if ( !xaho_reserve((ptr*)&pAutomaton->Nodes,
		&pAutomaton->NodeCapacity, 1u, sizeof(*pAutomaton->Nodes)) ) {
		xrtFree(pAutomaton);
		return NULL;
	}
	xaho_node_init(&pAutomaton->Nodes[0]);
	pAutomaton->NodeCount = 1u;
	return pAutomaton;
}



/* 增加自动机引用。 */
xrtaho* xrtAhoRef(xrtaho* pAutomaton)
{
	uint32 iReferences;

	if ( pAutomaton == NULL ) { return NULL; }
	iReferences = xrtAtomic32FetchAdd(&pAutomaton->References, 1u,
		XMEMORY_RELAXED);
	if ( iReferences == UINT32_MAX ) {
		(void)xrtAtomic32FetchSub(&pAutomaton->References, 1u,
			XMEMORY_RELAXED);
		xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick reference count overflow");
		return NULL;
	}
	return pAutomaton;
}



/* 释放自动机引用。 */
void xrtAhoDestroy(xrtaho* pAutomaton)
{
	if ( pAutomaton == NULL ) { return; }
	if ( xrtAtomic32FetchSub(&pAutomaton->References, 1u,
		XMEMORY_ACQ_REL) == 1u ) {
		xaho_destroy_storage(pAutomaton);
	}
}



/* 添加一个模式。 */
bool xrtAhoAdd(xrtaho* pAutomaton, xstrview Pattern)
{
	uint32 iNode = 0u;
	size_t i;
	size_t iRequiredNodes;
	size_t iRequiredEdges;
	size_t iRequiredOutputs;
	size_t iRequiredPatterns;
	uint64 iTotalPatternBytes;
	str sPattern;
	uint32 iPattern;

	if ( pAutomaton == NULL || !xaho_view_valid(Pattern) ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick pattern view is invalid");
	}
	if ( pAutomaton->Built ) {
		return xaho_fail(XERR_STATE, XRT_AHO_ERROR_STATE,
			"Aho-Corasick patterns cannot be added after build");
	}
	if ( Pattern.Size == 0u ) {
		return xaho_fail(XERR_VALUE, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick pattern cannot be empty");
	}
	if ( xaho_over_limit(pAutomaton->Options.iMaxPatternBytes,
		(uint64)Pattern.Size) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick pattern size limit exceeded");
	}
	if ( !xaho_u64_add(pAutomaton->iTotalPatternBytes,
		(uint64)Pattern.Size, &iTotalPatternBytes) ) { return false; }
	if ( xaho_over_limit(pAutomaton->Options.iMaxTotalPatternBytes,
		iTotalPatternBytes) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick total pattern size limit exceeded");
	}
	if ( xaho_over_limit(pAutomaton->Options.iMaxPatterns,
		(uint64)pAutomaton->PatternCount + 1u) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick pattern count limit exceeded");
	}

	for ( i = 0u; i < Pattern.Size; ++i ) {
		uint32 iNext = xaho_build_transition(pAutomaton, iNode,
			xaho_fold(pAutomaton, (uint8)Pattern.Data[i]));
		if ( iNext == XAHO_NONE ) { break; }
		iNode = iNext;
	}
	if ( i == Pattern.Size && pAutomaton->Nodes[iNode].iOutput != XAHO_NONE ) {
		return xaho_fail(XERR_EXISTS, XRT_AHO_ERROR_EXISTS,
			"Aho-Corasick pattern already exists");
	}

	if ( !xaho_size_add(pAutomaton->NodeCount,
		Pattern.Size - i, &iRequiredNodes) ||
		!xaho_size_add(pAutomaton->BuildEdgeCount,
			Pattern.Size - i, &iRequiredEdges) ||
		!xaho_size_add(pAutomaton->OutputCount, 1u, &iRequiredOutputs) ||
		!xaho_size_add(pAutomaton->PatternCount, 1u, &iRequiredPatterns) ) {
		return false;
	}
	if ( iRequiredNodes > UINT32_MAX || iRequiredEdges > UINT32_MAX ||
		iRequiredOutputs > UINT32_MAX || iRequiredPatterns > UINT32_MAX ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick index capacity exceeded");
	}
	if ( xaho_over_limit(pAutomaton->Options.iMaxNodes,
		(uint64)iRequiredNodes) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick node limit exceeded");
	}
	if ( !xaho_reserve((ptr*)&pAutomaton->Nodes,
			&pAutomaton->NodeCapacity, iRequiredNodes,
			sizeof(*pAutomaton->Nodes)) ||
		!xaho_reserve((ptr*)&pAutomaton->BuildEdges,
			&pAutomaton->BuildEdgeCapacity, iRequiredEdges,
			sizeof(*pAutomaton->BuildEdges)) ||
		!xaho_reserve((ptr*)&pAutomaton->Outputs,
			&pAutomaton->OutputCapacity, iRequiredOutputs,
			sizeof(*pAutomaton->Outputs)) ||
		!xaho_reserve((ptr*)&pAutomaton->Patterns,
			&pAutomaton->PatternCapacity, iRequiredPatterns,
			sizeof(*pAutomaton->Patterns)) ) {
		return false;
	}
	if ( Pattern.Size == SIZE_MAX ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick pattern size overflow");
	}
	sPattern = (str)xaho_alloc(Pattern.Size + 1u);
	if ( sPattern == NULL ) { return xaho_memory(); }
	memcpy(sPattern, Pattern.Data, Pattern.Size);
	sPattern[Pattern.Size] = 0;

	for ( ; i < Pattern.Size; ++i ) {
		uint32 iNewNode = (uint32)pAutomaton->NodeCount++;
		uint32 iNewEdge = (uint32)pAutomaton->BuildEdgeCount++;
		xahobuildedge* pEdge = &pAutomaton->BuildEdges[iNewEdge];

		xaho_node_init(&pAutomaton->Nodes[iNewNode]);
		pEdge->iByte = xaho_fold(pAutomaton, (uint8)Pattern.Data[i]);
		pEdge->iNode = iNewNode;
		pEdge->iNext = pAutomaton->Nodes[iNode].iBuildEdge;
		pAutomaton->Nodes[iNode].iBuildEdge = iNewEdge;
		iNode = iNewNode;
	}
	iPattern = (uint32)pAutomaton->PatternCount;
	pAutomaton->Outputs[pAutomaton->OutputCount].iPattern = iPattern;
	pAutomaton->Outputs[pAutomaton->OutputCount].iNext =
		pAutomaton->Nodes[iNode].iOutput;
	pAutomaton->Nodes[iNode].iOutput = (uint32)pAutomaton->OutputCount++;
	pAutomaton->Patterns[pAutomaton->PatternCount].Data = sPattern;
	pAutomaton->Patterns[pAutomaton->PatternCount].Size = Pattern.Size;
	pAutomaton->PatternCount += 1u;
	pAutomaton->iTotalPatternBytes = iTotalPatternBytes;
	if ( Pattern.Size > pAutomaton->iMaxPatternBytes ) {
		pAutomaton->iMaxPatternBytes = Pattern.Size;
	}
	return true;
}



/* 批量添加模式。 */
bool xrtAhoAddMany(
	xrtaho* pAutomaton,
	const xstrview* arrPatterns,
	size_t iCount
)
{
	size_t i;

	if ( pAutomaton == NULL || (iCount != 0u && arrPatterns == NULL) ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick automaton or pattern array is null");
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtAhoAdd(pAutomaton, arrPatterns[i]) ) { return false; }
	}
	return true;
}



/* 构建有序转移表和 failure link。 */
bool xrtAhoBuild(xrtaho* pAutomaton)
{
	xahotransition* pTransitions = NULL;
	uint32* arrQueue;
	size_t iTransitionBytes;
	size_t iQueueBytes;
	size_t i;
	size_t iPosition = 0u;
	size_t iHead = 0u;
	size_t iTail = 0u;

	if ( pAutomaton == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick automaton is null");
	}
	if ( pAutomaton->Built ) { return true; }
	if ( !xaho_size_multiply(pAutomaton->BuildEdgeCount,
			sizeof(*pTransitions), &iTransitionBytes) ||
		!xaho_size_multiply(pAutomaton->NodeCount,
			sizeof(*arrQueue), &iQueueBytes) ) {
		return false;
	}
	if ( iTransitionBytes != 0u ) {
		pTransitions = (xahotransition*)xaho_alloc(iTransitionBytes);
		if ( pTransitions == NULL ) { return xaho_memory(); }
	}
	arrQueue = (uint32*)xaho_alloc(iQueueBytes);
	if ( arrQueue == NULL ) {
		xrtFree(pTransitions);
		return xaho_memory();
	}

	for ( i = 0u; i < pAutomaton->NodeCount; ++i ) {
		xahonode* pNode = &pAutomaton->Nodes[i];
		uint32 iEdge = pNode->iBuildEdge;
		size_t iFirst = iPosition;

		while ( iEdge != XAHO_NONE ) {
			const xahobuildedge* pEdge = &pAutomaton->BuildEdges[iEdge];
			pTransitions[iPosition].iByte = pEdge->iByte;
			pTransitions[iPosition].iNode = pEdge->iNode;
			iPosition += 1u;
			iEdge = pEdge->iNext;
		}
		pNode->iTransition = (uint32)iFirst;
		pNode->iTransitionCount = (uint32)(iPosition - iFirst);
		if ( pNode->iTransitionCount > 1u ) {
			qsort(pTransitions + iFirst, pNode->iTransitionCount,
				sizeof(*pTransitions), xaho_transition_compare);
		}
	}

	pAutomaton->Transitions = pTransitions;
	pAutomaton->TransitionCount = pAutomaton->BuildEdgeCount;
	pAutomaton->Nodes[0].iFail = 0u;
	pAutomaton->Nodes[0].iOutputLink = XAHO_NONE;
	for ( i = 0u; i < pAutomaton->Nodes[0].iTransitionCount; ++i ) {
		uint32 iChild = pTransitions[pAutomaton->Nodes[0].iTransition + i].iNode;
		pAutomaton->Nodes[iChild].iFail = 0u;
		pAutomaton->Nodes[iChild].iOutputLink = XAHO_NONE;
		arrQueue[iTail++] = iChild;
	}

	while ( iHead < iTail ) {
		uint32 iNode = arrQueue[iHead++];
		const xahonode* pNode = &pAutomaton->Nodes[iNode];

		for ( i = 0u; i < pNode->iTransitionCount; ++i ) {
			const xahotransition* pTransition =
				&pTransitions[pNode->iTransition + i];
			uint32 iChild = pTransition->iNode;
			uint32 iFail = pNode->iFail;
			uint32 iTarget = xaho_transition(pAutomaton, iFail,
				pTransition->iByte);

			while ( iTarget == XAHO_NONE && iFail != 0u ) {
				iFail = pAutomaton->Nodes[iFail].iFail;
				iTarget = xaho_transition(pAutomaton, iFail,
					pTransition->iByte);
			}
			if ( iTarget == XAHO_NONE ) { iTarget = 0u; }
			pAutomaton->Nodes[iChild].iFail = iTarget;
			pAutomaton->Nodes[iChild].iOutputLink =
				pAutomaton->Nodes[iTarget].iOutput != XAHO_NONE ?
				iTarget : pAutomaton->Nodes[iTarget].iOutputLink;
			arrQueue[iTail++] = iChild;
		}
	}

	for ( i = 0u; i < pAutomaton->NodeCount; ++i ) {
		pAutomaton->Nodes[i].iBuildEdge = XAHO_NONE;
	}
	xrtFree(arrQueue);
	xrtFree(pAutomaton->BuildEdges);
	pAutomaton->BuildEdges = NULL;
	pAutomaton->BuildEdgeCount = 0u;
	pAutomaton->BuildEdgeCapacity = 0u;
	pAutomaton->Built = true;
	return true;
}



/* 返回构建状态。 */
bool xrtAhoBuilt(const xrtaho* pAutomaton)
{
	return pAutomaton != NULL && pAutomaton->Built;
}



/* 返回模式数量。 */
size_t xrtAhoPatternCount(const xrtaho* pAutomaton)
{
	return pAutomaton != NULL ? pAutomaton->PatternCount : 0u;
}



/* 返回模式视图。 */
xstrview xrtAhoPattern(const xrtaho* pAutomaton, size_t iIndex)
{
	xstrview Result = { NULL, 0u };

	if ( pAutomaton == NULL || iIndex >= pAutomaton->PatternCount ) {
		xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick pattern index is out of range");
		return Result;
	}
	Result.Data = pAutomaton->Patterns[iIndex].Data;
	Result.Size = pAutomaton->Patterns[iIndex].Size;
	return Result;
}



/* 获取自动机统计快照。 */
bool xrtAhoGetInfo(const xrtaho* pAutomaton, xrtahoinfo* pInfo)
{
	xrtahoinfo Info;

	if ( pAutomaton == NULL || pInfo == NULL ||
		pInfo->iSize < sizeof(*pInfo) ||
		pInfo->iVersion != XRT_AHO_INFO_VERSION ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick info request is invalid");
	}
	xrtAhoInfoInit(&Info);
	Info.bBuilt = pAutomaton->Built;
	Info.iPatterns = (uint64)pAutomaton->PatternCount;
	Info.iPatternBytes = pAutomaton->iTotalPatternBytes;
	Info.iMaxPatternBytes = (uint64)pAutomaton->iMaxPatternBytes;
	Info.iNodes = (uint64)pAutomaton->NodeCount;
	Info.iTransitions = (uint64)pAutomaton->TransitionCount;
	Info.iOutputs = (uint64)pAutomaton->OutputCount;
	Info.iAllocatedBytes = sizeof(*pAutomaton);
	xaho_info_bytes(&Info.iAllocatedBytes, pAutomaton->NodeCapacity,
		sizeof(*pAutomaton->Nodes));
	xaho_info_bytes(&Info.iAllocatedBytes, pAutomaton->BuildEdgeCapacity,
		sizeof(*pAutomaton->BuildEdges));
	xaho_info_bytes(&Info.iAllocatedBytes, pAutomaton->TransitionCount,
		sizeof(*pAutomaton->Transitions));
	xaho_info_bytes(&Info.iAllocatedBytes, pAutomaton->OutputCapacity,
		sizeof(*pAutomaton->Outputs));
	xaho_info_bytes(&Info.iAllocatedBytes, pAutomaton->PatternCapacity,
		sizeof(*pAutomaton->Patterns));
	if ( Info.iAllocatedBytes != UINT64_MAX ) {
		uint64 iPatternStorage = pAutomaton->iTotalPatternBytes +
			(uint64)pAutomaton->PatternCount;

		if ( iPatternStorage < pAutomaton->iTotalPatternBytes ||
			Info.iAllocatedBytes > (UINT64_MAX - iPatternStorage) ) {
			Info.iAllocatedBytes = UINT64_MAX;
		}
		else {
			Info.iAllocatedBytes += iPatternStorage;
		}
	}
	*pInfo = Info;
	return true;
}



/* 重置增量扫描状态。 */
void xrtAhoStateReset(xrtahostate* pState)
{
	if ( pState == NULL ) { return; }
	pState->iNode = 0u;
	pState->iEmitNode = XAHO_NONE;
	pState->iEmitOutput = XAHO_NONE;
	pState->iOffset = 0u;
	pState->iMatches = 0u;
}



/* 返回扫描偏移。 */
uint64 xrtAhoStateOffset(const xrtahostate* pState)
{
	return pState != NULL ? pState->iOffset : 0u;
}



/* 返回扫描匹配数量。 */
uint64 xrtAhoStateMatches(const xrtahostate* pState)
{
	return pState != NULL ? pState->iMatches : 0u;
}


/* 分块输入的预检与实际扫描共用同一套溢出和预算规则。 */
bool xrtAhoStateCheckInput(const xrtahostate* pState, size_t iSize)
{
	uint64 iEnd;
	if ( pState == NULL || pState->Automaton == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick scan state is null");
	}
	if ( !pState->Automaton->Built ) {
		return xaho_fail(XERR_STATE, XRT_AHO_ERROR_STATE,
			"Aho-Corasick automaton must be built before scanning");
	}
	if ( !xaho_u64_add(pState->iOffset, (uint64)iSize, &iEnd) ) { return false; }
	if ( xaho_over_limit(pState->Automaton->Options.iMaxInputBytes, iEnd) ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
			"Aho-Corasick input limit exceeded");
	}
	return true;
}



/* 扫描一个增量分块。 */
xrtahoscanstatus xrtAhoStateFeed(
	xrtahostate* pState,
	xstrview Chunk,
	xrtahomatchproc pMatch,
	ptr pUserData,
	size_t* pConsumed
)
{
	xrtaho* pAutomaton;
	size_t iPosition = 0u;

	if ( pConsumed != NULL ) { *pConsumed = 0u; }
	if ( pState == NULL || pState->Automaton == NULL ||
		!xaho_view_valid(Chunk) ) {
		xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick scan state or chunk is invalid");
		return XRT_AHO_SCAN_ERROR;
	}
	pAutomaton = pState->Automaton;
	if ( !xrtAhoStateCheckInput(pState, Chunk.Size) ) { return XRT_AHO_SCAN_ERROR; }

	for ( ;; ) {
		while ( pState->iEmitNode != XAHO_NONE ) {
			const xahonode* pNode = &pAutomaton->Nodes[pState->iEmitNode];

			if ( pState->iEmitOutput == XAHO_NONE ) {
				pState->iEmitNode = pNode->iOutputLink;
				pState->iEmitOutput = pState->iEmitNode != XAHO_NONE ?
					pAutomaton->Nodes[pState->iEmitNode].iOutput : XAHO_NONE;
				continue;
			}
			if ( pState->iMatches == UINT64_MAX ||
				xaho_over_limit(pAutomaton->Options.iMaxMatches,
					pState->iMatches + 1u) ) {
				xaho_fail(XERR_RANGE, XRT_AHO_ERROR_LIMIT,
					"Aho-Corasick match limit exceeded");
				if ( pConsumed != NULL ) { *pConsumed = iPosition; }
				return XRT_AHO_SCAN_ERROR;
			}
			{
				const xahooutput* pOutput =
					&pAutomaton->Outputs[pState->iEmitOutput];
				const xahopattern* pPattern =
					&pAutomaton->Patterns[pOutput->iPattern];
				xrtahomatch Match;

				pState->iEmitOutput = pOutput->iNext;
				pState->iMatches += 1u;
				Match.iPattern = pOutput->iPattern;
				Match.iEnd = pState->iOffset;
				Match.iStart = Match.iEnd - (uint64)pPattern->Size;
				Match.Pattern.Data = pPattern->Data;
				Match.Pattern.Size = pPattern->Size;
				if ( pMatch != NULL && !pMatch(&Match, pUserData) ) {
					if ( pConsumed != NULL ) { *pConsumed = iPosition; }
					return XRT_AHO_SCAN_STOPPED;
				}
			}
		}

		if ( iPosition >= Chunk.Size ) {
			if ( pConsumed != NULL ) { *pConsumed = iPosition; }
			return XRT_AHO_SCAN_COMPLETE;
		}
		{
			uint8 iByte = xaho_fold(pAutomaton,
				(uint8)Chunk.Data[iPosition]);
			uint32 iNode = pState->iNode;
			uint32 iNext = xaho_transition(pAutomaton, iNode, iByte);

			while ( iNext == XAHO_NONE && iNode != 0u ) {
				iNode = pAutomaton->Nodes[iNode].iFail;
				iNext = xaho_transition(pAutomaton, iNode, iByte);
			}
			pState->iNode = iNext != XAHO_NONE ? iNext : 0u;
			pState->iOffset += 1u;
			iPosition += 1u;
			pState->iEmitNode = pState->iNode;
			pState->iEmitOutput =
				pAutomaton->Nodes[pState->iEmitNode].iOutput;
		}
	}
}



/* 创建增量扫描状态。 */
xrtahostate* xrtAhoStateCreate(xrtaho* pAutomaton)
{
	xrtahostate* pState;

	if ( pAutomaton == NULL || !pAutomaton->Built ) {
		xaho_fail(pAutomaton == NULL ? XERR_ARGUMENT : XERR_STATE,
			pAutomaton == NULL ? XRT_AHO_ERROR_ARGUMENT : XRT_AHO_ERROR_STATE,
			"Aho-Corasick requires a built automaton for incremental scanning");
		return NULL;
	}
	pState = (xrtahostate*)xaho_alloc(sizeof(*pState));
	if ( pState == NULL ) {
		xaho_memory();
		return NULL;
	}
	xaho_state_init(pState, pAutomaton);
	if ( xrtAhoRef(pAutomaton) == NULL ) {
		xrtFree(pState);
		return NULL;
	}
	return pState;
}



/* 释放增量扫描状态。 */
void xrtAhoStateDestroy(xrtahostate* pState)
{
	if ( pState == NULL ) { return; }
	xrtAhoDestroy(pState->Automaton);
	xrtFree(pState);
}



/* 扫描一个完整视图。 */
xrtahoscanstatus xrtAhoScan(
	xrtaho* pAutomaton,
	xstrview Text,
	xrtahomatchproc pMatch,
	ptr pUserData
)
{
	xrtahostate State;
	size_t iConsumed;

	if ( !xaho_search_ready(pAutomaton, Text) ) {
		return XRT_AHO_SCAN_ERROR;
	}
	xaho_state_init(&State, pAutomaton);
	return xrtAhoStateFeed(&State, Text, pMatch, pUserData, &iConsumed);
}



/* 搜索全部匹配。 */
xrtahoresult* xrtAhoSearch(xrtaho* pAutomaton, xstrview Text)
{
	xahomatchvector Vector = { NULL, 0u, 0u, false };
	xrtahoscanstatus iStatus;
	xrtahoresult* pResult;

	if ( !xaho_search_ready(pAutomaton, Text) ) { return NULL; }
	iStatus = xrtAhoScan(pAutomaton, Text, xaho_collect_match, &Vector);
	if ( iStatus == XRT_AHO_SCAN_ERROR || Vector.Failed ) {
		xrtFree(Vector.Items);
		return NULL;
	}
	if ( Vector.Count > 1u ) {
		qsort(Vector.Items, Vector.Count, sizeof(*Vector.Items),
			xaho_match_compare);
	}
	pResult = (xrtahoresult*)xaho_alloc(sizeof(*pResult));
	if ( pResult == NULL ) {
		xrtFree(Vector.Items);
		xaho_memory();
		return NULL;
	}
	pResult->Automaton = xrtAhoRef(pAutomaton);
	if ( pResult->Automaton == NULL ) {
		xrtFree(Vector.Items);
		xrtFree(pResult);
		return NULL;
	}
	pResult->Items = Vector.Items;
	pResult->Count = Vector.Count;
	return pResult;
}



/* 搜索左优先最长非重叠匹配。 */
xrtahoresult* xrtAhoSearchNonOverlapping(
	xrtaho* pAutomaton,
	xstrview Text
)
{
	xrtahoresult* pResult = xrtAhoSearch(pAutomaton, Text);
	size_t iRead;
	size_t iWrite = 0u;
	uint64 iEnd = 0u;

	if ( pResult == NULL ) { return NULL; }
	if ( pResult->Count > 1u ) {
		qsort(pResult->Items, pResult->Count, sizeof(*pResult->Items),
			xaho_longest_compare);
	}
	for ( iRead = 0u; iRead < pResult->Count; ++iRead ) {
		xahomatchentry Item = pResult->Items[iRead];
		if ( Item.iStart < iEnd ) { continue; }
		pResult->Items[iWrite++] = Item;
		iEnd = Item.iEnd;
	}
	pResult->Count = iWrite;
	return pResult;
}



/* 查找第一个排序结果。 */
bool xrtAhoFind(
	xrtaho* pAutomaton,
	xstrview Text,
	xrtahomatch* pMatch,
	bool* pFound
)
{
	xahofindcontext Context;
	xrtahoscanstatus iStatus;

	if ( pMatch == NULL || pFound == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick find output is null");
	}
	memset(&Context, 0, sizeof(Context));
	*pFound = false;
	iStatus = xrtAhoScan(pAutomaton, Text, xaho_find_match, &Context);
	if ( iStatus == XRT_AHO_SCAN_ERROR ) { return false; }
	if ( Context.Found ) {
		xaho_match_public(pAutomaton, &Context.Match, pMatch);
		*pFound = true;
	}
	return true;
}



/* 判断是否存在匹配。 */
bool xrtAhoContains(
	xrtaho* pAutomaton,
	xstrview Text,
	bool* pContains
)
{
	xrtahoscanstatus iStatus;

	if ( pContains == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick contains output is null");
	}
	*pContains = false;
	iStatus = xrtAhoScan(pAutomaton, Text, xaho_stop_match, pContains);
	return iStatus != XRT_AHO_SCAN_ERROR;
}



/* 统计匹配数量。 */
bool xrtAhoCount(
	xrtaho* pAutomaton,
	xstrview Text,
	uint64* pCount
)
{
	xrtahostate State;
	size_t iConsumed;
	xrtahoscanstatus iStatus;

	if ( pCount == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick count output is null");
	}
	*pCount = 0u;
	if ( !xaho_search_ready(pAutomaton, Text) ) { return false; }
	xaho_state_init(&State, pAutomaton);
	iStatus = xrtAhoStateFeed(&State, Text, NULL, NULL, &iConsumed);
	if ( iStatus == XRT_AHO_SCAN_ERROR ) { return false; }
	*pCount = State.iMatches;
	return true;
}



/* 返回结果数量。 */
size_t xrtAhoResultCount(const xrtahoresult* pResult)
{
	return pResult != NULL ? pResult->Count : 0u;
}



/* 读取结果。 */
bool xrtAhoResultAt(
	const xrtahoresult* pResult,
	size_t iIndex,
	xrtahomatch* pMatch
)
{
	if ( pResult == NULL || pMatch == NULL ) {
		return xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick result or match output is null");
	}
	if ( iIndex >= pResult->Count ) {
		return xaho_fail(XERR_RANGE, XRT_AHO_ERROR_RANGE,
			"Aho-Corasick result index is out of range");
	}
	xaho_match_public(pResult->Automaton, &pResult->Items[iIndex], pMatch);
	return true;
}



/* 释放结果集。 */
void xrtAhoResultDestroy(xrtahoresult* pResult)
{
	if ( pResult == NULL ) { return; }
	xrtAhoDestroy(pResult->Automaton);
	xrtFree(pResult->Items);
	xrtFree(pResult);
}



/* 固定文本替换。 */
str xrtAhoReplace(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Replacement,
	size_t* pOutputSize
)
{
	xrtahoresult* pResult;
	xahointervalcursor Cursor;
	uint64 iStart;
	uint64 iEnd;
	size_t iPosition = 0u;
	size_t iOutput = 0u;
	str sResult;
	str sWrite;

	if ( pOutputSize != NULL ) { *pOutputSize = 0u; }
	if ( !xaho_view_valid(Replacement) ) {
		xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick replacement view is invalid");
		return NULL;
	}
	pResult = xrtAhoSearch(pAutomaton, Text);
	if ( pResult == NULL ) { return NULL; }
	if ( pResult->Count == 0u ) {
		xrtAhoResultDestroy(pResult);
		return xaho_copy_output(pAutomaton, Text, pOutputSize);
	}
	Cursor.Result = pResult;
	Cursor.Index = 0u;
	while ( xaho_interval_next(&Cursor, &iStart, &iEnd) ) {
		if ( !xaho_size_add(iOutput, (size_t)iStart - iPosition, &iOutput) ||
			!xaho_size_add(iOutput, Replacement.Size, &iOutput) ) {
			xrtAhoResultDestroy(pResult);
			return NULL;
		}
		iPosition = (size_t)iEnd;
	}
	if ( !xaho_size_add(iOutput, Text.Size - iPosition, &iOutput) ) {
		xrtAhoResultDestroy(pResult);
		return NULL;
	}
	sResult = xaho_output_create(pAutomaton, iOutput, pOutputSize);
	if ( sResult == NULL ) {
		xrtAhoResultDestroy(pResult);
		return NULL;
	}
	sWrite = sResult;
	iPosition = 0u;
	Cursor.Index = 0u;
	while ( xaho_interval_next(&Cursor, &iStart, &iEnd) ) {
		size_t iGap = (size_t)iStart - iPosition;
		if ( iGap != 0u ) { memcpy(sWrite, Text.Data + iPosition, iGap); }
		sWrite += iGap;
		if ( Replacement.Size != 0u ) {
			memcpy(sWrite, Replacement.Data, Replacement.Size);
			sWrite += Replacement.Size;
		}
		iPosition = (size_t)iEnd;
	}
	if ( iPosition < Text.Size ) {
		memcpy(sWrite, Text.Data + iPosition, Text.Size - iPosition);
	}
	xrtAhoResultDestroy(pResult);
	return sResult;
}



/* 按字节或 UTF-8 标量重复遮盖字符串。 */
static str xaho_mask(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Mask,
	bool bUnicode,
	size_t* pOutputSize
)
{
	xrtahoresult* pResult;
	xahointervalcursor Cursor;
	uint64 iStart;
	uint64 iEnd;
	size_t iPosition = 0u;
	size_t iOutput = 0u;
	str sResult;
	str sWrite;

	if ( pOutputSize != NULL ) { *pOutputSize = 0u; }
	if ( !xaho_view_valid(Mask) ) {
		xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick mask view is invalid");
		return NULL;
	}
	if ( bUnicode && (!xrtUtf8Valid(Text, NULL) || !xrtUtf8Valid(Mask, NULL)) ) {
		xaho_fail(XERR_VALUE, XRT_AHO_ERROR_UTF8,
			"Aho-Corasick Unicode mask requires valid UTF-8");
		return NULL;
	}
	pResult = xrtAhoSearch(pAutomaton, Text);
	if ( pResult == NULL ) { return NULL; }
	if ( pResult->Count == 0u ) {
		xrtAhoResultDestroy(pResult);
		return xaho_copy_output(pAutomaton, Text, pOutputSize);
	}
	Cursor.Result = pResult;
	Cursor.Index = 0u;
	while ( xaho_interval_next(&Cursor, &iStart, &iEnd) ) {
		size_t iRepeat = (size_t)(iEnd - iStart);
		size_t iMasked;

		if ( bUnicode ) {
			xstrview Part = { Text.Data + (size_t)iStart, iRepeat };
			iRepeat = xrtUtf8Count(Part);
			if ( iRepeat == XRT_NPOS ) {
				xrtAhoResultDestroy(pResult);
				xaho_fail(XERR_VALUE, XRT_AHO_ERROR_UTF8,
					"Aho-Corasick match is not aligned to UTF-8");
				return NULL;
			}
		}
		if ( !xaho_size_multiply(iRepeat, Mask.Size, &iMasked) ||
			!xaho_size_add(iOutput, (size_t)iStart - iPosition, &iOutput) ||
			!xaho_size_add(iOutput, iMasked, &iOutput) ) {
			xrtAhoResultDestroy(pResult);
			return NULL;
		}
		iPosition = (size_t)iEnd;
	}
	if ( !xaho_size_add(iOutput, Text.Size - iPosition, &iOutput) ) {
		xrtAhoResultDestroy(pResult);
		return NULL;
	}
	sResult = xaho_output_create(pAutomaton, iOutput, pOutputSize);
	if ( sResult == NULL ) {
		xrtAhoResultDestroy(pResult);
		return NULL;
	}
	sWrite = sResult;
	iPosition = 0u;
	Cursor.Index = 0u;
	while ( xaho_interval_next(&Cursor, &iStart, &iEnd) ) {
		size_t iRepeat = (size_t)(iEnd - iStart);
		size_t iGap = (size_t)iStart - iPosition;
		size_t i;

		if ( bUnicode ) {
			xstrview Part = { Text.Data + (size_t)iStart, iRepeat };
			iRepeat = xrtUtf8Count(Part);
		}
		if ( iGap != 0u ) { memcpy(sWrite, Text.Data + iPosition, iGap); }
		sWrite += iGap;
		for ( i = 0u; i < iRepeat; ++i ) {
			if ( Mask.Size != 0u ) {
				memcpy(sWrite, Mask.Data, Mask.Size);
				sWrite += Mask.Size;
			}
		}
		iPosition = (size_t)iEnd;
	}
	if ( iPosition < Text.Size ) {
		memcpy(sWrite, Text.Data + iPosition, Text.Size - iPosition);
	}
	xrtAhoResultDestroy(pResult);
	return sResult;
}



/* 按字节遮盖。 */
str xrtAhoMaskBytes(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Mask,
	size_t* pOutputSize
)
{
	return xaho_mask(pAutomaton, Text, Mask, false, pOutputSize);
}



/* 按 UTF-8 标量遮盖。 */
str xrtAhoMask(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Mask,
	size_t* pOutputSize
)
{
	return xaho_mask(pAutomaton, Text, Mask, true, pOutputSize);
}



/* 包裹合并后的匹配区间。 */
str xrtAhoHighlight(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Open,
	xstrview Close,
	size_t* pOutputSize
)
{
	xrtahoresult* pResult;
	xahointervalcursor Cursor;
	uint64 iStart;
	uint64 iEnd;
	size_t iOutput;
	size_t iTags;
	size_t iIntervals = 0u;
	str sResult;
	str sWrite;
	size_t iPosition = 0u;

	if ( pOutputSize != NULL ) { *pOutputSize = 0u; }
	if ( !xaho_view_valid(Open) || !xaho_view_valid(Close) ) {
		xaho_fail(XERR_ARGUMENT, XRT_AHO_ERROR_ARGUMENT,
			"Aho-Corasick highlight tag view is invalid");
		return NULL;
	}
	pResult = xrtAhoSearch(pAutomaton, Text);
	if ( pResult == NULL ) { return NULL; }
	if ( pResult->Count == 0u ) {
		xrtAhoResultDestroy(pResult);
		return xaho_copy_output(pAutomaton, Text, pOutputSize);
	}
	Cursor.Result = pResult;
	Cursor.Index = 0u;
	while ( xaho_interval_next(&Cursor, &iStart, &iEnd) ) {
		(void)iStart;
		(void)iEnd;
		iIntervals += 1u;
	}
	if ( !xaho_size_add(Open.Size, Close.Size, &iTags) ||
		!xaho_size_multiply(iTags, iIntervals, &iTags) ||
		!xaho_size_add(Text.Size, iTags, &iOutput) ) {
		xrtAhoResultDestroy(pResult);
		return NULL;
	}
	sResult = xaho_output_create(pAutomaton, iOutput, pOutputSize);
	if ( sResult == NULL ) {
		xrtAhoResultDestroy(pResult);
		return NULL;
	}
	sWrite = sResult;
	Cursor.Index = 0u;
	while ( xaho_interval_next(&Cursor, &iStart, &iEnd) ) {
		size_t iGap = (size_t)iStart - iPosition;
		size_t iSpan = (size_t)(iEnd - iStart);

		if ( iGap != 0u ) { memcpy(sWrite, Text.Data + iPosition, iGap); }
		sWrite += iGap;
		if ( Open.Size != 0u ) { memcpy(sWrite, Open.Data, Open.Size); }
		sWrite += Open.Size;
		if ( iSpan != 0u ) { memcpy(sWrite, Text.Data + (size_t)iStart, iSpan); }
		sWrite += iSpan;
		if ( Close.Size != 0u ) { memcpy(sWrite, Close.Data, Close.Size); }
		sWrite += Close.Size;
		iPosition = (size_t)iEnd;
	}
	if ( iPosition < Text.Size ) {
		memcpy(sWrite, Text.Data + iPosition, Text.Size - iPosition);
	}
	xrtAhoResultDestroy(pResult);
	return sResult;
}
