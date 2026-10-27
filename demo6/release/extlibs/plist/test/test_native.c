#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_plist.h"

#include <stdio.h>
#include <string.h>

extern ptr xplist_create(void);
extern void xplist_destroy(ptr pHandle);
extern ptr xplist_new_dict(ptr pHandle);
extern ptr xplist_new_string(ptr pHandle, xvalue* pValue);
extern bool xplist_dict_set(ptr pHandle, ptr pNode,
	xvalue* pKey, ptr pValue);
extern bool xplist_set_root(ptr pHandle, ptr pNode);
extern ptr xplist_parse(ptr pData, uint64 iSize, int iFormat,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth);
extern int64 xplist_error_code(void);
extern ptr xplist_stringify(ptr pHandle, int iFormat, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth);
extern char* xplist_stringify_text(ptr pHandle, bool bPretty,
	bool bSortKeys, uint32 iIndent, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth);
extern char* xplist_error_message(void);

int main(void)
{
	const char sKey[] = { 'a', 0, 'b' };
	const char sText[] = { 'x', 0, 'y' };
	ptr pHandle = xplist_create();
	ptr pRoot = xplist_new_dict(pHandle);
	ptr pXmlHandle = NULL;
	ptr pXmlRoot = NULL;
	xvalue* pKey = xrtValueString((xstrview){ sKey, sizeof(sKey) });
	xvalue* pText = xrtValueString((xstrview){ sText, sizeof(sText) });
	xbuffer* pEncoded = NULL;
	char* sXml = NULL;
	char* sError = NULL;
	xrtplistdocument* pParsed = NULL;
	xrtplisterror tError;
	xrtplistnode* pValue;
	int bOk = pHandle != NULL && pRoot != NULL && pKey != NULL &&
		pText != NULL && xplist_dict_set(pHandle, pRoot, pKey,
			xplist_new_string(pHandle, pText)) &&
		xplist_set_root(pHandle, pRoot);
	if ( bOk ) {
		pEncoded = (xbuffer*)xplist_stringify(pHandle,
			XRT_PLIST_FORMAT_BINARY, false, false, 2u,
			1024u, 1024u, 10u, 10u, 4u);
		bOk = pEncoded != NULL;
	}
	if ( bOk ) {
		pParsed = xrtPlistParseMemory(pEncoded->Data, pEncoded->Size,
			NULL, &tError);
		pValue = pParsed != NULL ? xrtPlistDictGet(xrtPlistRoot(pParsed),
			sKey, sizeof(sKey)) : NULL;
		bOk = pValue != NULL && xrtPlistByteSize(pValue) == sizeof(sText) &&
			memcmp(xrtPlistBytes(pValue), sText, sizeof(sText)) == 0;
	}
	if ( bOk ) {
		pXmlHandle = xplist_create();
		pXmlRoot = xplist_new_dict(pXmlHandle);
		bOk = pXmlHandle != NULL && pXmlRoot != NULL &&
			xplist_set_root(pXmlHandle, pXmlRoot);
	}
	if ( bOk ) {
		sXml = xplist_stringify_text(pXmlHandle, false, false, 2u,
			1024u, 1024u, 10u, 10u, 4u);
		bOk = sXml != NULL && strstr(sXml, "<plist") != NULL;
	}
	if ( bOk ) {
		bOk = xplist_parse((ptr)"bad", 3u, XRT_PLIST_FORMAT_AUTO,
			1024u, 1024u, 1024u, 10u, 10u, 4u) == NULL &&
			xplist_error_code() == XRT_PLIST_ERROR_FORMAT;
	}
	if ( bOk ) {
		sError = xplist_error_message();
		bOk = sError != NULL && sError[0] != 0;
	}
	xrtFree(sError);
	xrtFree(sXml);
	xrtPlistDestroy(pParsed);
	xrtBufferDestroy(pEncoded);
	xrtValueRelease(pText);
	xrtValueRelease(pKey);
	xplist_destroy(pXmlHandle);
	xplist_destroy(pHandle);
	if ( bOk ) { puts("test_plist_native: PASS"); }
	return bOk ? 0 : 2;
}
