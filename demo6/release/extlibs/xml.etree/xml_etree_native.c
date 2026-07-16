#include "xrt_xml_etree_internal.h"

#include <stdio.h>
#include <string.h>



static xrtxmlerror* __xetreeError(void)
{
	return (xrtxmlerror*)xrtThreadLocalGetOrCreate("xlang.xml.etree.error.v1",
		sizeof(xrtxmlerror));
}

static void __xetreeOptions(xrtxmloptions* pOptions, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtXmlOptionsInit(pOptions);
	pOptions->tLimits.iMaxInputBytes = iMaxInput;
	pOptions->tLimits.iMaxOutputBytes = iMaxOutput;
	pOptions->tLimits.iMaxItemBytes = iMaxItem;
	pOptions->tLimits.iMaxEntries = iMaxEntries;
	pOptions->tLimits.iMaxNodes = iMaxNodes;
	pOptions->tLimits.iMaxDepth = iMaxDepth;
}

XXAPI ptr xetree_parse(ptr pData, uint64 iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	__xetreeOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	return xrtXmlElementParseMemory(pData, iSize, &tOptions, __xetreeError());
}

XXAPI ptr xetree_load(str sPath, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	__xetreeOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	return xrtXmlElementLoad(sPath, &tOptions, __xetreeError());
}

XXAPI ptr xetree_create(str sName) { return xrtXmlElementCreate(sName); }
XXAPI ptr xetree_adopt(ptr pHandle) { return pHandle; }
XXAPI void xetree_release(ptr pHandle) { xrtXmlElementRelease((xrtxmlelement*)pHandle); }
XXAPI int64 xetree_kind(ptr pHandle) { return xrtXmlElementKind((xrtxmlelement*)pHandle); }
XXAPI str xetree_name(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementName((xrtxmlelement*)pHandle)); }
XXAPI str xetree_local_name(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementLocalName((xrtxmlelement*)pHandle)); }
XXAPI str xetree_namespace_uri(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementNamespaceUri((xrtxmlelement*)pHandle)); }
XXAPI str xetree_prefix(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementPrefix((xrtxmlelement*)pHandle)); }
XXAPI str xetree_text(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementText((xrtxmlelement*)pHandle)); }
XXAPI str xetree_tail(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementTail((xrtxmlelement*)pHandle)); }
XXAPI bool xetree_set_text(ptr pHandle, str sText) { return xrtXmlElementSetText((xrtxmlelement*)pHandle, sText); }
XXAPI bool xetree_set_tail(ptr pHandle, str sText) { return xrtXmlElementSetTail((xrtxmlelement*)pHandle, sText); }
XXAPI int64 xetree_child_count(ptr pHandle) { return xrtXmlElementChildCount((xrtxmlelement*)pHandle); }
XXAPI ptr xetree_child(ptr pHandle, uint32 iIndex) { return xrtXmlElementChild((xrtxmlelement*)pHandle, iIndex); }
XXAPI ptr xetree_append(ptr pHandle, str sName) { return xrtXmlElementAppend((xrtxmlelement*)pHandle, sName); }
XXAPI str xetree_get_attribute(ptr pHandle, str sName) {
	str sValue = xrtXmlElementGetAttribute((xrtxmlelement*)pHandle, sName);
	return sValue != NULL ? __xrtXmlEtreeCopy((const char*)sValue) : NULL;
}
XXAPI bool xetree_set_attribute(ptr pHandle, str sName, str sValue) { return xrtXmlElementSetAttribute((xrtxmlelement*)pHandle, sName, sValue); }
XXAPI bool xetree_remove_attribute(ptr pHandle, str sName) { return xrtXmlElementRemoveAttribute((xrtxmlelement*)pHandle, sName); }
XXAPI xvalue xetree_attributes(ptr pHandle) {
	xrtxmlelement* pElement = (xrtxmlelement*)pHandle;
	xvalue pResult = xvoCreateTable();
	uint32 i;
	if ( pResult == NULL ) { return NULL; }
	for ( i = 0u; i < xrtXmlElementAttributeCount(pElement); i++ ) {
		str sName = xrtXmlElementAttributeName(pElement, i);
		str sValue = xrtXmlElementAttributeValue(pElement, i);
		if ( !xvoTableSetText(pResult, sName, (uint32)strlen((const char*)sName),
			sValue, (uint32)strlen((const char*)sValue), FALSE) ) {
			xvoUnref(pResult);
			return NULL;
		}
	}
	return pResult;
}
XXAPI ptr xetree_find(ptr pHandle, str sPath) { return xrtXmlElementFind((xrtxmlelement*)pHandle, sPath); }
XXAPI xvalue xetree_find_all(ptr pHandle, str sPath) { return xrtXmlElementFindAll((xrtxmlelement*)pHandle, sPath); }

static void __xetreeWriteOptions(xrtxmletreewriteoptions* pOptions,
	bool bPretty, uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtXmlElementWriteOptionsInit(pOptions);
	pOptions->bPretty = bPretty;
	pOptions->iIndent = iIndent;
	pOptions->bXmlDeclaration = bDeclaration;
	pOptions->iMaxOutputBytes = iMaxOutput;
}

XXAPI ptr xetree_stringify(ptr pHandle, bool bPretty, uint32 iIndent,
	bool bDeclaration, uint64 iMaxOutput)
{
	xrtxmletreewriteoptions tOptions;
	__xetreeWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	return xrtXmlElementStringify((xrtxmlelement*)pHandle, &tOptions,
		__xetreeError());
}

XXAPI bool xetree_save(ptr pHandle, str sPath, bool bPretty,
	uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtxmletreewriteoptions tOptions;
	__xetreeWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	return xrtXmlElementSave(sPath, (xrtxmlelement*)pHandle, &tOptions,
		__xetreeError());
}

XXAPI int64 xetree_error_code(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? p->iCode : XRT_XML_ERROR_MEMORY; }
XXAPI int64 xetree_error_line(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iLine : 0; }
XXAPI int64 xetree_error_column(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iColumn : 0; }
XXAPI int64 xetree_error_offset(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iOffset : 0; }
XXAPI str xetree_error_message(void) { xrtxmlerror* p = __xetreeError(); return __xrtXmlEtreeCopy(p != NULL ? p->sMessage : "XML ElementTree state is unavailable"); }
