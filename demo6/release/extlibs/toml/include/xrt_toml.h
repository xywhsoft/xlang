#ifndef XRT_TOML_H
#define XRT_TOML_H

/* TOML 公共契约直接依赖 Value、连续缓冲和通用 Reader/Writer。 */
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

#define XRT_TOML_OPTIONS_VERSION 1u

typedef enum {
	XRT_TOML_OK = 0,
	XRT_TOML_ERROR_ARGUMENT = 1,
	XRT_TOML_ERROR_MEMORY = 2,
	XRT_TOML_ERROR_FORMAT = 3,
	XRT_TOML_ERROR_IO = 4,
	XRT_TOML_ERROR_LIMIT = 5,
	XRT_TOML_ERROR_TYPE = 6
} xrttomlstatus;

typedef enum {
	XRT_TOML_DATE = 1,
	XRT_TOML_TIME = 2,
	XRT_TOML_DATETIME = 3,
	XRT_TOML_OFFSET_DATETIME = 4
} xrttomldatetimekind;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrttomloptions;

typedef struct {
	int iCode;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrttomlerror;

/* TOML 时间值。offset datetime 仅在 XRT time 无法无损表达微秒时使用本记录。 */
typedef struct {
	int16 iYear;
	int16 iMonth;
	int16 iDay;
	int16 iHour;
	int16 iMinute;
	int16 iSecond;
	int32 iMicrosecond;
	int16 iTimezoneMinutes;
	uint8 iKind;
	uint8 iReserved;
} xrttomldatetime;

XRT_API void xrtTomlOptionsInit(xrttomloptions* pOptions);
XRT_API void xrtTomlErrorInit(xrttomlerror* pError);
XRT_API xvalue* xrtTomlParse(xreader* pReader, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError);
XRT_API xvalue* xrtTomlParseMemory(const void* pData, uint64 iSize,
	const xrttomloptions* pOptions, xrttomlerror* pError);
XRT_API xvalue* xrtTomlLoad(str sPath, const xrttomloptions* pOptions,
	xrttomlerror* pError);

XRT_API xbuffer* xrtTomlStringify(xvalue* pValue, const xrttomloptions* pOptions,
	xrttomlerror* pError);
XRT_API bool xrtTomlWrite(xvalue* pValue, xwriter* pWriter, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError);
XRT_API bool xrtTomlSave(str sPath, xvalue* pValue, const xrttomloptions* pOptions,
	xrttomlerror* pError);

/*
 * TOML 的 local date/time 不能无损落入统一 UTC time 值。
 * 因此以普通 xvalue object 保存：__toml_type、year、month、day、hour、
 * minute、second、microsecond、offsetMinutes。该表示对 C/xlang 都透明。
 */
XRT_API xvalue* xrtTomlCreateDate(int iYear, int iMonth, int iDay);
XRT_API xvalue* xrtTomlCreateTime(int iHour, int iMinute, int iSecond,
	int iMicrosecond);
XRT_API xvalue* xrtTomlCreateDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond);
XRT_API xvalue* xrtTomlCreateOffsetDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond,
	int iTimezoneMinutes);
XRT_API bool xrtTomlIsDateTimeValue(const xvalue* pValue, int* pKind);
XRT_API bool xrtTomlGetDateTime(const xvalue* pValue, xrttomldatetime* pResult);
XRT_API str xrtTomlDateTimeToString(const xrttomldatetime* pValue);

#ifdef __cplusplus
}
#endif

#endif
