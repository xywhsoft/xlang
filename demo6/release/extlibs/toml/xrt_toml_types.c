#include "xrt_toml_internal.h"

#include <limits.h>

/* 普通 TOML 表可以拥有同名字段；只有构造器产生的对象才是日期。 */
#define XRT_TOML_DATETIME_TYPE_ID UINT64_C(0x544f4d4c44415445)

static const char* __xrtTomlKindName(int iKind)
{
	switch ( iKind ) {
	case XRT_TOML_DATE: return "date";
	case XRT_TOML_TIME: return "time";
	case XRT_TOML_DATETIME: return "datetime";
	case XRT_TOML_OFFSET_DATETIME: return "offset-datetime";
	default: return NULL;
	}
}

static bool __xrtTomlValidDateParts(int iYear, int iMonth, int iDay)
{
	return iYear >= 0 && iYear <= 9999 && iMonth >= 1 && iMonth <= 12 &&
		iDay >= 1 && iDay <= xrtDaysInMonth(iYear, iMonth);
}

static bool __xrtTomlValidTimeParts(int iHour, int iMinute, int iSecond,
	int iMicrosecond)
{
	return iHour >= 0 && iHour <= 23 && iMinute >= 0 && iMinute <= 59 &&
		iSecond >= 0 && iSecond <= 59 && iMicrosecond >= 0 &&
		iMicrosecond <= 999999;
}

static bool __xrtTomlValidDateTime(const xrttomldatetime* pValue)
{
	if ( pValue == NULL || __xrtTomlKindName(pValue->iKind) == NULL ||
		pValue->iReserved != 0 ) { return false; }
	if ( pValue->iKind == XRT_TOML_DATE ) {
		return __xrtTomlValidDateParts(pValue->iYear, pValue->iMonth,
			pValue->iDay) && pValue->iHour == 0 && pValue->iMinute == 0 &&
			pValue->iSecond == 0 && pValue->iMicrosecond == 0 &&
			pValue->iTimezoneMinutes == 0;
	}
	if ( pValue->iKind == XRT_TOML_TIME ) {
		return pValue->iYear == 0 && pValue->iMonth == 0 &&
			pValue->iDay == 0 && pValue->iTimezoneMinutes == 0 &&
			__xrtTomlValidTimeParts(pValue->iHour, pValue->iMinute,
				pValue->iSecond, pValue->iMicrosecond);
	}
	return __xrtTomlValidDateParts(pValue->iYear, pValue->iMonth,
		pValue->iDay) && __xrtTomlValidTimeParts(pValue->iHour,
		pValue->iMinute, pValue->iSecond, pValue->iMicrosecond) &&
		(pValue->iKind == XRT_TOML_DATETIME ?
			pValue->iTimezoneMinutes == 0 :
			(pValue->iTimezoneMinutes >= -1439 &&
			 pValue->iTimezoneMinutes <= 1439));
}

static bool __xrtTomlSetInt(xvalue* pObject, const char* sKey, int iValue)
{
	xvalue* pValue = xrtValueInt(iValue);
	return pValue != NULL && xrtValueObjectSetTake(pObject, xrtStrView(sKey),
		&pValue);
}

static bool __xrtTomlSetText(xvalue* pObject, const char* sKey,
	const char* sValue)
{
	xvalue* pValue = xrtValueString(xrtStrView(sValue));
	return pValue != NULL && xrtValueObjectSetTake(pObject, xrtStrView(sKey),
		&pValue);
}

static bool __xrtTomlGetInt(const xvalue* pObject, const char* sKey,
	int* pResult)
{
	int64 iValue;
	xvalue* pValue = xrtValueObjectGet(pObject, xrtStrView(sKey));
	if ( !xrtValueGetInt(pValue, &iValue) || iValue < INT_MIN ||
		iValue > INT_MAX ) {
		return false;
	}
	*pResult = (int)iValue;
	return true;
}

static bool __xrtTomlGetShort(const xvalue* pObject, const char* sKey,
	int16* pResult)
{
	int iValue;
	if ( !__xrtTomlGetInt(pObject, sKey, &iValue) || iValue < INT16_MIN ||
		iValue > INT16_MAX ) {
		return false;
	}
	*pResult = (int16)iValue;
	return true;
}

static xvalue* __xrtTomlCreateDateTimeValue(const xrttomldatetime* pValue)
{
	xvalue* pResult;
	const char* sKind = pValue != NULL ? __xrtTomlKindName(pValue->iKind) : NULL;

	if ( sKind == NULL || !__xrtTomlValidDateTime(pValue) ) {
		return NULL;
	}
	pResult = xrtValueObject();
	if ( pResult == NULL || !__xrtTomlSetText(pResult, "__toml_type", sKind) ||
		!__xrtTomlSetInt(pResult, "year", pValue->iYear) ||
		!__xrtTomlSetInt(pResult, "month", pValue->iMonth) ||
		!__xrtTomlSetInt(pResult, "day", pValue->iDay) ||
		!__xrtTomlSetInt(pResult, "hour", pValue->iHour) ||
		!__xrtTomlSetInt(pResult, "minute", pValue->iMinute) ||
		!__xrtTomlSetInt(pResult, "second", pValue->iSecond) ||
		!__xrtTomlSetInt(pResult, "microsecond", pValue->iMicrosecond) ||
		!__xrtTomlSetInt(pResult, "offsetMinutes", pValue->iTimezoneMinutes) ||
		!xrtValueTypeIdBind(pResult, XRT_TOML_DATETIME_TYPE_ID) ) {
		xrtValueRelease(pResult);
		return NULL;
	}
	return pResult;
}

XRT_API xvalue* xrtTomlCreateDate(int iYear, int iMonth, int iDay)
{
	if ( !__xrtTomlValidDateParts(iYear, iMonth, iDay) ) { return NULL; }
	xrttomldatetime tValue = { (int16)iYear, (int16)iMonth, (int16)iDay,
		0, 0, 0, 0, 0, XRT_TOML_DATE, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XRT_API xvalue* xrtTomlCreateTime(int iHour, int iMinute, int iSecond,
	int iMicrosecond)
{
	if ( !__xrtTomlValidTimeParts(iHour, iMinute, iSecond, iMicrosecond) ) {
		return NULL;
	}
	xrttomldatetime tValue = { 0, 0, 0, (int16)iHour, (int16)iMinute,
		(int16)iSecond, iMicrosecond, 0, XRT_TOML_TIME, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XRT_API xvalue* xrtTomlCreateDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond)
{
	if ( !__xrtTomlValidDateParts(iYear, iMonth, iDay) ||
		!__xrtTomlValidTimeParts(iHour, iMinute, iSecond, iMicrosecond) ) {
		return NULL;
	}
	xrttomldatetime tValue = { (int16)iYear, (int16)iMonth, (int16)iDay,
		(int16)iHour, (int16)iMinute, (int16)iSecond, iMicrosecond, 0,
		XRT_TOML_DATETIME, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XRT_API xvalue* xrtTomlCreateOffsetDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond,
	int iTimezoneMinutes)
{
	if ( !__xrtTomlValidDateParts(iYear, iMonth, iDay) ||
		!__xrtTomlValidTimeParts(iHour, iMinute, iSecond, iMicrosecond) ||
		iTimezoneMinutes < -1439 || iTimezoneMinutes > 1439 ) {
		return NULL;
	}
	xrttomldatetime tValue = { (int16)iYear, (int16)iMonth, (int16)iDay,
		(int16)iHour, (int16)iMinute, (int16)iSecond, iMicrosecond,
		(int16)iTimezoneMinutes, XRT_TOML_OFFSET_DATETIME, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XRT_API bool xrtTomlGetDateTime(const xvalue* pValue,
	xrttomldatetime* pResult)
{
	xstrview Kind;
	xvalue* pKind;
	int iKind;

	if ( pValue == NULL || pResult == NULL ||
		xrtValueType(pValue) != XVALUE_OBJECT ||
		xrtValueTypeId(pValue) != XRT_TOML_DATETIME_TYPE_ID ) {
		return false;
	}
	pKind = xrtValueObjectGet(pValue, xrtStrView("__toml_type"));
	if ( pKind == NULL || xrtValueType(pKind) != XVALUE_STRING ||
		!xrtValueGetString(pKind, &Kind) ) { return false; }
	if ( Kind.Size == 4u && memcmp(Kind.Data, "date", 4u) == 0 ) {
		iKind = XRT_TOML_DATE;
	} else if ( Kind.Size == 4u && memcmp(Kind.Data, "time", 4u) == 0 ) {
		iKind = XRT_TOML_TIME;
	} else if ( Kind.Size == 8u && memcmp(Kind.Data, "datetime", 8u) == 0 ) {
		iKind = XRT_TOML_DATETIME;
	} else if ( Kind.Size == 15u &&
		memcmp(Kind.Data, "offset-datetime", 15u) == 0 ) {
		iKind = XRT_TOML_OFFSET_DATETIME;
	} else {
		return false;
	}
	memset(pResult, 0, sizeof(*pResult));
	pResult->iKind = (uint8)iKind;
	return __xrtTomlGetShort(pValue, "year", &pResult->iYear) &&
		__xrtTomlGetShort(pValue, "month", &pResult->iMonth) &&
		__xrtTomlGetShort(pValue, "day", &pResult->iDay) &&
		__xrtTomlGetShort(pValue, "hour", &pResult->iHour) &&
		__xrtTomlGetShort(pValue, "minute", &pResult->iMinute) &&
		__xrtTomlGetShort(pValue, "second", &pResult->iSecond) &&
		__xrtTomlGetInt(pValue, "microsecond", &pResult->iMicrosecond) &&
		__xrtTomlGetShort(pValue, "offsetMinutes", &pResult->iTimezoneMinutes) &&
		__xrtTomlValidDateTime(pResult);
}

XRT_API bool xrtTomlIsDateTimeValue(const xvalue* pValue, int* pKind)
{
	xrttomldatetime Value;
	bool bResult = xrtTomlGetDateTime(pValue, &Value);
	if ( pKind != NULL ) { *pKind = bResult ? Value.iKind : 0; }
	return bResult;
}

XRT_API str xrtTomlDateTimeToString(const xrttomldatetime* pValue)
{
	char sText[96];
	char sFraction[16] = "";
	char sZone[16] = "";
	int iSize;

	if ( !__xrtTomlValidDateTime(pValue) ) {
		return NULL;
	}
	if ( pValue->iMicrosecond != 0 ) {
		int iEnd;
		snprintf(sFraction, sizeof(sFraction), ".%06d", pValue->iMicrosecond);
		iEnd = (int)strlen(sFraction) - 1;
		while ( iEnd > 0 && sFraction[iEnd] == '0' ) { sFraction[iEnd--] = 0; }
	}
	if ( pValue->iKind == XRT_TOML_OFFSET_DATETIME ) {
		int iOffset = pValue->iTimezoneMinutes;
		if ( iOffset == 0 ) { strcpy(sZone, "Z"); }
		else {
			char cSign = iOffset < 0 ? '-' : '+';
			if ( iOffset < 0 ) { iOffset = -iOffset; }
			snprintf(sZone, sizeof(sZone), "%c%02d:%02d", cSign,
				iOffset / 60, iOffset % 60);
		}
	}
	if ( pValue->iKind == XRT_TOML_DATE ) {
		iSize = snprintf(sText, sizeof(sText), "%04d-%02d-%02d",
			pValue->iYear, pValue->iMonth, pValue->iDay);
	} else if ( pValue->iKind == XRT_TOML_TIME ) {
		iSize = snprintf(sText, sizeof(sText), "%02d:%02d:%02d%s",
			pValue->iHour, pValue->iMinute, pValue->iSecond, sFraction);
	} else {
		iSize = snprintf(sText, sizeof(sText), "%04d-%02d-%02dT%02d:%02d:%02d%s%s",
			pValue->iYear, pValue->iMonth, pValue->iDay, pValue->iHour,
			pValue->iMinute, pValue->iSecond, sFraction, sZone);
	}
	return iSize > 0 && (size_t)iSize < sizeof(sText) ? xrtStrDup(sText) : NULL;
}
