#include "xui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_DATE_PICKER_PAD		12.0f
#define XUI_DATE_PICKER_TIME_INSET	12.0f
#define XUI_DATE_PICKER_TIME_MIN_HEIGHT	84.0f
#define XUI_DATE_PICKER_CELL_COUNT	42
#define XUI_DATE_PICKER_CLOSE_NONE	0
#define XUI_DATE_PICKER_CLOSE_SILENT	1
#define XUI_DATE_PICKER_CLOSE_CANCEL	2
#define XUI_DATE_PICKER_CLOSE_COMMIT	3
#define XUI_DATE_PICKER_CLOSE_CLEAR	4
#define XUI_DATE_PICKER_PART_YEAR_FIELD	100
#define XUI_DATE_PICKER_PART_MONTH_FIELD	101
#define XUI_DATE_PICKER_PART_YEAR_OPTION	102
#define XUI_DATE_PICKER_PART_MONTH_OPTION	103
#define XUI_DATE_PICKER_EDIT_NONE	0
#define XUI_DATE_PICKER_EDIT_YEAR	1
#define XUI_DATE_PICKER_EDIT_TIME	2
#define XUI_DATE_PICKER_COMBO_NONE	0
#define XUI_DATE_PICKER_COMBO_YEAR	1
#define XUI_DATE_PICKER_COMBO_MONTH	2
#define XUI_DATE_PICKER_YEAR_OPTION_COUNT	9
#define XUI_DATE_PICKER_YEAR_INPUT_LIMIT	8
#define XUI_DATE_PICKER_YEAR_MIN		1LL
#define XUI_DATE_PICKER_YEAR_MAX		99999999LL
#define XUI_DATE_PICKER_YEAR_FIELD_W		112.0f
#define XUI_DATE_PICKER_MONTH_FIELD_W		62.0f
#define XUI_DATE_PICKER_HEADER_COMBO_GAP	6.0f

typedef struct xui_date_picker_data_t {
	xui_widget pPopup;
	xui_widget pPanel;
	xui_font pFont;
	xui_date_picker_proc onChanging;
	xui_date_picker_proc onChange;
	xui_date_picker_proc onCommit;
	xui_date_picker_proc onCancel;
	xui_date_picker_proc onClear;
	void* pChangingUser;
	void* pChangeUser;
	void* pCommitUser;
	void* pCancelUser;
	void* pClearUser;
	int iMode;
	int bNullable;
	int bHasValue;
	int bHasMin;
	int bHasMax;
	int bShowSecond;
	int iFirstDayOfWeek;
	int iPopupPlacement;
	int bPopupSizeCustom;
	int bFormatCustom;
	int bClosingExplicit;
	int iCloseReason;
	int iHoverPart;
	int iActivePart;
	int iHoverPanel;
	int iHoverIndex;
	int iActivePanel;
	int iActiveTimePanel;
	int iActiveTimeField;
	int iOpenComboKind;
	int iOpenComboPanel;
	int iEditKind;
	int iEditPanel;
	int iEditField;
	int iEditSelectAll;
	int iEditError;
	int iChangingCount;
	int iChangeCount;
	int iCommitCount;
	int iCancelCount;
	int iClearCount;
	xtime tValue;
	xtime tStart;
	xtime tEnd;
	xtime tDraftValue;
	xtime tDraftStart;
	xtime tDraftEnd;
	xtime tViewMonth[XUI_DATE_PICKER_PANEL_CAPACITY];
	xtime tMin;
	xtime tMax;
	xtime tDefaultRangeSpan;
	xui_rect_t tButtonRect;
	xui_rect_t tTextRect;
	xui_rect_t tCalendarRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tTimePanelRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tHeaderRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tPrevRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tNextRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tMonthLabelRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tYearRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t tMonthRect[XUI_DATE_PICKER_PANEL_CAPACITY];
	xui_rect_t arrDayRect[XUI_DATE_PICKER_PANEL_CAPACITY][XUI_DATE_PICKER_CELL_COUNT];
	xtime arrDayValue[XUI_DATE_PICKER_PANEL_CAPACITY][XUI_DATE_PICKER_CELL_COUNT];
	xui_rect_t arrTimeRect[XUI_DATE_PICKER_PANEL_CAPACITY][3];
	xui_rect_t arrFooterRect[4];
	char sEdit[16];
	int iEditLen;
	char sText[96];
	char sFormat[64];
	char sRangeSeparator[24];
	float fPopupWidth;
	float fPopupHeight;
	float fBorderWidth;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iOpenBackgroundColor;
	uint32_t iDisabledBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iArrowColor;
	uint32_t iDisabledArrowColor;
	uint32_t iPopupPanelColor;
	uint32_t iPopupBorderColor;
	uint32_t iPopupShadowColor;
	uint32_t iPopupTextColor;
	uint32_t iPopupMutedTextColor;
	uint32_t iAccentColor;
	uint32_t iFieldColor;
	uint32_t iFieldBorderColor;
	uint32_t iSelectedTextColor;
	uint32_t iDisabledDayColor;
	uint32_t iSeparatorColor;
} xui_date_picker_data_t;

static xui_date_picker_data_t* __xuiDatePickerGetData(xui_widget pWidget);
static int __xuiDatePickerApplyPopupStyle(xui_widget pWidget, xui_date_picker_data_t* pData);
static void __xuiDatePickerCancelEdit(xui_date_picker_data_t* pData);

static uint32_t __xuiDatePickerAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static float __xuiDatePickerMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiDatePickerClampInt(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int64 __xuiDatePickerClampYear(int64 iYear)
{
	if ( iYear < XUI_DATE_PICKER_YEAR_MIN ) return XUI_DATE_PICKER_YEAR_MIN;
	if ( iYear > XUI_DATE_PICKER_YEAR_MAX ) return XUI_DATE_PICKER_YEAR_MAX;
	return iYear;
}

static int __xuiDatePickerParseYearText(const char* sText, int64* pYear)
{
	int64 iValue;
	int i;

	if ( sText == NULL || sText[0] == '\0' ) return 0;
	iValue = 0;
	for ( i = 0; sText[i] != '\0'; i++ ) {
		if ( sText[i] < '0' || sText[i] > '9' ) return 0;
		iValue = iValue * 10 + (int64)(sText[i] - '0');
		if ( iValue > XUI_DATE_PICKER_YEAR_MAX ) return 0;
	}
	if ( iValue < XUI_DATE_PICKER_YEAR_MIN ) return 0;
	if ( pYear != NULL ) *pYear = iValue;
	return 1;
}

static int __xuiDatePickerTimeFieldMax(int iField)
{
	return (iField == 0) ? 23 : 59;
}

static void __xuiDatePickerClampTimeEditText(xui_date_picker_data_t* pData)
{
	int iValue;
	int iClamped;
	int iMax;

	if ( (pData == NULL) || (pData->iEditKind != XUI_DATE_PICKER_EDIT_TIME) || (pData->iEditLen <= 0) ) {
		return;
	}
	iMax = __xuiDatePickerTimeFieldMax(pData->iEditField);
	iValue = atoi(pData->sEdit);
	iClamped = __xuiDatePickerClampInt(iValue, 0, iMax);
	if ( iClamped != iValue ) {
		snprintf(pData->sEdit, sizeof(pData->sEdit), "%02d", iClamped);
		pData->iEditLen = (int)strlen(pData->sEdit);
	}
}

static int __xuiDatePickerRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static xui_rect_t __xuiDatePickerInsetRect(xui_rect_t tRect, float fInset)
{
	tRect.fX += fInset;
	tRect.fY += fInset;
	tRect.fW -= fInset * 2.0f;
	tRect.fH -= fInset * 2.0f;
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	return xuiInternalSnapRect(tRect);
}

static int __xuiDatePickerEditing(const xui_date_picker_data_t* pData)
{
	return (pData != NULL) && (pData->iEditKind != XUI_DATE_PICKER_EDIT_NONE);
}

static int __xuiDatePickerModeValid(int iMode)
{
	return (iMode >= XUI_DATE_PICKER_MODE_DATE) && (iMode <= XUI_DATE_PICKER_MODE_DATETIME_RANGE);
}

static int __xuiDatePickerPlacementValid(int iPlacement)
{
	return (iPlacement == XUI_DATE_PICKER_POPUP_AUTO) || (iPlacement == XUI_DATE_PICKER_POPUP_BOTTOM) || (iPlacement == XUI_DATE_PICKER_POPUP_TOP);
}

static int __xuiDatePickerIsRange(int iMode)
{
	return (iMode == XUI_DATE_PICKER_MODE_DATE_RANGE) ||
	       (iMode == XUI_DATE_PICKER_MODE_TIME_RANGE) ||
	       (iMode == XUI_DATE_PICKER_MODE_DATETIME_RANGE);
}

static int __xuiDatePickerHasDate(int iMode)
{
	return (iMode == XUI_DATE_PICKER_MODE_DATE) ||
	       (iMode == XUI_DATE_PICKER_MODE_DATETIME) ||
	       (iMode == XUI_DATE_PICKER_MODE_DATE_RANGE) ||
	       (iMode == XUI_DATE_PICKER_MODE_DATETIME_RANGE);
}

static int __xuiDatePickerHasTime(int iMode)
{
	return (iMode == XUI_DATE_PICKER_MODE_TIME) ||
	       (iMode == XUI_DATE_PICKER_MODE_DATETIME) ||
	       (iMode == XUI_DATE_PICKER_MODE_TIME_RANGE) ||
	       (iMode == XUI_DATE_PICKER_MODE_DATETIME_RANGE);
}

static int __xuiDatePickerPanelCount(int iMode)
{
	return __xuiDatePickerIsRange(iMode) ? 2 : 1;
}

static void __xuiDatePickerCopyText(char* sDst, int iDstSize, const char* sSrc)
{
	if ( (sDst == NULL) || (iDstSize <= 0) ) return;
	if ( sSrc == NULL ) sSrc = "";
	snprintf(sDst, (size_t)iDstSize, "%s", sSrc);
	sDst[iDstSize - 1] = '\0';
}

static const char* __xuiDatePickerDefaultFormat(int iMode, int bShowSecond)
{
	switch ( iMode ) {
	case XUI_DATE_PICKER_MODE_TIME:
	case XUI_DATE_PICKER_MODE_TIME_RANGE:
		return bShowSecond ? "hh:nn:ss" : "hh:nn";
	case XUI_DATE_PICKER_MODE_DATETIME:
	case XUI_DATE_PICKER_MODE_DATETIME_RANGE:
		return bShowSecond ? "yyyy-mm-dd hh:nn:ss" : "yyyy-mm-dd hh:nn";
	case XUI_DATE_PICKER_MODE_DATE:
	case XUI_DATE_PICKER_MODE_DATE_RANGE:
	default:
		return "yyyy-mm-dd";
	}
}

static void __xuiDatePickerSyncDefaultFormat(xui_date_picker_data_t* pData)
{
	if ( (pData != NULL) && !pData->bFormatCustom ) {
		__xuiDatePickerCopyText(pData->sFormat, (int)sizeof(pData->sFormat), __xuiDatePickerDefaultFormat(pData->iMode, pData->bShowSecond));
	}
}

static xtime __xuiDatePickerMakeTime(int iHour, int iMinute, int iSecond, int bShowSecond)
{
	iHour = __xuiDatePickerClampInt(iHour, 0, 23);
	iMinute = __xuiDatePickerClampInt(iMinute, 0, 59);
	iSecond = bShowSecond ? __xuiDatePickerClampInt(iSecond, 0, 59) : 0;
	return (xtime)iHour * XRT_TIME_HOUR + (xtime)iMinute * XRT_TIME_MINUTE + (xtime)iSecond;
}

static xtime __xuiDatePickerNormalizeTimePart(const xui_date_picker_data_t* pData, xtime tValue)
{
	int iHour;
	int iMinute;
	int iSecond;

	xrtDecodeSerial(tValue, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
	return __xuiDatePickerMakeTime(iHour, iMinute, iSecond, (pData != NULL) ? pData->bShowSecond : 0);
}

static xtime __xuiDatePickerNormalizeValue(const xui_date_picker_data_t* pData, xtime tValue)
{
	if ( pData == NULL ) return tValue;
	if ( (pData->iMode == XUI_DATE_PICKER_MODE_DATE) || (pData->iMode == XUI_DATE_PICKER_MODE_DATE_RANGE) ) {
		return xrtDatePart(tValue);
	}
	if ( (pData->iMode == XUI_DATE_PICKER_MODE_TIME) || (pData->iMode == XUI_DATE_PICKER_MODE_TIME_RANGE) ) {
		return __xuiDatePickerNormalizeTimePart(pData, tValue);
	}
	return xrtDatePart(tValue) + __xuiDatePickerNormalizeTimePart(pData, tValue);
}

static xtime __xuiDatePickerLimitValue(const xui_date_picker_data_t* pData, xtime tValue)
{
	if ( (pData != NULL) && ((pData->iMode == XUI_DATE_PICKER_MODE_TIME) || (pData->iMode == XUI_DATE_PICKER_MODE_TIME_RANGE)) ) {
		return xrtTimePart(tValue);
	}
	return tValue;
}

static xtime __xuiDatePickerClampValue(const xui_date_picker_data_t* pData, xtime tValue)
{
	xtime tCompare;

	if ( pData == NULL ) return tValue;
	tValue = __xuiDatePickerNormalizeValue(pData, tValue);
	tCompare = __xuiDatePickerLimitValue(pData, tValue);
	if ( pData->bHasMin && (tCompare < __xuiDatePickerLimitValue(pData, pData->tMin)) ) {
		tValue = ((pData->iMode == XUI_DATE_PICKER_MODE_TIME) || (pData->iMode == XUI_DATE_PICKER_MODE_TIME_RANGE)) ?
			__xuiDatePickerNormalizeValue(pData, __xuiDatePickerLimitValue(pData, pData->tMin)) :
			__xuiDatePickerNormalizeValue(pData, pData->tMin);
		tCompare = __xuiDatePickerLimitValue(pData, tValue);
	}
	if ( pData->bHasMax && (tCompare > __xuiDatePickerLimitValue(pData, pData->tMax)) ) {
		tValue = ((pData->iMode == XUI_DATE_PICKER_MODE_TIME) || (pData->iMode == XUI_DATE_PICKER_MODE_TIME_RANGE)) ?
			__xuiDatePickerNormalizeValue(pData, __xuiDatePickerLimitValue(pData, pData->tMax)) :
			__xuiDatePickerNormalizeValue(pData, pData->tMax);
	}
	return __xuiDatePickerNormalizeValue(pData, tValue);
}

static int __xuiDatePickerValueEnabled(const xui_date_picker_data_t* pData, xtime tValue)
{
	xtime tCompare;

	if ( pData == NULL ) return 0;
	tCompare = __xuiDatePickerLimitValue(pData, __xuiDatePickerNormalizeValue(pData, tValue));
	if ( pData->bHasMin && (tCompare < __xuiDatePickerLimitValue(pData, pData->tMin)) ) return 0;
	if ( pData->bHasMax && (tCompare > __xuiDatePickerLimitValue(pData, pData->tMax)) ) return 0;
	return 1;
}

static xtime __xuiDatePickerDefaultSpan(const xui_date_picker_data_t* pData)
{
	if ( (pData != NULL) && (pData->tDefaultRangeSpan > 0) ) return pData->tDefaultRangeSpan;
	if ( (pData != NULL) && (pData->iMode == XUI_DATE_PICKER_MODE_DATE_RANGE) ) return XRT_TIME_DAY;
	return XRT_TIME_HOUR * 4;
}

static xtime __xuiDatePickerAddSpan(const xui_date_picker_data_t* pData, xtime tValue, xtime tSpan)
{
	xtime tEnd;

	if ( pData == NULL ) return tValue + tSpan;
	if ( pData->iMode == XUI_DATE_PICKER_MODE_DATE_RANGE ) {
		return xrtDatePart(tValue + ((tSpan > 0) ? tSpan : XRT_TIME_DAY));
	}
	if ( pData->iMode == XUI_DATE_PICKER_MODE_TIME_RANGE ) {
		tEnd = xrtTimePart(tValue) + ((tSpan > 0) ? tSpan : (XRT_TIME_HOUR * 4));
		if ( tEnd >= XRT_TIME_DAY ) tEnd = XRT_TIME_DAY - 1;
		return __xuiDatePickerNormalizeValue(pData, tEnd);
	}
	return __xuiDatePickerNormalizeValue(pData, tValue + ((tSpan > 0) ? tSpan : (XRT_TIME_HOUR * 4)));
}

static void __xuiDatePickerDefaultValues(xui_date_picker_data_t* pData, xtime* pStart, xtime* pEnd)
{
	xtime tNow;
	xtime tStart;
	xtime tEnd;

	tNow = xrtNow();
	if ( pData == NULL ) {
		if ( pStart != NULL ) *pStart = tNow;
		if ( pEnd != NULL ) *pEnd = tNow;
		return;
	}
	switch ( pData->iMode ) {
	case XUI_DATE_PICKER_MODE_DATE:
		tStart = xrtDatePart(tNow);
		tEnd = tStart;
		break;
	case XUI_DATE_PICKER_MODE_TIME:
		tStart = __xuiDatePickerNormalizeValue(pData, xrtTimePart(tNow));
		tEnd = tStart;
		break;
	case XUI_DATE_PICKER_MODE_DATE_RANGE:
		tStart = xrtDatePart(tNow);
		tEnd = __xuiDatePickerAddSpan(pData, tStart, __xuiDatePickerDefaultSpan(pData));
		break;
	case XUI_DATE_PICKER_MODE_TIME_RANGE:
		tStart = __xuiDatePickerNormalizeValue(pData, xrtTimePart(tNow));
		tEnd = __xuiDatePickerAddSpan(pData, tStart, __xuiDatePickerDefaultSpan(pData));
		break;
	case XUI_DATE_PICKER_MODE_DATETIME_RANGE:
		tStart = __xuiDatePickerNormalizeValue(pData, tNow);
		tEnd = __xuiDatePickerAddSpan(pData, tStart, __xuiDatePickerDefaultSpan(pData));
		break;
	case XUI_DATE_PICKER_MODE_DATETIME:
	default:
		tStart = __xuiDatePickerNormalizeValue(pData, tNow);
		tEnd = tStart;
		break;
	}
	tStart = __xuiDatePickerClampValue(pData, tStart);
	tEnd = __xuiDatePickerClampValue(pData, tEnd);
	if ( __xuiDatePickerIsRange(pData->iMode) && (tEnd < tStart) ) tEnd = tStart;
	if ( pStart != NULL ) *pStart = tStart;
	if ( pEnd != NULL ) *pEnd = tEnd;
}

static void __xuiDatePickerNormalizeRange(xui_date_picker_data_t* pData, xtime* pStart, xtime* pEnd)
{
	xtime tSwap;

	if ( (pData == NULL) || (pStart == NULL) || (pEnd == NULL) ) return;
	*pStart = __xuiDatePickerClampValue(pData, *pStart);
	*pEnd = __xuiDatePickerClampValue(pData, *pEnd);
	if ( *pEnd < *pStart ) {
		tSwap = *pStart;
		*pStart = *pEnd;
		*pEnd = tSwap;
	}
}

static void __xuiDatePickerEnsureValue(xui_date_picker_data_t* pData)
{
	xtime tStart;
	xtime tEnd;

	if ( pData == NULL ) return;
	if ( pData->bNullable && !pData->bHasValue ) return;
	if ( pData->bHasValue ) {
		if ( __xuiDatePickerIsRange(pData->iMode) ) {
			__xuiDatePickerNormalizeRange(pData, &pData->tStart, &pData->tEnd);
		} else {
			pData->tValue = __xuiDatePickerClampValue(pData, pData->tValue);
		}
		return;
	}
	__xuiDatePickerDefaultValues(pData, &tStart, &tEnd);
	pData->tValue = tStart;
	pData->tStart = tStart;
	pData->tEnd = tEnd;
	pData->bHasValue = 1;
}

static void __xuiDatePickerInitViewMonth(xui_date_picker_data_t* pData)
{
	int64 iYear;
	int iMonth;

	if ( pData == NULL ) return;
	__xuiDatePickerEnsureValue(pData);
	if ( !__xuiDatePickerHasDate(pData->iMode) ) {
		pData->tViewMonth[0] = xrtFirstDayOfMonth(xrtNow());
		pData->tViewMonth[1] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, pData->tViewMonth[0]);
		return;
	}
	xrtDecodeSerial(__xuiDatePickerIsRange(pData->iMode) ? pData->tStart : pData->tValue, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	pData->tViewMonth[0] = xrtDateSerial(iYear, iMonth, 1);
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		xrtDecodeSerial(pData->tEnd, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
		pData->tViewMonth[1] = xrtDateSerial(iYear, iMonth, 1);
		if ( pData->tViewMonth[1] <= pData->tViewMonth[0] ) {
			pData->tViewMonth[1] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, pData->tViewMonth[0]);
		}
	} else {
		pData->tViewMonth[1] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, pData->tViewMonth[0]);
	}
}

static void __xuiDatePickerAppend(char* sOut, int* pLen, int iCap, const char* sText)
{
	int iLen;

	if ( (sOut == NULL) || (pLen == NULL) || (sText == NULL) || (iCap <= 0) ) return;
	iLen = (int)strlen(sText);
	if ( *pLen + iLen >= iCap ) iLen = iCap - *pLen - 1;
	if ( iLen <= 0 ) return;
	memcpy(sOut + *pLen, sText, (size_t)iLen);
	*pLen += iLen;
	sOut[*pLen] = '\0';
}

static void __xuiDatePickerAppendInt(char* sOut, int* pLen, int iCap, int64 iValue, int iWidth)
{
	char sText[32];

	if ( iWidth == 4 ) {
		snprintf(sText, sizeof(sText), "%04lld", (long long)iValue);
	} else if ( iWidth == 2 ) {
		snprintf(sText, sizeof(sText), "%02lld", (long long)iValue);
	} else {
		snprintf(sText, sizeof(sText), "%lld", (long long)iValue);
	}
	__xuiDatePickerAppend(sOut, pLen, iCap, sText);
}

static void __xuiDatePickerFormatOne(xui_date_picker_data_t* pData, xtime tValue, char* sOut, int iCap)
{
	const char* sFmt;
	int64 iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	int iLen;
	int i;

	if ( (sOut == NULL) || (iCap <= 0) ) return;
	sOut[0] = '\0';
	if ( pData == NULL ) return;
	sFmt = pData->sFormat;
	xrtDecodeSerial(tValue, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
	iLen = 0;
	for ( i = 0; sFmt[i] != '\0' && iLen < iCap - 1; ) {
		if ( strncmp(sFmt + i, "yyyy", 4) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iYear, 4);
			i += 4;
		} else if ( strncmp(sFmt + i, "yy", 2) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iYear % 100, 2);
			i += 2;
		} else if ( strncmp(sFmt + i, "mm", 2) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iMonth, 2);
			i += 2;
		} else if ( strncmp(sFmt + i, "dd", 2) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iDay, 2);
			i += 2;
		} else if ( strncmp(sFmt + i, "hh", 2) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iHour, 2);
			i += 2;
		} else if ( strncmp(sFmt + i, "nn", 2) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iMinute, 2);
			i += 2;
		} else if ( strncmp(sFmt + i, "ss", 2) == 0 ) {
			__xuiDatePickerAppendInt(sOut, &iLen, iCap, iSecond, 2);
			i += 2;
		} else {
			sOut[iLen++] = sFmt[i++];
			sOut[iLen] = '\0';
		}
	}
}

static void __xuiDatePickerSyncText(xui_date_picker_data_t* pData)
{
	char sA[48];
	char sB[48];

	if ( pData == NULL ) return;
	if ( !pData->bHasValue && pData->bNullable ) {
		pData->sText[0] = '\0';
		return;
	}
	__xuiDatePickerEnsureValue(pData);
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		__xuiDatePickerFormatOne(pData, pData->tStart, sA, (int)sizeof(sA));
		__xuiDatePickerFormatOne(pData, pData->tEnd, sB, (int)sizeof(sB));
		snprintf(pData->sText, sizeof(pData->sText), "%s%s%s", sA, pData->sRangeSeparator, sB);
	} else {
		__xuiDatePickerFormatOne(pData, pData->tValue, pData->sText, (int)sizeof(pData->sText));
	}
	pData->sText[sizeof(pData->sText) - 1] = '\0';
}

static void __xuiDatePickerPreferredPopupSize(int iMode, float* pW, float* pH)
{
	float fW;
	float fH;

	switch ( iMode ) {
	case XUI_DATE_PICKER_MODE_TIME:
		fW = 268.0f; fH = 168.0f;
		break;
	case XUI_DATE_PICKER_MODE_DATETIME:
		fW = 548.0f; fH = 334.0f;
		break;
	case XUI_DATE_PICKER_MODE_DATE_RANGE:
		fW = 628.0f; fH = 326.0f;
		break;
	case XUI_DATE_PICKER_MODE_TIME_RANGE:
		fW = 444.0f; fH = 204.0f;
		break;
	case XUI_DATE_PICKER_MODE_DATETIME_RANGE:
		fW = 672.0f; fH = 408.0f;
		break;
	case XUI_DATE_PICKER_MODE_DATE:
	default:
		fW = 318.0f; fH = 326.0f;
		break;
	}
	if ( pW != NULL ) *pW = fW;
	if ( pH != NULL ) *pH = fH;
}

static void __xuiDatePickerUpdatePopupSizeForMode(xui_date_picker_data_t* pData)
{
	if ( (pData != NULL) && !pData->bPopupSizeCustom ) {
		__xuiDatePickerPreferredPopupSize(pData->iMode, &pData->fPopupWidth, &pData->fPopupHeight);
	}
}

static int __xuiDatePickerDescValid(const xui_date_picker_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( !__xuiDatePickerModeValid(pDesc->iMode) ) return 0;
	if ( (pDesc->iPopupPlacement != 0) && !__xuiDatePickerPlacementValid(pDesc->iPopupPlacement) ) return 0;
	if ( (pDesc->iFirstDayOfWeek < 0) || (pDesc->iFirstDayOfWeek > 6) ) return 0;
	return 1;
}

static void __xuiDatePickerDefaults(xui_date_picker_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iMode = XUI_DATE_PICKER_MODE_DATE;
	pData->iPopupPlacement = XUI_DATE_PICKER_POPUP_AUTO;
	pData->iFirstDayOfWeek = 0;
	pData->iHoverPart = XUI_DATE_PICKER_PART_NONE;
	pData->iActivePart = XUI_DATE_PICKER_PART_NONE;
	pData->iHoverPanel = -1;
	pData->iHoverIndex = -1;
	pData->iActivePanel = 0;
	pData->iActiveTimePanel = 0;
	pData->iActiveTimeField = -1;
	pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
	pData->iOpenComboPanel = -1;
	pData->iEditKind = XUI_DATE_PICKER_EDIT_NONE;
	pData->iEditPanel = -1;
	pData->iEditField = -1;
	pData->tDefaultRangeSpan = 0;
	pData->fBorderWidth = 1.0f;
	pData->iTextColor = XUI_COLOR_RGBA(34, 48, 64, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(128, 142, 156, 255);
	pData->iBackgroundColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iHoverBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iOpenBackgroundColor = XUI_COLOR_RGBA(244, 250, 255, 255);
	pData->iDisabledBackgroundColor = XUI_COLOR_RGBA(236, 241, 247, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(168, 185, 205, 255);
	pData->iHoverBorderColor = XUI_COLOR_RGBA(92, 158, 220, 255);
	pData->iFocusBorderColor = XUI_COLOR_RGBA(46, 124, 214, 255);
	pData->iArrowColor = XUI_COLOR_RGBA(60, 82, 104, 255);
	pData->iDisabledArrowColor = XUI_COLOR_RGBA(150, 162, 174, 255);
	pData->iPopupPanelColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pData->iPopupBorderColor = XUI_COLOR_RGBA(135, 170, 205, 255);
	pData->iPopupShadowColor = XUI_COLOR_RGBA(20, 44, 72, 42);
	pData->iPopupTextColor = XUI_COLOR_RGBA(28, 42, 58, 255);
	pData->iPopupMutedTextColor = XUI_COLOR_RGBA(104, 122, 142, 255);
	pData->iAccentColor = XUI_COLOR_RGBA(46, 124, 214, 255);
	pData->iFieldColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iFieldBorderColor = XUI_COLOR_RGBA(198, 214, 230, 255);
	pData->iSelectedTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iDisabledDayColor = XUI_COLOR_RGBA(168, 178, 190, 255);
	pData->iSeparatorColor = XUI_COLOR_RGBA(216, 228, 238, 255);
	__xuiDatePickerCopyText(pData->sRangeSeparator, (int)sizeof(pData->sRangeSeparator), " - ");
	__xuiDatePickerSyncDefaultFormat(pData);
	__xuiDatePickerUpdatePopupSizeForMode(pData);
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerInitViewMonth(pData);
	__xuiDatePickerSyncText(pData);
}

static void __xuiDatePickerApplyDesc(xui_date_picker_data_t* pData, const xui_date_picker_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) return;
	pData->iMode = __xuiDatePickerModeValid(pDesc->iMode) ? pDesc->iMode : XUI_DATE_PICKER_MODE_DATE;
	pData->bNullable = pDesc->bNullable ? 1 : 0;
	pData->bShowSecond = pDesc->bShowSecond ? 1 : 0;
	pData->iFirstDayOfWeek = __xuiDatePickerClampInt(pDesc->iFirstDayOfWeek, 0, 6);
	if ( pDesc->pFont != NULL ) pData->pFont = pDesc->pFont;
	if ( pDesc->sFormat != NULL ) {
		__xuiDatePickerCopyText(pData->sFormat, (int)sizeof(pData->sFormat), pDesc->sFormat);
		pData->bFormatCustom = 1;
	} else {
		__xuiDatePickerSyncDefaultFormat(pData);
	}
	if ( pDesc->sRangeSeparator != NULL ) __xuiDatePickerCopyText(pData->sRangeSeparator, (int)sizeof(pData->sRangeSeparator), pDesc->sRangeSeparator);
	if ( pDesc->tDefaultRangeSpan > 0 ) pData->tDefaultRangeSpan = pDesc->tDefaultRangeSpan;
	pData->bHasMin = pDesc->bHasMin ? 1 : 0;
	pData->bHasMax = pDesc->bHasMax ? 1 : 0;
	pData->tMin = pDesc->tMin;
	pData->tMax = pDesc->tMax;
	if ( pDesc->bHasRangeValue ) {
		pData->tStart = pDesc->tStart;
		pData->tEnd = pDesc->tEnd;
		pData->bHasValue = 1;
	} else if ( pDesc->bHasValue ) {
		pData->tValue = pDesc->tValue;
		pData->tStart = pDesc->tValue;
		pData->tEnd = pDesc->tValue;
		pData->bHasValue = 1;
	}
	if ( (pDesc->fPopupWidth > 0.0f) && (pDesc->fPopupHeight > 0.0f) ) {
		pData->fPopupWidth = pDesc->fPopupWidth;
		pData->fPopupHeight = pDesc->fPopupHeight;
		pData->bPopupSizeCustom = 1;
	} else {
		__xuiDatePickerUpdatePopupSizeForMode(pData);
	}
	if ( __xuiDatePickerPlacementValid(pDesc->iPopupPlacement) ) pData->iPopupPlacement = pDesc->iPopupPlacement;
	if ( __xuiDatePickerAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiDatePickerAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiDatePickerAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiDatePickerAlpha(pDesc->iHoverBackgroundColor) != 0 ) pData->iHoverBackgroundColor = pDesc->iHoverBackgroundColor;
	if ( __xuiDatePickerAlpha(pDesc->iOpenBackgroundColor) != 0 ) pData->iOpenBackgroundColor = pDesc->iOpenBackgroundColor;
	if ( __xuiDatePickerAlpha(pDesc->iDisabledBackgroundColor) != 0 ) pData->iDisabledBackgroundColor = pDesc->iDisabledBackgroundColor;
	if ( __xuiDatePickerAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiDatePickerAlpha(pDesc->iHoverBorderColor) != 0 ) pData->iHoverBorderColor = pDesc->iHoverBorderColor;
	if ( __xuiDatePickerAlpha(pDesc->iFocusBorderColor) != 0 ) pData->iFocusBorderColor = pDesc->iFocusBorderColor;
	if ( __xuiDatePickerAlpha(pDesc->iArrowColor) != 0 ) pData->iArrowColor = pDesc->iArrowColor;
	if ( __xuiDatePickerAlpha(pDesc->iDisabledArrowColor) != 0 ) pData->iDisabledArrowColor = pDesc->iDisabledArrowColor;
	if ( __xuiDatePickerAlpha(pDesc->iPopupPanelColor) != 0 ) pData->iPopupPanelColor = pDesc->iPopupPanelColor;
	if ( __xuiDatePickerAlpha(pDesc->iPopupBorderColor) != 0 ) pData->iPopupBorderColor = pDesc->iPopupBorderColor;
	if ( __xuiDatePickerAlpha(pDesc->iPopupShadowColor) != 0 ) pData->iPopupShadowColor = pDesc->iPopupShadowColor;
	if ( __xuiDatePickerAlpha(pDesc->iPopupTextColor) != 0 ) pData->iPopupTextColor = pDesc->iPopupTextColor;
	if ( __xuiDatePickerAlpha(pDesc->iPopupMutedTextColor) != 0 ) pData->iPopupMutedTextColor = pDesc->iPopupMutedTextColor;
	if ( __xuiDatePickerAlpha(pDesc->iAccentColor) != 0 ) pData->iAccentColor = pDesc->iAccentColor;
	if ( __xuiDatePickerAlpha(pDesc->iFieldColor) != 0 ) pData->iFieldColor = pDesc->iFieldColor;
	if ( __xuiDatePickerAlpha(pDesc->iFieldBorderColor) != 0 ) pData->iFieldBorderColor = pDesc->iFieldBorderColor;
	if ( __xuiDatePickerAlpha(pDesc->iSelectedTextColor) != 0 ) pData->iSelectedTextColor = pDesc->iSelectedTextColor;
	if ( __xuiDatePickerAlpha(pDesc->iDisabledDayColor) != 0 ) pData->iDisabledDayColor = pDesc->iDisabledDayColor;
	if ( __xuiDatePickerAlpha(pDesc->iSeparatorColor) != 0 ) pData->iSeparatorColor = pDesc->iSeparatorColor;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerInitViewMonth(pData);
	__xuiDatePickerSyncText(pData);
}

static int __xuiDatePickerStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pColor == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiDatePickerStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pValue == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiDatePickerStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	if ( pWidget == NULL ) return pBaseFont;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) && (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) return pFont;
	}
	if ( pBaseFont != NULL ) return pBaseFont;
	return xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
}

static void __xuiDatePickerResolve(xui_widget pWidget, xui_date_picker_data_t* pData, xui_date_picker_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = __xuiDatePickerStyleFont(pWidget, pResolved->pFont);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.text.color", &pResolved->iTextColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.background.color", &pResolved->iBackgroundColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.background.open_color", &pResolved->iOpenBackgroundColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.background.disabled_color", &pResolved->iDisabledBackgroundColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.border.color", &pResolved->iBorderColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.border.focus_color", &pResolved->iFocusBorderColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.arrow.color", &pResolved->iArrowColor);
	(void)__xuiDatePickerStyleColor(pWidget, "datepicker.arrow.disabled_color", &pResolved->iDisabledArrowColor);
	(void)__xuiDatePickerStyleFloat(pWidget, "datepicker.border.width", &pResolved->fBorderWidth);
}

static int __xuiDatePickerInvalidateAll(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	if ( pData != NULL && pData->pPanel != NULL ) {
		(void)xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiDatePickerNotifyDraft(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iChangingCount++;
	if ( pData->onChanging != NULL ) {
		if ( __xuiDatePickerIsRange(pData->iMode) ) {
			pData->onChanging(pWidget, pData->tDraftStart, pData->tDraftEnd, pData->iMode, pData->pChangingUser);
		} else {
			pData->onChanging(pWidget, pData->tDraftValue, 0, pData->iMode, pData->pChangingUser);
		}
	}
}

static void __xuiDatePickerNotifyValue(xui_widget pWidget, xui_date_picker_data_t* pData, xui_date_picker_proc proc, void* pUser)
{
	if ( (pData == NULL) || (proc == NULL) ) return;
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		proc(pWidget, pData->tStart, pData->tEnd, pData->iMode, pUser);
	} else {
		proc(pWidget, pData->tValue, 0, pData->iMode, pUser);
	}
}

static uint32_t __xuiDatePickerState(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	xui_context pContext;
	xui_widget pFocus;
	uint32_t iState;

	if ( pWidget == NULL ) return 0u;
	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (pData != NULL) && (pData->pPopup != NULL) && xuiPopupIsOpen(pData->pPopup) ) {
		iState |= XUI_DATE_PICKER_STATE_OPEN;
	} else {
		iState &= ~XUI_DATE_PICKER_STATE_OPEN;
	}
	if ( pData != NULL ) {
		pContext = xuiWidgetGetContext(pWidget);
		pFocus = xuiGetFocusWidget(pContext);
		if ( (pFocus == pWidget) || (pFocus == pData->pPanel) || (pFocus == pData->pPopup) ) {
			iState |= XUI_WIDGET_STATE_FOCUS;
		}
	}
	return iState;
}

static int __xuiDatePickerSyncState(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	return xuiWidgetSetStateId(pWidget, __xuiDatePickerState(pWidget, pData));
}

static void __xuiDatePickerUpdateOwnerRects(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	xui_rect_t tRect;
	float fButtonW;

	if ( (pWidget == NULL) || (pData == NULL) ) return;
	tRect = xuiInternalSnapRect(xuiWidgetGetContentRect(pWidget));
	fButtonW = tRect.fH;
	if ( fButtonW < 24.0f ) fButtonW = 24.0f;
	if ( fButtonW > 34.0f ) fButtonW = 34.0f;
	pData->tButtonRect = xuiInternalSnapRect((xui_rect_t){tRect.fX + tRect.fW - fButtonW, tRect.fY, fButtonW, tRect.fH});
	pData->tTextRect = xuiInternalSnapRect((xui_rect_t){tRect.fX + 8.0f, tRect.fY, __xuiDatePickerMax(1.0f, tRect.fW - fButtonW - 14.0f), tRect.fH});
}

static void __xuiDatePickerLayoutCalendar(xui_date_picker_data_t* pData, int iPanel, xui_rect_t tPanel)
{
	static const float fHeaderH = 32.0f;
	static const float fWeekH = 22.0f;
	float fCellW;
	float fCellH;
	float fGridY;
	float fButtonW;
	float fLabelW;
	float fYearW;
	float fMonthW;
	float fGap;
	float fComboW;
	float fComboX;
	xtime tFirst;
	xtime tCell;
	int iWeekday;
	int iOffset;
	int i;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return;
	tPanel = xuiInternalSnapRect(tPanel);
	pData->tCalendarRect[iPanel] = tPanel;
	pData->tHeaderRect[iPanel] = xuiInternalSnapRect((xui_rect_t){tPanel.fX, tPanel.fY, tPanel.fW, fHeaderH});
	fButtonW = 28.0f;
	pData->tPrevRect[iPanel] = xuiInternalSnapRect((xui_rect_t){tPanel.fX + 2.0f, tPanel.fY + 2.0f, fButtonW, fHeaderH - 4.0f});
	pData->tNextRect[iPanel] = xuiInternalSnapRect((xui_rect_t){tPanel.fX + tPanel.fW - fButtonW - 2.0f, tPanel.fY + 2.0f, fButtonW, fHeaderH - 4.0f});
	fLabelW = __xuiDatePickerMax(80.0f, tPanel.fW - fButtonW * 2.0f - 18.0f);
	if ( fLabelW > 150.0f ) fLabelW = 150.0f;
	pData->tMonthLabelRect[iPanel] = xuiInternalSnapRect((xui_rect_t){tPanel.fX + (tPanel.fW - fLabelW) * 0.5f, tPanel.fY + 4.0f, fLabelW, fHeaderH - 8.0f});
	fYearW = XUI_DATE_PICKER_YEAR_FIELD_W;
	fMonthW = XUI_DATE_PICKER_MONTH_FIELD_W;
	fGap = XUI_DATE_PICKER_HEADER_COMBO_GAP;
	fComboW = fYearW + fMonthW + fGap;
	if ( fComboW > tPanel.fW - fButtonW * 2.0f - 12.0f ) {
		fComboW = tPanel.fW - fButtonW * 2.0f - 12.0f;
		if ( fComboW < 104.0f ) fComboW = 104.0f;
		fYearW = (fComboW - fGap) * 0.64f;
		fMonthW = fComboW - fGap - fYearW;
	}
	fComboX = tPanel.fX + (tPanel.fW - fComboW) * 0.5f;
	pData->tYearRect[iPanel] = xuiInternalSnapRect((xui_rect_t){fComboX, tPanel.fY + 4.0f, fYearW, fHeaderH - 8.0f});
	pData->tMonthRect[iPanel] = xuiInternalSnapRect((xui_rect_t){fComboX + fYearW + fGap, tPanel.fY + 4.0f, fMonthW, fHeaderH - 8.0f});
	fGridY = tPanel.fY + fHeaderH + fWeekH;
	fCellW = tPanel.fW / 7.0f;
	fCellH = (tPanel.fH - fHeaderH - fWeekH) / 6.0f;
	if ( fCellH < 20.0f ) fCellH = 20.0f;
	tFirst = xrtFirstDayOfMonth(pData->tViewMonth[iPanel]);
	xrtDecodeSerial(tFirst, NULL, NULL, NULL, NULL, NULL, NULL, &iWeekday, NULL);
	iOffset = (iWeekday - pData->iFirstDayOfWeek + 7) % 7;
	for ( i = 0; i < XUI_DATE_PICKER_CELL_COUNT; i++ ) {
		pData->arrDayRect[iPanel][i] = xuiInternalSnapRect((xui_rect_t){
			tPanel.fX + (float)(i % 7) * fCellW,
			fGridY + (float)(i / 7) * fCellH,
			fCellW,
			fCellH
		});
		tCell = tFirst + ((xtime)i - (xtime)iOffset) * XRT_TIME_DAY;
		pData->arrDayValue[iPanel][i] = xrtDatePart(tCell);
	}
}

static void __xuiDatePickerLayoutTime(xui_date_picker_data_t* pData, int iPanel, xui_rect_t tPanel)
{
	float fFieldW;
	float fFieldY;
	float fGap;
	float fGroupH;
	float fInnerW;
	float fX;
	int iCount;
	int i;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return;
	tPanel = xuiInternalSnapRect(tPanel);
	pData->tTimePanelRect[iPanel] = tPanel;
	iCount = pData->bShowSecond ? 3 : 2;
	fGap = 8.0f;
	fInnerW = tPanel.fW - XUI_DATE_PICKER_TIME_INSET * 2.0f;
	if ( fInnerW < (float)iCount ) {
		fInnerW = (float)iCount;
	}
	fFieldW = (fInnerW - fGap * (float)(iCount - 1)) / (float)iCount;
	if ( fFieldW < 1.0f ) fFieldW = 1.0f;
	if ( fFieldW > 72.0f ) fFieldW = 72.0f;
	fX = tPanel.fX + XUI_DATE_PICKER_TIME_INSET + (fInnerW - (fFieldW * (float)iCount + fGap * (float)(iCount - 1))) * 0.5f;
	fGroupH = 46.0f;
	fFieldY = tPanel.fY + (tPanel.fH - fGroupH) * 0.5f + 18.0f;
	if ( fFieldY < tPanel.fY + 44.0f ) {
		fFieldY = tPanel.fY + 44.0f;
	}
	if ( fFieldY + 28.0f > tPanel.fY + tPanel.fH - 8.0f ) {
		fFieldY = tPanel.fY + tPanel.fH - 36.0f;
	}
	for ( i = 0; i < 3; i++ ) {
		if ( i < iCount ) {
			pData->arrTimeRect[iPanel][i] = xuiInternalSnapRect((xui_rect_t){fX + (float)i * (fFieldW + fGap), fFieldY, fFieldW, 28.0f});
		} else {
			pData->arrTimeRect[iPanel][i] = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		}
	}
}

static void __xuiDatePickerUpdatePanelRects(xui_date_picker_data_t* pData)
{
	float fW;
	float fH;
	float fPad;
	float fFooterY;
	float fFooterH;
	float fColW;
	float fCalH;
	float fTimeH;
	float fX;
	float fButtonW;
	int i;

	if ( pData == NULL ) return;
	fW = pData->fPopupWidth;
	fH = pData->fPopupHeight;
	fPad = XUI_DATE_PICKER_PAD;
	fFooterH = 28.0f;
	fFooterY = fH - fPad - fFooterH;
	for ( i = 0; i < XUI_DATE_PICKER_PANEL_CAPACITY; i++ ) {
		pData->tCalendarRect[i] = (xui_rect_t){0};
		pData->tTimePanelRect[i] = (xui_rect_t){0};
		pData->tYearRect[i] = (xui_rect_t){0};
		pData->tMonthRect[i] = (xui_rect_t){0};
	}
	if ( __xuiDatePickerHasDate(pData->iMode) && __xuiDatePickerHasTime(pData->iMode) ) {
		if ( __xuiDatePickerIsRange(pData->iMode) ) {
			fColW = (fW - fPad * 3.0f) * 0.5f;
			fCalH = 238.0f;
			fTimeH = __xuiDatePickerMax(XUI_DATE_PICKER_TIME_MIN_HEIGHT, fFooterY - fPad * 2.0f - fCalH);
			__xuiDatePickerLayoutCalendar(pData, 0, (xui_rect_t){fPad, fPad, fColW, fCalH});
			__xuiDatePickerLayoutCalendar(pData, 1, (xui_rect_t){fPad * 2.0f + fColW, fPad, fColW, fCalH});
			__xuiDatePickerLayoutTime(pData, 0, (xui_rect_t){fPad, fPad + fCalH + 6.0f, fColW, fTimeH});
			__xuiDatePickerLayoutTime(pData, 1, (xui_rect_t){fPad * 2.0f + fColW, fPad + fCalH + 6.0f, fColW, fTimeH});
		} else {
			fCalH = fFooterY - fPad * 2.0f;
			fColW = 292.0f;
			if ( fColW > fW * 0.58f ) fColW = fW * 0.58f;
			__xuiDatePickerLayoutCalendar(pData, 0, (xui_rect_t){fPad, fPad, fColW, fCalH});
			__xuiDatePickerLayoutTime(pData, 0, (xui_rect_t){fPad * 2.0f + fColW, fPad + 58.0f, fW - fColW - fPad * 3.0f, 96.0f});
		}
	} else if ( __xuiDatePickerHasDate(pData->iMode) ) {
		fCalH = fFooterY - fPad * 2.0f;
		if ( __xuiDatePickerIsRange(pData->iMode) ) {
			fColW = (fW - fPad * 3.0f) * 0.5f;
			__xuiDatePickerLayoutCalendar(pData, 0, (xui_rect_t){fPad, fPad, fColW, fCalH});
			__xuiDatePickerLayoutCalendar(pData, 1, (xui_rect_t){fPad * 2.0f + fColW, fPad, fColW, fCalH});
		} else {
			__xuiDatePickerLayoutCalendar(pData, 0, (xui_rect_t){fPad, fPad, fW - fPad * 2.0f, fCalH});
		}
	} else {
		if ( __xuiDatePickerIsRange(pData->iMode) ) {
			fColW = (fW - fPad * 3.0f) * 0.5f;
			__xuiDatePickerLayoutTime(pData, 0, (xui_rect_t){fPad, fPad + 12.0f, fColW, fFooterY - fPad * 2.0f});
			__xuiDatePickerLayoutTime(pData, 1, (xui_rect_t){fPad * 2.0f + fColW, fPad + 12.0f, fColW, fFooterY - fPad * 2.0f});
		} else {
			__xuiDatePickerLayoutTime(pData, 0, (xui_rect_t){fPad, fPad + 12.0f, fW - fPad * 2.0f, fFooterY - fPad * 2.0f});
		}
	}
	for ( i = 0; i < 4; i++ ) pData->arrFooterRect[i] = (xui_rect_t){0};
	fButtonW = 66.0f;
	fX = fW - fPad - fButtonW;
	pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_OK] = xuiInternalSnapRect((xui_rect_t){fX, fFooterY, fButtonW, fFooterH});
	fX -= fButtonW + 8.0f;
	pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_CANCEL] = xuiInternalSnapRect((xui_rect_t){fX, fFooterY, fButtonW, fFooterH});
	if ( pData->bNullable ) {
		fX -= fButtonW + 8.0f;
		pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_CLEAR] = xuiInternalSnapRect((xui_rect_t){fX, fFooterY, fButtonW, fFooterH});
	}
	pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_TODAY] = xuiInternalSnapRect((xui_rect_t){fPad, fFooterY, 78.0f, fFooterH});
}

static int __xuiDatePickerDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiDatePickerAlpha(iColor) == 0 ) return XUI_OK;
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiDatePickerDrawStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiDatePickerAlpha(iColor) == 0) ) return XUI_OK;
	fWidth = xuiInternalSnapSize(fWidth);
	tRect = xuiInternalStrokeCenterRectInside(tRect, fWidth, NULL);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return XUI_OK;
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor) : XUI_OK;
}

static int __xuiDatePickerDrawChevron(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bOpen, uint32_t iColor)
{
	float fCx;
	float fY;
	float fLeft;
	float fRight;
	int iRet;

	if ( pProxy->drawLine == NULL ) return XUI_OK;
	fCx = tRect.fX + tRect.fW * 0.5f;
	fY = tRect.fY + tRect.fH * 0.5f;
	fLeft = fCx - 5.0f;
	fRight = fCx + 5.0f;
	if ( bOpen ) {
		iRet = pProxy->drawLine(pProxy, pDraw, fLeft, fY + 3.0f, fCx, fY - 3.0f, 1.6f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return pProxy->drawLine(pProxy, pDraw, fCx, fY - 3.0f, fRight, fY + 3.0f, 1.6f, iColor);
	}
	iRet = pProxy->drawLine(pProxy, pDraw, fLeft, fY - 3.0f, fCx, fY + 3.0f, 1.6f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, fCx, fY + 3.0f, fRight, fY - 3.0f, 1.6f, iColor);
}

static int __xuiDatePickerDrawArrow(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int iDir, uint32_t iColor)
{
	float fCx;
	float fCy;
	int iRet;

	if ( pProxy->drawLine == NULL ) return XUI_OK;
	fCx = tRect.fX + tRect.fW * 0.5f;
	fCy = tRect.fY + tRect.fH * 0.5f;
	if ( iDir < 0 ) {
		iRet = pProxy->drawLine(pProxy, pDraw, fCx + 4.0f, fCy - 6.0f, fCx - 4.0f, fCy, 1.6f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return pProxy->drawLine(pProxy, pDraw, fCx - 4.0f, fCy, fCx + 4.0f, fCy + 6.0f, 1.6f, iColor);
	}
	iRet = pProxy->drawLine(pProxy, pDraw, fCx - 4.0f, fCy - 6.0f, fCx + 4.0f, fCy, 1.6f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, fCx + 4.0f, fCy, fCx - 4.0f, fCy + 6.0f, 1.6f, iColor);
}

static int __xuiDatePickerCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_date_picker_data_t* pData;
	xui_date_picker_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	uint32_t iState;
	uint32_t iBackground;
	uint32_t iBorder;
	uint32_t iText;
	uint32_t iArrow;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiDatePickerResolve(pWidget, pData, &tResolved);
	__xuiDatePickerUpdateOwnerRects(pWidget, pData);
	tRect = xuiInternalSnapRect(xuiWidgetGetContentRect(pWidget));
	iState = __xuiDatePickerState(pWidget, pData);
	iBackground = tResolved.iBackgroundColor;
	iBorder = tResolved.iBorderColor;
	iText = tResolved.iTextColor;
	iArrow = tResolved.iArrowColor;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0u ) {
		iBackground = tResolved.iDisabledBackgroundColor;
		iBorder = tResolved.iBorderColor;
		iText = tResolved.iDisabledTextColor;
		iArrow = tResolved.iDisabledArrowColor;
	} else if ( (iState & XUI_DATE_PICKER_STATE_OPEN) != 0u ) {
		iBackground = tResolved.iOpenBackgroundColor;
		iBorder = tResolved.iFocusBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0u ) {
		iBorder = tResolved.iFocusBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_HOVER) != 0u ) {
		iBackground = tResolved.iHoverBackgroundColor;
		iBorder = tResolved.iHoverBorderColor;
	}
	iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tRect, iBackground);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiDatePickerDrawFill(pProxy, pDraw, pData->tButtonRect, XUI_COLOR_RGBA(242, 248, 254, 255));
	if ( iRet != XUI_OK ) return iRet;
	if ( (pProxy->drawLine != NULL) && (__xuiDatePickerAlpha(iBorder) != 0) ) {
		(void)pProxy->drawLine(pProxy, pDraw, pData->tButtonRect.fX, pData->tButtonRect.fY + 4.0f, pData->tButtonRect.fX, pData->tButtonRect.fY + pData->tButtonRect.fH - 4.0f, 1.0f, iBorder);
	}
	if ( (pProxy->drawText != NULL) && (tResolved.pFont != NULL) && (__xuiDatePickerAlpha(iText) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sText, pData->tTextRect, iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiDatePickerDrawChevron(pProxy, pDraw, pData->tButtonRect, (iState & XUI_DATE_PICKER_STATE_OPEN) != 0u, iArrow);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiDatePickerDrawStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
}

static int __xuiDatePickerDrawButton(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, xui_rect_t tRect, const char* sText, uint32_t iText, uint32_t iFill, uint32_t iBorder, int bAccent)
{
	int iRet;

	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XUI_OK;
	iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiDatePickerDrawStroke(pProxy, pDraw, tRect, bAccent ? 1.5f : 1.0f, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pProxy->drawText != NULL) && (pFont != NULL) && (sText != NULL) ) {
		return pProxy->drawText(pProxy, pDraw, pFont, sText, tRect, iText, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return XUI_OK;
}

static xui_rect_t __xuiDatePickerComboPopupRect(const xui_date_picker_data_t* pData, int iKind, int iPanel)
{
	xui_rect_t tField;
	xui_rect_t tCalendar;
	xui_rect_t tRect;
	float fW;
	float fH;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return (xui_rect_t){0};
	tField = (iKind == XUI_DATE_PICKER_COMBO_MONTH) ? pData->tMonthRect[iPanel] : pData->tYearRect[iPanel];
	tCalendar = pData->tCalendarRect[iPanel];
	if ( tField.fW <= 0.0f || tField.fH <= 0.0f ) return (xui_rect_t){0};
	if ( iKind == XUI_DATE_PICKER_COMBO_MONTH ) {
		fW = 144.0f;
		fH = 96.0f;
		tRect = (xui_rect_t){tField.fX + tField.fW - fW, tField.fY + tField.fH + 2.0f, fW, fH};
	} else {
		fW = __xuiDatePickerMax(tField.fW, XUI_DATE_PICKER_YEAR_FIELD_W);
		fH = 22.0f * (float)XUI_DATE_PICKER_YEAR_OPTION_COUNT;
		tRect = (xui_rect_t){tField.fX, tField.fY + tField.fH + 2.0f, fW, fH};
	}
	if ( tRect.fX < tCalendar.fX + 2.0f ) tRect.fX = tCalendar.fX + 2.0f;
	if ( tRect.fX + tRect.fW > tCalendar.fX + tCalendar.fW - 2.0f ) tRect.fX = tCalendar.fX + tCalendar.fW - tRect.fW - 2.0f;
	if ( tRect.fY + tRect.fH > pData->fPopupHeight - 50.0f ) tRect.fY = pData->fPopupHeight - 50.0f - tRect.fH;
	if ( tRect.fY < tField.fY + tField.fH + 2.0f ) tRect.fY = tField.fY + tField.fH + 2.0f;
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiDatePickerYearOptionRect(const xui_date_picker_data_t* pData, int iPanel, int iIndex)
{
	xui_rect_t tDrop;

	if ( iIndex < 0 || iIndex >= XUI_DATE_PICKER_YEAR_OPTION_COUNT ) return (xui_rect_t){0};
	tDrop = __xuiDatePickerComboPopupRect(pData, XUI_DATE_PICKER_COMBO_YEAR, iPanel);
	return xuiInternalSnapRect((xui_rect_t){tDrop.fX, tDrop.fY + (float)iIndex * 22.0f, tDrop.fW, 22.0f});
}

static xui_rect_t __xuiDatePickerMonthOptionRect(const xui_date_picker_data_t* pData, int iPanel, int iIndex)
{
	xui_rect_t tDrop;
	float fCellW;
	float fCellH;

	if ( iIndex < 0 || iIndex >= 12 ) return (xui_rect_t){0};
	tDrop = __xuiDatePickerComboPopupRect(pData, XUI_DATE_PICKER_COMBO_MONTH, iPanel);
	fCellW = tDrop.fW / 3.0f;
	fCellH = tDrop.fH / 4.0f;
	return xuiInternalSnapRect((xui_rect_t){tDrop.fX + (float)(iIndex % 3) * fCellW, tDrop.fY + (float)(iIndex / 3) * fCellH, fCellW, fCellH});
}

static int64 __xuiDatePickerYearOptionValue(const xui_date_picker_data_t* pData, int iPanel, int iIndex)
{
	int64 iYear;
	int64 iFirst;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return 1;
	xrtDecodeSerial(pData->tViewMonth[iPanel], &iYear, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	iFirst = iYear - (XUI_DATE_PICKER_YEAR_OPTION_COUNT / 2);
	if ( iFirst < XUI_DATE_PICKER_YEAR_MIN ) iFirst = XUI_DATE_PICKER_YEAR_MIN;
	if ( iFirst + XUI_DATE_PICKER_YEAR_OPTION_COUNT - 1 > XUI_DATE_PICKER_YEAR_MAX ) {
		iFirst = XUI_DATE_PICKER_YEAR_MAX - XUI_DATE_PICKER_YEAR_OPTION_COUNT + 1;
	}
	if ( iFirst < XUI_DATE_PICKER_YEAR_MIN ) iFirst = XUI_DATE_PICKER_YEAR_MIN;
	return iFirst + iIndex;
}

static int __xuiDatePickerDrawComboField(xui_date_picker_data_t* pData, const xui_date_picker_data_t* pResolved, xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, const char* sText, int bOpen, int bActive, int bError, int bSelectAll)
{
	xui_rect_t tText;
	xui_rect_t tArrow;
	xui_rect_t tSelect;
	uint32_t iBorder;
	int iRet;

	iBorder = bError ? XUI_COLOR_RGBA(226, 74, 92, 255) : (bActive ? pResolved->iAccentColor : pResolved->iFieldBorderColor);
	iRet = __xuiDatePickerDrawButton(pProxy, pDraw, pResolved->pFont, tRect, "", pResolved->iPopupTextColor, pResolved->iFieldColor, iBorder, bActive);
	if ( iRet != XUI_OK ) return iRet;
	tArrow = xuiInternalSnapRect((xui_rect_t){tRect.fX + tRect.fW - 20.0f, tRect.fY, 20.0f, tRect.fH});
	tText = xuiInternalSnapRect((xui_rect_t){tRect.fX + 6.0f, tRect.fY, tRect.fW - 28.0f, tRect.fH});
	if ( bSelectAll && (pProxy->drawRectFill != NULL || pProxy->drawRectFill != NULL) ) {
		tSelect = __xuiDatePickerInsetRect(tText, 3.0f);
		(void)__xuiDatePickerDrawFill(pProxy, pDraw, tSelect, XUI_COLOR_RGBA(218, 236, 252, 255));
	}
	if ( pProxy->drawLine != NULL ) {
		(void)pProxy->drawLine(pProxy, pDraw, tArrow.fX, tArrow.fY + 4.0f, tArrow.fX, tArrow.fY + tArrow.fH - 4.0f, 1.0f, iBorder);
	}
	if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
		(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, sText, tText, pResolved->iPopupTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return __xuiDatePickerDrawChevron(pProxy, pDraw, tArrow, bOpen, pResolved->iPopupMutedTextColor);
}

static int __xuiDatePickerDrawCalendar(xui_widget pOwner, xui_date_picker_data_t* pData, const xui_date_picker_data_t* pResolved, xui_proxy pProxy, xui_draw_context pDraw, int iPanel)
{
	static const char* arrWeek[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	char sText[32];
	xui_rect_t tRect;
	xtime tMonth;
	xtime tDay;
	xtime tDate;
	int64 iYear;
	int iMonth;
	int iDay;
	int iCellMonth;
	int i;
	int iRet;
	int bCurrentMonth;
	int bSelected;
	int bInRange;
	int bEnabled;
	uint32_t iFill;
	uint32_t iText;
	uint32_t iBorder;

	(void)pOwner;
	if ( pData->tCalendarRect[iPanel].fW <= 0.0f ) return XUI_OK;
	tRect = pData->tCalendarRect[iPanel];
	iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tRect, XUI_COLOR_RGBA(255, 255, 255, 160));
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiDatePickerDrawStroke(pProxy, pDraw, tRect, 1.0f, pResolved->iSeparatorColor);
	if ( iRet != XUI_OK ) return iRet;
	tMonth = pData->tViewMonth[iPanel];
	xrtDecodeSerial(tMonth, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	(void)__xuiDatePickerDrawButton(pProxy, pDraw, pResolved->pFont, pData->tPrevRect[iPanel], "", pResolved->iPopupTextColor, pResolved->iFieldColor, pResolved->iFieldBorderColor, 0);
	(void)__xuiDatePickerDrawButton(pProxy, pDraw, pResolved->pFont, pData->tNextRect[iPanel], "", pResolved->iPopupTextColor, pResolved->iFieldColor, pResolved->iFieldBorderColor, 0);
	(void)__xuiDatePickerDrawArrow(pProxy, pDraw, pData->tPrevRect[iPanel], -1, pResolved->iPopupMutedTextColor);
	(void)__xuiDatePickerDrawArrow(pProxy, pDraw, pData->tNextRect[iPanel], 1, pResolved->iPopupMutedTextColor);
	if ( pData->iEditKind == XUI_DATE_PICKER_EDIT_YEAR && pData->iEditPanel == iPanel ) {
		snprintf(sText, sizeof(sText), "%s", pData->sEdit);
	} else {
		snprintf(sText, sizeof(sText), "%04lld", (long long)iYear);
	}
	(void)__xuiDatePickerDrawComboField(pData, pResolved, pProxy, pDraw, pData->tYearRect[iPanel], sText,
		(pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_YEAR) && (pData->iOpenComboPanel == iPanel),
		(pData->iEditKind == XUI_DATE_PICKER_EDIT_YEAR && pData->iEditPanel == iPanel) ||
		(pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_YEAR && pData->iOpenComboPanel == iPanel),
		(pData->iEditKind == XUI_DATE_PICKER_EDIT_YEAR && pData->iEditPanel == iPanel && pData->iEditError),
		(pData->iEditKind == XUI_DATE_PICKER_EDIT_YEAR && pData->iEditPanel == iPanel && pData->iEditSelectAll));
	snprintf(sText, sizeof(sText), "%02d", iMonth);
	(void)__xuiDatePickerDrawComboField(pData, pResolved, pProxy, pDraw, pData->tMonthRect[iPanel], sText,
		(pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_MONTH) && (pData->iOpenComboPanel == iPanel),
		(pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_MONTH) && (pData->iOpenComboPanel == iPanel),
		0, 0);
	for ( i = 0; i < 7; i++ ) {
		tRect = pData->arrDayRect[iPanel][i];
		tRect.fY = pData->tCalendarRect[iPanel].fY + 34.0f;
		tRect.fH = 20.0f;
		if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
			(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, arrWeek[(i + pData->iFirstDayOfWeek) % 7], tRect, pResolved->iPopupMutedTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
	}
	for ( i = 0; i < XUI_DATE_PICKER_CELL_COUNT; i++ ) {
		tDay = pData->arrDayValue[iPanel][i];
		xrtDecodeSerial(tDay, NULL, &iCellMonth, &iDay, NULL, NULL, NULL, NULL, NULL);
		bCurrentMonth = (iCellMonth == iMonth);
		bEnabled = __xuiDatePickerValueEnabled(pData, tDay);
		tDate = __xuiDatePickerNormalizeValue(pData, tDay);
		bSelected = 0;
		bInRange = 0;
		if ( __xuiDatePickerIsRange(pData->iMode) ) {
			bSelected = (xrtDatePart(pData->tDraftStart) == tDay) || (xrtDatePart(pData->tDraftEnd) == tDay);
			bInRange = (tDay >= xrtDatePart(pData->tDraftStart)) && (tDay <= xrtDatePart(pData->tDraftEnd));
		} else {
			bSelected = (xrtDatePart(pData->tDraftValue) == tDay);
		}
		iFill = 0;
		iText = bCurrentMonth ? pResolved->iPopupTextColor : pResolved->iPopupMutedTextColor;
		iBorder = 0;
		if ( !bEnabled ) {
			iText = pResolved->iDisabledDayColor;
		} else if ( bSelected ) {
			iFill = pResolved->iAccentColor;
			iText = pResolved->iSelectedTextColor;
			iBorder = pResolved->iAccentColor;
		} else if ( bInRange ) {
			iFill = XUI_COLOR_RGBA(218, 236, 252, 255);
			iText = pResolved->iPopupTextColor;
		} else if ( xrtDatePart(xrtNow()) == tDay ) {
			iBorder = pResolved->iAccentColor;
		}
		tRect = pData->arrDayRect[iPanel][i];
		tRect.fX += 2.0f;
		tRect.fY += 2.0f;
		tRect.fW -= 4.0f;
		tRect.fH -= 4.0f;
		if ( __xuiDatePickerAlpha(iFill) != 0 ) {
			iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tRect, iFill);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( __xuiDatePickerAlpha(iBorder) != 0 ) {
			iRet = __xuiDatePickerDrawStroke(pProxy, pDraw, tRect, 1.0f, iBorder);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
			snprintf(sText, sizeof(sText), "%d", iDay);
			(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, sText, tRect, iText, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
		(void)tDate;
	}
	return XUI_OK;
}

static xtime __xuiDatePickerDraftForPanel(const xui_date_picker_data_t* pData, int iPanel)
{
	if ( pData == NULL ) return 0;
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		return (iPanel == 1) ? pData->tDraftEnd : pData->tDraftStart;
	}
	return pData->tDraftValue;
}

static int __xuiDatePickerDrawTimePanel(xui_date_picker_data_t* pData, const xui_date_picker_data_t* pResolved, xui_proxy pProxy, xui_draw_context pDraw, int iPanel)
{
	static const char* arrName[3] = {"Hour", "Min", "Sec"};
	char sText[32];
	xui_rect_t tRect;
	xtime tValue;
	int iHour;
	int iMinute;
	int iSecond;
	int arrValue[3];
	float fTitleY;
	int i;
	int iRet;

	if ( pData->tTimePanelRect[iPanel].fW <= 0.0f ) return XUI_OK;
	tRect = pData->tTimePanelRect[iPanel];
	iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tRect, XUI_COLOR_RGBA(255, 255, 255, 150));
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiDatePickerDrawStroke(pProxy, pDraw, tRect, 1.0f, pResolved->iSeparatorColor);
	if ( iRet != XUI_OK ) return iRet;
	fTitleY = tRect.fY + 7.0f;
	if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
		snprintf(sText, sizeof(sText), "%s time", (__xuiDatePickerIsRange(pData->iMode) ? ((iPanel == 0) ? "Start" : "End") : "Select"));
		(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, sText, (xui_rect_t){tRect.fX + 8.0f, fTitleY, tRect.fW - 16.0f, 16.0f}, pResolved->iPopupMutedTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	tValue = __xuiDatePickerDraftForPanel(pData, iPanel);
	xrtDecodeSerial(tValue, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
	arrValue[0] = iHour;
	arrValue[1] = iMinute;
	arrValue[2] = iSecond;
	for ( i = 0; i < (pData->bShowSecond ? 3 : 2); i++ ) {
		tRect = pData->arrTimeRect[iPanel][i];
		if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
			(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, arrName[i], (xui_rect_t){tRect.fX, tRect.fY - 18.0f, tRect.fW, 16.0f}, pResolved->iPopupMutedTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
		iRet = __xuiDatePickerDrawButton(pProxy, pDraw, pResolved->pFont, tRect, "", pResolved->iPopupTextColor, pResolved->iFieldColor, ((pData->iActiveTimePanel == iPanel) && (pData->iActiveTimeField == i)) ? pResolved->iAccentColor : pResolved->iFieldBorderColor, (pData->iActiveTimePanel == iPanel) && (pData->iActiveTimeField == i));
		if ( iRet != XUI_OK ) return iRet;
		if ( pData->iEditKind == XUI_DATE_PICKER_EDIT_TIME && pData->iEditPanel == iPanel && pData->iEditField == i && pData->iEditSelectAll ) {
			(void)__xuiDatePickerDrawFill(pProxy, pDraw, __xuiDatePickerInsetRect(tRect, 4.0f), XUI_COLOR_RGBA(218, 236, 252, 255));
		}
		if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
			if ( pData->iEditKind == XUI_DATE_PICKER_EDIT_TIME && pData->iEditPanel == iPanel && pData->iEditField == i ) {
				snprintf(sText, sizeof(sText), "%s", pData->sEdit);
			} else {
				snprintf(sText, sizeof(sText), "%02d", arrValue[i]);
			}
			(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, sText, tRect, pResolved->iPopupTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
	}
	return XUI_OK;
}

static int __xuiDatePickerDrawComboPopup(xui_date_picker_data_t* pData, const xui_date_picker_data_t* pResolved, xui_proxy pProxy, xui_draw_context pDraw)
{
	char sText[32];
	xui_rect_t tDrop;
	xui_rect_t tItem;
	xtime tMonth;
	int64 iYear;
	int iMonth;
	int iPanel;
	int i;
	int64 iValue;
	int iRet;
	int bSelected;
	uint32_t iFill;
	uint32_t iText;

	if ( pData == NULL || pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_NONE ) return XUI_OK;
	iPanel = pData->iOpenComboPanel;
	if ( iPanel < 0 || iPanel >= __xuiDatePickerPanelCount(pData->iMode) ) return XUI_OK;
	tDrop = __xuiDatePickerComboPopupRect(pData, pData->iOpenComboKind, iPanel);
	if ( tDrop.fW <= 0.0f || tDrop.fH <= 0.0f ) return XUI_OK;
	iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tDrop, pResolved->iFieldColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiDatePickerDrawStroke(pProxy, pDraw, tDrop, 1.0f, pResolved->iAccentColor);
	if ( iRet != XUI_OK ) return iRet;
	tMonth = pData->tViewMonth[iPanel];
	xrtDecodeSerial(tMonth, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	if ( pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_YEAR ) {
		for ( i = 0; i < XUI_DATE_PICKER_YEAR_OPTION_COUNT; i++ ) {
			iValue = __xuiDatePickerYearOptionValue(pData, iPanel, i);
			tItem = __xuiDatePickerInsetRect(__xuiDatePickerYearOptionRect(pData, iPanel, i), 2.0f);
			bSelected = (iValue == iYear);
			iFill = bSelected ? pResolved->iAccentColor : ((pData->iHoverPart == XUI_DATE_PICKER_PART_YEAR_OPTION && pData->iHoverPanel == iPanel && pData->iHoverIndex == i) ? XUI_COLOR_RGBA(232, 244, 255, 255) : 0);
			iText = bSelected ? pResolved->iSelectedTextColor : pResolved->iPopupTextColor;
			if ( __xuiDatePickerAlpha(iFill) != 0 ) {
				iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tItem, iFill);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
				snprintf(sText, sizeof(sText), "%04lld", (long long)iValue);
				(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, sText, tItem, iText, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			}
		}
	} else {
		for ( i = 0; i < 12; i++ ) {
			tItem = __xuiDatePickerInsetRect(__xuiDatePickerMonthOptionRect(pData, iPanel, i), 2.0f);
			bSelected = (i + 1 == iMonth);
			iFill = bSelected ? pResolved->iAccentColor : ((pData->iHoverPart == XUI_DATE_PICKER_PART_MONTH_OPTION && pData->iHoverPanel == iPanel && pData->iHoverIndex == i) ? XUI_COLOR_RGBA(232, 244, 255, 255) : 0);
			iText = bSelected ? pResolved->iSelectedTextColor : pResolved->iPopupTextColor;
			if ( __xuiDatePickerAlpha(iFill) != 0 ) {
				iRet = __xuiDatePickerDrawFill(pProxy, pDraw, tItem, iFill);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( pProxy->drawText != NULL && pResolved->pFont != NULL ) {
				snprintf(sText, sizeof(sText), "%02d", i + 1);
				(void)pProxy->drawText(pProxy, pDraw, pResolved->pFont, sText, tItem, iText, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			}
		}
	}
	return XUI_OK;
}

static int __xuiDatePickerPanelRender(xui_widget pPanel, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pOwner;
	xui_date_picker_data_t* pData;
	xui_date_picker_data_t tResolved;
	xui_proxy pProxy;
	int i;
	int iRet;

	(void)iStateId;
	pOwner = (xui_widget)pUser;
	if ( (pPanel == NULL) || (pDraw == NULL) || (pOwner == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiDatePickerGetData(pOwner);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pOwner));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiDatePickerResolve(pOwner, pData, &tResolved);
	__xuiDatePickerUpdatePanelRects(pData);
	for ( i = 0; i < __xuiDatePickerPanelCount(pData->iMode); i++ ) {
		iRet = __xuiDatePickerDrawCalendar(pOwner, pData, &tResolved, pProxy, pDraw, i);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiDatePickerDrawTimePanel(pData, &tResolved, pProxy, pDraw, i);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, (xui_rect_t){XUI_DATE_PICKER_PAD, pData->fPopupHeight - 48.0f, pData->fPopupWidth - XUI_DATE_PICKER_PAD * 2.0f, 1.0f}, tResolved.iSeparatorColor);
	}
	(void)__xuiDatePickerDrawButton(pProxy, pDraw, tResolved.pFont, pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_TODAY], __xuiDatePickerHasDate(pData->iMode) ? "Today" : "Now", tResolved.iPopupTextColor, tResolved.iFieldColor, tResolved.iFieldBorderColor, 0);
	if ( pData->bNullable ) {
		(void)__xuiDatePickerDrawButton(pProxy, pDraw, tResolved.pFont, pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_CLEAR], "Clear", tResolved.iPopupTextColor, tResolved.iFieldColor, tResolved.iFieldBorderColor, 0);
	}
	(void)__xuiDatePickerDrawButton(pProxy, pDraw, tResolved.pFont, pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_CANCEL], "Cancel", tResolved.iPopupTextColor, tResolved.iFieldColor, tResolved.iFieldBorderColor, 0);
	(void)__xuiDatePickerDrawButton(pProxy, pDraw, tResolved.pFont, pData->arrFooterRect[XUI_DATE_PICKER_FOOTER_OK], "OK", tResolved.iSelectedTextColor, tResolved.iAccentColor, tResolved.iAccentColor, 1);
	iRet = __xuiDatePickerDrawComboPopup(pData, &tResolved, pProxy, pDraw);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_OK;
}

static void __xuiDatePickerPrepareDraft(xui_date_picker_data_t* pData)
{
	if ( pData == NULL ) return;
	__xuiDatePickerEnsureValue(pData);
	if ( pData->bHasValue ) {
		pData->tDraftValue = pData->tValue;
		pData->tDraftStart = pData->tStart;
		pData->tDraftEnd = pData->tEnd;
	} else {
		__xuiDatePickerDefaultValues(pData, &pData->tDraftStart, &pData->tDraftEnd);
		pData->tDraftValue = pData->tDraftStart;
	}
	pData->iActivePanel = 0;
	pData->iActiveTimePanel = 0;
	pData->iActiveTimeField = -1;
	pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
	pData->iOpenComboPanel = -1;
	__xuiDatePickerCancelEdit(pData);
	__xuiDatePickerInitViewMonth(pData);
}

static int __xuiDatePickerCommitDraft(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	int bChanged;
	xtime tOldValue;
	xtime tOldStart;
	xtime tOldEnd;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tOldValue = pData->tValue;
	tOldStart = pData->tStart;
	tOldEnd = pData->tEnd;
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		__xuiDatePickerNormalizeRange(pData, &pData->tDraftStart, &pData->tDraftEnd);
		pData->tStart = pData->tDraftStart;
		pData->tEnd = pData->tDraftEnd;
		pData->tValue = pData->tStart;
		bChanged = !pData->bHasValue || (tOldStart != pData->tStart) || (tOldEnd != pData->tEnd);
	} else {
		pData->tDraftValue = __xuiDatePickerClampValue(pData, pData->tDraftValue);
		pData->tValue = pData->tDraftValue;
		pData->tStart = pData->tValue;
		pData->tEnd = pData->tValue;
		bChanged = !pData->bHasValue || (tOldValue != pData->tValue);
	}
	pData->bHasValue = 1;
	__xuiDatePickerSyncText(pData);
	if ( bChanged ) {
		pData->iChangeCount++;
		__xuiDatePickerNotifyValue(pWidget, pData, pData->onChange, pData->pChangeUser);
	}
	pData->iCommitCount++;
	__xuiDatePickerNotifyValue(pWidget, pData, pData->onCommit, pData->pCommitUser);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

static int __xuiDatePickerCancelDraft(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iCancelCount++;
	__xuiDatePickerNotifyValue(pWidget, pData, pData->onCancel, pData->pCancelUser);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

static int __xuiDatePickerClearInternal(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	int bChanged;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pData->bNullable ) return XUI_OK;
	bChanged = pData->bHasValue;
	pData->bHasValue = 0;
	pData->sText[0] = '\0';
	pData->iClearCount++;
	__xuiDatePickerNotifyValue(pWidget, pData, pData->onClear, pData->pClearUser);
	if ( bChanged ) {
		pData->iChangeCount++;
		__xuiDatePickerNotifyValue(pWidget, pData, pData->onChange, pData->pChangeUser);
	}
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

static int __xuiDatePickerCloseWithReason(xui_widget pWidget, xui_date_picker_data_t* pData, int iReason)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pPopup != NULL ) {
		if ( iReason == XUI_DATE_PICKER_CLOSE_CANCEL ) {
			(void)__xuiDatePickerCancelDraft(pWidget, pData);
		} else if ( iReason == XUI_DATE_PICKER_CLOSE_COMMIT ) {
			(void)__xuiDatePickerCommitDraft(pWidget, pData);
		} else if ( iReason == XUI_DATE_PICKER_CLOSE_CLEAR ) {
			(void)__xuiDatePickerClearInternal(pWidget, pData);
		}
		pData->bClosingExplicit = 1;
		pData->iCloseReason = iReason;
		iRet = xuiPopupSetOpen(pData->pPopup, 0);
		pData->bClosingExplicit = 0;
		pData->iCloseReason = XUI_DATE_PICKER_CLOSE_NONE;
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)__xuiDatePickerSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiDatePickerOpenInternal(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) return XUI_OK;
	__xuiDatePickerPrepareDraft(pData);
	iRet = __xuiDatePickerApplyPopupStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetOpen(pData->pPopup, 1);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->pPanel != NULL ) (void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pPanel);
	(void)__xuiDatePickerSyncState(pWidget, pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

static void __xuiDatePickerSelectDay(xui_widget pOwner, xui_date_picker_data_t* pData, int iPanel, int iIndex)
{
	xtime tDay;
	xtime tTime;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iIndex < 0) || (iIndex >= XUI_DATE_PICKER_CELL_COUNT) ) return;
	tDay = pData->arrDayValue[iPanel][iIndex];
	if ( !__xuiDatePickerValueEnabled(pData, tDay) ) return;
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		if ( pData->iMode == XUI_DATE_PICKER_MODE_DATETIME_RANGE ) {
			tTime = xrtTimePart((iPanel == 1) ? pData->tDraftEnd : pData->tDraftStart);
			tDay = xrtDatePart(tDay) + tTime;
		}
		if ( iPanel == 1 ) {
			pData->tDraftEnd = __xuiDatePickerClampValue(pData, tDay);
			pData->iActivePanel = 0;
		} else {
			pData->tDraftStart = __xuiDatePickerClampValue(pData, tDay);
			pData->iActivePanel = 1;
		}
		__xuiDatePickerNormalizeRange(pData, &pData->tDraftStart, &pData->tDraftEnd);
	} else {
		if ( pData->iMode == XUI_DATE_PICKER_MODE_DATETIME ) {
			tDay = xrtDatePart(tDay) + xrtTimePart(pData->tDraftValue);
		}
		pData->tDraftValue = __xuiDatePickerClampValue(pData, tDay);
	}
	__xuiDatePickerNotifyDraft(pOwner, pData);
	(void)__xuiDatePickerInvalidateAll(pOwner, pData);
}

static void __xuiDatePickerSetDraftTime(xui_date_picker_data_t* pData, int iPanel, int iField, int iDelta)
{
	xtime* pTarget;
	xtime tValue;
	xtime tDate;
	int iHour;
	int iMinute;
	int iSecond;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iField < 0) || (iField > 2) ) return;
	pTarget = &pData->tDraftValue;
	if ( __xuiDatePickerIsRange(pData->iMode) ) pTarget = (iPanel == 1) ? &pData->tDraftEnd : &pData->tDraftStart;
	tValue = *pTarget;
	xrtDecodeSerial(tValue, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
	if ( iField == 0 ) iHour = __xuiDatePickerClampInt(iHour + iDelta, 0, 23);
	else if ( iField == 1 ) iMinute = __xuiDatePickerClampInt(iMinute + iDelta, 0, 59);
	else iSecond = __xuiDatePickerClampInt(iSecond + iDelta, 0, 59);
	tDate = ((pData->iMode == XUI_DATE_PICKER_MODE_DATETIME) || (pData->iMode == XUI_DATE_PICKER_MODE_DATETIME_RANGE)) ? xrtDatePart(tValue) : 0;
	*pTarget = __xuiDatePickerClampValue(pData, tDate + __xuiDatePickerMakeTime(iHour, iMinute, iSecond, pData->bShowSecond));
	if ( __xuiDatePickerIsRange(pData->iMode) ) __xuiDatePickerNormalizeRange(pData, &pData->tDraftStart, &pData->tDraftEnd);
}

static void __xuiDatePickerSetDraftTimeValue(xui_date_picker_data_t* pData, int iPanel, int iField, int iValue)
{
	xtime* pTarget;
	xtime tValue;
	xtime tDate;
	int iHour;
	int iMinute;
	int iSecond;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iField < 0) || (iField > 2) ) return;
	pTarget = &pData->tDraftValue;
	if ( __xuiDatePickerIsRange(pData->iMode) ) pTarget = (iPanel == 1) ? &pData->tDraftEnd : &pData->tDraftStart;
	tValue = *pTarget;
	xrtDecodeSerial(tValue, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
	if ( iField == 0 ) iHour = __xuiDatePickerClampInt(iValue, 0, 23);
	else if ( iField == 1 ) iMinute = __xuiDatePickerClampInt(iValue, 0, 59);
	else iSecond = __xuiDatePickerClampInt(iValue, 0, 59);
	tDate = ((pData->iMode == XUI_DATE_PICKER_MODE_DATETIME) || (pData->iMode == XUI_DATE_PICKER_MODE_DATETIME_RANGE)) ? xrtDatePart(tValue) : 0;
	*pTarget = __xuiDatePickerClampValue(pData, tDate + __xuiDatePickerMakeTime(iHour, iMinute, iSecond, pData->bShowSecond));
	if ( __xuiDatePickerIsRange(pData->iMode) ) __xuiDatePickerNormalizeRange(pData, &pData->tDraftStart, &pData->tDraftEnd);
}

static void __xuiDatePickerCancelEdit(xui_date_picker_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iEditKind = XUI_DATE_PICKER_EDIT_NONE;
	pData->iEditPanel = -1;
	pData->iEditField = -1;
	pData->iEditSelectAll = 0;
	pData->iEditError = 0;
	pData->iEditLen = 0;
	pData->sEdit[0] = '\0';
}

static void __xuiDatePickerBeginYearEdit(xui_date_picker_data_t* pData, int iPanel)
{
	int64 iYear;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return;
	xrtDecodeSerial(pData->tViewMonth[iPanel], &iYear, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	pData->iEditKind = XUI_DATE_PICKER_EDIT_YEAR;
	pData->iEditPanel = iPanel;
	pData->iEditField = -1;
	snprintf(pData->sEdit, sizeof(pData->sEdit), "%04lld", (long long)iYear);
	pData->iEditLen = (int)strlen(pData->sEdit);
	pData->iEditSelectAll = 1;
	pData->iEditError = 0;
}

static void __xuiDatePickerBeginTimeEdit(xui_date_picker_data_t* pData, int iPanel, int iField)
{
	xtime tValue;
	int iHour;
	int iMinute;
	int iSecond;
	int iValue;

	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iField < 0) || (iField > 2) ) return;
	tValue = __xuiDatePickerDraftForPanel(pData, iPanel);
	xrtDecodeSerial(tValue, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
	iValue = (iField == 0) ? iHour : ((iField == 1) ? iMinute : iSecond);
	pData->iEditKind = XUI_DATE_PICKER_EDIT_TIME;
	pData->iEditPanel = iPanel;
	pData->iEditField = iField;
	snprintf(pData->sEdit, sizeof(pData->sEdit), "%02d", iValue);
	pData->iEditLen = (int)strlen(pData->sEdit);
	pData->iEditSelectAll = 1;
	pData->iEditError = 0;
}

static void __xuiDatePickerSetViewMonth(xui_widget pOwner, xui_date_picker_data_t* pData, int iPanel, int64 iYear, int iMonth)
{
	xtime tMonth;

	(void)pOwner;
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return;
	iYear = __xuiDatePickerClampYear(iYear);
	iMonth = __xuiDatePickerClampInt(iMonth, 1, 12);
	tMonth = xrtDateSerial(iYear, iMonth, 1);
	pData->tViewMonth[iPanel] = tMonth;
	if ( __xuiDatePickerIsRange(pData->iMode) ) {
		if ( iPanel == 0 && pData->tViewMonth[1] <= pData->tViewMonth[0] ) {
			pData->tViewMonth[1] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, pData->tViewMonth[0]);
		} else if ( iPanel == 1 && pData->tViewMonth[0] >= pData->tViewMonth[1] ) {
			pData->tViewMonth[0] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, -1, pData->tViewMonth[1]);
		}
	} else if ( iPanel == 0 ) {
		pData->tViewMonth[1] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, pData->tViewMonth[0]);
	}
}

static int __xuiDatePickerCommitEdit(xui_widget pOwner, xui_date_picker_data_t* pData, int bNotify)
{
	int iValue;
	int64 iYear;
	int64 iEditYear;
	int iMonth;
	int iMax;

	if ( (pOwner == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiDatePickerEditing(pData) ) return XUI_OK;
	if ( pData->iEditKind == XUI_DATE_PICKER_EDIT_YEAR ) {
		if ( pData->iEditLen <= 0 ) {
			pData->iEditError = 1;
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		if ( !__xuiDatePickerParseYearText(pData->sEdit, &iEditYear) ) {
			pData->iEditError = 1;
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		xrtDecodeSerial(pData->tViewMonth[pData->iEditPanel], &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
		(void)iYear;
		__xuiDatePickerSetViewMonth(pOwner, pData, pData->iEditPanel, iEditYear, iMonth);
		__xuiDatePickerCancelEdit(pData);
		return __xuiDatePickerInvalidateAll(pOwner, pData);
	}
	if ( pData->iEditKind == XUI_DATE_PICKER_EDIT_TIME ) {
		iValue = (pData->iEditLen > 0) ? atoi(pData->sEdit) : 0;
		iMax = __xuiDatePickerTimeFieldMax(pData->iEditField);
		iValue = __xuiDatePickerClampInt(iValue, 0, iMax);
		__xuiDatePickerSetDraftTimeValue(pData, pData->iEditPanel, pData->iEditField, iValue);
		__xuiDatePickerCancelEdit(pData);
		if ( bNotify ) __xuiDatePickerNotifyDraft(pOwner, pData);
		return __xuiDatePickerInvalidateAll(pOwner, pData);
	}
	__xuiDatePickerCancelEdit(pData);
	return XUI_OK;
}

static void __xuiDatePickerSetDraftNow(xui_widget pOwner, xui_date_picker_data_t* pData)
{
	xtime tStart;
	xtime tEnd;

	if ( pData == NULL ) return;
	__xuiDatePickerDefaultValues(pData, &tStart, &tEnd);
	pData->tDraftValue = tStart;
	pData->tDraftStart = tStart;
	pData->tDraftEnd = tEnd;
	__xuiDatePickerInitViewMonth(pData);
	__xuiDatePickerNotifyDraft(pOwner, pData);
	(void)__xuiDatePickerInvalidateAll(pOwner, pData);
}

static int __xuiDatePickerHitPanel(xui_date_picker_data_t* pData, float fX, float fY, int* pPanel, int* pIndex)
{
	int i;
	int j;

	if ( pPanel != NULL ) *pPanel = -1;
	if ( pIndex != NULL ) *pIndex = -1;
	if ( pData == NULL ) return XUI_DATE_PICKER_PART_NONE;
	if ( pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_YEAR && pData->iOpenComboPanel >= 0 ) {
		for ( j = 0; j < XUI_DATE_PICKER_YEAR_OPTION_COUNT; j++ ) {
			if ( __xuiDatePickerRectContains(__xuiDatePickerYearOptionRect(pData, pData->iOpenComboPanel, j), fX, fY) ) {
				if ( pPanel != NULL ) *pPanel = pData->iOpenComboPanel;
				if ( pIndex != NULL ) *pIndex = j;
				return XUI_DATE_PICKER_PART_YEAR_OPTION;
			}
		}
	}
	if ( pData->iOpenComboKind == XUI_DATE_PICKER_COMBO_MONTH && pData->iOpenComboPanel >= 0 ) {
		for ( j = 0; j < 12; j++ ) {
			if ( __xuiDatePickerRectContains(__xuiDatePickerMonthOptionRect(pData, pData->iOpenComboPanel, j), fX, fY) ) {
				if ( pPanel != NULL ) *pPanel = pData->iOpenComboPanel;
				if ( pIndex != NULL ) *pIndex = j;
				return XUI_DATE_PICKER_PART_MONTH_OPTION;
			}
		}
	}
	for ( i = 0; i < 4; i++ ) {
		if ( __xuiDatePickerRectContains(pData->arrFooterRect[i], fX, fY) ) {
			if ( pIndex != NULL ) *pIndex = i;
			if ( i == XUI_DATE_PICKER_FOOTER_TODAY ) return XUI_DATE_PICKER_PART_TODAY;
			if ( i == XUI_DATE_PICKER_FOOTER_CLEAR ) return XUI_DATE_PICKER_PART_CLEAR;
			if ( i == XUI_DATE_PICKER_FOOTER_CANCEL ) return XUI_DATE_PICKER_PART_CANCEL;
			if ( i == XUI_DATE_PICKER_FOOTER_OK ) return XUI_DATE_PICKER_PART_OK;
		}
	}
	for ( i = 0; i < __xuiDatePickerPanelCount(pData->iMode); i++ ) {
		if ( __xuiDatePickerRectContains(pData->tPrevRect[i], fX, fY) ) {
			if ( pPanel != NULL ) *pPanel = i;
			return XUI_DATE_PICKER_PART_PREV;
		}
		if ( __xuiDatePickerRectContains(pData->tNextRect[i], fX, fY) ) {
			if ( pPanel != NULL ) *pPanel = i;
			return XUI_DATE_PICKER_PART_NEXT;
		}
		if ( __xuiDatePickerRectContains(pData->tYearRect[i], fX, fY) ) {
			if ( pPanel != NULL ) *pPanel = i;
			return XUI_DATE_PICKER_PART_YEAR_FIELD;
		}
		if ( __xuiDatePickerRectContains(pData->tMonthRect[i], fX, fY) ) {
			if ( pPanel != NULL ) *pPanel = i;
			return XUI_DATE_PICKER_PART_MONTH_FIELD;
		}
		for ( j = 0; j < XUI_DATE_PICKER_CELL_COUNT; j++ ) {
			if ( __xuiDatePickerRectContains(pData->arrDayRect[i][j], fX, fY) ) {
				if ( pPanel != NULL ) *pPanel = i;
				if ( pIndex != NULL ) *pIndex = j;
				return XUI_DATE_PICKER_PART_DAY;
			}
		}
		for ( j = 0; j < (pData->bShowSecond ? 3 : 2); j++ ) {
			if ( __xuiDatePickerRectContains(pData->arrTimeRect[i][j], fX, fY) ) {
				if ( pPanel != NULL ) *pPanel = i;
				if ( pIndex != NULL ) *pIndex = j;
				return XUI_DATE_PICKER_PART_TIME_HOUR + j;
			}
		}
	}
	return XUI_DATE_PICKER_PART_NONE;
}

static int __xuiDatePickerPanelPointerDown(xui_widget pPanel, xui_widget pOwner, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	float fX;
	float fY;
	int64 iYear;
	int iMonth;
	int iPanel;
	int iIndex;
	int iHit;

	if ( (pPanel == NULL) || (pOwner == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) return XUI_OK;
	tWorld = xuiWidgetGetWorldRect(pPanel);
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	__xuiDatePickerUpdatePanelRects(pData);
	iHit = __xuiDatePickerHitPanel(pData, fX, fY, &iPanel, &iIndex);
	pData->iActivePart = iHit;
	pData->iActivePanel = (iPanel >= 0) ? iPanel : pData->iActivePanel;
	if ( iHit == XUI_DATE_PICKER_PART_YEAR_OPTION ) {
		xrtDecodeSerial(pData->tViewMonth[iPanel], NULL, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
		__xuiDatePickerCancelEdit(pData);
		pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
		pData->iOpenComboPanel = -1;
		__xuiDatePickerSetViewMonth(pOwner, pData, iPanel, __xuiDatePickerYearOptionValue(pData, iPanel, iIndex), iMonth);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_MONTH_OPTION ) {
		xrtDecodeSerial(pData->tViewMonth[iPanel], &iYear, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		__xuiDatePickerCancelEdit(pData);
		pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
		pData->iOpenComboPanel = -1;
		__xuiDatePickerSetViewMonth(pOwner, pData, iPanel, iYear, iIndex + 1);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_YEAR_FIELD ) {
		pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_YEAR;
		pData->iOpenComboPanel = iPanel;
		__xuiDatePickerBeginYearEdit(pData, iPanel);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_MONTH_FIELD ) {
		__xuiDatePickerCancelEdit(pData);
		pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_MONTH;
		pData->iOpenComboPanel = iPanel;
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( __xuiDatePickerEditing(pData) ) {
		if ( __xuiDatePickerCommitEdit(pOwner, pData, 1) != XUI_OK ) {
			(void)__xuiDatePickerInvalidateAll(pOwner, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
	pData->iOpenComboPanel = -1;
	if ( iHit == XUI_DATE_PICKER_PART_PREV || iHit == XUI_DATE_PICKER_PART_NEXT ) {
		pData->tViewMonth[iPanel] = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, (iHit == XUI_DATE_PICKER_PART_PREV) ? -1 : 1, pData->tViewMonth[iPanel]);
		xrtDecodeSerial(pData->tViewMonth[iPanel], &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
		__xuiDatePickerSetViewMonth(pOwner, pData, iPanel, iYear, iMonth);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_DAY ) {
		__xuiDatePickerSelectDay(pOwner, pData, iPanel, iIndex);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit >= XUI_DATE_PICKER_PART_TIME_HOUR && iHit <= XUI_DATE_PICKER_PART_TIME_SECOND ) {
		pData->iActiveTimePanel = iPanel;
		pData->iActiveTimeField = iIndex;
		__xuiDatePickerBeginTimeEdit(pData, iPanel, iIndex);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_TODAY ) {
		__xuiDatePickerSetDraftNow(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_CLEAR ) {
		(void)__xuiDatePickerCloseWithReason(pOwner, pData, XUI_DATE_PICKER_CLOSE_CLEAR);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_CANCEL ) {
		(void)__xuiDatePickerCloseWithReason(pOwner, pData, XUI_DATE_PICKER_CLOSE_CANCEL);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iHit == XUI_DATE_PICKER_PART_OK ) {
		(void)__xuiDatePickerCloseWithReason(pOwner, pData, XUI_DATE_PICKER_CLOSE_COMMIT);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiDatePickerPanelPointerMove(xui_widget pPanel, xui_widget pOwner, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	int iPanel;
	int iIndex;
	int iHit;
	float fX;
	float fY;

	(void)pOwner;
	if ( (pPanel == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) return XUI_OK;
	tWorld = xuiWidgetGetWorldRect(pPanel);
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	__xuiDatePickerUpdatePanelRects(pData);
	iHit = __xuiDatePickerHitPanel(pData, fX, fY, &iPanel, &iIndex);
	if ( (pData->iHoverPart != iHit) || (pData->iHoverPanel != iPanel) || (pData->iHoverIndex != iIndex) ) {
		pData->iHoverPart = iHit;
		pData->iHoverPanel = iPanel;
		pData->iHoverIndex = iIndex;
		return xuiWidgetInvalidate(pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiDatePickerPanelWheel(xui_widget pPanel, xui_widget pOwner, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	float fX;
	float fY;
	int iPanel;
	int iIndex;
	int iHit;
	int iDelta;

	if ( (pPanel == NULL) || (pOwner == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tWorld = xuiWidgetGetWorldRect(pPanel);
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	__xuiDatePickerUpdatePanelRects(pData);
	iHit = __xuiDatePickerHitPanel(pData, fX, fY, &iPanel, &iIndex);
	if ( iHit >= XUI_DATE_PICKER_PART_TIME_HOUR && iHit <= XUI_DATE_PICKER_PART_TIME_SECOND ) {
		iDelta = (pEvent->fWheelY > 0.0f) ? 1 : -1;
		pData->iActiveTimePanel = iPanel;
		pData->iActiveTimeField = iIndex;
		__xuiDatePickerSetDraftTime(pData, iPanel, iIndex, iDelta);
		__xuiDatePickerNotifyDraft(pOwner, pData);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiDatePickerPanelKeyDown(xui_widget pPanel, xui_widget pOwner, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	int iDelta;

	(void)pPanel;
	if ( (pOwner == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) return XUI_OK;
	if ( __xuiDatePickerEditing(pData) ) {
		if ( (pEvent->iKey == 'A' || pEvent->iKey == 'a') && ((pEvent->iModifiers & XUI_MOD_CTRL) != 0u) ) {
			pData->iEditSelectAll = 1;
			(void)__xuiDatePickerInvalidateAll(pOwner, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_ENTER ) {
			(void)__xuiDatePickerCommitEdit(pOwner, pData, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			__xuiDatePickerCancelEdit(pData);
			pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
			pData->iOpenComboPanel = -1;
			(void)__xuiDatePickerInvalidateAll(pOwner, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_BACKSPACE ) {
			if ( pData->iEditSelectAll ) {
				pData->iEditLen = 0;
				pData->sEdit[0] = '\0';
				pData->iEditSelectAll = 0;
			} else if ( pData->iEditLen > 0 ) {
				pData->sEdit[--pData->iEditLen] = '\0';
			}
			pData->iEditError = 0;
			(void)__xuiDatePickerInvalidateAll(pOwner, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_DELETE ) {
			pData->iEditLen = 0;
			pData->sEdit[0] = '\0';
			pData->iEditSelectAll = 0;
			pData->iEditError = 0;
			(void)__xuiDatePickerInvalidateAll(pOwner, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pData->iOpenComboKind != XUI_DATE_PICKER_COMBO_NONE ) {
		if ( pEvent->iKey == XUI_KEY_ESCAPE || pEvent->iKey == XUI_KEY_ENTER ) {
			pData->iOpenComboKind = XUI_DATE_PICKER_COMBO_NONE;
			pData->iOpenComboPanel = -1;
			(void)__xuiDatePickerInvalidateAll(pOwner, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
		(void)__xuiDatePickerCloseWithReason(pOwner, pData, XUI_DATE_PICKER_CLOSE_CANCEL);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iKey == XUI_KEY_ENTER ) {
		(void)__xuiDatePickerCloseWithReason(pOwner, pData, XUI_DATE_PICKER_CLOSE_COMMIT);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( (pEvent->iKey == XUI_KEY_UP || pEvent->iKey == XUI_KEY_DOWN) && pData->iActiveTimeField >= 0 ) {
		iDelta = (pEvent->iKey == XUI_KEY_UP) ? 1 : -1;
		__xuiDatePickerSetDraftTime(pData, pData->iActiveTimePanel, pData->iActiveTimeField, iDelta);
		__xuiDatePickerNotifyDraft(pOwner, pData);
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiDatePickerPanelText(xui_widget pPanel, xui_widget pOwner, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	uint32_t iCodepoint;
	int iLimit;

	(void)pPanel;
	if ( (pOwner == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiDatePickerEditing(pData) || pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) return XUI_OK;
	iCodepoint = pEvent->iCodepoint;
	if ( iCodepoint == 0 && pEvent->iTextSize > 0 ) {
		iCodepoint = (uint32_t)(unsigned char)pEvent->sText[0];
	}
	iLimit = (pData->iEditKind == XUI_DATE_PICKER_EDIT_YEAR) ? XUI_DATE_PICKER_YEAR_INPUT_LIMIT : 2;
	if ( iCodepoint < '0' || iCodepoint > '9' ) return XUI_EVENT_DISPATCH_STOP;
	if ( pData->iEditSelectAll ) {
		pData->iEditLen = 0;
		pData->sEdit[0] = '\0';
		pData->iEditSelectAll = 0;
	}
	if ( pData->iEditLen < iLimit ) {
		pData->sEdit[pData->iEditLen++] = (char)iCodepoint;
		pData->sEdit[pData->iEditLen] = '\0';
		__xuiDatePickerClampTimeEditText(pData);
		pData->iEditError = 0;
		(void)__xuiDatePickerInvalidateAll(pOwner, pData);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiDatePickerPanelEvent(xui_widget pPanel, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pOwner;
	xui_date_picker_data_t* pData;

	pOwner = (xui_widget)pUser;
	if ( (pPanel == NULL) || (pEvent == NULL) || (pOwner == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiDatePickerGetData(pOwner);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		return __xuiDatePickerPanelPointerDown(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_POINTER_MOVE:
		return __xuiDatePickerPanelPointerMove(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiDatePickerPanelWheel(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActivePart = XUI_DATE_PICKER_PART_NONE;
		return xuiWidgetInvalidate(pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_KEY_DOWN:
		return __xuiDatePickerPanelKeyDown(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_TEXT:
		return __xuiDatePickerPanelText(pPanel, pOwner, pData, pEvent);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiDatePickerSyncState(pOwner, pData);
		return xuiWidgetInvalidate(pOwner, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		return XUI_OK;
	}
}

static int __xuiDatePickerPointerDown(xui_widget pWidget, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	if ( (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) return XUI_OK;
	if ( !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	if ( pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup) ) {
		(void)__xuiDatePickerCloseWithReason(pWidget, pData, XUI_DATE_PICKER_CLOSE_SILENT);
	} else {
		(void)__xuiDatePickerOpenInternal(pWidget, pData);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiDatePickerKeyDown(xui_widget pWidget, xui_date_picker_data_t* pData, const xui_event_t* pEvent)
{
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE || !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	if ( (pEvent->iKey == XUI_KEY_ENTER) || (pEvent->iKey == XUI_KEY_SPACE) ) {
		if ( pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup) ) {
			(void)__xuiDatePickerCloseWithReason(pWidget, pData, XUI_DATE_PICKER_CLOSE_SILENT);
		} else {
			(void)__xuiDatePickerOpenInternal(pWidget, pData);
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiDatePickerEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_date_picker_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		return __xuiDatePickerPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) return XUI_EVENT_DISPATCH_STOP;
		break;
	case XUI_EVENT_KEY_DOWN:
		return __xuiDatePickerKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiDatePickerSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_BOUNDS_CHANGED:
		if ( (pData->pPopup != NULL) && xuiPopupIsOpen(pData->pPopup) ) {
			(void)__xuiDatePickerApplyPopupStyle(pWidget, pData);
		}
		return XUI_OK;
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( ((pEvent->iType == XUI_EVENT_ENABLED_CHANGED) && !xuiWidgetGetEnabled(pWidget)) ||
		     ((pEvent->iType == XUI_EVENT_VISIBLE_CHANGED) && !xuiWidgetGetVisible(pWidget)) ) {
			(void)__xuiDatePickerCloseWithReason(pWidget, pData, XUI_DATE_PICKER_CLOSE_SILENT);
		}
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiDatePickerPopupChanged(xui_widget pPopup, int bOpen, void* pUser)
{
	xui_widget pOwner;
	xui_date_picker_data_t* pData;

	(void)pPopup;
	pOwner = (xui_widget)pUser;
	pData = __xuiDatePickerGetData(pOwner);
	if ( (pOwner == NULL) || (pData == NULL) ) return;
	if ( !bOpen && !pData->bClosingExplicit ) {
		(void)__xuiDatePickerCancelDraft(pOwner, pData);
	}
	(void)__xuiDatePickerSyncState(pOwner, pData);
	(void)__xuiDatePickerInvalidateAll(pOwner, pData);
}

static void __xuiDatePickerDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_ABSOLUTE;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiDatePickerDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiDatePickerContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget;
	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSize->fX = 180.0f;
	pSize->fY = 30.0f;
	return XUI_OK;
}

static int __xuiDatePickerInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetDefaultAction(pWidget, NULL, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiDatePickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiDatePickerEvent, NULL);
	return iRet;
}

static int __xuiDatePickerInitPanelEvents(xui_widget pPanel, xui_widget pOwner)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_DOWN, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_MOVE, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_UP, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_POINTER_WHEEL, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_KEY_DOWN, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_TEXT, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_FOCUS, __xuiDatePickerPanelEvent, pOwner);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pPanel, XUI_EVENT_BLUR, __xuiDatePickerPanelEvent, pOwner);
	return iRet;
}

static int __xuiDatePickerCreatePopup(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	xui_popup_desc_t tDesc;
	xui_cache_policy_t tPolicy;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tDesc.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tDesc.iOutsidePolicy = XUI_POPUP_OUTSIDE_CLOSE;
	tDesc.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tDesc.iEscapePolicy = XUI_POPUP_ESCAPE_CLOSE;
	tDesc.iFocusPolicy = XUI_POPUP_FOCUS_CUSTOM;
	tDesc.fContentWidth = pData->fPopupWidth;
	tDesc.fContentHeight = pData->fPopupHeight;
	tDesc.fGap = 2.0f;
	tDesc.fPadding = 0.0f;
	tDesc.fBorderWidth = 1.0f;
	tDesc.fShadowSize = 6.0f;
	tDesc.iPanelColor = pData->iPopupPanelColor;
	tDesc.iBorderColor = pData->iPopupBorderColor;
	tDesc.iShadowColor = pData->iPopupShadowColor;
	tDesc.iBackdropColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pData->pPopup, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	pData->pPanel = xuiPopupGetContentWidget(pData->pPopup);
	if ( pData->pPanel == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiDatePickerDefaultCachePolicy(&tPolicy);
	(void)xuiWidgetSetFocusable(pData->pPanel, 1);
	(void)xuiWidgetSetTabStop(pData->pPanel, 0);
	(void)xuiWidgetSetLayoutType(pData->pPanel, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pPanel, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pData->pPanel, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetCachePolicy(pData->pPanel, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pData->pPanel, __xuiDatePickerPanelRender, pWidget);
	iRet = __xuiDatePickerInitPanelEvents(pData->pPanel, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetChange(pData->pPopup, __xuiDatePickerPopupChanged, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetFocusPolicy(pData->pPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pPanel);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiDatePickerApplyPopupStyle(pWidget, pData);
}

static int __xuiDatePickerApplyPopupStyle(xui_widget pWidget, xui_date_picker_data_t* pData)
{
	xui_date_picker_data_t tResolved;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pPopup == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDatePickerResolve(pWidget, pData, &tResolved);
	iRet = xuiPopupSetOwner(pData->pPopup, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetFocusRestore(pData->pPopup, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetContentSize(pData->pPopup, pData->fPopupWidth, pData->fPopupHeight);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetColors(pData->pPopup, tResolved.iPopupPanelColor, tResolved.iPopupBorderColor, tResolved.iPopupShadowColor, XUI_COLOR_RGBA(0, 0, 0, 0));
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetMetrics(pData->pPopup, 0.0f, 1.0f, 6.0f);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetClosePolicy(pData->pPopup, XUI_POPUP_OUTSIDE_CLOSE, XUI_POPUP_OWNER_PASSTHROUGH, XUI_POPUP_ESCAPE_CLOSE);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetFocusPolicy(pData->pPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pPanel);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiPopupSetGap(pData->pPopup, 2.0f);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiPopupClearAnchorRect(pData->pPopup);
	if ( pData->iPopupPlacement == XUI_DATE_PICKER_POPUP_TOP ) {
		(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_TOP_LEFT);
		(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_UP);
	} else {
		(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_BOTTOM_LEFT);
		(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	}
	if ( pData->pPanel != NULL ) {
		(void)xuiWidgetSetRect(pData->pPanel, (xui_rect_t){0.0f, 0.0f, pData->fPopupWidth, pData->fPopupHeight});
	}
	return XUI_OK;
}

static int __xuiDatePickerInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_date_picker_data_t* pData;
	const xui_date_picker_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_date_picker_data_t*)pTypeData;
	pDesc = (const xui_date_picker_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiDatePickerDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDatePickerDefaults(pData);
	__xuiDatePickerApplyDesc(pData, pDesc);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiDatePickerInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiDatePickerCreatePopup(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)__xuiDatePickerSyncState(pWidget, pData);
	return XUI_OK;
}

static void __xuiDatePickerDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_date_picker_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_date_picker_data_t*)pTypeData;
	if ( pData != NULL ) {
		if ( pData->pPopup != NULL ) xuiWidgetDestroy(pData->pPopup);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiDatePickerRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiDatePickerRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.background.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.background.open_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.background.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.border.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.arrow.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.arrow.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "datepicker.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiDatePickerRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_date_picker_data_t* __xuiDatePickerGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "datepicker");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_date_picker_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiDatePickerGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "datepicker");
	if ( pType != NULL ) {
		__xuiDatePickerRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "datepicker";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_date_picker_data_t);
	tDesc.onInit = __xuiDatePickerInit;
	tDesc.onDestroy = __xuiDatePickerDestroy;
	tDesc.onContentMeasure = __xuiDatePickerContentMeasure;
	tDesc.onCacheRender = __xuiDatePickerCacheRender;
	__xuiDatePickerDefaultLayout(&tDesc.tLayout);
	__xuiDatePickerDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiDatePickerRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiDatePickerCreate(xui_context pContext, xui_widget* ppWidget, const xui_date_picker_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiDatePickerDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiDatePickerGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiDatePickerSetChanging(xui_widget pWidget, xui_date_picker_proc onChanging, void* pUser)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChanging = onChanging;
	pData->pChangingUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetChange(xui_widget pWidget, xui_date_picker_proc onChange, void* pUser)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetCommit(xui_widget pWidget, xui_date_picker_proc onCommit, void* pUser)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCommit = onCommit;
	pData->pCommitUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetCancel(xui_widget pWidget, xui_date_picker_proc onCancel, void* pUser)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCancel = onCancel;
	pData->pCancelUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetClear(xui_widget pWidget, xui_date_picker_proc onClear, void* pUser)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onClear = onClear;
	pData->pClearUser = pUser;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetMode(xui_widget pWidget, int iMode)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || !__xuiDatePickerModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iMode == iMode ) return XUI_OK;
	pData->iMode = iMode;
	__xuiDatePickerSyncDefaultFormat(pData);
	__xuiDatePickerUpdatePopupSizeForMode(pData);
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerInitViewMonth(pData);
	__xuiDatePickerSyncText(pData);
	if ( pData->pPopup != NULL ) (void)__xuiDatePickerApplyPopupStyle(pWidget, pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetMode(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iMode : XUI_DATE_PICKER_MODE_DATE;
}

XUI_API int xuiDatePickerSetNullable(xui_widget pWidget, int bNullable)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bNullable = bNullable ? 1 : 0;
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetNullable(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->bNullable : 0;
}

XUI_API int xuiDatePickerSetValue(xui_widget pWidget, xtime tValue)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tValue = __xuiDatePickerClampValue(pData, tValue);
	pData->tStart = pData->tValue;
	pData->tEnd = pData->tValue;
	pData->bHasValue = 1;
	__xuiDatePickerInitViewMonth(pData);
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API xtime xuiDatePickerGetValue(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return 0;
	__xuiDatePickerEnsureValue(pData);
	return pData->tValue;
}

XUI_API int xuiDatePickerHasValue(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->bHasValue : 0;
}

XUI_API int xuiDatePickerClearValue(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pData->bNullable ) {
		__xuiDatePickerEnsureValue(pData);
	} else {
		pData->bHasValue = 0;
	}
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerSetRangeValue(xui_widget pWidget, xtime tStart, xtime tEnd)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDatePickerNormalizeRange(pData, &tStart, &tEnd);
	pData->tStart = tStart;
	pData->tEnd = tEnd;
	pData->tValue = tStart;
	pData->bHasValue = 1;
	__xuiDatePickerInitViewMonth(pData);
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetRangeValue(xui_widget pWidget, xtime* pStart, xtime* pEnd)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDatePickerEnsureValue(pData);
	if ( pStart != NULL ) *pStart = pData->tStart;
	if ( pEnd != NULL ) *pEnd = pData->tEnd;
	return XUI_OK;
}

XUI_API int xuiDatePickerHasRangeValue(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? (pData->bHasValue && __xuiDatePickerIsRange(pData->iMode)) : 0;
}

XUI_API int xuiDatePickerSetLimits(xui_widget pWidget, xtime tMin, xtime tMax)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( tMax < tMin ) {
		xtime tSwap = tMin;
		tMin = tMax;
		tMax = tSwap;
	}
	pData->tMin = tMin;
	pData->tMax = tMax;
	pData->bHasMin = 1;
	pData->bHasMax = 1;
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerSetLimitRange(xui_widget pWidget, int bHasMin, xtime tMin, int bHasMax, xtime tMax)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bHasMin && bHasMax && (tMax < tMin) ) {
		xtime tSwap = tMin;
		tMin = tMax;
		tMax = tSwap;
	}
	pData->tMin = tMin;
	pData->tMax = tMax;
	pData->bHasMin = bHasMin ? 1 : 0;
	pData->bHasMax = bHasMax ? 1 : 0;
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetLimitRange(xui_widget pWidget, int* pHasMin, xtime* pMin, int* pHasMax, xtime* pMax)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pHasMin != NULL ) *pHasMin = pData->bHasMin;
	if ( pMin != NULL ) *pMin = pData->tMin;
	if ( pHasMax != NULL ) *pHasMax = pData->bHasMax;
	if ( pMax != NULL ) *pMax = pData->tMax;
	return XUI_OK;
}

XUI_API int xuiDatePickerClearLimits(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bHasMin = 0;
	pData->bHasMax = 0;
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerSetFormat(xui_widget pWidget, const char* sFormat)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sFormat == NULL || sFormat[0] == '\0' ) {
		pData->bFormatCustom = 0;
		__xuiDatePickerSyncDefaultFormat(pData);
	} else {
		pData->bFormatCustom = 1;
		__xuiDatePickerCopyText(pData->sFormat, (int)sizeof(pData->sFormat), sFormat);
	}
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API const char* xuiDatePickerGetFormat(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->sFormat : "";
}

XUI_API int xuiDatePickerSetRangeSeparator(xui_widget pWidget, const char* sSeparator)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiDatePickerCopyText(pData->sRangeSeparator, (int)sizeof(pData->sRangeSeparator), (sSeparator != NULL) ? sSeparator : " - ");
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API const char* xuiDatePickerGetRangeSeparator(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->sRangeSeparator : "";
}

XUI_API int xuiDatePickerSetShowSecond(xui_widget pWidget, int bShowSecond)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowSecond = bShowSecond ? 1 : 0;
	__xuiDatePickerSyncDefaultFormat(pData);
	__xuiDatePickerEnsureValue(pData);
	__xuiDatePickerSyncText(pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetShowSecond(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->bShowSecond : 0;
}

XUI_API int xuiDatePickerSetFirstDayOfWeek(xui_widget pWidget, int iFirstDayOfWeek)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iFirstDayOfWeek < 0) || (iFirstDayOfWeek > 6) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iFirstDayOfWeek = iFirstDayOfWeek;
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetFirstDayOfWeek(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iFirstDayOfWeek : 0;
}

XUI_API int xuiDatePickerSetDefaultRangeSpan(xui_widget pWidget, xtime tSpan)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (tSpan < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tDefaultRangeSpan = tSpan;
	return XUI_OK;
}

XUI_API xtime xuiDatePickerGetDefaultRangeSpan(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->tDefaultRangeSpan : 0;
}

XUI_API int xuiDatePickerOpen(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? __xuiDatePickerOpenInternal(pWidget, pData) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiDatePickerClose(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? __xuiDatePickerCloseWithReason(pWidget, pData, XUI_DATE_PICKER_CLOSE_SILENT) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiDatePickerToggle(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return (pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup)) ? xuiDatePickerClose(pWidget) : xuiDatePickerOpen(pWidget);
}

XUI_API int xuiDatePickerIsOpen(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL && pData->pPopup != NULL) ? xuiPopupIsOpen(pData->pPopup) : 0;
}

XUI_API int xuiDatePickerSetPopupSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (fWidth <= 0.0f) || (fHeight <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fPopupWidth = fWidth;
	pData->fPopupHeight = fHeight;
	pData->bPopupSizeCustom = 1;
	if ( pData->pPopup != NULL ) (void)__xuiDatePickerApplyPopupStyle(pWidget, pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetPopupSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pWidth != NULL ) *pWidth = pData->fPopupWidth;
	if ( pHeight != NULL ) *pHeight = pData->fPopupHeight;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetPopupPlacement(xui_widget pWidget, int iPlacement)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || !__xuiDatePickerPlacementValid(iPlacement) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPlacement = iPlacement;
	if ( pData->pPopup != NULL ) return __xuiDatePickerApplyPopupStyle(pWidget, pData);
	return XUI_OK;
}

XUI_API int xuiDatePickerGetPopupPlacement(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iPopupPlacement : XUI_DATE_PICKER_POPUP_AUTO;
}

XUI_API int xuiDatePickerSetMetrics(xui_widget pWidget, float fBorderWidth)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (fBorderWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiDatePickerGetMetrics(xui_widget pWidget, float* pBorderWidth)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetColors(xui_widget pWidget, uint32_t iText, uint32_t iDisabledText, uint32_t iBackground, uint32_t iHoverBackground, uint32_t iOpenBackground, uint32_t iDisabledBackground)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iText;
	pData->iDisabledTextColor = iDisabledText;
	pData->iBackgroundColor = iBackground;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iOpenBackgroundColor = iOpenBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetColors(xui_widget pWidget, uint32_t* pText, uint32_t* pDisabledText, uint32_t* pBackground, uint32_t* pHoverBackground, uint32_t* pOpenBackground, uint32_t* pDisabledBackground)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pOpenBackground != NULL ) *pOpenBackground = pData->iOpenBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetBorderColors(xui_widget pWidget, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iFocusBorder)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBorderColor = iBorder;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iFocusBorderColor = iFocusBorder;
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetBorderColors(xui_widget pWidget, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pFocusBorder)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pFocusBorder != NULL ) *pFocusBorder = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetArrowColors(xui_widget pWidget, uint32_t iArrow, uint32_t iDisabledArrow)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iArrowColor = iArrow;
	pData->iDisabledArrowColor = iDisabledArrow;
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetArrowColors(xui_widget pWidget, uint32_t* pArrow, uint32_t* pDisabledArrow)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pArrow != NULL ) *pArrow = pData->iArrowColor;
	if ( pDisabledArrow != NULL ) *pDisabledArrow = pData->iDisabledArrowColor;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetPopupColors(xui_widget pWidget, uint32_t iPanel, uint32_t iBorder, uint32_t iShadow, uint32_t iText, uint32_t iMutedText, uint32_t iAccent, uint32_t iField, uint32_t iFieldBorder, uint32_t iSelectedText, uint32_t iDisabledDay, uint32_t iSeparator)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPanelColor = iPanel;
	pData->iPopupBorderColor = iBorder;
	pData->iPopupShadowColor = iShadow;
	pData->iPopupTextColor = iText;
	pData->iPopupMutedTextColor = iMutedText;
	pData->iAccentColor = iAccent;
	pData->iFieldColor = iField;
	pData->iFieldBorderColor = iFieldBorder;
	pData->iSelectedTextColor = iSelectedText;
	pData->iDisabledDayColor = iDisabledDay;
	pData->iSeparatorColor = iSeparator;
	if ( pData->pPopup != NULL ) (void)__xuiDatePickerApplyPopupStyle(pWidget, pData);
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API int xuiDatePickerGetPopupColors(xui_widget pWidget, uint32_t* pPanel, uint32_t* pBorder, uint32_t* pShadow, uint32_t* pText, uint32_t* pMutedText, uint32_t* pAccent, uint32_t* pField, uint32_t* pFieldBorder, uint32_t* pSelectedText, uint32_t* pDisabledDay, uint32_t* pSeparator)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPanel != NULL ) *pPanel = pData->iPopupPanelColor;
	if ( pBorder != NULL ) *pBorder = pData->iPopupBorderColor;
	if ( pShadow != NULL ) *pShadow = pData->iPopupShadowColor;
	if ( pText != NULL ) *pText = pData->iPopupTextColor;
	if ( pMutedText != NULL ) *pMutedText = pData->iPopupMutedTextColor;
	if ( pAccent != NULL ) *pAccent = pData->iAccentColor;
	if ( pField != NULL ) *pField = pData->iFieldColor;
	if ( pFieldBorder != NULL ) *pFieldBorder = pData->iFieldBorderColor;
	if ( pSelectedText != NULL ) *pSelectedText = pData->iSelectedTextColor;
	if ( pDisabledDay != NULL ) *pDisabledDay = pData->iDisabledDayColor;
	if ( pSeparator != NULL ) *pSeparator = pData->iSeparatorColor;
	return XUI_OK;
}

XUI_API int xuiDatePickerSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiDatePickerInvalidateAll(pWidget, pData);
}

XUI_API xui_font xuiDatePickerGetFont(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API const char* xuiDatePickerGetText(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->sText : "";
}

XUI_API xui_widget xuiDatePickerGetPopupWidget(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->pPopup : NULL;
}

XUI_API xui_widget xuiDatePickerGetPanelWidget(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->pPanel : NULL;
}

XUI_API xui_rect_t xuiDatePickerGetButtonRect(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0};
	__xuiDatePickerUpdateOwnerRects(pWidget, pData);
	return pData->tButtonRect;
}

XUI_API xui_rect_t xuiDatePickerGetTextRect(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0};
	__xuiDatePickerUpdateOwnerRects(pWidget, pData);
	return pData->tTextRect;
}

XUI_API xui_rect_t xuiDatePickerGetCalendarPanelRect(xui_widget pWidget, int iPanel)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->tCalendarRect[iPanel];
}

XUI_API xui_rect_t xuiDatePickerGetTimePanelRect(xui_widget pWidget, int iPanel)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->tTimePanelRect[iPanel];
}

XUI_API xui_rect_t xuiDatePickerGetPrevRect(xui_widget pWidget, int iPanel)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->tPrevRect[iPanel];
}

XUI_API xui_rect_t xuiDatePickerGetNextRect(xui_widget pWidget, int iPanel)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->tNextRect[iPanel];
}

XUI_API xui_rect_t xuiDatePickerGetDayRect(xui_widget pWidget, int iPanel, int iIndex)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iIndex < 0) || (iIndex >= XUI_DATE_PICKER_CELL_COUNT) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->arrDayRect[iPanel][iIndex];
}

XUI_API xtime xuiDatePickerGetDayValue(xui_widget pWidget, int iPanel, int iIndex)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iIndex < 0) || (iIndex >= XUI_DATE_PICKER_CELL_COUNT) ) return 0;
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->arrDayValue[iPanel][iIndex];
}

XUI_API xui_rect_t xuiDatePickerGetTimeRect(xui_widget pWidget, int iPanel, int iField)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iPanel < 0) || (iPanel >= XUI_DATE_PICKER_PANEL_CAPACITY) || (iField < 0) || (iField > 2) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->arrTimeRect[iPanel][iField];
}

XUI_API xui_rect_t xuiDatePickerGetFooterRect(xui_widget pWidget, int iIndex)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= 4) ) return (xui_rect_t){0};
	__xuiDatePickerUpdatePanelRects(pData);
	return pData->arrFooterRect[iIndex];
}

XUI_API int xuiDatePickerGetHoverPart(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iHoverPart : XUI_DATE_PICKER_PART_NONE;
}

XUI_API int xuiDatePickerGetActivePart(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iActivePart : XUI_DATE_PICKER_PART_NONE;
}

XUI_API uint32_t xuiDatePickerGetState(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return __xuiDatePickerState(pWidget, pData);
}

XUI_API int xuiDatePickerGetChangingCount(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iChangingCount : 0;
}

XUI_API int xuiDatePickerGetChangeCount(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API int xuiDatePickerGetCommitCount(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iCommitCount : 0;
}

XUI_API int xuiDatePickerGetCancelCount(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iCancelCount : 0;
}

XUI_API int xuiDatePickerGetClearCount(xui_widget pWidget)
{
	xui_date_picker_data_t* pData = __xuiDatePickerGetData(pWidget);
	return (pData != NULL) ? pData->iClearCount : 0;
}
