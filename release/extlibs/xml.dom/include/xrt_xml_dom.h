#ifndef XRT_XML_DOM_H
#define XRT_XML_DOM_H

/* DOM 序列化接口返回连续缓冲。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif

#include "../../xml.sax/include/xrt_xml.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_XML_DOM_WRITE_OPTIONS_VERSION 1u

typedef enum {
	XRT_XML_DOM_ELEMENT_NODE = 1,
	XRT_XML_DOM_TEXT_NODE = 3,
	XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE = 7,
	XRT_XML_DOM_COMMENT_NODE = 8,
	XRT_XML_DOM_DOCUMENT_NODE = 9
} xrtxmldomnodetype;

typedef struct xrtxmldomnode_struct xrtxmldomnode;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint32 iIndent;
	bool bPretty;
	bool bXmlDeclaration;
	uint64 iMaxOutputBytes;
} xrtxmldomwriteoptions;

XRT_API void xrtXmlDomWriteOptionsInit(xrtxmldomwriteoptions* pOptions);
XRT_API xrtxmldomnode* xrtXmlDomParse(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API xrtxmldomnode* xrtXmlDomParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API xrtxmldomnode* xrtXmlDomLoad(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);

XRT_API xrtxmldomnode* xrtXmlDomCreateDocument(void);
XRT_API xrtxmldomnode* xrtXmlDomCreateElement(xrtxmldomnode* pDocument,
	str sName);
XRT_API xrtxmldomnode* xrtXmlDomCreateText(xrtxmldomnode* pDocument,
	str sText);
XRT_API xrtxmldomnode* xrtXmlDomCreateComment(xrtxmldomnode* pDocument,
	str sText);
XRT_API xrtxmldomnode* xrtXmlDomCreateProcessingInstruction(
	xrtxmldomnode* pDocument, str sTarget, str sText);
XRT_API xrtxmldomnode* xrtXmlDomNodeRetain(xrtxmldomnode* pNode);
XRT_API void xrtXmlDomNodeRelease(xrtxmldomnode* pNode);

XRT_API int xrtXmlDomNodeType(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomNodeTypeName(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomNodeName(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomNodeLocalName(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomNodeNamespaceUri(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomNodePrefix(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomNodeValue(const xrtxmldomnode* pNode);
XRT_API bool xrtXmlDomNodeSetValue(xrtxmldomnode* pNode, str sValue);
XRT_API xrtxmldomnode* xrtXmlDomNodeParent(const xrtxmldomnode* pNode);
XRT_API uint32 xrtXmlDomNodeChildCount(const xrtxmldomnode* pNode);
XRT_API xrtxmldomnode* xrtXmlDomNodeChild(const xrtxmldomnode* pNode,
	uint32 iIndex);
XRT_API xrtxmldomnode* xrtXmlDomDocumentElement(const xrtxmldomnode* pDocument);
XRT_API bool xrtXmlDomAppendChild(xrtxmldomnode* pParent,
	xrtxmldomnode* pChild);
XRT_API bool xrtXmlDomRemoveChild(xrtxmldomnode* pParent,
	xrtxmldomnode* pChild);
XRT_API str xrtXmlDomTextContent(const xrtxmldomnode* pNode);

XRT_API uint32 xrtXmlDomAttributeCount(const xrtxmldomnode* pNode);
XRT_API str xrtXmlDomAttributeName(const xrtxmldomnode* pNode, uint32 iIndex);
XRT_API str xrtXmlDomAttributeValue(const xrtxmldomnode* pNode, uint32 iIndex);
XRT_API str xrtXmlDomGetAttribute(const xrtxmldomnode* pNode, str sName);
XRT_API bool xrtXmlDomSetAttribute(xrtxmldomnode* pNode, str sName, str sValue);
XRT_API bool xrtXmlDomRemoveAttribute(xrtxmldomnode* pNode, str sName);

XRT_API xbuffer* xrtXmlDomStringify(const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError);
/* 空 Writer 或空保存路径在序列化前报告 ARGUMENT。 */
XRT_API bool xrtXmlDomWrite(const xrtxmldomnode* pNode, xwriter* pWriter,
	bool bTakeOwnership, const xrtxmldomwriteoptions* pOptions,
	xrtxmlerror* pError);
XRT_API bool xrtXmlDomSave(str sPath, const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError);

#ifdef __cplusplus
}
#endif

#endif
