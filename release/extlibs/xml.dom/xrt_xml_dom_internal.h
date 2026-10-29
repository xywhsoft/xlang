#ifndef XRT_XML_DOM_INTERNAL_H
#define XRT_XML_DOM_INTERNAL_H

#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/xrt_xml_dom.h"

#include <stdio.h>
#include <string.h>

typedef struct xrtxmldomdocument_struct xrtxmldomdocument;

struct xrtxmldomnode_struct {
	xrtxmldomdocument* pDocument;
	xrtxmldomnode* pParent;
	int iType;
	str sName;
	str sLocalName;
	str sNamespaceUri;
	str sPrefix;
	str sValue;
	xrtxmlattribute* pAttributes;
	uint32 iAttributeCount;
	uint32 iAttributeCapacity;
	xrtxmldomnode** pChildren;
	uint32 iChildCount;
	uint32 iChildCapacity;
};

struct xrtxmldomdocument_struct {
	uint32 iReferences;
	xrtxmldomnode* pDocumentNode;
	xrtxmldomnode** pNodes;
	uint32 iNodeCount;
	uint32 iNodeCapacity;
};

str __xrtXmlDomCopy(const char* sText);
void __xrtXmlDomSetError(xrtxmlerror* pError, int iCode,
	const char* sMessage);
bool __xrtXmlDomValidName(const char* sName);
xrtxmldomnode* __xrtXmlDomCreateNode(xrtxmldomdocument* pDocument,
	int iType, const char* sName, const char* sValue);
bool __xrtXmlDomAppendText(str* psTarget, const char* sText);
void __xrtXmlDomFreeDocument(xrtxmldomdocument* pDocument);

#endif
