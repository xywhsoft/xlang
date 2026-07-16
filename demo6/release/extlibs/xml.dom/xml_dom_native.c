#include "xrt_xml_dom_internal.h"

#include <string.h>



static xrtxmlerror* __xdomError(void)
{
	return (xrtxmlerror*)xrtThreadLocalGetOrCreate("xlang.xml.dom.error.v1",
		sizeof(xrtxmlerror));
}

static void __xdomOptions(xrtxmloptions* pOptions, uint64 iMaxInput,
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

static str __xdomCopy(const char* sText)
{
	return __xrtXmlDomCopy(sText != NULL ? sText : "");
}

XXAPI ptr xdom_parse(ptr pData, uint64 iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	__xdomOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	return xrtXmlDomParseMemory(pData, iSize, &tOptions, __xdomError());
}

XXAPI ptr xdom_load(str sPath, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	__xdomOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	return xrtXmlDomLoad(sPath, &tOptions, __xdomError());
}

XXAPI ptr xdom_create_document(void) { return xrtXmlDomCreateDocument(); }
XXAPI ptr xdom_adopt(ptr pHandle) { return pHandle; }
XXAPI void xdom_release(ptr pHandle) { xrtXmlDomNodeRelease((xrtxmldomnode*)pHandle); }
XXAPI int64 xdom_type(ptr pHandle) { return xrtXmlDomNodeType((xrtxmldomnode*)pHandle); }
XXAPI str xdom_type_name(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeTypeName((xrtxmldomnode*)pHandle)); }
XXAPI str xdom_name(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeName((xrtxmldomnode*)pHandle)); }
XXAPI str xdom_local_name(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeLocalName((xrtxmldomnode*)pHandle)); }
XXAPI str xdom_namespace_uri(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeNamespaceUri((xrtxmldomnode*)pHandle)); }
XXAPI str xdom_prefix(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodePrefix((xrtxmldomnode*)pHandle)); }
XXAPI str xdom_value(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeValue((xrtxmldomnode*)pHandle)); }
XXAPI bool xdom_set_value(ptr pHandle, str sValue) { return xrtXmlDomNodeSetValue((xrtxmldomnode*)pHandle, sValue); }
XXAPI str xdom_text_content(ptr pHandle) { return xrtXmlDomTextContent((xrtxmldomnode*)pHandle); }
XXAPI ptr xdom_parent(ptr pHandle) { return xrtXmlDomNodeParent((xrtxmldomnode*)pHandle); }
XXAPI int64 xdom_child_count(ptr pHandle) { return xrtXmlDomNodeChildCount((xrtxmldomnode*)pHandle); }
XXAPI ptr xdom_child(ptr pHandle, uint32 iIndex) { return xrtXmlDomNodeChild((xrtxmldomnode*)pHandle, iIndex); }
XXAPI ptr xdom_document_element(ptr pHandle) { return xrtXmlDomDocumentElement((xrtxmldomnode*)pHandle); }
XXAPI bool xdom_append_child(ptr pParent, ptr pChild) { return xrtXmlDomAppendChild((xrtxmldomnode*)pParent, (xrtxmldomnode*)pChild); }
XXAPI bool xdom_remove_child(ptr pParent, ptr pChild) { return xrtXmlDomRemoveChild((xrtxmldomnode*)pParent, (xrtxmldomnode*)pChild); }

XXAPI ptr xdom_create_element(ptr pDocument, str sName) { return xrtXmlDomCreateElement((xrtxmldomnode*)pDocument, sName); }
XXAPI ptr xdom_create_text(ptr pDocument, str sText) { return xrtXmlDomCreateText((xrtxmldomnode*)pDocument, sText); }
XXAPI ptr xdom_create_comment(ptr pDocument, str sText) { return xrtXmlDomCreateComment((xrtxmldomnode*)pDocument, sText); }
XXAPI ptr xdom_create_pi(ptr pDocument, str sTarget, str sText) { return xrtXmlDomCreateProcessingInstruction((xrtxmldomnode*)pDocument, sTarget, sText); }

XXAPI str xdom_get_attribute(ptr pHandle, str sName)
{
	str sValue = xrtXmlDomGetAttribute((xrtxmldomnode*)pHandle, sName);
	return sValue != NULL ? __xdomCopy((const char*)sValue) : NULL;
}

XXAPI bool xdom_set_attribute(ptr pHandle, str sName, str sValue) { return xrtXmlDomSetAttribute((xrtxmldomnode*)pHandle, sName, sValue); }
XXAPI bool xdom_remove_attribute(ptr pHandle, str sName) { return xrtXmlDomRemoveAttribute((xrtxmldomnode*)pHandle, sName); }

XXAPI xvalue xdom_attributes(ptr pHandle)
{
	xrtxmldomnode* pNode = (xrtxmldomnode*)pHandle;
	xvalue pResult = xvoCreateTable();
	uint32 i;
	if ( pResult == NULL ) { return NULL; }
	for ( i = 0u; i < xrtXmlDomAttributeCount(pNode); i++ ) {
		str sName = xrtXmlDomAttributeName(pNode, i);
		str sValue = xrtXmlDomAttributeValue(pNode, i);
		if ( !xvoTableSetText(pResult, sName, (uint32)strlen((const char*)sName),
			sValue, (uint32)strlen((const char*)sValue), FALSE) ) {
			xvoUnref(pResult);
			return NULL;
		}
	}
	return pResult;
}

static void __xdomWriteOptions(xrtxmldomwriteoptions* pOptions,
	bool bPretty, uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtXmlDomWriteOptionsInit(pOptions);
	pOptions->bPretty = bPretty;
	pOptions->iIndent = iIndent;
	pOptions->bXmlDeclaration = bDeclaration;
	pOptions->iMaxOutputBytes = iMaxOutput;
}

XXAPI ptr xdom_stringify(ptr pHandle, bool bPretty, uint32 iIndent,
	bool bDeclaration, uint64 iMaxOutput)
{
	xrtxmldomwriteoptions tOptions;
	__xdomWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	return xrtXmlDomStringify((xrtxmldomnode*)pHandle, &tOptions,
		__xdomError());
}

XXAPI bool xdom_save(ptr pHandle, str sPath, bool bPretty,
	uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtxmldomwriteoptions tOptions;
	__xdomWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	return xrtXmlDomSave(sPath, (xrtxmldomnode*)pHandle, &tOptions,
		__xdomError());
}

XXAPI int64 xdom_error_code(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? p->iCode : XRT_XML_ERROR_MEMORY; }
XXAPI int64 xdom_error_line(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? (int64)p->iLine : 0; }
XXAPI int64 xdom_error_column(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? (int64)p->iColumn : 0; }
XXAPI int64 xdom_error_offset(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? (int64)p->iOffset : 0; }
XXAPI str xdom_error_message(void) { xrtxmlerror* p = __xdomError(); return __xdomCopy(p != NULL ? p->sMessage : "XML DOM state is unavailable"); }
