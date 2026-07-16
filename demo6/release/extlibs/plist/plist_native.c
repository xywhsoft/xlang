#include "xrt_plist_internal.h"



static xrtplisterror* __xplistError(void)
{
	return (xrtplisterror*)xrtThreadLocalGetOrCreate("xlang.plist.error.v1",
		sizeof(xrtplisterror));
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

XXAPI ptr xplist_parse(ptr pData, uint64 iSize, int iFormat,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	__xplistOptions(&tOptions, iFormat, FALSE, FALSE, 2u, iMaxInput,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	return xrtPlistParseMemory(pData, iSize, &tOptions, __xplistError());
}

XXAPI ptr xplist_load(str sPath, int iFormat, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	__xplistOptions(&tOptions, iFormat, FALSE, FALSE, 2u, iMaxInput,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	return xrtPlistLoad(sPath, &tOptions, __xplistError());
}

XXAPI ptr xplist_create(void) { return xrtPlistCreate(); }
XXAPI ptr xplist_adopt(ptr pHandle) { return pHandle; }
XXAPI void xplist_destroy(ptr pHandle) { xrtPlistDestroy((xrtplistdocument*)pHandle); }
XXAPI ptr xplist_root(ptr pHandle) { return xrtPlistRoot((xrtplistdocument*)pHandle); }
XXAPI bool xplist_set_root(ptr pHandle, ptr pNode) { return xrtPlistSetRoot((xrtplistdocument*)pHandle, (xrtplistnode*)pNode); }
XXAPI int64 xplist_node_count(ptr pHandle) { xrtplistdocument* p = (xrtplistdocument*)pHandle; return p != NULL ? p->iNodeCount : 0; }

XXAPI int64 xplist_type(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistNodeType((xrtplistnode*)pNode); }
XXAPI bool xplist_bool(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistBool((xrtplistnode*)pNode); }
XXAPI int64 xplist_int(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistInt((xrtplistnode*)pNode); }
XXAPI double xplist_real(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistReal((xrtplistnode*)pNode); }
XXAPI int64 xplist_date(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistDate((xrtplistnode*)pNode); }
XXAPI xvalue xplist_date_value(ptr pHandle, ptr pNode) { (void)pHandle; return xvoCreateTime(xrtPlistDate((xrtplistnode*)pNode)); }
XXAPI uint64 xplist_uid(ptr pHandle, ptr pNode) { (void)pHandle; return xrtPlistUid((xrtplistnode*)pNode); }
XXAPI xvalue xplist_string(ptr pHandle, ptr pNode)
{
	uint64 iSize = xrtPlistByteSize((xrtplistnode*)pNode);
	(void)pHandle;
	if ( iSize > UINT32_MAX ) {
		xrtSetError(xrtFormat("plist string exceeds the xvalue string limit"), TRUE);
		return NULL;
	}
	return xvoCreateText((ptr)xrtPlistBytes((xrtplistnode*)pNode), (uint32)iSize, FALSE);
}

XXAPI ptr xplist_data(ptr pHandle, ptr pNode)
{
	xrtplistnode* pValue = (xrtplistnode*)pNode;
	xbuffer pBuffer;
	uint64 iSize;
	(void)pHandle;
	iSize = xrtPlistByteSize(pValue);
	if ( iSize > UINT32_MAX ) { return NULL; }
	pBuffer = xrtBufferCreate(iSize < 4096u ? 4096u : (uint32)iSize);
	if ( pBuffer == NULL || (iSize != 0u && !xrtBufferAppend(pBuffer,
		(ptr)xrtPlistBytes(pValue), (uint32)iSize, XBUF_BINARY)) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	return pBuffer;
}

XXAPI int64 xplist_count(ptr pHandle, ptr pNode)
{
	xrtplistnode* pValue = (xrtplistnode*)pNode;
	(void)pHandle;
	return pValue != NULL && pValue->iType == XRT_PLIST_ARRAY ?
		xrtPlistArrayCount(pValue) : xrtPlistDictCount(pValue);
}

XXAPI ptr xplist_array_get(ptr pHandle, ptr pNode, uint32 iIndex) { (void)pHandle; return xrtPlistArrayGet((xrtplistnode*)pNode, iIndex); }
XXAPI xvalue xplist_dict_key(ptr pHandle, ptr pNode, uint32 iIndex)
{
	uint64 iSize = xrtPlistDictKeySize((xrtplistnode*)pNode, iIndex);
	(void)pHandle;
	if ( iSize > UINT32_MAX ) {
		xrtSetError(xrtFormat("plist dictionary key exceeds the xvalue string limit"), TRUE);
		return NULL;
	}
	return xvoCreateText((ptr)xrtPlistDictKey((xrtplistnode*)pNode, iIndex),
		(uint32)iSize, FALSE);
}
XXAPI ptr xplist_dict_value(ptr pHandle, ptr pNode, uint32 iIndex) { (void)pHandle; return xrtPlistDictValue((xrtplistnode*)pNode, iIndex); }

XXAPI ptr xplist_new_null(ptr pHandle) { return xrtPlistCreateNull((xrtplistdocument*)pHandle); }
XXAPI ptr xplist_new_bool(ptr pHandle, bool bValue) { return xrtPlistCreateBool((xrtplistdocument*)pHandle, bValue); }
XXAPI ptr xplist_new_int(ptr pHandle, int64 iValue) { return xrtPlistCreateInt((xrtplistdocument*)pHandle, iValue); }
XXAPI ptr xplist_new_real(ptr pHandle, double fValue) { return xrtPlistCreateReal((xrtplistdocument*)pHandle, fValue); }
XXAPI ptr xplist_new_string(ptr pHandle, str sValue) { return xrtPlistCreateString((xrtplistdocument*)pHandle, sValue); }
XXAPI ptr xplist_new_data(ptr pHandle, ptr pData, uint64 iSize) { return xrtPlistCreateData((xrtplistdocument*)pHandle, pData, iSize); }
XXAPI ptr xplist_new_date(ptr pHandle, int64 tValue) { return xrtPlistCreateDate((xrtplistdocument*)pHandle, (xtime)tValue); }
XXAPI ptr xplist_new_date_value(ptr pHandle, xvalue pValue) { return xvoIsTime(pValue) ? xrtPlistCreateDate((xrtplistdocument*)pHandle, xvoGetTime(pValue)) : NULL; }
XXAPI ptr xplist_new_array(ptr pHandle) { return xrtPlistCreateArray((xrtplistdocument*)pHandle); }
XXAPI ptr xplist_new_dict(ptr pHandle) { return xrtPlistCreateDict((xrtplistdocument*)pHandle); }
XXAPI ptr xplist_new_uid(ptr pHandle, uint64 iValue) { return xrtPlistCreateUid((xrtplistdocument*)pHandle, iValue); }
XXAPI bool xplist_array_append(ptr pHandle, ptr pNode, ptr pValue) { (void)pHandle; return xrtPlistArrayAppend((xrtplistnode*)pNode, (xrtplistnode*)pValue); }
XXAPI bool xplist_dict_set(ptr pHandle, ptr pNode, str sKey, ptr pValue) { (void)pHandle; return xrtPlistDictSet((xrtplistnode*)pNode, sKey, strlen((const char*)sKey), (xrtplistnode*)pValue); }

XXAPI ptr xplist_stringify(ptr pHandle, int iFormat, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	__xplistOptions(&tOptions, iFormat, bPretty, bSortKeys, iIndent, 0u,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	return xrtPlistStringify((xrtplistdocument*)pHandle, &tOptions,
		__xplistError());
}

XXAPI bool xplist_save(ptr pHandle, str sPath, int iFormat, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtplistoptions tOptions;
	__xplistOptions(&tOptions, iFormat, bPretty, bSortKeys, iIndent, 0u,
		iMaxOutput, iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	return xrtPlistSave(sPath, (xrtplistdocument*)pHandle, &tOptions,
		__xplistError());
}

XXAPI int64 xplist_error_code(void) { xrtplisterror* p = __xplistError(); return p != NULL ? p->iCode : XRT_PLIST_ERROR_MEMORY; }
XXAPI int64 xplist_error_line(void) { xrtplisterror* p = __xplistError(); return p != NULL ? (int64)p->iLine : 0; }
XXAPI int64 xplist_error_column(void) { xrtplisterror* p = __xplistError(); return p != NULL ? (int64)p->iColumn : 0; }
XXAPI int64 xplist_error_offset(void) { xrtplisterror* p = __xplistError(); return p != NULL ? (int64)p->iOffset : 0; }
XXAPI str xplist_error_message(void) { xrtplisterror* p = __xplistError(); const char* s = p != NULL && p->sMessage[0] != 0 ? p->sMessage : "plist operation failed"; return xrtCopyStr((str)s, 0u); }
