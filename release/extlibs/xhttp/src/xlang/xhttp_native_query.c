#include "xhttp_native_internal.h"



/* 生成默认严格或浏览器兼容 percent 策略。 */
static xqueryparamsconfig xhttpNativeParamsConfig(bool bLenient)
{
	xqueryparamsconfig Config;

	xrtQueryParamsConfigInit(&Config);
	if ( bLenient ) {
		Config.Flags |= XQUERY_PARAMS_LENIENT_PERCENT;
	}
	return Config;
}



/* 创建空的拥有型 QueryParams。 */
XLANG_XHTTP_EXPORT void* xhttp_params_create(bool bLenient)
{
	xqueryparamsconfig Config = xhttpNativeParamsConfig(bLenient);

	return xrtQueryParamsCreate(&Config);
}



/* 无参数构造使用严格 percent 策略。 */
XLANG_XHTTP_EXPORT void* xhttp_params_create_default(void)
{
	return xhttp_params_create(false);
}



/* 解析 form-urlencoded 文本并保留重复键、顺序和无等号项。 */
XLANG_XHTTP_EXPORT void* xhttp_params_parse(
	const char* sText,
	size_t iSize,
	bool bLenient
)
{
	xqueryparamsconfig Config = xhttpNativeParamsConfig(bLenient);
	size_t iError = 0u;

	return xrtQueryParamsParse(
		(xstrview){ sText, iSize },
		&Config,
		&iError
	);
}



/* point 构造直接接管 QueryParams 句柄。 */
XLANG_XHTTP_EXPORT void* xhttp_params_wrap(void* pHandle)
{
	return pHandle;
}



/* 深复制 QueryParams。 */
XLANG_XHTTP_EXPORT void* xhttp_params_clone(void* pHandle)
{
	return pHandle != NULL ?
		xrtQueryParamsClone((xqueryparams*)pHandle) : NULL;
}



/* 释放拥有型 QueryParams。 */
XLANG_XHTTP_EXPORT void xhttp_params_destroy(void* pHandle)
{
	xrtQueryParamsDestroy((xqueryparams*)pHandle);
}



/* 返回可传给 XRT 底层 API 的原生容器句柄。 */
XLANG_XHTTP_EXPORT void* xhttp_params_native(void* pHandle)
{
	return pHandle;
}



/* 返回 pair 数量或有效已解码字节数。 */
XLANG_XHTTP_EXPORT unsigned long long xhttp_params_value(
	void* pHandle,
	long long iField
)
{
	xqueryparams* pParams = (xqueryparams*)pHandle;

	if ( pParams == NULL ) {
		return 0u;
	}
	return iField == 0 ?
		xrtQueryParamsCount(pParams) :
		xrtQueryParamsBytes(pParams);
}



/* 清空容器但保留容量。 */
XLANG_XHTTP_EXPORT void xhttp_params_clear(void* pHandle)
{
	xrtQueryParamsClear((xqueryparams*)pHandle);
}



/* 预留 pair 和有效字节容量。 */
XLANG_XHTTP_EXPORT bool xhttp_params_reserve(
	void* pHandle,
	size_t iPairs,
	size_t iBytes
)
{
	return (pHandle != NULL) && xrtQueryParamsReserve(
		(xqueryparams*)pHandle,
		iPairs,
		iBytes
	);
}



/* 追加或设置一个显式保留 HAS_VALUE 状态的 pair。 */
static bool xhttpNativeParamsWritePair(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	bool bHasValue,
	bool bSet
)
{
	xquerypair Pair;

	if ( pHandle == NULL ) {
		return false;
	}
	memset(&Pair, 0, sizeof(Pair));
	Pair.Key = (xstrview){ sName, iName };
	Pair.Value = (xstrview){ sValue, iValue };
	if ( bHasValue ) {
		Pair.Flags |= XQUERY_HAS_VALUE;
	}
	return bSet ?
		xrtQueryParamsSetPair((xqueryparams*)pHandle, Pair) :
		xrtQueryParamsAppendPair((xqueryparams*)pHandle, Pair);
}



XLANG_XHTTP_EXPORT bool xhttp_params_append(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	bool bHasValue
)
{
	return xhttpNativeParamsWritePair(
		pHandle,
		sName,
		iName,
		sValue,
		iValue,
		bHasValue,
		false
	);
}



XLANG_XHTTP_EXPORT bool xhttp_params_set(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	bool bHasValue
)
{
	return xhttpNativeParamsWritePair(
		pHandle,
		sName,
		iName,
		sValue,
		iValue,
		bHasValue,
		true
	);
}



/* 删除、计数或查询同名项。 */
XLANG_XHTTP_EXPORT unsigned long long xhttp_params_name_value(
	void* pHandle,
	const char* sName,
	size_t iName,
	long long iOperation
)
{
	xqueryparams* pParams = (xqueryparams*)pHandle;
	xstrview Name = { sName, iName };

	if ( pParams == NULL ) {
		return 0u;
	}
	if ( iOperation == 0 ) {
		return xrtQueryParamsRemove(pParams, Name);
	}
	if ( iOperation == 1 ) {
		return xrtQueryParamsCountName(pParams, Name);
	}
	return xrtQueryParamsHas(pParams, Name) ? 1u : 0u;
}



/* 读取指定位置的借用 pair。 */
static bool xhttpNativeParamsAt(
	void* pHandle,
	size_t iIndex,
	xquerypair* pPair
)
{
	if ( (pHandle == NULL) || (pPair == NULL) ) {
		xhttpNativeError(
			XERR_ARGUMENT,
			"read-query-parameter",
			"Query parameter handle is invalid"
		);
		return false;
	}
	if ( !xrtQueryParamsAt(
		(xqueryparams*)pHandle, iIndex, pPair
	) ) {
		return false;
	}
	return true;
}



/* 复制指定位置的名称或值。 */
XLANG_XHTTP_EXPORT void* xhttp_params_at_text(
	void* pHandle,
	unsigned long long iIndex,
	long long iField
)
{
	xquerypair Pair;
	xstrview Text;

	if ( !xhttpNativeParamsAt(pHandle, (size_t)iIndex, &Pair) ) {
		return NULL;
	}
	Text = iField == 0 ? Pair.Key : Pair.Value;
	return xhttpNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 查询指定位置是否包含显式等号和值。 */
XLANG_XHTTP_EXPORT bool xhttp_params_at_has_value(
	void* pHandle,
	unsigned long long iIndex
)
{
	xquerypair Pair;

	return xhttpNativeParamsAt(pHandle, (size_t)iIndex, &Pair) &&
		((Pair.Flags & XQUERY_HAS_VALUE) != 0u);
}



/* 复制第 N 个同名值；不存在时返回空句柄且不制造错误。 */
XLANG_XHTTP_EXPORT void* xhttp_params_get(
	void* pHandle,
	const char* sName,
	size_t iName,
	unsigned long long iOccurrence
)
{
	xqueryparams* pParams = (xqueryparams*)pHandle;
	xquerypair Pair;
	size_t iIndex = 0u;
	uint64 iFound = 0u;
	xquerynext Next;

	if ( pParams == NULL ) {
		return NULL;
	}
	while ( (Next = xrtQueryParamsFind(
		pParams,
		(xstrview){ sName, iName },
		&iIndex,
		&Pair
	)) == XQUERY_NEXT_ITEM ) {
		if ( iFound == iOccurrence ) {
			if ( (Pair.Flags & XQUERY_HAS_VALUE) == 0u ) {
				return NULL;
			}
			return xhttpNativeBuffer((xbytesview){
				(cbytes)Pair.Value.Data,
				Pair.Value.Size
			});
		}
		iFound++;
	}
	return NULL;
}



/* 原子追加一段 form-urlencoded 文本。 */
XLANG_XHTTP_EXPORT bool xhttp_params_parse_append(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	size_t iError = 0u;

	return (pHandle != NULL) && xrtQueryParamsParseAppend(
		(xqueryparams*)pHandle,
		(xstrview){ sText, iSize },
		&iError
	);
}



/* 稳定排序或紧凑化拥有型字符串区。 */
XLANG_XHTTP_EXPORT bool xhttp_params_maintain(
	void* pHandle,
	long long iOperation
)
{
	if ( pHandle == NULL ) {
		return false;
	}
	return iOperation == 0 ?
		xrtQueryParamsSort((xqueryparams*)pHandle) :
		xrtQueryParamsCompact((xqueryparams*)pHandle);
}



/* 构建 form-urlencoded 文本并把所有权移交给 Buffer。 */
static void* xhttp_params_build_impl(void* pHandle, size_t* outputSize
)
{
	str sText;
	size_t iSize = 0u;

	if ( pHandle == NULL ) {
		return NULL;
	}
	sText = xrtQueryParamsBuild((xqueryparams*)pHandle, &iSize);
	if ( outputSize != NULL ) *outputSize = iSize;
	return sText;
}

XLANG_XHTTP_EXPORT void* xhttp_params_build_owned(void* pHandle)
{
    return xhttp_params_build_impl(pHandle, NULL);
}

/* Compatibility C entry continues to return an owned Buffer. */
XLANG_XHTTP_EXPORT void* xhttp_params_build(void* pHandle)
{
    size_t size = 0;
    str text = (str)xhttp_params_build_impl(pHandle, &size);
    return text != NULL ? xhttpNativeBufferTake((bytes)text, size, size + 1u) : NULL;
}



/* 对单段字节执行 form-urlencoded 编码。 */
static void* xhttp_form_encode_build_impl(
	const void* pData,
	size_t iSize, size_t* outputSize
)
{
	str sText;
	size_t iOutput = 0u;

	sText = xrtFormEncodeNew(pData, iSize, &iOutput);
	if ( outputSize != NULL ) *outputSize = iOutput;
	return sText;
}

XLANG_XHTTP_EXPORT void* xhttp_form_encode_owned(
	const void* pData,
	size_t iSize
)
{
    return xhttp_form_encode_build_impl(pData, iSize, NULL);
}

/* Compatibility C entry continues to return an owned Buffer. */
XLANG_XHTTP_EXPORT void* xhttp_form_encode(
	void* pData,
	size_t iSize
)
{
    size_t size = 0;
    str text = (str)xhttp_form_encode_build_impl(pData, iSize, &size);
    return text != NULL ? xhttpNativeBufferTake((bytes)text, size, size + 1u) : NULL;
}



/* 文本便利入口保持稳定字符串 ABI。 */
XLANG_XHTTP_EXPORT void* xhttp_form_encode_text(
	const char* sText,
	size_t iSize
)
{
	/* point 的 C ABI 为 void*；编码器只读这段借用文本。 */
	return xhttp_form_encode((void*)sText, iSize);
}



/* 严格解码 form-urlencoded 文本并保留任意二进制字节。 */
XLANG_XHTTP_EXPORT void* xhttp_form_decode(
	const char* sText,
	size_t iSize
)
{
	bytes pData;
	size_t iOutput = 0u;

	pData = xrtFormDecodeNew((xstrview){ sText, iSize }, &iOutput);
	return pData != NULL ?
		xhttpNativeBufferTake(pData, iOutput, iOutput + 1u) : NULL;
}



/* 无分配验证并统计原始 Query 项。 */
XLANG_XHTTP_EXPORT long long xhttp_query_count(
	const char* sText,
	size_t iSize
)
{
	size_t iCount = 0u;

	return xrtQueryValidate(
		(xstrview){ sText, iSize },
		NULL,
		&iCount
	) ? (long long)iCount : -1;
}

XLANG_XHTTP_EXPORT void* xhttp_form_encode_text_owned(const char* text, size_t size)
{
    return xhttp_form_encode_owned(text, size);
}

XLANG_XHTTP_EXPORT void* xhttp_form_encode_owned_abi(void* data, size_t size)
{
    return xhttp_form_encode_owned(data, size);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_EXPORT void xhttp_params_destroy_abi(void* handle)
{
    (void)xhttp_params_destroy(handle);
}
