#include "xrt_plist_internal.h"
#include "../xml.dom/include/xrt_xml_dom.h"



static bool __xrtPlistXmlWhitespace(const char* sText)
{
	const unsigned char* p = (const unsigned char*)(sText != NULL ? sText : "");
	while ( *p != 0u ) {
		if ( *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' ) {
			return false;
		}
		p++;
	}
	return true;
}

static xrtxmldomnode* __xrtPlistXmlOnlyElement(xrtxmldomnode* pParent,
	xrtplisterror* pError)
{
	xrtxmldomnode* pResult = NULL;
	uint32 i;
	for ( i = 0u; i < xrtXmlDomNodeChildCount(pParent); i++ ) {
		xrtxmldomnode* pChild = xrtXmlDomNodeChild(pParent, i);
		int iType = xrtXmlDomNodeType(pChild);
		if ( iType == XRT_XML_DOM_ELEMENT_NODE ) {
			if ( pResult != NULL ) {
				xrtXmlDomNodeRelease(pChild);
				xrtXmlDomNodeRelease(pResult);
				__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
					0u, 0u, 0u, "plist container has more than one root value");
				return NULL;
			}
			pResult = pChild;
		} else if ( iType == XRT_XML_DOM_TEXT_NODE &&
			!__xrtPlistXmlWhitespace((const char*)xrtXmlDomNodeValue(pChild)) ) {
			xrtXmlDomNodeRelease(pChild);
			xrtXmlDomNodeRelease(pResult);
			__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
				0u, 0u, 0u, "unexpected text outside the plist root value");
			return NULL;
		} else {
			xrtXmlDomNodeRelease(pChild);
		}
	}
	return pResult;
}

static bool __xrtPlistXmlContainerElements(xrtxmldomnode* pParent,
	xrtxmldomnode*** ppItems, uint32* pCount, xrtplisterror* pError)
{
	xrtxmldomnode** pItems = NULL;
	uint32 iCount = 0u;
	uint32 i;
	for ( i = 0u; i < xrtXmlDomNodeChildCount(pParent); i++ ) {
		xrtxmldomnode* pChild = xrtXmlDomNodeChild(pParent, i);
		int iType = xrtXmlDomNodeType(pChild);
		if ( iType == XRT_XML_DOM_ELEMENT_NODE ) {
			xrtxmldomnode** pNew = (xrtxmldomnode**)xrtRealloc(pItems,
				sizeof(xrtxmldomnode*) * (iCount + 1u));
			if ( pNew == NULL ) {
				xrtXmlDomNodeRelease(pChild);
				goto memory_error;
			}
			pItems = pNew;
			pItems[iCount++] = pChild;
		} else if ( iType == XRT_XML_DOM_TEXT_NODE &&
			!__xrtPlistXmlWhitespace((const char*)xrtXmlDomNodeValue(pChild)) ) {
			xrtXmlDomNodeRelease(pChild);
			__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
				0u, 0u, 0u, "unexpected text in plist container");
			goto error;
		} else {
			xrtXmlDomNodeRelease(pChild);
		}
	}
	*ppItems = pItems;
	*pCount = iCount;
	return true;

memory_error:
	__xrtPlistSetError(pError, XRT_PLIST_ERROR_MEMORY,
		0u, 0u, 0u, "could not allocate plist XML child list");
error:
	while ( iCount != 0u ) { xrtXmlDomNodeRelease(pItems[--iCount]); }
	xrtFree(pItems);
	return false;
}

static char* __xrtPlistXmlTrim(str sText)
{
	const char* pStart = (const char*)(sText != NULL ? sText : (str)"");
	const char* pEnd = pStart + strlen(pStart);
	while ( pStart < pEnd && (*pStart == ' ' || *pStart == '\t' ||
		*pStart == '\r' || *pStart == '\n') ) { pStart++; }
	while ( pEnd > pStart && (pEnd[-1] == ' ' || pEnd[-1] == '\t' ||
		pEnd[-1] == '\r' || pEnd[-1] == '\n') ) { pEnd--; }
	return xrtStrDupN(pStart, (size_t)(pEnd - pStart));
}

static bool __xrtPlistXmlDate(const char* sText, xtime* pValue)
{
	if ( sText == NULL || pValue == NULL ) { return false; }
	return xrtTimeParseRFC3339(xrtStrView(sText), pValue);
}

static xrtplistnode* __xrtPlistXmlValue(xrtplistdocument* pDocument,
	xrtxmldomnode* pElement, xrtplistcontext* pContext, uint32 iDepth)
{
	const char* sName = (const char*)xrtXmlDomNodeName(pElement);
	str sContent = NULL;
	char* sTrimmed = NULL;
	xrtplistnode* pResult = NULL;
	if ( !__xrtPlistLimitNode(pContext, iDepth, 0u) ) { return NULL; }
	if ( strcmp(sName, "true") == 0 || strcmp(sName, "false") == 0 ) {
		if ( xrtXmlDomNodeChildCount(pElement) != 0u ) { goto invalid_empty; }
		return xrtPlistCreateBool(pDocument, sName[0] == 't');
	}
	if ( strcmp(sName, "array") == 0 || strcmp(sName, "dict") == 0 ) {
		xrtxmldomnode** pChildren = NULL;
		uint32 iCount = 0u;
		uint32 i;
		if ( !__xrtPlistXmlContainerElements(pElement, &pChildren, &iCount,
			pContext->Error) ) { return NULL; }
		if ( pContext->Options.tLimits.iMaxEntries != 0u &&
			(uint64)iCount > pContext->Options.tLimits.iMaxEntries *
			(strcmp(sName, "dict") == 0 ? 2u : 1u) ) {
			__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_LIMIT,
				0u, 0u, 0u, "plist container exceeds the configured entry limit");
			goto container_cleanup;
		}
		if ( strcmp(sName, "array") == 0 ) {
			pResult = xrtPlistCreateArray(pDocument);
			for ( i = 0u; pResult != NULL && i < iCount; i++ ) {
				xrtplistnode* pChild = __xrtPlistXmlValue(pDocument,
					pChildren[i], pContext, iDepth + 1u);
				if ( pChild == NULL || !xrtPlistArrayAppend(pResult, pChild) ) {
					pResult = NULL;
				}
			}
		} else {
			pResult = xrtPlistCreateDict(pDocument);
			if ( iCount % 2u != 0u ) {
				__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_FORMAT,
					0u, 0u, 0u, "plist dict must contain key/value pairs");
				pResult = NULL;
			}
			for ( i = 0u; pResult != NULL && i < iCount; i += 2u ) {
				str sKey;
				xrtplistnode* pChild;
				if ( strcmp((const char*)xrtXmlDomNodeName(pChildren[i]), "key") != 0 ) {
					__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_FORMAT,
						0u, 0u, 0u, "plist dict value is missing a key");
					pResult = NULL; break;
				}
				sKey = xrtXmlDomTextContent(pChildren[i]);
				pChild = __xrtPlistXmlValue(pDocument, pChildren[i + 1u],
					pContext, iDepth + 1u);
				if ( sKey == NULL || !__xrtPlistLimitItem(pContext,
					strlen((const char*)sKey), 0u) ||
					xrtPlistDictGet(pResult, sKey, strlen((const char*)sKey)) != NULL ||
					pChild == NULL || !xrtPlistDictSet(pResult, sKey,
					strlen((const char*)sKey), pChild) ) {
					if ( pContext->Error->iCode == XRT_PLIST_OK ) {
						__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_FORMAT,
							0u, 0u, 0u, "duplicate or invalid plist dict key");
					}
					pResult = NULL;
				}
				xrtFree(sKey);
			}
		}
container_cleanup:
		for ( i = 0u; i < iCount; i++ ) { xrtXmlDomNodeRelease(pChildren[i]); }
		xrtFree(pChildren);
		return pResult;
	}
	sContent = xrtXmlDomTextContent(pElement);
	if ( sContent == NULL ) {
		__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
			0u, 0u, 0u, "could not read plist XML value text");
		goto cleanup;
	}
	if ( !__xrtPlistLimitItem(pContext,
		strlen((const char*)sContent), 0u) ) { goto cleanup; }
	if ( strcmp(sName, "string") == 0 ) {
		pResult = xrtPlistCreateStringEx(pDocument, sContent,
			strlen((const char*)sContent));
		if ( pResult == NULL ) {
			__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
				0u, 0u, 0u, "could not allocate plist XML string");
		}
	} else if ( strcmp(sName, "integer") == 0 ) {
		int64 iValue;
		if ( xrtIntParse((xstrview){ sContent, strlen((const char*)sContent) },
			10u, XNUMBER_PARSE_SPACE, &iValue) ) {
			pResult = xrtPlistCreateInt(pDocument, iValue);
			if ( pResult == NULL ) {
				__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
					0u, 0u, 0u, "could not allocate plist XML integer");
			}
		}
	} else if ( strcmp(sName, "real") == 0 ) {
		double fValue;
		if ( xrtNumParse((xstrview){ sContent,
			strlen((const char*)sContent) }, XNUMBER_PARSE_SPACE,
			&fValue) && isfinite(fValue) ) {
			pResult = xrtPlistCreateReal(pDocument, fValue);
			if ( pResult == NULL ) {
				__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
					0u, 0u, 0u, "could not allocate plist XML real");
			}
		}
	} else if ( strcmp(sName, "date") == 0 ) {
		xtime tValue;
		sTrimmed = __xrtPlistXmlTrim(sContent);
		if ( __xrtPlistXmlDate(sTrimmed, &tValue) ) {
			pResult = xrtPlistCreateDate(pDocument, tValue);
			if ( pResult == NULL ) {
				__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
					0u, 0u, 0u, "could not allocate plist XML date");
			}
		}
	} else if ( strcmp(sName, "data") == 0 ) {
		xbase64config tBase64 = { NULL, XBASE64_IGNORE_SPACE };
		size_t iSize = 0u;
		uint8* pData;
		xrtClearError();
		pData = (uint8*)xrtBase64DecodeNew((const char*)sContent,
			strlen((const char*)sContent), &iSize, &tBase64);
		if ( pData != NULL && __xrtPlistLimitItem(pContext, iSize, 0u) ) {
			pResult = xrtPlistCreateData(pDocument, pData, (uint64)iSize);
			if ( pResult == NULL ) {
				__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
					0u, 0u, 0u, "could not allocate plist XML data");
			}
		} else if ( pData == NULL &&
			xrtErrorKind(xrtGetError()) == XERR_MEMORY ) {
			__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_MEMORY,
				0u, 0u, 0u, "could not decode plist XML data");
		}
		xrtFree(pData);
	} else {
		__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_TYPE,
			0u, 0u, 0u, "unsupported plist XML element: %s", sName);
	}
	if ( pResult == NULL && pContext->Error->iCode == XRT_PLIST_OK ) {
		int iCode = sTrimmed == NULL && strcmp(sName, "date") == 0 ?
			XRT_PLIST_ERROR_MEMORY : XRT_PLIST_ERROR_FORMAT;
		__xrtPlistSetError(pContext->Error, iCode,
			0u, 0u, 0u, "invalid plist XML value: %s", sName);
	}
cleanup:
	xrtFree(sTrimmed);
	xrtFree(sContent);
	return pResult;

invalid_empty:
	__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_FORMAT,
		0u, 0u, 0u, "plist boolean element must be empty");
	return NULL;
}

xrtplistdocument* __xrtPlistParseXml(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtxmloptions tXmlOptions;
	xrtxmlerror tXmlError;
	xrtxmldomnode* pXmlDocument;
	xrtxmldomnode* pPlist;
	xrtxmldomnode* pValue;
	xrtplistdocument* pDocument = NULL;
	xrtplistcontext tContext;
	str sVersion;
	if ( iSize > SIZE_MAX || !xrtUtf8Valid(
		(xstrview){ (cstr)pData, (size_t)iSize }, NULL) ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, 0u, "plist XML input is not valid UTF-8");
		return NULL;
	}
	xrtXmlOptionsInit(&tXmlOptions);
	tXmlOptions.tLimits = pOptions->tLimits;
	tXmlOptions.bAllowDtd = true;
	tXmlOptions.bAllowExternalEntities = false;
	pXmlDocument = xrtXmlDomParseMemory(pData, iSize, &tXmlOptions, &tXmlError);
	if ( pXmlDocument == NULL ) {
		int iCode = XRT_PLIST_ERROR_FORMAT;
		if ( tXmlError.iCode == XRT_XML_ERROR_MEMORY )
			iCode = XRT_PLIST_ERROR_MEMORY;
		else if ( tXmlError.iCode == XRT_XML_ERROR_LIMIT )
			iCode = XRT_PLIST_ERROR_LIMIT;
		else if ( tXmlError.iCode == XRT_XML_ERROR_IO )
			iCode = XRT_PLIST_ERROR_IO;
		__xrtPlistSetError(pError, iCode,
			tXmlError.iLine, tXmlError.iColumn, tXmlError.iOffset,
			"invalid plist XML: %s", tXmlError.sMessage);
		return NULL;
	}
	pPlist = xrtXmlDomDocumentElement(pXmlDocument);
	if ( pPlist == NULL ||
		strcmp((const char*)xrtXmlDomNodeName(pPlist), "plist") != 0 ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, 0u, "plist XML root element must be <plist>");
		goto cleanup;
	}
	sVersion = xrtXmlDomGetAttribute(pPlist, (str)"version");
	if ( sVersion != NULL && strcmp((const char*)sVersion, "1.0") != 0 ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
			0u, 0u, 0u, "unsupported plist XML version");
		goto cleanup;
	}
	pValue = __xrtPlistXmlOnlyElement(pPlist, pError);
	if ( pValue == NULL ) {
		if ( pError->iCode == XRT_PLIST_OK ) {
			__xrtPlistSetError(pError, XRT_PLIST_ERROR_FORMAT,
				0u, 0u, 0u, "plist XML does not contain a root value");
		}
		goto cleanup;
	}
	pDocument = xrtPlistCreate();
	memset(&tContext, 0, sizeof(tContext));
	tContext.Options = *pOptions;
	tContext.Error = pError;
	if ( pDocument == NULL || !xrtPlistSetRoot(pDocument,
		__xrtPlistXmlValue(pDocument, pValue, &tContext, 1u)) ) {
		if ( pError->iCode == XRT_PLIST_OK ) {
			__xrtPlistSetError(pError, XRT_PLIST_ERROR_MEMORY,
				0u, 0u, 0u, "could not build plist XML value tree");
		}
		xrtPlistDestroy(pDocument);
		pDocument = NULL;
	}
	xrtXmlDomNodeRelease(pValue);

cleanup:
	xrtXmlDomNodeRelease(pPlist);
	xrtXmlDomNodeRelease(pXmlDocument);
	return pDocument;
}
