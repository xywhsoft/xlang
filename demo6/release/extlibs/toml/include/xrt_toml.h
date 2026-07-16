#ifndef XRT_TOML_H
#define XRT_TOML_H

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

XXAPI void xrtTomlOptionsInit(xrttomloptions* pOptions);
XXAPI void xrtTomlErrorInit(xrttomlerror* pError);
XXAPI xvalue xrtTomlParse(xreader pReader, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError);
XXAPI xvalue xrtTomlParseMemory(const void* pData, uint64 iSize,
	const xrttomloptions* pOptions, xrttomlerror* pError);
XXAPI xvalue xrtTomlLoad(str sPath, const xrttomloptions* pOptions,
	xrttomlerror* pError);

XXAPI xbuffer xrtTomlStringify(xvalue pValue, const xrttomloptions* pOptions,
	xrttomlerror* pError);
XXAPI bool xrtTomlWrite(xvalue pValue, xwriter pWriter, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError);
XXAPI bool xrtTomlSave(str sPath, xvalue pValue, const xrttomloptions* pOptions,
	xrttomlerror* pError);

XXAPI const xrt_type_desc* xrtTomlDateType(void);
XXAPI const xrt_type_desc* xrtTomlTimeType(void);
XXAPI const xrt_type_desc* xrtTomlDateTimeType(void);
XXAPI const xrt_type_desc* xrtTomlOffsetDateTimeType(void);
XXAPI xvalue xrtTomlCreateDate(int iYear, int iMonth, int iDay);
XXAPI xvalue xrtTomlCreateTime(int iHour, int iMinute, int iSecond,
	int iMicrosecond);
XXAPI xvalue xrtTomlCreateDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond);
XXAPI xvalue xrtTomlCreateOffsetDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond,
	int iTimezoneMinutes);
XXAPI bool xrtTomlIsDateTimeValue(xvalue pValue, int* pKind);
XXAPI const xrttomldatetime* xrtTomlGetDateTime(xvalue pValue);
XXAPI str xrtTomlDateTimeToString(const xrttomldatetime* pValue);

#ifdef __cplusplus
}
#endif

#endif
