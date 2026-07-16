#ifndef XRT_XML_ETREE_H
#define XRT_XML_ETREE_H

#include "../../xml.sax/include/xrt_xml.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_XML_ETREE_WRITE_OPTIONS_VERSION 1u

typedef enum {
	XRT_XML_ELEMENT = 1,
	XRT_XML_COMMENT = 2,
	XRT_XML_PROCESSING_INSTRUCTION = 3
} xrtxmlelementkind;

typedef struct xrtxmlelement_struct xrtxmlelement;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint32 iIndent;
	bool bPretty;
	bool bXmlDeclaration;
	uint64 iMaxOutputBytes;
} xrtxmletreewriteoptions;

XXAPI void xrtXmlElementWriteOptionsInit(xrtxmletreewriteoptions* pOptions);
XXAPI xrtxmlelement* xrtXmlElementParse(xreader pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XXAPI xrtxmlelement* xrtXmlElementParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XXAPI xrtxmlelement* xrtXmlElementLoad(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);

XXAPI xrtxmlelement* xrtXmlElementCreate(str sName);
XXAPI xrtxmlelement* xrtXmlElementAppend(xrtxmlelement* pParent, str sName);
XXAPI xrtxmlelement* xrtXmlElementRetain(xrtxmlelement* pElement);
XXAPI void xrtXmlElementRelease(xrtxmlelement* pElement);
XXAPI int xrtXmlElementKind(const xrtxmlelement* pElement);
XXAPI const xrtxmlelement* xrtXmlElementParent(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementName(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementLocalName(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementNamespaceUri(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementPrefix(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementText(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementTail(const xrtxmlelement* pElement);
XXAPI bool xrtXmlElementSetText(xrtxmlelement* pElement, str sText);
XXAPI bool xrtXmlElementSetTail(xrtxmlelement* pElement, str sText);
XXAPI uint32 xrtXmlElementChildCount(const xrtxmlelement* pElement);
XXAPI xrtxmlelement* xrtXmlElementChild(const xrtxmlelement* pElement,
	uint32 iIndex);
XXAPI uint32 xrtXmlElementAttributeCount(const xrtxmlelement* pElement);
XXAPI str xrtXmlElementAttributeName(const xrtxmlelement* pElement,
	uint32 iIndex);
XXAPI str xrtXmlElementAttributeValue(const xrtxmlelement* pElement,
	uint32 iIndex);
XXAPI str xrtXmlElementGetAttribute(const xrtxmlelement* pElement, str sName);
XXAPI bool xrtXmlElementSetAttribute(xrtxmlelement* pElement, str sName,
	str sValue);
XXAPI bool xrtXmlElementRemoveAttribute(xrtxmlelement* pElement, str sName);
XXAPI xrtxmlelement* xrtXmlElementFind(const xrtxmlelement* pElement,
	str sPath);
XXAPI xvalue xrtXmlElementFindAll(const xrtxmlelement* pElement, str sPath);

XXAPI xbuffer xrtXmlElementStringify(const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError);
XXAPI bool xrtXmlElementWrite(const xrtxmlelement* pElement, xwriter pWriter,
	bool bTakeOwnership, const xrtxmletreewriteoptions* pOptions,
	xrtxmlerror* pError);
XXAPI bool xrtXmlElementSave(str sPath, const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError);

#ifdef __cplusplus
}
#endif

#endif
