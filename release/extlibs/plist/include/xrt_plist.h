#ifndef XRT_PLIST_H
#define XRT_PLIST_H

/* Plist 公共契约直接依赖连续缓冲和通用 Reader/Writer。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif

#include "xrt.h"

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

XRT_API void xrtPlistOptionsInit(xrtplistoptions* pOptions);
XRT_API void xrtPlistErrorInit(xrtplisterror* pError);

/* 拥有型 Reader 在退出时关闭；正文成功但关闭失败返回 IO。 */
XRT_API xrtplistdocument* xrtPlistParse(xreader* pReader, bool bTakeOwnership,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
XRT_API xrtplistdocument* xrtPlistParseMemory(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
XRT_API xrtplistdocument* xrtPlistLoad(str sPath,
	const xrtplistoptions* pOptions, xrtplisterror* pError);

XRT_API xrtplistdocument* xrtPlistCreate(void);
XRT_API void xrtPlistDestroy(xrtplistdocument* pDocument);
XRT_API xrtplistnode* xrtPlistRoot(const xrtplistdocument* pDocument);
XRT_API bool xrtPlistSetRoot(xrtplistdocument* pDocument,
	xrtplistnode* pRoot);

XRT_API xrtplistnode* xrtPlistCreateNull(xrtplistdocument* pDocument);
XRT_API xrtplistnode* xrtPlistCreateBool(xrtplistdocument* pDocument,
	bool bValue);
XRT_API xrtplistnode* xrtPlistCreateInt(xrtplistdocument* pDocument,
	int64 iValue);
XRT_API xrtplistnode* xrtPlistCreateReal(xrtplistdocument* pDocument,
	double fValue);
XRT_API xrtplistnode* xrtPlistCreateString(xrtplistdocument* pDocument,
	str sValue);
XRT_API xrtplistnode* xrtPlistCreateStringEx(xrtplistdocument* pDocument,
	const void* pValue, uint64 iSize);
XRT_API xrtplistnode* xrtPlistCreateData(xrtplistdocument* pDocument,
	const void* pValue, uint64 iSize);
XRT_API xrtplistnode* xrtPlistCreateDate(xrtplistdocument* pDocument,
	xtime tValue);
XRT_API xrtplistnode* xrtPlistCreateArray(xrtplistdocument* pDocument);
XRT_API xrtplistnode* xrtPlistCreateDict(xrtplistdocument* pDocument);
XRT_API xrtplistnode* xrtPlistCreateUid(xrtplistdocument* pDocument,
	uint64 iValue);

XRT_API int xrtPlistNodeType(const xrtplistnode* pNode);
XRT_API bool xrtPlistBool(const xrtplistnode* pNode);
XRT_API int64 xrtPlistInt(const xrtplistnode* pNode);
XRT_API double xrtPlistReal(const xrtplistnode* pNode);
XRT_API xtime xrtPlistDate(const xrtplistnode* pNode);
XRT_API uint64 xrtPlistUid(const xrtplistnode* pNode);
XRT_API const void* xrtPlistBytes(const xrtplistnode* pNode);
XRT_API uint64 xrtPlistByteSize(const xrtplistnode* pNode);

XRT_API uint32 xrtPlistArrayCount(const xrtplistnode* pNode);
XRT_API xrtplistnode* xrtPlistArrayGet(const xrtplistnode* pNode,
	uint32 iIndex);
XRT_API bool xrtPlistArrayAppend(xrtplistnode* pNode,
	xrtplistnode* pValue);
XRT_API uint32 xrtPlistDictCount(const xrtplistnode* pNode);
XRT_API const void* xrtPlistDictKey(const xrtplistnode* pNode,
	uint32 iIndex);
XRT_API uint64 xrtPlistDictKeySize(const xrtplistnode* pNode,
	uint32 iIndex);
XRT_API xrtplistnode* xrtPlistDictValue(const xrtplistnode* pNode,
	uint32 iIndex);
XRT_API xrtplistnode* xrtPlistDictGet(const xrtplistnode* pNode,
	const void* pKey, uint64 iKeySize);
XRT_API bool xrtPlistDictSet(xrtplistnode* pNode, const void* pKey,
	uint64 iKeySize, xrtplistnode* pValue);

XRT_API xbuffer* xrtPlistStringify(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);
/* 拥有型 Writer 关闭失败返回 false；已有错误保持首因。 */
XRT_API bool xrtPlistWrite(const xrtplistdocument* pDocument, xwriter* pWriter,
	bool bTakeOwnership, const xrtplistoptions* pOptions,
	xrtplisterror* pError);
XRT_API bool xrtPlistSave(str sPath, const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError);

#ifdef __cplusplus
}
#endif

#endif
