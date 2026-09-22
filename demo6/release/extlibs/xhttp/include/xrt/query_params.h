#ifndef XRT_QUERY_PARAMS_H
#define XRT_QUERY_PARAMS_H

#include "form.h"
#include "query.h"



#if defined(XHTTP_FEATURE_QUERY_PARAMS) && \
	(!defined(XHTTP_FEATURE_QUERY) || \
	 !defined(XHTTP_FEATURE_FORM_URLENCODED))
	#error "XRT query params requires query and form-urlencoded support"
#endif



#if defined(XHTTP_FEATURE_QUERY_PARAMS)

/* 宽松 percent 模式保留无效百分号，适合兼容浏览器 URLSearchParams 输入。 */
#define XQUERY_PARAMS_LENIENT_PERCENT UINT32_C(0x00000001)



/* QueryParams 拥有有序、区分大小写且允许重复的已解码名称和值。 */
typedef struct xqueryparams xqueryparams;



/* 容器限额只计算有效 pair 与已解码字节，不计算增长余量和废弃存储。 */
typedef struct xqueryparamsconfig {
	size_t InitialPairs;
	size_t InitialBytes;
	size_t MaxPairs;
	size_t MaxName;
	size_t MaxValue;
	size_t MaxBytes;
	uint32 Flags;
} xqueryparamsconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_QUERY_PARAMS)

/* 初始化适合 URL 查询和表单数据的容量、限额与严格 percent 策略。 */
XRT_API void xrtQueryParamsConfigInit(xqueryparamsconfig* pConfig);



/* 创建拥有型空容器；配置为空时使用默认值。 */
XRT_API xqueryparams* xrtQueryParamsCreate(
	const xqueryparamsconfig* pConfig
);



/* 按 form-urlencoded 规则解析并创建容器；成功时 ErrorOffset 等于输入长度。 */
XRT_API xqueryparams* xrtQueryParamsParse(
	xstrview Text,
	const xqueryparamsconfig* pConfig,
	size_t* pErrorOffset
);



/* 深复制全部 pair、配置和有效字节。 */
XRT_API xqueryparams* xrtQueryParamsClone(
	const xqueryparams* pParams
);



/* 销毁容器；空指针是安全的空操作。 */
XRT_API void xrtQueryParamsDestroy(xqueryparams* pParams);



/* 删除全部 pair，但保留已经分配的容量供后续复用。 */
XRT_API void xrtQueryParamsClear(xqueryparams* pParams);



/* 预留至少指定 pair 数量和有效已解码字节容量。 */
XRT_API bool xrtQueryParamsReserve(
	xqueryparams* pParams,
	size_t iPairs,
	size_t iBytes
);



/* 返回当前 pair 数量；空容器返回零。 */
XRT_API size_t xrtQueryParamsCount(const xqueryparams* pParams);



/* 返回全部有效名称和值的已解码字节数；空容器返回零。 */
XRT_API size_t xrtQueryParamsBytes(const xqueryparams* pParams);



/* 追加一个拥有型 pair，并保留 HAS_VALUE 状态。 */
XRT_API bool xrtQueryParamsAppendPair(
	xqueryparams* pParams,
	xquerypair Pair
);



/* 追加一个始终带等号和值的常用 pair。 */
XRT_API bool xrtQueryParamsAppend(
	xqueryparams* pParams,
	xstrview Name,
	xstrview Value
);



/* 用一个 pair 替换全部同名项；首个同名位置保持不变。 */
XRT_API bool xrtQueryParamsSetPair(
	xqueryparams* pParams,
	xquerypair Pair
);



/* 用一个始终带值的常用 pair 替换全部同名项。 */
XRT_API bool xrtQueryParamsSet(
	xqueryparams* pParams,
	xstrview Name,
	xstrview Value
);



/* 删除全部同名项并返回删除数量；名称按字节区分大小写。 */
XRT_API size_t xrtQueryParamsRemove(
	xqueryparams* pParams,
	xstrview Name
);



/* 返回同名项数量；名称按字节区分大小写。 */
XRT_API size_t xrtQueryParamsCountName(
	const xqueryparams* pParams,
	xstrview Name
);



/* 判断是否存在同名项；无匹配不是错误。 */
XRT_API bool xrtQueryParamsHas(
	const xqueryparams* pParams,
	xstrview Name
);



/* 复制指定位置的借用 pair；视图在下一次容器修改前有效。 */
XRT_API bool xrtQueryParamsAt(
	const xqueryparams* pParams,
	size_t iIndex,
	xquerypair* pPair
);



/* 复制首个同名 pair；无匹配返回 false 且不是错误。 */
XRT_API bool xrtQueryParamsGet(
	const xqueryparams* pParams,
	xstrview Name,
	xquerypair* pPair
);



/* 从 Index 开始查找下一个同名项；Index 必须不大于 Count，ITEM 时指向下一位置。 */
XRT_API xquerynext xrtQueryParamsFind(
	const xqueryparams* pParams,
	xstrview Name,
	size_t* pIndex,
	xquerypair* pPair
);



/* 原子追加一段 form-urlencoded 文本；失败时原容器保持逻辑不变。 */
XRT_API bool xrtQueryParamsParseAppend(
	xqueryparams* pParams,
	xstrview Text,
	size_t* pErrorOffset
);



/* 按名称字节稳定排序，重复名称保持原先顺序。 */
XRT_API bool xrtQueryParamsSort(xqueryparams* pParams);



/* 丢弃修改产生的废弃字节，并把字符串区收缩为当前有效内容。 */
XRT_API bool xrtQueryParamsCompact(xqueryparams* pParams);



/* 按 form-urlencoded 规则写出且不附加零字符；空输出可查询精确长度。 */
XRT_API bool xrtQueryParamsWrite(
	const xqueryparams* pParams,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建零结尾文本；返回值由 xrtFree 释放。 */
XRT_API str xrtQueryParamsBuild(
	const xqueryparams* pParams,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
