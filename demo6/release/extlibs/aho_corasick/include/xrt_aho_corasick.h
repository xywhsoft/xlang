#ifndef XRT_AHO_CORASICK_H
#define XRT_AHO_CORASICK_H

#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif

#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_AHO_OPTIONS_VERSION 1u
#define XRT_AHO_INFO_VERSION 1u



/* Aho-Corasick 模块的稳定错误代码。 */
typedef enum xrtahostatus {
	XRT_AHO_OK = 0,
	XRT_AHO_ERROR_ARGUMENT = 1,
	XRT_AHO_ERROR_MEMORY = 2,
	XRT_AHO_ERROR_STATE = 3,
	XRT_AHO_ERROR_LIMIT = 4,
	XRT_AHO_ERROR_RANGE = 5,
	XRT_AHO_ERROR_UTF8 = 6,
	XRT_AHO_ERROR_EXISTS = 7
} xrtahostatus;



/* 扫描状态区分正常完成、调用方主动停止和实际失败。 */
typedef enum xrtahoscanstatus {
	XRT_AHO_SCAN_ERROR = -1,
	XRT_AHO_SCAN_STOPPED = 0,
	XRT_AHO_SCAN_COMPLETE = 1
} xrtahoscanstatus;



/* 零值限制表示不限制；大小写忽略只折叠 ASCII A-Z。 */
typedef struct xrtahooptions {
	uint32 iSize;
	uint32 iVersion;
	bool bIgnoreAsciiCase;
	uint8 arrReserved[7];
	uint64 iMaxPatterns;
	uint64 iMaxPatternBytes;
	uint64 iMaxTotalPatternBytes;
	uint64 iMaxNodes;
	uint64 iMaxMatches;
	uint64 iMaxInputBytes;
	uint64 iMaxOutputBytes;
} xrtahooptions;



/* 统计只计算模块请求的有效字节，不包含底层分配器元数据。 */
typedef struct xrtahoinfo {
	uint32 iSize;
	uint32 iVersion;
	bool bBuilt;
	uint8 arrReserved[7];
	uint64 iPatterns;
	uint64 iPatternBytes;
	uint64 iMaxPatternBytes;
	uint64 iNodes;
	uint64 iTransitions;
	uint64 iOutputs;
	uint64 iAllocatedBytes;
} xrtahoinfo;



/* 匹配位置使用 UTF-8 原始字节偏移；Pattern 在自动机存活期间有效。 */
typedef struct xrtahomatch {
	uint64 iPattern;
	uint64 iStart;
	uint64 iEnd;
	xstrview Pattern;
} xrtahomatch;



typedef struct xrtaho xrtaho;
typedef struct xrtahoresult xrtahoresult;
typedef struct xrtahostate xrtahostate;



/* 返回 false 只表示调用方主动停止，不会设置错误。 */
typedef bool (*xrtahomatchproc)(const xrtahomatch* pMatch, ptr pUserData);



/* 初始化默认选项；默认不施加资源上限。 */
XRT_API void xrtAhoOptionsInit(xrtahooptions* pOptions);



/* 初始化自动机统计结构。 */
XRT_API void xrtAhoInfoInit(xrtahoinfo* pInfo);



/* 创建空自动机；添加和构建阶段不允许并发调用。 */
XRT_API xrtaho* xrtAhoCreate(const xrtahooptions* pOptions);



/* 增加自动机引用；构建后可用于跨线程共享只读自动机。 */
XRT_API xrtaho* xrtAhoRef(xrtaho* pAutomaton);



/* 释放自动机引用；结果集和扫描状态会自动保留自己的引用。 */
XRT_API void xrtAhoDestroy(xrtaho* pAutomaton);



/* 添加非空模式；忽略大小写时拒绝 ASCII 大小写等价的重复模式。 */
XRT_API bool xrtAhoAdd(xrtaho* pAutomaton, xstrview Pattern);



/* 批量添加模式；失败时已成功添加的前置模式保持有效。 */
XRT_API bool xrtAhoAddMany(
	xrtaho* pAutomaton,
	const xstrview* arrPatterns,
	size_t iCount
);



/* 构建不可变 failure link；成功后不能继续添加模式。 */
XRT_API bool xrtAhoBuild(xrtaho* pAutomaton);



/* 返回自动机是否已经完成构建。 */
XRT_API bool xrtAhoBuilt(const xrtaho* pAutomaton);



/* 返回已注册模式数量。 */
XRT_API size_t xrtAhoPatternCount(const xrtaho* pAutomaton);



/* 返回指定模式的借用视图；索引越界时返回空视图并设置错误。 */
XRT_API xstrview xrtAhoPattern(const xrtaho* pAutomaton, size_t iIndex);



/* 获取模式、节点、转移和有效分配量快照。 */
XRT_API bool xrtAhoGetInfo(const xrtaho* pAutomaton, xrtahoinfo* pInfo);



/* 搜索全部匹配；结果按起点、结束位置、模式索引升序排序。 */
XRT_API xrtahoresult* xrtAhoSearch(xrtaho* pAutomaton, xstrview Text);



/* 搜索确定性的左优先最长非重叠匹配。 */
XRT_API xrtahoresult* xrtAhoSearchNonOverlapping(
	xrtaho* pAutomaton,
	xstrview Text
);



/* 无结果数组地查找排序规则下的第一个匹配。 */
XRT_API bool xrtAhoFind(
	xrtaho* pAutomaton,
	xstrview Text,
	xrtahomatch* pMatch,
	bool* pFound
);



/* 无结果数组地判断是否存在任意匹配。 */
XRT_API bool xrtAhoContains(
	xrtaho* pAutomaton,
	xstrview Text,
	bool* pContains
);



/* 无结果数组地统计全部重叠匹配。 */
XRT_API bool xrtAhoCount(
	xrtaho* pAutomaton,
	xstrview Text,
	uint64* pCount
);



/* 返回结果数量。 */
XRT_API size_t xrtAhoResultCount(const xrtahoresult* pResult);



/* 读取一个匹配；返回的 Pattern 由结果持有的自动机引用保证有效。 */
XRT_API bool xrtAhoResultAt(
	const xrtahoresult* pResult,
	size_t iIndex,
	xrtahomatch* pMatch
);



/* 释放结果集及其自动机引用。 */
XRT_API void xrtAhoResultDestroy(xrtahoresult* pResult);



/* 创建可跨输入分块保持 failure 状态的增量扫描器。 */
XRT_API xrtahostate* xrtAhoStateCreate(xrtaho* pAutomaton);



/* 重置扫描位置、匹配计数和待发送结果，但保留自动机引用。 */
XRT_API void xrtAhoStateReset(xrtahostate* pState);



/* 返回当前已消费的总字节数。 */
XRT_API uint64 xrtAhoStateOffset(const xrtahostate* pState);



/* 返回当前已经发送的匹配数量。 */
XRT_API uint64 xrtAhoStateMatches(const xrtahostate* pState);



/*
	扫描一个分块。主动停止时 pConsumed 返回已经消费的本分块字节数，
	调用方可把剩余切片再次传入；同一字节的待发送重叠结果不会丢失。
*/
XRT_API xrtahoscanstatus xrtAhoStateFeed(
	xrtahostate* pState,
	xstrview Chunk,
	xrtahomatchproc pMatch,
	ptr pUserData,
	size_t* pConsumed
);



/* 释放增量扫描器及其自动机引用。 */
XRT_API void xrtAhoStateDestroy(xrtahostate* pState);



/* 扫描完整视图；回调停止与实际失败由返回状态明确区分。 */
XRT_API xrtahoscanstatus xrtAhoScan(
	xrtaho* pAutomaton,
	xstrview Text,
	xrtahomatchproc pMatch,
	ptr pUserData
);



/* 合并重叠区间并用固定文本替换；结果由 xrtFree 释放。 */
XRT_API str xrtAhoReplace(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Replacement,
	size_t* pOutputSize
);



/* 按匹配区间中的字节数重复遮盖文本；允许任意二进制输入。 */
XRT_API str xrtAhoMaskBytes(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Mask,
	size_t* pOutputSize
);



/* 按匹配区间中的 UTF-8 标量数重复遮盖文本；无效 UTF-8 会失败。 */
XRT_API str xrtAhoMask(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Mask,
	size_t* pOutputSize
);



/* 合并重叠区间并用开始、结束文本包裹；结果由 xrtFree 释放。 */
XRT_API str xrtAhoHighlight(
	xrtaho* pAutomaton,
	xstrview Text,
	xstrview Open,
	xstrview Close,
	size_t* pOutputSize
);

#ifdef __cplusplus
}
#endif

#endif
