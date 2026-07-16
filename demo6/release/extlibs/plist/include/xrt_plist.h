#ifndef XRT_PLIST_H
#define XRT_PLIST_H

#include "../../../../lib/xrt/xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_PLIST_OPTIONS_VERSION 1u

typedef enum {
	XRT_PLIST_FORMAT_AUTO = 0,
	XRT_PLIST_FORMAT_XML = 1,
	XRT_PLIST_FORMAT_BINARY = 2
} xrtplistformat;

typedef enum {
	XRT_PLIST_NULL = 0,
	XRT_PLIST_BOOL = 1,
	XRT_PLIST_INT = 2,
	XRT_PLIST_REAL = 3,
	XRT_PLIST_STRING = 4,
	XRT_PLIST_DATA = 5,
	XRT_PLIST_DATE = 6,
	XRT_PLIST_ARRAY = 7,
	XRT_PLIST_DICT = 8,
	XRT_PLIST_UID = 9
} xrtplisttype;

typedef enum {
	XRT_PLIST_OK = 0,
	XRT_PLIST_ERROR_ARGUMENT = 1,
	XRT_PLIST_ERROR_IO = 2,
	XRT_PLIST_ERROR_FORMAT = 3,
	XRT_PLIST_ERROR_TYPE = 4,
	XRT_PLIST_ERROR_LIMIT = 5,
	XRT_PLIST_ERROR_MEMORY = 6
} xrtplisterrorcode;

typedef struct xrtplistdocument_struct xrtplistdocument;
typedef struct xrtplistnode_struct xrtplistnode;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iFormat;
	uint32 iIndent;
	bool bPretty;
	bool bSortKeys;
	xrtresourcelimits tLimits;
} xrtplistoptions;

typedef struct {
	int iCode;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtplisterror;

XXAPI void xrtPlistOptionsInit(xrtplistoptions* pOptions);
XXAPI void xrtPlistErrorInit(xrtplisterror* pError);

XXAPI xrtplistdocument* xrtPlistParse(xreader pReader, bool bTakeOwnership,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
XXAPI xrtplistdocument* xrtPlistParseMemory(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
XXAPI xrtplistdocument* xrtPlistLoad(str sPath,
	const xrtplistoptions* pOptions, xrtplisterror* pError);

XXAPI xrtplistdocument* xrtPlistCreate(void);
XXAPI void xrtPlistDestroy(xrtplistdocument* pDocument);
XXAPI xrtplistnode* xrtPlistRoot(const xrtplistdocument* pDocument);
XXAPI bool xrtPlistSetRoot(xrtplistdocument* pDocument,
	xrtplistnode* pRoot);

XXAPI xrtplistnode* xrtPlistCreateNull(xrtplistdocument* pDocument);
XXAPI xrtplistnode* xrtPlistCreateBool(xrtplistdocument* pDocument,
	bool bValue);
XXAPI xrtplistnode* xrtPlistCreateInt(xrtplistdocument* pDocument,
	int64 iValue);
XXAPI xrtplistnode* xrtPlistCreateReal(xrtplistdocument* pDocument,
	double fValue);
XXAPI xrtplistnode* xrtPlistCreateString(xrtplistdocument* pDocument,
	str sValue);
XXAPI xrtplistnode* xrtPlistCreateStringEx(xrtplistdocument* pDocument,
	const void* pValue, uint64 iSize);
XXAPI xrtplistnode* xrtPlistCreateData(xrtplistdocument* pDocument,
	const void* pValue, uint64 iSize);
XXAPI xrtplistnode* xrtPlistCreateDate(xrtplistdocument* pDocument,
	xtime tValue);
XXAPI xrtplistnode* xrtPlistCreateArray(xrtplistdocument* pDocument);
XXAPI xrtplistnode* xrtPlistCreateDict(xrtplistdocument* pDocument);
XXAPI xrtplistnode* xrtPlistCreateUid(xrtplistdocument* pDocument,
	uint64 iValue);

XXAPI int xrtPlistNodeType(const xrtplistnode* pNode);
XXAPI bool xrtPlistBool(const xrtplistnode* pNode);
XXAPI int64 xrtPlistInt(const xrtplistnode* pNode);
XXAPI double xrtPlistReal(const xrtplistnode* pNode);
XXAPI xtime xrtPlistDate(const xrtplistnode* pNode);
XXAPI uint64 xrtPlistUid(const xrtplistnode* pNode);
XXAPI const void* xrtPlistBytes(const xrtplistnode* pNode);
XXAPI uint64 xrtPlistByteSize(const xrtplistnode* pNode);

XXAPI uint32 xrtPlistArrayCount(const xrtplistnode* pNode);
XXAPI xrtplistnode* xrtPlistArrayGet(const xrtplistnode* pNode,
	uint32 iIndex);
XXAPI bool xrtPlistArrayAppend(xrtplistnode* pNode,
	xrtplistnode* pValue);
XXAPI uint32 xrtPlistDictCount(const xrtplistnode* pNode);
XXAPI const void* xrtPlistDictKey(const xrtplistnode* pNode,
	uint32 iIndex);
XXAPI uint64 xrtPlistDictKeySize(const xrtplistnode* pNode,
	uint32 iIndex);
XXAPI xrtplistnode* xrtPlistDictValue(const xrtplistnode* pNode,
	uint32 iIndex);
XXAPI xrtplistnode* xrtPlistDictGet(const xrtplistnode* pNode,
	const void* pKey, uint64 iKeySize);
XXAPI bool xrtPlistDictSet(xrtplistnode* pNode, const void* pKey,
	uint64 iKeySize, xrtplistnode* pValue);

XXAPI xbuffer xrtPlistStringify(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
XXAPI bool xrtPlistWrite(const xrtplistdocument* pDocument, xwriter pWriter,
	bool bTakeOwnership, const xrtplistoptions* pOptions,
	xrtplisterror* pError);
XXAPI bool xrtPlistSave(str sPath, const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);

#ifdef __cplusplus
}
#endif

#endif
