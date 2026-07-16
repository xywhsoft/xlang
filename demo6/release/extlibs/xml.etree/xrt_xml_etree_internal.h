#ifndef XRT_XML_ETREE_INTERNAL_H
#define XRT_XML_ETREE_INTERNAL_H

#include "include/xrt_xml_etree.h"

#include <stdio.h>
#include <string.h>

typedef struct xrtxmldocument_struct xrtxmldocument;

struct xrtxmlelement_struct {
	xrtxmldocument* pDocument;
	xrtxmlelement* pParent;
	int iKind;
	str sName;
	str sLocalName;
	str sNamespaceUri;
	str sPrefix;
	str sText;
	str sTail;
	xrtxmlattribute* pAttributes;
	uint32 iAttributeCount;
	uint32 iAttributeCapacity;
	xrtxmlelement** pChildren;
	uint32 iChildCount;
	uint32 iChildCapacity;
};

struct xrtxmldocument_struct {
	uint32 iReferences;
	xrtxmlelement* pRoot;
};

str __xrtXmlEtreeCopy(const char* sText);
void __xrtXmlEtreeSetError(xrtxmlerror* pError, int iCode,
	const char* sMessage);
xrtxmlelement* __xrtXmlEtreeCreateNode(xrtxmldocument* pDocument,
	int iKind, const char* sName);
bool __xrtXmlEtreeAppendNode(xrtxmlelement* pParent,
	xrtxmlelement* pChild);
bool __xrtXmlEtreeAppendText(str* psTarget, const char* sText);
void __xrtXmlEtreeFreeNode(xrtxmlelement* pElement);

#endif
