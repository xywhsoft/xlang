#ifndef XRT_QUERY_H
#define XRT_QUERY_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_QUERY_CODEC) && \
	(!defined(XHTTP_FEATURE_QUERY) || !defined(XRT_FEATURE_CODEC_PERCENT))
	#error "XRT query codec requires query and codec_percent"
#endif



#if defined(XHTTP_FEATURE_QUERY)

/* 查询项包含显式等号和值；值视图为空时仍可由该标志区分 a 与 a=。 */
#define XQUERY_HAS_VALUE UINT32_C(0x00000001)



/* 查询迭代结果明确区分条目、正常结束和调用错误。 */
typedef enum xquerynext {
	XQUERY_NEXT_ERROR = -1,
	XQUERY_NEXT_END = 0,
	XQUERY_NEXT_ITEM = 1
} xquerynext;



/* 原始查询项借用输入文本，不执行 percent 或 form 解码。 */
typedef struct xquerypair {
	uint32 Flags;
	xstrview Key;
	xstrview Value;
} xquerypair;



/* 查询限额中的零表示不限制；底层迭代器本身没有隐藏上限。 */
typedef struct xquerylimits {
	size_t MaxPairs;
	size_t MaxKey;
	size_t MaxValue;
} xquerylimits;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_QUERY)

/*
	读取下一个非空查询段；Offset 初始为零，输入可以包含一个前导问号。
	连续、前导和尾随 & 产生的空段被跳过，空 key 和空 value 仍被保留。
*/
XRT_API xquerynext xrtQueryNext(
	xstrview Query,
	size_t* pOffset,
	xquerypair* pPair
);



/* 统计查询项；正常空查询返回零，失败时不修改 Count。 */
XRT_API bool xrtQueryCount(xstrview Query, size_t* pCount);



/* 按显式 pair/key/value 限额验证结构；Limits 为空时仅统计。 */
XRT_API bool xrtQueryValidate(
	xstrview Query,
	const xquerylimits* pLimits,
	size_t* pCount
);



/*
	从 Offset 指定位置查找下一个原始 key；可重复调用以遍历重复 key。
	未找到返回 XQUERY_NEXT_END，比较按字节区分大小写。
*/
XRT_API xquerynext xrtQueryFind(
	xstrview Query,
	xstrview Key,
	size_t* pOffset,
	xquerypair* pPair
);



/* 写出不含前导问号和零结尾的已编码原始查询；空输出可查询精确长度。 */
XRT_API bool xrtQueryRawWrite(
	const xquerypair* pPairs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建零结尾的已编码原始查询；返回值由 xrtFree 释放。 */
XRT_API str xrtQueryRawBuild(
	const xquerypair* pPairs,
	size_t iCount,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_QUERY_CODEC)

/* 按 RFC 3986 编码键和值并写出查询；空输出可查询不含零结尾的精确长度。 */
XRT_API bool xrtQueryWrite(
	const xquerypair* pPairs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 按 RFC 3986 编码键和值并构建零结尾查询；返回值由 xrtFree 释放。 */
XRT_API str xrtQueryBuild(
	const xquerypair* pPairs,
	size_t iCount,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
