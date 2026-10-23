#include "xrt_plist_internal.h"



static xonce __xplistErrorOnce = XRT_ONCE_INIT;
static xthreadkey* __xplistErrorKey = NULL;

static bool __xplistInitErrorKey(ptr pContext)
{
	(void)pContext;
	__xplistErrorKey = xrtThreadKeyCreate(xrtFree);
	return __xplistErrorKey != NULL;
}

static xrtplisterror* __xplistError(void)
{
	xrtplisterror* pError;

	if ( !xrtOnce(&__xplistErrorOnce, __xplistInitErrorKey, NULL) ||
		__xplistErrorKey == NULL ) {
		return NULL;
	}
	pError = (xrtplisterror*)xrtThreadKeyGet(__xplistErrorKey);
	if ( pError != NULL ) { return pError; }
	pError = (xrtplisterror*)xrtMalloc(sizeof(*pError));
	if ( pError == NULL || !xrtThreadKeySet(__xplistErrorKey, pError) ) {
		xrtFree(pError);
		return NULL;
	}
	xrtPlistErrorInit(pError);
	return pError;
}

static void __xplistClearError(xrtplisterror* pError)
{
	xrtClearError();
	if ( pError != NULL ) { xrtPlistErrorInit(pError); }
}

static void __xplistOptions(xrtplistoptions* pOptions, int iFormat,
	bool bPretty, bool bSortKeys, uint32 iIndent, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtPlistOptionsInit(pOptions);
	pOptions->iFormat = iFormat;
	pOptions->bPretty = bPretty;
	pOptions->bSortKeys = bSortKeys;
	pOptions->iIndent = iIndent;
	pOptions->tLimits.iMaxInputBytes = iMaxInput;
	pOptions->tLimits.iMaxOutputBytes = iMaxOutput;
	pOptions->tLimits.iMaxItemBytes = iMaxItem;
	pOptions->tLimits.iMaxEntries = iMaxEntries;
	pOptions->tLimits.iMaxNodes = iMaxNodes;
	pOptions->tLimits.iMaxDepth = iMaxDepth;
}

XRT_API ptr xplist_parse(ptr pData, uint64 iSize, int iFormat,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	xrtplisterror* pError = __xplistError();
	ptr pResult;
	__xplistOptions(&tOptions, iFormat, false, false, 2u, iMaxInput,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	__xplistClearError(pError);
	pResult = xrtPlistParseMemory(pData, iSize, &tOptions, pError);
	if ( pResult == NULL ) { xrtClearError(); }
	return pResult;
}

XRT_API ptr xplist_parse_text(cstr sText, uint64 iSize, int iFormat,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	return xplist_parse((ptr)sText, iSize, iFormat, iMaxInput, iMaxOutput,
		iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
}

XRT_API ptr xplist_load(cstr sPath, int iFormat, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	xrtplisterror* pError = __xplistError();
	ptr pResult;
	__xplistOptions(&tOptions, iFormat, false, false, 2u, iMaxInput,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	__xplistClearError(pError);
	pResult = xrtPlistLoad((str)sPath, &tOptions, pError);
	if ( pResult == NULL ) { xrtClearError(); }
	return pResult;
}

XRT_API ptr xplist_create(void)
{
	__xplistClearError(__xplistError());
	return xrtPlistCreate();
}
XRT_API ptr xplist_adopt(ptr pHandle) { return pHandle; }
XRT_API void xplist_destroy(ptr pHandle) { xrtPlistDestroy((xrtplistdocument*)pHandle); }
XRT_API ptr xplist_root(ptr pHandle) { return xrtPlistRoot((xrtplistdocument*)pHandle); }
XRT_API bool xplist_set_root(ptr pHandle, ptr pNode) { return xrtPlistSetRoot((xrtplistdocument*)pHandle, (xrtplistnode*)pNode); }
XRT_API int64 xplist_node_count(ptr pHandle) { xrtplistdocument* p = (xrtplistdocument*)pHandle; return p != NULL ? p->iNodeCount : 0; }

XRT_API int64 xplist_type(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistNodeType((xrtplistnode*)pNode); }
XRT_API bool xplist_bool(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistBool((xrtplistnode*)pNode); }
XRT_API int64 xplist_int(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistInt((xrtplistnode*)pNode); }
XRT_API double xplist_real(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistReal((xrtplistnode*)pNode); }
XRT_API int64 xplist_date(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistDate((xrtplistnode*)pNode); }
XRT_API xvalue* xplist_date_value(ptr pHandle, ptr pNode) { (void)pHandle; return xrtValueTime(xrtPlistDate((xrtplistnode*)pNode)); }
XRT_API uint64 xplist_uid(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistUid((xrtplistnode*)pNode); }
XRT_API xvalue* xplist_string(ptr pHandle, ptr pNode)
{
	uint64 iSize = xrtPlistByteSize((xrtplistnode*)pNode);
	(void)pHandle;
	if ( iSize > SIZE_MAX ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.plist", 0,
			"plist string exceeds the xvalue string limit");
		return NULL;
	}
	return xrtValueString((xstrview){
		(const char*)xrtPlistBytes((xrtplistnode*)pNode), (size_t)iSize });
}

XRT_API ptr xplist_data(ptr pHandle, ptr pNode)
{
	xrtplistnode* pValue = (xrtplistnode*)pNode;
	xbuffer* pBuffer;
	uint64 iSize;
	(void)pHandle;
	iSize = xrtPlistByteSize(pValue);
	if ( iSize > UINT32_MAX ) { return NULL; }
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL || !xrtBufferReserve(pBuffer,
		(size_t)(iSize < 4096u ? 4096u : iSize)) ||
		(iSize != 0u && !xrtBufferAppend(pBuffer,
			(xbytesview){ (cbytes)xrtPlistBytes(pValue), (size_t)iSize })) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	return pBuffer;
}

XRT_API int64 xplist_count(ptr pHandle, ptr pNode)
{
	xrtplistnode* pValue = (xrtplistnode*)pNode;
	(void)pHandle;
	return pValue != NULL && pValue->iType == XRT_PLIST_ARRAY ?
		xrtPlistArrayCount(pValue) : xrtPlistDictCount(pValue);
}

XRT_API ptr xplist_array_get(ptr pHandle, ptr pNode, uint32 iIndex) { (void)pHandle; return xrtPlistArrayGet((xrtplistnode*)pNode, iIndex); }
XRT_API xvalue* xplist_dict_key(ptr pHandle, ptr pNode, uint32 iIndex)
{
	uint64 iSize = xrtPlistDictKeySize((xrtplistnode*)pNode, iIndex);
	(void)pHandle;
	if ( iSize > SIZE_MAX ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.plist", 0,
			"plist dictionary key exceeds the xvalue string limit");
		return NULL;
	}
	return xrtValueString((xstrview){
		(const char*)xrtPlistDictKey((xrtplistnode*)pNode, iIndex), (size_t)iSize });
}
XRT_API ptr xplist_dict_value(ptr pHandle, ptr pNode, uint32 iIndex) { (void)pHandle; return xrtPlistDictValue((xrtplistnode*)pNode, iIndex); }

XRT_API ptr xplist_new_null(ptr pHandle) { return xrtPlistCreateNull((xrtplistdocument*)pHandle); }
XRT_API ptr xplist_new_bool(ptr pHandle, bool bValue) { return xrtPlistCreateBool((xrtplistdocument*)pHandle, bValue); }
XRT_API ptr xplist_new_int(ptr pHandle, int64 iValue) { return xrtPlistCreateInt((xrtplistdocument*)pHandle, iValue); }
XRT_API ptr xplist_new_real(ptr pHandle, double fValue) { return xrtPlistCreateReal((xrtplistdocument*)pHandle, fValue); }
XRT_API ptr xplist_new_string(ptr pHandle, xvalue* pValue)
{
	xstrview tText;
	return xrtValueGetString(pValue, &tText) ?
		xrtPlistCreateStringEx((xrtplistdocument*)pHandle,
			tText.Data, tText.Size) : NULL;
}
XRT_API ptr xplist_new_data(ptr pHandle, ptr pData, uint64 iSize) { return xrtPlistCreateData((xrtplistdocument*)pHandle, pData, iSize); }
XRT_API ptr xplist_new_date(ptr pHandle, int64 tValue) { return xrtPlistCreateDate((xrtplistdocument*)pHandle, (xtime)tValue); }
XRT_API ptr xplist_new_date_value(ptr pHandle, xvalue* pValue)
{
	xtime tValue;
	return xrtValueGetTime(pValue, &tValue) ?
		xrtPlistCreateDate((xrtplistdocument*)pHandle, tValue) : NULL;
}
XRT_API ptr xplist_new_array(ptr pHandle) { return xrtPlistCreateArray((xrtplistdocument*)pHandle); }
XRT_API ptr xplist_new_dict(ptr pHandle) { return xrtPlistCreateDict((xrtplistdocument*)pHandle); }
XRT_API ptr xplist_new_uid(ptr pHandle, uint64 iValue) { return xrtPlistCreateUid((xrtplistdocument*)pHandle, iValue); }
XRT_API bool xplist_array_append(ptr pHandle, ptr pNode, ptr pValue) { (void)pHandle; return xrtPlistArrayAppend((xrtplistnode*)pNode, (xrtplistnode*)pValue); }
XRT_API bool xplist_dict_set(ptr pHandle, ptr pNode,
	xvalue* pKey, ptr pValue)
{
	xstrview tKey;
	(void)pHandle;
	return xrtValueGetString(pKey, &tKey) &&
		xrtPlistDictSet((xrtplistnode*)pNode, tKey.Data,
			tKey.Size, (xrtplistnode*)pValue);
}

XRT_API ptr xplist_stringify(ptr pHandle, int iFormat, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	xrtplisterror* pError = __xplistError();
	ptr pResult;
	__xplistOptions(&tOptions, iFormat, bPretty, bSortKeys, iIndent, 0u,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	__xplistClearError(pError);
	pResult = xrtPlistStringify((xrtplistdocument*)pHandle, &tOptions, pError);
	if ( pResult == NULL ) { xrtClearError(); }
	return pResult;
}

/* 仅接管 XML 文本；二进制 plist 继续由 xbuffer 返回。 */
XRT_API ptr xplist_stringify_text(ptr pHandle, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xbuffer* pResult = (xbuffer*)xplist_stringify(pHandle,
		XRT_PLIST_FORMAT_XML, bPretty, bSortKeys, iIndent, iMaxOutput,
		iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	char* sText;
	if ( pResult == NULL ) { return NULL; }
	if ( !xrtBufferAppendByte(pResult, 0) ) {
		xrtBufferDestroy(pResult);
		return NULL;
	}
	sText = (char*)xrtBufferTake(pResult, NULL, NULL);
	xrtBufferDestroy(pResult);
	return sText;
}

XRT_API bool xplist_save(ptr pHandle, cstr sPath, int iFormat, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	xrtplisterror* pError = __xplistError();
	bool bResult;
	__xplistOptions(&tOptions, iFormat, bPretty, bSortKeys, iIndent, 0u,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	__xplistClearError(pError);
	bResult = xrtPlistSave((str)sPath, (xrtplistdocument*)pHandle,
		&tOptions, pError);
	if ( !bResult ) { xrtClearError(); }
	return bResult;
}

XRT_API int64 xplist_error_code(void)
{
	xrtplisterror* p = __xplistError();
	const xerror* pCause = xrtGetError();
	if ( p == NULL ) { return XRT_PLIST_ERROR_MEMORY; }
	if ( p->iCode == XRT_PLIST_OK && pCause != NULL &&
		xrtErrorKind(pCause) == XERR_MEMORY ) { return XRT_PLIST_ERROR_MEMORY; }
	return p->iCode;
}
XRT_API int64 xplist_error_line(void) { xrtplisterror* p = __xplistError(); return p != NULL ? (int64)p->iLine : 0; }
XRT_API int64 xplist_error_column(void) { xrtplisterror* p = __xplistError(); return p != NULL ? (int64)p->iColumn : 0; }
XRT_API int64 xplist_error_offset(void) { xrtplisterror* p = __xplistError(); return p != NULL ? (int64)p->iOffset : 0; }
XRT_API ptr xplist_error_message(void) { xrtplisterror* p = __xplistError(); const char* s = p != NULL && p->sMessage[0] != 0 ? p->sMessage : "plist operation failed"; return xrtStrDup(s); }
