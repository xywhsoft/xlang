#ifndef XRT_XML_DOM_H
#define XRT_XML_DOM_H

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

XXAPI void xrtXmlDomWriteOptionsInit(xrtxmldomwriteoptions* pOptions);
XXAPI xrtxmldomnode* xrtXmlDomParse(xreader pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XXAPI xrtxmldomnode* xrtXmlDomParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XXAPI xrtxmldomnode* xrtXmlDomLoad(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);

XXAPI xrtxmldomnode* xrtXmlDomCreateDocument(void);
XXAPI xrtxmldomnode* xrtXmlDomCreateElement(xrtxmldomnode* pDocument,
	str sName);
XXAPI xrtxmldomnode* xrtXmlDomCreateText(xrtxmldomnode* pDocument,
	str sText);
XXAPI xrtxmldomnode* xrtXmlDomCreateComment(xrtxmldomnode* pDocument,
	str sText);
XXAPI xrtxmldomnode* xrtXmlDomCreateProcessingInstruction(
	xrtxmldomnode* pDocument, str sTarget, str sText);
XXAPI xrtxmldomnode* xrtXmlDomNodeRetain(xrtxmldomnode* pNode);
XXAPI void xrtXmlDomNodeRelease(xrtxmldomnode* pNode);

XXAPI int xrtXmlDomNodeType(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomNodeTypeName(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomNodeName(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomNodeLocalName(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomNodeNamespaceUri(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomNodePrefix(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomNodeValue(const xrtxmldomnode* pNode);
XXAPI bool xrtXmlDomNodeSetValue(xrtxmldomnode* pNode, str sValue);
XXAPI xrtxmldomnode* xrtXmlDomNodeParent(const xrtxmldomnode* pNode);
XXAPI uint32 xrtXmlDomNodeChildCount(const xrtxmldomnode* pNode);
XXAPI xrtxmldomnode* xrtXmlDomNodeChild(const xrtxmldomnode* pNode,
	uint32 iIndex);
XXAPI xrtxmldomnode* xrtXmlDomDocumentElement(const xrtxmldomnode* pDocument);
XXAPI bool xrtXmlDomAppendChild(xrtxmldomnode* pParent,
	xrtxmldomnode* pChild);
XXAPI bool xrtXmlDomRemoveChild(xrtxmldomnode* pParent,
	xrtxmldomnode* pChild);
XXAPI str xrtXmlDomTextContent(const xrtxmldomnode* pNode);

XXAPI uint32 xrtXmlDomAttributeCount(const xrtxmldomnode* pNode);
XXAPI str xrtXmlDomAttributeName(const xrtxmldomnode* pNode, uint32 iIndex);
XXAPI str xrtXmlDomAttributeValue(const xrtxmldomnode* pNode, uint32 iIndex);
XXAPI str xrtXmlDomGetAttribute(const xrtxmldomnode* pNode, str sName);
XXAPI bool xrtXmlDomSetAttribute(xrtxmldomnode* pNode, str sName, str sValue);
XXAPI bool xrtXmlDomRemoveAttribute(xrtxmldomnode* pNode, str sName);

XXAPI xbuffer xrtXmlDomStringify(const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError);
XXAPI bool xrtXmlDomWrite(const xrtxmldomnode* pNode, xwriter pWriter,
	bool bTakeOwnership, const xrtxmldomwriteoptions* pOptions,
	xrtxmlerror* pError);
XXAPI bool xrtXmlDomSave(str sPath, const xrtxmldomnode* pNode,
	const xrtxmldomwriteoptions* pOptions, xrtxmlerror* pError);

#ifdef __cplusplus
}
#endif

#endif
