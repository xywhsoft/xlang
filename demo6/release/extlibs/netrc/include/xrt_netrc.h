#ifndef XRT_NETRC_H
#define XRT_NETRC_H

/* NETRC 公共契约直接依赖 Value、连续缓冲和通用 Reader/Writer。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_NETRC_OPTIONS_VERSION 1u

enum {
	XRT_NETRC_OK = 0,
	XRT_NETRC_ERROR_ARGUMENT = 1,
	XRT_NETRC_ERROR_SYNTAX = 2,
	XRT_NETRC_ERROR_DUPLICATE = 3,
	XRT_NETRC_ERROR_LIMIT = 4,
	XRT_NETRC_ERROR_UTF8 = 5,
	XRT_NETRC_ERROR_IO = 6,
	XRT_NETRC_ERROR_MEMORY = 7,
	XRT_NETRC_ERROR_TYPE = 8
};

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
	bool bStrictDuplicates;
	uint8 iReserved[7];
} xrtnetrcoptions;

typedef struct {
	int iCode;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtnetrcerror;

/* 深度、压缩比、资源标志及保留字段不适用于 netrc，须保持初始化值。 */
XRT_API void xrtNetrcOptionsInit(xrtnetrcoptions* pOptions);
XRT_API void xrtNetrcErrorInit(xrtnetrcerror* pError);
/* 拥有型流在所有退出路径关闭；正文成功但关闭失败时返回 IO 错误。 */
XRT_API xvalue* xrtNetrcParse(xreader* pReader, bool bTakeOwnership,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError);
XRT_API xvalue* xrtNetrcParseMemory(const void* pData, uint64 iSize,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError);
XRT_API xvalue* xrtNetrcLoad(str sPath, const xrtnetrcoptions* pOptions,
	xrtnetrcerror* pError);
XRT_API xbuffer* xrtNetrcStringify(xvalue* pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError);
/* 拥有型 Writer 的关闭失败计为 IO；已有写入错误保持首因。 */
XRT_API bool xrtNetrcWrite(xvalue* pDocument, xwriter* pWriter,
	bool bTakeOwnership, const xrtnetrcoptions* pOptions,
	xrtnetrcerror* pError);
XRT_API bool xrtNetrcSave(str sPath, xvalue* pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError);

#ifdef __cplusplus
}
#endif

#endif
