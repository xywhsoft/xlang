#include "xhttp_protocol_native_internal.h"



/* 释放尚未发布的 Cookies 所有权。 */
static void xhttpProtocolCookiesFree(
	xhttp_protocol_cookies* pCookies
)
{
	if ( pCookies == NULL ) {
		return;
	}
	xrtFree(pCookies->Pairs);
	xrtFree(pCookies->Text);
	memset(pCookies, 0, sizeof(*pCookies));
	xrtFree(pCookies);
}



/* 复制并解析 Cookie 字段值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_parse(
	const char* sText,
	size_t iSize
)
{
	xhttp_protocol_cookies* pCookies;
	str sCopy;
	size_t iCount = 0u;

	sCopy = xhttpProtocolNativeTextCopy(sText, iSize);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( !xrtCookieParse(
		(xstrview){ sCopy, iSize }, NULL, 0u, &iCount, NULL
	) ) {
		xrtFree(sCopy);
		return NULL;
	}
	pCookies = (xhttp_protocol_cookies*)xrtCalloc(
		1u, sizeof(*pCookies)
	);
	if ( pCookies == NULL ) {
		xrtFree(sCopy);
		return NULL;
	}
	pCookies->Text = sCopy;
	pCookies->Size = iSize;
	pCookies->Count = iCount;
	if ( iCount != 0u ) {
		pCookies->Pairs = (xcookiepair*)xrtCalloc(
			iCount, sizeof(*pCookies->Pairs)
		);
		if ( pCookies->Pairs == NULL ) {
			xhttpProtocolCookiesFree(pCookies);
			return NULL;
		}
		if ( !xrtCookieParse(
			(xstrview){ sCopy, iSize },
			pCookies->Pairs,
			iCount,
			&iCount,
			NULL
		) ) {
			xhttpProtocolCookiesFree(pCookies);
			return NULL;
		}
	}
	return pCookies;
}



/* point 构造直接接管 Cookies 句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 深复制 Cookies 及其原始文本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_clone(
	void* pHandle
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;

	if ( pCookies == NULL ) {
		return NULL;
	}
	return xhttp_protocol_cookies_parse(
		pCookies->Text, pCookies->Size
	);
}



/* 释放 Cookies 拥有的文本和 pair 数组。 */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_cookies_destroy(
	void* pHandle
)
{
	xhttpProtocolCookiesFree((xhttp_protocol_cookies*)pHandle);
}



/* 返回底层连续 xcookiepair 数组。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_native(
	void* pHandle
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;

	return pCookies != NULL ? pCookies->Pairs : NULL;
}



/* 返回 Cookie pair 数量。 */
XLANG_XHTTP_PROTOCOL_EXPORT unsigned long long xhttp_protocol_cookies_count(
	void* pHandle
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;

	return pCookies != NULL ? pCookies->Count : 0u;
}



/* 复制原始 Cookie 字段值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_text(
	void* pHandle
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;

	return pCookies != NULL ?
		xhttpProtocolNativeBuffer((xbytesview){
			(cbytes)pCookies->Text,
			pCookies->Size
		}) : NULL;
}



/* 复制指定位置 Cookie 的名称或值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_at_text(
	void* pHandle,
	unsigned long long iIndex,
	long long iField
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;
	xstrview Text;

	if ( (pCookies == NULL) || (iIndex >= pCookies->Count) ) {
		xhttpProtocolNativeError(
			XERR_RANGE,
			"read-cookie",
			"Cookie index is out of range"
		);
		return NULL;
	}
	Text = iField == 0 ?
		pCookies->Pairs[iIndex].Name :
		pCookies->Pairs[iIndex].Value;
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 复制第 N 个区分大小写的同名 Cookie 值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_get(
	void* pHandle,
	const char* sName,
	size_t iName,
	unsigned long long iOccurrence
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;
	xcookiepair Pair;
	xcookienext Next;
	size_t iOffset = 0u;
	uint64 iFound = 0u;

	if ( pCookies == NULL ) {
		return NULL;
	}
	while ( (Next = xrtCookieFind(
		(xstrview){ pCookies->Text, pCookies->Size },
		(xstrview){ sName, iName },
		&iOffset,
		&Pair
	)) == XCOOKIE_NEXT_ITEM ) {
		if ( iFound == iOccurrence ) {
			return xhttpProtocolNativeBuffer((xbytesview){
				(cbytes)Pair.Value.Data,
				Pair.Value.Size
			});
		}
		iFound++;
	}
	return NULL;
}



/* 规范构建当前 Cookies 的字段值。 */
static void* xhttp_protocol_cookies_build_impl(
	void* pHandle, size_t* outputSize
)
{
	xhttp_protocol_cookies* pCookies =
		(xhttp_protocol_cookies*)pHandle;
	str sText;
	size_t iSize = 0u;

	if ( pCookies == NULL ) {
		return NULL;
	}
	sText = xrtCookieBuild(
		pCookies->Pairs, pCookies->Count, &iSize
	);
	if ( outputSize != NULL ) *outputSize = iSize;
	return sText;
}

XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_build_owned(
	void* pHandle
)
{
    return xhttp_protocol_cookies_build_impl(pHandle, NULL);
}

/* Compatibility C entry continues to return an owned Buffer. */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_cookies_build(
	void* pHandle
)
{
    size_t size = 0;
    str text = (str)xhttp_protocol_cookies_build_impl(pHandle, &size);
    return text != NULL ? xhttpProtocolNativeBufferTake((bytes)text, size, size + 1u) : NULL;
}



/* 无分配地校验 Cookie 字段并返回 pair 数；失败返回负值。 */
XLANG_XHTTP_PROTOCOL_EXPORT long long xhttp_protocol_cookie_count(
	const char* sText,
	size_t iSize
)
{
	size_t iCount = 0u;

	return xrtCookieValidate(
		(xstrview){ sText, iSize }, NULL, &iCount
	) ? (long long)iCount : -1;
}



/* 复制并解析 Set-Cookie 字段值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_parse(
	const char* sText,
	size_t iSize
)
{
	xhttp_protocol_set_cookie* pCookie;
	str sCopy = xhttpProtocolNativeTextCopy(sText, iSize);

	if ( sCopy == NULL ) {
		return NULL;
	}
	pCookie = (xhttp_protocol_set_cookie*)xrtCalloc(
		1u, sizeof(*pCookie)
	);
	if ( pCookie == NULL ) {
		xrtFree(sCopy);
		return NULL;
	}
	if ( !xrtSetCookieParse(
		(xstrview){ sCopy, iSize }, &pCookie->Value
	) ) {
		xrtFree(sCopy);
		xrtFree(pCookie);
		return NULL;
	}
	pCookie->Text = sCopy;
	pCookie->Size = iSize;
	return pCookie;
}



/* point 构造直接接管 SetCookie 句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 深复制 SetCookie 及其原始文本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_clone(
	void* pHandle
)
{
	xhttp_protocol_set_cookie* pCookie =
		(xhttp_protocol_set_cookie*)pHandle;

	return pCookie != NULL ?
		xhttp_protocol_set_cookie_parse(
			pCookie->Text, pCookie->Size
		) : NULL;
}



/* 释放 SetCookie 拥有的文本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_set_cookie_destroy(
	void* pHandle
)
{
	xhttp_protocol_set_cookie* pCookie =
		(xhttp_protocol_set_cookie*)pHandle;

	if ( pCookie != NULL ) {
		xrtFree(pCookie->Text);
		memset(pCookie, 0, sizeof(*pCookie));
		xrtFree(pCookie);
	}
}



/* 返回可传给 XRT C API 的原生 xsetcookie 句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_native(
	void* pHandle
)
{
	xhttp_protocol_set_cookie* pCookie =
		(xhttp_protocol_set_cookie*)pHandle;

	return pCookie != NULL ? &pCookie->Value : NULL;
}



/* 复制 Set-Cookie 原文或解析后的字符串组件。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_text(
	void* pHandle,
	long long iField
)
{
	xhttp_protocol_set_cookie* pCookie =
		(xhttp_protocol_set_cookie*)pHandle;
	xstrview Text;

	if ( pCookie == NULL ) {
		return NULL;
	}
	switch ( iField ) {
		case 0:
			Text = (xstrview){ pCookie->Text, pCookie->Size };
			break;
		case 1: Text = pCookie->Value.Name; break;
		case 2: Text = pCookie->Value.Value; break;
		case 3: Text = pCookie->Value.Domain; break;
		case 4: Text = pCookie->Value.Path; break;
		case 5: Text = pCookie->Value.RawAttributes; break;
		default:
			xhttpProtocolNativeError(
				XERR_RANGE,
				"read-set-cookie",
				"Set-Cookie component index is out of range"
			);
			return NULL;
	}
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 返回 Set-Cookie 标志、时间或枚举标量。 */
XLANG_XHTTP_PROTOCOL_EXPORT long long xhttp_protocol_set_cookie_value(
	void* pHandle,
	long long iField
)
{
	xhttp_protocol_set_cookie* pCookie =
		(xhttp_protocol_set_cookie*)pHandle;

	if ( pCookie == NULL ) {
		return 0;
	}
	switch ( iField ) {
		case 0: return (long long)pCookie->Value.Flags;
		case 1: return pCookie->Value.Expires;
		case 2: return pCookie->Value.MaxAge;
		case 3: return (long long)pCookie->Value.SameSite;
		case 4: return (long long)pCookie->Value.Priority;
		default: return 0;
	}
}



/* 返回 Set-Cookie 原始属性数量。 */
XLANG_XHTTP_PROTOCOL_EXPORT unsigned long long
xhttp_protocol_set_cookie_attribute_count(void* pHandle)
{
	xhttp_protocol_set_cookie* pCookie =
		(xhttp_protocol_set_cookie*)pHandle;
	xcookieattribute Attribute;
	xcookieattributenext Next;
	size_t iOffset = 0u;
	size_t iCount = 0u;

	if ( pCookie == NULL ) {
		return 0u;
	}
	while ( (Next = xrtSetCookieAttributeNext(
		pCookie->Value.RawAttributes,
		&iOffset,
		&Attribute
	)) == XCOOKIE_ATTRIBUTE_ITEM ) {
		iCount++;
	}
	return Next == XCOOKIE_ATTRIBUTE_END ? iCount : 0u;
}



/* 定位第 N 个 Set-Cookie 原始属性。 */
static bool xhttpProtocolSetCookieAttributeAt(
	xhttp_protocol_set_cookie* pCookie,
	size_t iIndex,
	xcookieattribute* pAttribute
)
{
	xcookieattributenext Next;
	size_t iOffset = 0u;
	size_t i = 0u;

	if ( (pCookie == NULL) || (pAttribute == NULL) ) {
		return false;
	}
	while ( (Next = xrtSetCookieAttributeNext(
		pCookie->Value.RawAttributes,
		&iOffset,
		pAttribute
	)) == XCOOKIE_ATTRIBUTE_ITEM ) {
		if ( i == iIndex ) {
			return true;
		}
		i++;
	}
	xhttpProtocolNativeError(
		XERR_RANGE,
		"read-set-cookie-attribute",
		"Set-Cookie attribute index is out of range"
	);
	return false;
}



/* 复制指定 Set-Cookie 原始属性的名称或值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_attribute_text(
	void* pHandle,
	unsigned long long iIndex,
	long long iField
)
{
	xcookieattribute Attribute;
	xstrview Text;

	if ( !xhttpProtocolSetCookieAttributeAt(
		(xhttp_protocol_set_cookie*)pHandle,
		(size_t)iIndex,
		&Attribute
	) ) {
		return NULL;
	}
	Text = iField == 0 ? Attribute.Name : Attribute.Value;
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 返回指定 Set-Cookie 原始属性是否带等号和值。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_set_cookie_attribute_value(
	void* pHandle,
	unsigned long long iIndex
)
{
	xcookieattribute Attribute;

	return xhttpProtocolSetCookieAttributeAt(
		(xhttp_protocol_set_cookie*)pHandle,
		(size_t)iIndex,
		&Attribute
	) && ((Attribute.Flags & XCOOKIE_ATTRIBUTE_HAS_VALUE) != 0u);
}



/* 语言谓词用 false 表示非法内容；参数错误等运行失败仍保留异常。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_set_cookie_validate(
	const char* sText,
	size_t iSize
)
{
	bool bValid;

	xrtClearError();
	bValid = xrtSetCookieValidate((xstrview){ sText, iSize });
	if ( !bValid && (xrtErrorKind(xrtGetError()) == XERR_VALUE) ) {
		xrtClearError();
	}
	return bValid;
}



/* 从常用属性直接构建 Set-Cookie 字段值。 */
static void* xhttp_protocol_set_cookie_build_impl(
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	const char* sDomain,
	size_t iDomain,
	const char* sPath,
	size_t iPath,
	bool bHasExpires,
	long long iExpires,
	bool bHasMaxAge,
	long long iMaxAge,
	long long iSameSite,
	long long iPriority,
	bool bSecure,
	bool bHttpOnly,
	bool bPartitioned, size_t* outputSize
)
{
	xsetcookie Cookie;
	str sText;
	size_t iSize = 0u;

	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Name = (xstrview){ sName, iName };
	Cookie.Value = (xstrview){ sValue, iValue };
	if ( iDomain != 0u ) {
		Cookie.Flags |= XSET_COOKIE_HAS_DOMAIN;
		Cookie.Domain = (xstrview){ sDomain, iDomain };
	}
	if ( iPath != 0u ) {
		Cookie.Flags |= XSET_COOKIE_HAS_PATH;
		Cookie.Path = (xstrview){ sPath, iPath };
	}
	if ( bHasExpires ) {
		Cookie.Flags |= XSET_COOKIE_HAS_EXPIRES;
		Cookie.Expires = (xtime)iExpires;
	}
	if ( bHasMaxAge ) {
		Cookie.Flags |= XSET_COOKIE_HAS_MAX_AGE;
		Cookie.MaxAge = (int64)iMaxAge;
	}
	if ( iSameSite != XCOOKIE_SAME_SITE_DEFAULT ) {
		Cookie.Flags |= XSET_COOKIE_HAS_SAME_SITE;
		Cookie.SameSite = (xcookiesamesite)iSameSite;
	}
	if ( iPriority != XCOOKIE_PRIORITY_UNSPECIFIED ) {
		Cookie.Flags |= XSET_COOKIE_HAS_PRIORITY;
		Cookie.Priority = (xcookiepriority)iPriority;
	}
	if ( bSecure ) {
		Cookie.Flags |= XSET_COOKIE_SECURE;
	}
	if ( bHttpOnly ) {
		Cookie.Flags |= XSET_COOKIE_HTTP_ONLY;
	}
	if ( bPartitioned ) {
		Cookie.Flags |= XSET_COOKIE_PARTITIONED;
	}
	sText = xrtSetCookieBuild(&Cookie, &iSize);
	if ( outputSize != NULL ) *outputSize = iSize;
	return sText;
}

XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_build_owned(
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	const char* sDomain,
	size_t iDomain,
	const char* sPath,
	size_t iPath,
	bool bHasExpires,
	long long iExpires,
	bool bHasMaxAge,
	long long iMaxAge,
	long long iSameSite,
	long long iPriority,
	bool bSecure,
	bool bHttpOnly,
	bool bPartitioned
)
{
    return xhttp_protocol_set_cookie_build_impl(sName, iName, sValue, iValue, sDomain, iDomain, sPath, iPath, bHasExpires, iExpires, bHasMaxAge, iMaxAge, iSameSite, iPriority, bSecure, bHttpOnly, bPartitioned, NULL);
}

/* Compatibility C entry continues to return an owned Buffer. */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_set_cookie_build(
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	const char* sDomain,
	size_t iDomain,
	const char* sPath,
	size_t iPath,
	bool bHasExpires,
	long long iExpires,
	bool bHasMaxAge,
	long long iMaxAge,
	long long iSameSite,
	long long iPriority,
	bool bSecure,
	bool bHttpOnly,
	bool bPartitioned
)
{
    size_t size = 0;
    str text = (str)xhttp_protocol_set_cookie_build_impl(sName, iName, sValue, iValue, sDomain, iDomain, sPath, iPath, bHasExpires, iExpires, bHasMaxAge, iMaxAge, iSameSite, iPriority, bSecure, bHttpOnly, bPartitioned, &size);
    return text != NULL ? xhttpProtocolNativeBufferTake((bytes)text, size, size + 1u) : NULL;
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_cookies_destroy_abi(void* handle)
{
    (void)xhttp_protocol_cookies_destroy(handle);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_set_cookie_destroy_abi(void* handle)
{
    (void)xhttp_protocol_set_cookie_destroy(handle);
}
