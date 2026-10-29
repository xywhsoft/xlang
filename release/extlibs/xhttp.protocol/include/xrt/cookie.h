#ifndef XRT_COOKIE_H
#define XRT_COOKIE_H

#include <xrt.h>

#if defined(XHTTP_FEATURE_SET_COOKIE)
#endif



#if defined(XHTTP_FEATURE_COOKIE) && !defined(XRT_FEATURE_HTTP)
	#error "XRT cookie requires XRT_FEATURE_HTTP"
#endif

#if defined(XHTTP_FEATURE_SET_COOKIE) && \
	(!defined(XHTTP_FEATURE_COOKIE) || !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRT set-cookie requires cookie and time_text"
#endif



#if defined(XHTTP_FEATURE_COOKIE)

/* Cookie 请求字段中的名称和值均为不要求零结尾的借用视图。 */
typedef struct xcookiepair {
	xstrview Name;
	xstrview Value;
} xcookiepair;



/* Cookie 扫描结果明确区分字段项、正常结束和语法错误。 */
typedef enum xcookienext {
	XCOOKIE_NEXT_ERROR = -1,
	XCOOKIE_NEXT_END = 0,
	XCOOKIE_NEXT_ITEM = 1
} xcookienext;



/* Cookie 限额中的零表示不限制，Bytes 约束完整字段值的原始长度。 */
typedef struct xcookielimits {
	size_t MaxPairs;
	size_t MaxName;
	size_t MaxValue;
	size_t MaxBytes;
} xcookielimits;

#endif



#if defined(XHTTP_FEATURE_SET_COOKIE)

/* RFC 10025 用户代理接收算法允许的 name/value 总长度。 */
#define XSET_COOKIE_MAX_PAIR_BYTES		4096u

/* RFC 10025 用户代理接收算法允许处理的单个属性值长度。 */
#define XSET_COOKIE_MAX_ATTRIBUTE_VALUE	1024u



/* SameSite 的 Default 表示缺失或无法识别的属性值。 */
typedef enum xcookiesamesite {
	XCOOKIE_SAME_SITE_DEFAULT = 0,
	XCOOKIE_SAME_SITE_LAX,
	XCOOKIE_SAME_SITE_STRICT,
	XCOOKIE_SAME_SITE_NONE
} xcookiesamesite;



/* Priority 是已部署的扩展属性；Unspecified 表示没有有效属性。 */
typedef enum xcookiepriority {
	XCOOKIE_PRIORITY_UNSPECIFIED = 0,
	XCOOKIE_PRIORITY_LOW,
	XCOOKIE_PRIORITY_MEDIUM,
	XCOOKIE_PRIORITY_HIGH
} xcookiepriority;



#define XCOOKIE_ATTRIBUTE_HAS_VALUE	UINT32_C(0x00000001)

/* Set-Cookie 属性保留值存在位，因此 Foo 与 Foo= 不会混淆。 */
typedef struct xcookieattribute {
	uint32 Flags;
	xstrview Name;
	xstrview Value;
} xcookieattribute;



/* Set-Cookie 属性扫描结果明确区分属性、正常结束和输入错误。 */
typedef enum xcookieattributenext {
	XCOOKIE_ATTRIBUTE_ERROR = -1,
	XCOOKIE_ATTRIBUTE_END = 0,
	XCOOKIE_ATTRIBUTE_ITEM = 1
} xcookieattributenext;



#define XSET_COOKIE_HAS_DOMAIN		UINT32_C(0x00000001)
#define XSET_COOKIE_HAS_PATH			UINT32_C(0x00000002)
#define XSET_COOKIE_HAS_EXPIRES		UINT32_C(0x00000004)
#define XSET_COOKIE_HAS_MAX_AGE		UINT32_C(0x00000008)
#define XSET_COOKIE_HAS_SAME_SITE	UINT32_C(0x00000010)
#define XSET_COOKIE_SECURE			UINT32_C(0x00000020)
#define XSET_COOKIE_HTTP_ONLY		UINT32_C(0x00000040)
#define XSET_COOKIE_PARTITIONED		UINT32_C(0x00000080)
#define XSET_COOKIE_HAS_PRIORITY	UINT32_C(0x00000100)



/*
	解析结果借用输入并把原始属性区保存在 RawAttributes 中。
	构建时 RawAttributes 必须为空，Extensions 则提供受校验的扩展属性。
*/
typedef struct xsetcookie {
	uint32 Flags;
	xstrview Name;
	xstrview Value;
	xstrview Domain;
	xstrview Path;
	xstrview RawAttributes;
	xtime Expires;
	int64 MaxAge;
	xcookiesamesite SameSite;
	xcookiepriority Priority;
	const xcookieattribute* Extensions;
	size_t ExtensionCount;
} xsetcookie;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_COOKIE)

/* 逐项扫描完整 Cookie 字段值；成功项和失败均只在返回时提交 Offset。 */
XRT_API xcookienext xrtCookieNext(
	xstrview Text,
	size_t* pOffset,
	xcookiepair* pPair
);



/* 完整校验 Cookie 字段值并应用显式限额；成功时可返回 pair 数量。 */
XRT_API bool xrtCookieValidate(
	xstrview Text,
	const xcookielimits* pLimits,
	size_t* pCount
);



/*
	完整校验字段后，从 Offset 开始查找下一个区分大小写的 Cookie 名称。
	重复调用可遍历同名 Cookie，未找到返回 XCOOKIE_NEXT_END。
*/
XRT_API xcookienext xrtCookieFind(
	xstrview Text,
	xstrview Name,
	size_t* pOffset,
	xcookiepair* pPair
);



/*
	完整预检后把借用 pair 写入调用方数组；Pairs 为空可查询数量。
	容量不足时 Count 返回所需数量，数组保持不变。
*/
XRT_API bool xrtCookieParse(
	xstrview Text,
	xcookiepair* pPairs,
	size_t iCapacity,
	size_t* pCount,
	const xcookielimits* pLimits
);



/* 写出不含字段名和零结尾的规范 Cookie 字段值；空输出可查询精确长度。 */
XRT_API bool xrtCookieWrite(
	const xcookiepair* pPairs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建零结尾 Cookie 字段值；返回值由 xrtFree 释放。 */
XRT_API str xrtCookieBuild(
	const xcookiepair* pPairs,
	size_t iCount,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_SET_COOKIE)

/* 扫描 xsetcookie.RawAttributes；未知属性和值存在位会原样暴露。 */
XRT_API xcookieattributenext xrtSetCookieAttributeNext(
	xstrview Text,
	size_t* pOffset,
	xcookieattribute* pAttribute
);



/* 按 RFC 10025 宽松 cookie-date 算法解析 UTC 时间。 */
XRT_API bool xrtCookieDateParse(xstrview Text, xtime* pTime);



/* 按 RFC 10025 服务器生成语法严格校验一个 Set-Cookie 字段值。 */
XRT_API bool xrtSetCookieValidate(xstrview Text);



/*
	按 RFC 10025 用户代理接收算法宽松解析 Set-Cookie 字段值。
	无效的单个已知属性被忽略，禁止控制字节和超长 cookie-pair 会使整体失败。
*/
XRT_API bool xrtSetCookieParse(xstrview Text, xsetcookie* pCookie);



/*
	从结构化数据写出不含字段名和零结尾的 Set-Cookie 字段值。
	结构化构建器执行前缀、SameSite=None 与 Partitioned 的安全约束。
*/
XRT_API bool xrtSetCookieWrite(
	const xsetcookie* pCookie,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建零结尾 Set-Cookie 字段值；返回值由 xrtFree 释放。 */
XRT_API str xrtSetCookieBuild(
	const xsetcookie* pCookie,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif

