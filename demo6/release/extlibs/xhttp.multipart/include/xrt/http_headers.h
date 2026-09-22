#ifndef XRT_HTTP_HEADERS_H
#define XRT_HTTP_HEADERS_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_HTTP_HEADERS) && !defined(XRT_FEATURE_HTTP)
	#error "xhttp owned Headers require XRT HTTP protocol support"
#endif



#if defined(XHTTP_FEATURE_HTTP_HEADERS)

typedef struct xhttpheaders xhttpheaders;



/* 零上限不表示无限；默认配置由初始化函数给出明确的安全边界。 */
typedef struct xhttpheadersconfig {
	size_t InitialFields;
	size_t InitialBytes;
	size_t MaxFields;
	size_t MaxName;
	size_t MaxValue;
	size_t MaxBytes;
} xhttpheadersconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_HEADERS)

/* 初始化拥有型 Header 容器的默认容量与安全上限。 */
XRT_API void xrtHttpHeadersConfigInit(xhttpheadersconfig* pConfig);



/* 创建拥有全部名称和值副本的 Header 容器。 */
XRT_API xhttpheaders* xrtHttpHeadersCreate(
	const xhttpheadersconfig* pConfig
);



/* 释放 Header 容器；空指针安全。 */
XRT_API void xrtHttpHeadersDestroy(xhttpheaders* pHeaders);



/* 清空内容并保留容量供后续报文复用。 */
XRT_API void xrtHttpHeadersClear(xhttpheaders* pHeaders);



/* 预留字段数量和名称、值的逻辑字节容量。 */
XRT_API bool xrtHttpHeadersReserve(
	xhttpheaders* pHeaders,
	size_t iFields,
	size_t iBytes
);



/* 回收删除和替换遗留的字符串空洞。 */
XRT_API bool xrtHttpHeadersCompact(xhttpheaders* pHeaders);



/* 返回字段数量。 */
XRT_API size_t xrtHttpHeadersCount(const xhttpheaders* pHeaders);



/* 返回全部有效名称和值的总字节数。 */
XRT_API size_t xrtHttpHeadersBytes(const xhttpheaders* pHeaders);



/* 返回连续、借用的字段数组；容器修改后失效。 */
XRT_API const xhttpfield* xrtHttpHeadersData(
	const xhttpheaders* pHeaders
);



/* 返回指定位置的借用字段，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpHeadersAt(
	const xhttpheaders* pHeaders,
	size_t iIndex
);



/* 追加允许重名的字段并拥有名称和值副本。 */
XRT_API bool xrtHttpHeadersAdd(
	xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value
);



/* 替换首个同名字段并移除其余同名字段。 */
XRT_API bool xrtHttpHeadersSet(
	xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value
);



/* 移除全部同名字段并返回移除数量。 */
XRT_API size_t xrtHttpHeadersRemove(
	xhttpheaders* pHeaders,
	xstrview Name
);



/* 判断是否存在同名字段。 */
XRT_API bool xrtHttpHeadersHas(
	const xhttpheaders* pHeaders,
	xstrview Name
);



/* 返回同名字段数量。 */
XRT_API size_t xrtHttpHeadersCountName(
	const xhttpheaders* pHeaders,
	xstrview Name
);



/* 返回首个同名借用字段。 */
XRT_API const xhttpfield* xrtHttpHeadersGet(
	const xhttpheaders* pHeaders,
	xstrview Name
);



/* 只在同名字段唯一时返回条目，重复字段报告值错误。 */
XRT_API xhttpnext xrtHttpHeadersGetUnique(
	const xhttpheaders* pHeaders,
	xstrview Name,
	const xhttpfield** ppField
);



/* 返回第 N 个同名借用字段。 */
XRT_API const xhttpfield* xrtHttpHeadersGetNth(
	const xhttpheaders* pHeaders,
	xstrview Name,
	size_t iIndex
);



/* 有界复制同名字段值，并始终返回完整匹配数量。 */
XRT_API size_t xrtHttpHeadersGetAll(
	const xhttpheaders* pHeaders,
	xstrview Name,
	xstrview* pValues,
	size_t iCapacity
);



/* 深复制 Header 容器及其配置。 */
XRT_API xhttpheaders* xrtHttpHeadersClone(
	const xhttpheaders* pHeaders
);



/* 不分配地交换两个容器拥有的完整状态。 */
XRT_API bool xrtHttpHeadersSwap(
	xhttpheaders* pLeft,
	xhttpheaders* pRight
);



/* 事务性解析并追加字段块，失败时保留原内容。 */
XRT_API bool xrtHttpHeadersAddBlock(
	xhttpheaders* pHeaders,
	xstrview Block,
	size_t* pErrorOffset
);



/* 创建容器并解析完整字段块。 */
XRT_API xhttpheaders* xrtHttpHeadersParse(
	xstrview Block,
	const xhttpheadersconfig* pConfig,
	size_t* pErrorOffset
);



/* 写出字段行和最终空行；空输出用于查询精确长度。 */
XRT_API bool xrtHttpHeadersWrite(
	const xhttpheaders* pHeaders,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建带零结尾的完整字段块。 */
XRT_API str xrtHttpHeadersBuild(
	const xhttpheaders* pHeaders,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
