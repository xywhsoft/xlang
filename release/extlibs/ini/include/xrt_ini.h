#ifndef XRT_INI_H
#define XRT_INI_H

/* INI 公共契约直接依赖连续缓冲和通用 Reader/Writer。 */
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

#define XRT_INI_OPTIONS_VERSION 1u

typedef enum {
	XRT_INI_OK = 0,
	XRT_INI_ERROR_ARGUMENT = 1,
	XRT_INI_ERROR_MEMORY = 2,
	XRT_INI_ERROR_FORMAT = 3,
	XRT_INI_ERROR_IO = 4,
	XRT_INI_ERROR_LIMIT = 5,
	XRT_INI_ERROR_DUPLICATE = 6,
	XRT_INI_ERROR_NOT_FOUND = 7,
	XRT_INI_ERROR_CONVERSION = 8,
	XRT_INI_ERROR_INTERPOLATION = 9
} xrtinistatus;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	bool bCaseSensitive;
	/* false 时后一次赋值覆盖旧值，顺序不变，续行跟随本次赋值。 */
	bool bStrictDuplicates;
	bool bInterpolation;
	bool bReserved;
	/* iMaxItemBytes 同时约束解析和 set 的节名、键名、值的 UTF-8 字节数。
	 * iMaxEntries 限制所有节的键总数；iMaxOutputBytes 同时约束拥有型
	 * get 与序列化文本的字节数，0 为不限。压缩倍率、标志不适用。 */
	xrtresourcelimits tLimits;
} xrtinioptions;

typedef struct {
	int iCode;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtinierror;

typedef struct xrtiniconfig xrtiniconfig;

XRT_API void xrtIniOptionsInit(xrtinioptions* pOptions);
XRT_API void xrtIniErrorInit(xrtinierror* pError);
XRT_API xrtiniconfig* xrtIniCreate(const xrtinioptions* pOptions);
/* 拥有型 Reader 在所有退出路径销毁；对象和 XRT 错误槽都保留已有首因。
 * 正文成功后才报告关闭失败，内存故障为 MEMORY，其他关闭错误为 IO。 */
XRT_API xrtiniconfig* xrtIniParse(xreader* pReader, bool bTakeOwnership,
	const xrtinioptions* pOptions);
XRT_API xrtiniconfig* xrtIniParseMemory(const void* pData, uint64 iSize,
	const xrtinioptions* pOptions);
XRT_API xrtiniconfig* xrtIniLoad(str sPath, const xrtinioptions* pOptions);
XRT_API const xrtinierror* xrtIniError(const xrtiniconfig* pConfig);
XRT_API void xrtIniDestroy(xrtiniconfig* pConfig);

XRT_API uint32 xrtIniSectionCount(const xrtiniconfig* pConfig);
XRT_API const char* xrtIniSectionName(const xrtiniconfig* pConfig, uint32 iIndex);
XRT_API bool xrtIniHasSection(const xrtiniconfig* pConfig, str sSection);
XRT_API uint32 xrtIniKeyCount(const xrtiniconfig* pConfig, str sSection);
XRT_API const char* xrtIniKeyName(const xrtiniconfig* pConfig, str sSection, uint32 iIndex);
XRT_API bool xrtIniHas(const xrtiniconfig* pConfig, str sSection, str sKey);
XRT_API const char* xrtIniGetRaw(const xrtiniconfig* pConfig, str sSection, str sKey);
XRT_API str xrtIniGet(const xrtiniconfig* pConfig, str sSection, str sKey);
XRT_API bool xrtIniGetInt(const xrtiniconfig* pConfig, str sSection, str sKey, int64* pValue);
XRT_API bool xrtIniGetFloat(const xrtiniconfig* pConfig, str sSection, str sKey, double* pValue);
XRT_API bool xrtIniGetBool(const xrtiniconfig* pConfig, str sSection, str sKey, bool* pValue);

/* 超过单项限额返回 LIMIT；失败保留原内容、顺序和计数，不留下新增节。 */
XRT_API bool xrtIniSet(xrtiniconfig* pConfig, str sSection, str sKey, str sValue);
XRT_API bool xrtIniRemove(xrtiniconfig* pConfig, str sSection, str sKey);
XRT_API bool xrtIniRemoveSection(xrtiniconfig* pConfig, str sSection);
XRT_API xbuffer* xrtIniStringify(const xrtiniconfig* pConfig);
/* 拥有型 Writer 关闭失败时返回 false；对象和 XRT 错误槽都保留已有首因。 */
XRT_API bool xrtIniWrite(const xrtiniconfig* pConfig, xwriter* pWriter, bool bTakeOwnership);
XRT_API bool xrtIniSave(const xrtiniconfig* pConfig, str sPath);

#ifdef __cplusplus
}
#endif

#endif
