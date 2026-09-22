#ifndef XRT_INTERNAL_QUERY_H
#define XRT_INTERNAL_QUERY_H

#include "xrt_internal.h"

#include "../../include/xrt/query.h"



#if defined(XHTTP_FEATURE_QUERY)

/* 验证借用字符串视图的空值一致性。 */
bool __xrtQueryViewValid(xstrview Text);



/* 验证 Query 项数组的公共状态，不限制原始分隔符。 */
bool __xrtQueryPairsValid(const xquerypair* pPairs, size_t iCount);



/* 判断元数据范围是否覆盖 Query 项数组或任一借用文本。 */
bool __xrtQueryMetadataOverlap(
	const xquerypair* pPairs,
	size_t iCount,
	const void* pMetadata,
	size_t iMetadataSize
);

/* 共享查询段扫描器允许上层明确控制是否识别前导问号。 */
xquerynext __xrtQueryNext(
	xstrview Query,
	size_t* pOffset,
	xquerypair* pPair,
	bool bQuestion
);

#endif

#endif
