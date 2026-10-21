#ifndef XRT_XML_ETREE_H
#define XRT_XML_ETREE_H

/* ElementTree 在 SAX 契约之外还返回 Value 集合和连续缓冲。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif

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

XRT_API void xrtXmlElementWriteOptionsInit(xrtxmletreewriteoptions* pOptions);
XRT_API xrtxmlelement* xrtXmlElementParse(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API xrtxmlelement* xrtXmlElementParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API xrtxmlelement* xrtXmlElementLoad(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);

XRT_API xrtxmlelement* xrtXmlElementCreate(str sName);
XRT_API xrtxmlelement* xrtXmlElementAppend(xrtxmlelement* pParent, str sName);
XRT_API xrtxmlelement* xrtXmlElementRetain(xrtxmlelement* pElement);
XRT_API void xrtXmlElementRelease(xrtxmlelement* pElement);
XRT_API int xrtXmlElementKind(const xrtxmlelement* pElement);
XRT_API const xrtxmlelement* xrtXmlElementParent(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementName(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementLocalName(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementNamespaceUri(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementPrefix(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementText(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementTail(const xrtxmlelement* pElement);
XRT_API bool xrtXmlElementSetText(xrtxmlelement* pElement, str sText);
XRT_API bool xrtXmlElementSetTail(xrtxmlelement* pElement, str sText);
XRT_API uint32 xrtXmlElementChildCount(const xrtxmlelement* pElement);
XRT_API xrtxmlelement* xrtXmlElementChild(const xrtxmlelement* pElement,
	uint32 iIndex);
XRT_API uint32 xrtXmlElementAttributeCount(const xrtxmlelement* pElement);
XRT_API str xrtXmlElementAttributeName(const xrtxmlelement* pElement,
	uint32 iIndex);
XRT_API str xrtXmlElementAttributeValue(const xrtxmlelement* pElement,
	uint32 iIndex);
XRT_API str xrtXmlElementGetAttribute(const xrtxmlelement* pElement, str sName);
XRT_API bool xrtXmlElementSetAttribute(xrtxmlelement* pElement, str sName,
	str sValue);
XRT_API bool xrtXmlElementRemoveAttribute(xrtxmlelement* pElement, str sName);
XRT_API xrtxmlelement* xrtXmlElementFind(const xrtxmlelement* pElement,
	str sPath);
XRT_API xvalue* xrtXmlElementFindAll(const xrtxmlelement* pElement, str sPath);

XRT_API xbuffer* xrtXmlElementStringify(const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError);
/* 空 Writer 或空保存路径在序列化前报告 ARGUMENT。 */
XRT_API bool xrtXmlElementWrite(const xrtxmlelement* pElement, xwriter* pWriter,
	bool bTakeOwnership, const xrtxmletreewriteoptions* pOptions,
	xrtxmlerror* pError);
XRT_API bool xrtXmlElementSave(str sPath, const xrtxmlelement* pElement,
	const xrtxmletreewriteoptions* pOptions, xrtxmlerror* pError);

#ifdef __cplusplus
}
#endif

#endif
