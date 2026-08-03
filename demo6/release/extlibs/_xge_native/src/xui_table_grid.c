#include "xui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_TABLE_GRID_EDITOR_NONE	0
#define XUI_TABLE_GRID_EDITOR_INPUT	1
#define XUI_TABLE_GRID_EDITOR_NUMERIC	2
#define XUI_TABLE_GRID_EDITOR_COMBO	3
#define XUI_TABLE_GRID_EDITOR_COLOR	4
#define XUI_TABLE_GRID_EDITOR_DATE	5
#define XUI_TABLE_GRID_EDITOR_TEXTAREA	6

typedef struct xui_table_grid_data_t {
	xui_widget pTable;
	xui_widget pInput;
	xui_widget pNumeric;
	xui_widget pCombo;
	xui_widget pColor;
	xui_widget pDate;
	xui_widget pTextPopup;
	xui_widget pTextContent;
	xui_widget pTextEdit;
	xui_widget pTextOk;
	xui_widget pTextCancel;
	xui_font pFont;
	xui_table_view_count_proc onCount;
	xui_table_view_cell_proc onCell;
	xui_table_grid_set_proc onSet;
	void* pAdapterUser;
	void* pSetUser;
	xui_table_grid_validate_proc onValidate;
	void* pValidateUser;
	xui_table_grid_change_proc onChange;
	void* pChangeUser;
	xui_table_grid_editor_proc onEditor;
	void* pEditorUser;
	xui_table_grid_editor_config_proc onEditorConfig;
	void* pEditorConfigUser;
	xui_table_grid_editor_config_t tEditorConfig;
	int iEditMode;
	int iEditingRow;
	int iEditingColumn;
	int iEditingType;
	int iActiveEditor;
	int iCommitCount;
	int iRejectCount;
	int iCancelCount;
	int iPickerCount;
	int iChangeCount;
	char sOriginalValue[XUI_TABLE_GRID_VALUE_CAPACITY];
} xui_table_grid_data_t;

static xui_table_grid_data_t* __xuiTableGridGetData(xui_widget pWidget);

static int __xuiTableGridTextEqualNoCase(const char* sA, const char* sB)
{
	unsigned char a;
	unsigned char b;

	if ( sA == NULL || sB == NULL ) {
		return sA == sB;
	}
	while ( (*sA != '\0') && (*sB != '\0') ) {
		a = (unsigned char)*sA++;
		b = (unsigned char)*sB++;
		if ( a >= 'A' && a <= 'Z' ) a = (unsigned char)(a - 'A' + 'a');
		if ( b >= 'A' && b <= 'Z' ) b = (unsigned char)(b - 'A' + 'a');
		if ( a != b ) return 0;
	}
	return *sA == *sB;
}

static int __xuiTableGridDescValid(const xui_table_grid_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iColumnCount < 0) || (pDesc->iRowCount < 0) ) {
		return 0;
	}
	return 1;
}

static int __xuiTableGridEditModeValid(int iMode)
{
	return (iMode == XUI_TABLE_GRID_EDIT_DISPLAY) ||
	       (iMode == XUI_TABLE_GRID_EDIT_QUICK) ||
	       (iMode == XUI_TABLE_GRID_EDIT_IMMEDIATE);
}

static int __xuiTableGridIsNumericType(int iType)
{
	return (iType == XUI_TABLE_CELL_TYPE_INT) || (iType == XUI_TABLE_CELL_TYPE_FLOAT);
}

static int __xuiTableGridIsPickerType(int iType)
{
	return (iType == XUI_TABLE_CELL_TYPE_PICKER) ||
	       (iType == XUI_TABLE_CELL_TYPE_FILE) ||
	       (iType == XUI_TABLE_CELL_TYPE_IMAGE);
}

static int __xuiTableGridIsDateType(int iType)
{
	return (iType == XUI_TABLE_CELL_TYPE_DATE) ||
	       (iType == XUI_TABLE_CELL_TYPE_TIME) ||
	       (iType == XUI_TABLE_CELL_TYPE_DATETIME);
}

static int __xuiTableGridBoolValue(const char* sText, const void* pValue)
{
	if ( pValue != NULL ) {
		return ((intptr_t)pValue) != 0;
	}
	if ( sText == NULL ) {
		return 0;
	}
	return (strcmp(sText, "1") == 0) ||
	       __xuiTableGridTextEqualNoCase(sText, "true") ||
	       __xuiTableGridTextEqualNoCase(sText, "yes") ||
	       __xuiTableGridTextEqualNoCase(sText, "on");
}

static int __xuiTableGridValidateNumberText(const char* sText, int iType)
{
	int i;
	int iDot;

	if ( sText == NULL || sText[0] == '\0' ) {
		return 1;
	}
	i = 0;
	if ( sText[i] == '-' || sText[i] == '+' ) {
		i++;
	}
	iDot = 0;
	for ( ; sText[i] != '\0'; i++ ) {
		if ( sText[i] >= '0' && sText[i] <= '9' ) {
			continue;
		}
		if ( (iType == XUI_TABLE_CELL_TYPE_FLOAT) && (sText[i] == '.') && (iDot == 0) ) {
			iDot = 1;
			continue;
		}
		return 0;
	}
	return 1;
}

static int __xuiTableGridInferPrecision(const char* sText, int iType)
{
	const char* pDot;
	int iPrecision;

	if ( iType == XUI_TABLE_CELL_TYPE_INT ) {
		return 0;
	}
	if ( sText == NULL ) {
		return 3;
	}
	pDot = strchr(sText, '.');
	if ( pDot == NULL ) {
		return 3;
	}
	iPrecision = 0;
	pDot++;
	while ( (*pDot >= '0') && (*pDot <= '9') && (iPrecision < 6) ) {
		iPrecision++;
		pDot++;
	}
	return iPrecision;
}

static float __xuiTableGridStepForPrecision(int iPrecision)
{
	float fStep;
	int i;

	if ( iPrecision <= 0 ) {
		return 1.0f;
	}
	fStep = 1.0f;
	for ( i = 0; i < iPrecision; i++ ) {
		fStep *= 0.1f;
	}
	return fStep;
}

static int __xuiTableGridDateModeForType(int iType)
{
	if ( iType == XUI_TABLE_CELL_TYPE_TIME ) {
		return XUI_DATE_PICKER_MODE_TIME;
	}
	if ( iType == XUI_TABLE_CELL_TYPE_DATETIME ) {
		return XUI_DATE_PICKER_MODE_DATETIME;
	}
	return XUI_DATE_PICKER_MODE_DATE;
}

static const char* __xuiTableGridDefaultDateFormat(int iMode, int bShowSecond)
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

static xui_widget __xuiTableGridActiveEditor(const xui_table_grid_data_t* pData)
{
	if ( pData == NULL ) {
		return NULL;
	}
	switch ( pData->iActiveEditor ) {
	case XUI_TABLE_GRID_EDITOR_INPUT: return pData->pInput;
	case XUI_TABLE_GRID_EDITOR_NUMERIC: return pData->pNumeric;
	case XUI_TABLE_GRID_EDITOR_COMBO: return pData->pCombo;
	case XUI_TABLE_GRID_EDITOR_COLOR: return pData->pColor;
	case XUI_TABLE_GRID_EDITOR_DATE: return pData->pDate;
	default: return NULL;
	}
}

static int __xuiTableGridCellRectWorld(xui_widget pWidget, xui_table_grid_data_t* pData, int iRow, int iColumn, xui_rect_t* pRect)
{
	xui_rect_t tTable;
	xui_rect_t tCell;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pTable == NULL) || (pRect == NULL) ) {
		return 0;
	}
	if ( xuiTableViewGetCellRect(pData->pTable, iRow, iColumn, &tCell) != XUI_OK ) {
		return 0;
	}
	tTable = xuiWidgetGetWorldRect(pData->pTable);
	tCell.fX += tTable.fX;
	tCell.fY += tTable.fY;
	*pRect = tCell;
	(void)pWidget;
	return 1;
}

static int __xuiTableGridClipEditorRect(xui_table_grid_data_t* pData, xui_rect_t* pRect)
{
	xui_rect_t tView;
	float fRight;
	float fBottom;
	float fViewRight;
	float fViewBottom;

	if ( (pData == NULL) || (pData->pTable == NULL) || (pRect == NULL) ) {
		return 0;
	}
	tView = xuiTableViewGetViewportRect(pData->pTable);
	tView.fX -= 1.0f;
	tView.fY -= 1.0f;
	tView.fW += 2.0f;
	tView.fH += 2.0f;
	fRight = pRect->fX + pRect->fW;
	fBottom = pRect->fY + pRect->fH;
	fViewRight = tView.fX + tView.fW;
	fViewBottom = tView.fY + tView.fH;
	if ( pRect->fX < tView.fX ) {
		pRect->fW -= tView.fX - pRect->fX;
		pRect->fX = tView.fX;
	}
	if ( pRect->fY < tView.fY ) {
		pRect->fH -= tView.fY - pRect->fY;
		pRect->fY = tView.fY;
	}
	if ( fRight > fViewRight ) {
		pRect->fW -= fRight - fViewRight;
	}
	if ( fBottom > fViewBottom ) {
		pRect->fH -= fBottom - fViewBottom;
	}
	if ( pRect->fW < 0.0f ) pRect->fW = 0.0f;
	if ( pRect->fH < 0.0f ) pRect->fH = 0.0f;
	return (pRect->fW > 4.0f) && (pRect->fH > 4.0f);
}

static int __xuiTableGridEditingRect(xui_widget pWidget, xui_table_grid_data_t* pData, xui_rect_t* pRect)
{
	xui_rect_t tTable;
	xui_rect_t tCell;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pTable == NULL) || (pRect == NULL) ||
	     (pData->iEditingRow < 0) || (pData->iEditingColumn < 0) ) {
		return 0;
	}
	if ( xuiTableViewGetCellRect(pData->pTable, pData->iEditingRow, pData->iEditingColumn, &tCell) != XUI_OK ) {
		return 0;
	}
	tCell.fX -= 1.0f;
	tCell.fY -= 1.0f;
	tCell.fW += 2.0f;
	tCell.fH += 2.0f;
	if ( !__xuiTableGridClipEditorRect(pData, &tCell) ) {
		return 0;
	}
	tTable = xuiWidgetGetRect(pData->pTable);
	tCell.fX += tTable.fX;
	tCell.fY += tTable.fY;
	*pRect = tCell;
	return 1;
}

static int __xuiTableGridPlaceEditor(xui_widget pWidget, xui_table_grid_data_t* pData, xui_widget pEditor, xui_rect_t tRect)
{
	(void)pWidget;
	(void)pData;
	if ( pEditor == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tRect = xuiInternalSnapRect(tRect);
	(void)xuiWidgetSetRect(pEditor, tRect);
	(void)xuiWidgetSetVisible(pEditor, 1);
	(void)xuiWidgetArrange(pEditor, tRect);
	return xuiWidgetInvalidate(pEditor, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableGridSetEditorLayerTree(xui_widget pWidget, int iLayer, int iZIndex)
{
	xui_widget pChild;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetSetLayer(pWidget, iLayer, iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( pChild = xuiWidgetGetFirstChild(pWidget); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		iRet = __xuiTableGridSetEditorLayerTree(pChild, iLayer, iZIndex);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiTableGridHideEditors(xui_table_grid_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	if ( pData->pInput != NULL ) (void)xuiWidgetSetVisible(pData->pInput, 0);
	if ( pData->pNumeric != NULL ) (void)xuiWidgetSetVisible(pData->pNumeric, 0);
	if ( pData->pCombo != NULL ) {
		(void)xuiComboBoxClose(pData->pCombo);
		(void)xuiWidgetSetVisible(pData->pCombo, 0);
	}
	if ( pData->pColor != NULL ) {
		(void)xuiColorPickerClose(pData->pColor);
		(void)xuiWidgetSetVisible(pData->pColor, 0);
	}
	if ( pData->pDate != NULL ) {
		(void)xuiDatePickerClose(pData->pDate);
		(void)xuiWidgetSetVisible(pData->pDate, 0);
	}
	if ( pData->pTextPopup != NULL ) {
		(void)xuiPopupSetOpen(pData->pTextPopup, 0);
	}
}

static int __xuiTableGridEditorPopupOpen(xui_table_grid_data_t* pData)
{
	if ( pData == NULL ) return 0;
	if ( (pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_COMBO) && xuiComboBoxIsOpen(pData->pCombo) ) return 1;
	if ( (pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_COLOR) && xuiColorPickerIsOpen(pData->pColor) ) return 1;
	if ( (pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_DATE) && xuiDatePickerIsOpen(pData->pDate) ) return 1;
	if ( (pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_TEXTAREA) && xuiPopupIsOpen(pData->pTextPopup) ) return 1;
	return 0;
}

static int __xuiTableGridPointInWidget(xui_widget pWidget, float fX, float fY)
{
	xui_rect_t tRect;

	if ( pWidget == NULL || !xuiWidgetGetVisible(pWidget) ) {
		return 0;
	}
	tRect = xuiWidgetGetWorldRect(pWidget);
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static int __xuiTableGridWidgetContains(xui_widget pAncestor, xui_widget pWidget)
{
	xui_widget pScan;

	if ( pAncestor == NULL || pWidget == NULL ) {
		return 0;
	}
	for ( pScan = pWidget; pScan != NULL; pScan = xuiWidgetGetParent(pScan) ) {
		if ( pScan == pAncestor ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiTableGridEventInActiveEditor(xui_widget pWidget, xui_table_grid_data_t* pData, const xui_event_t* pEvent)
{
	xui_widget pEditor;
	xui_widget pCapture;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) || (pData->iEditingRow < 0) ) {
		return 0;
	}
	pEditor = __xuiTableGridActiveEditor(pData);
	if ( pEditor == NULL ) {
		return 0;
	}
	if ( __xuiTableGridPointInWidget(pEditor, pEvent->fX, pEvent->fY) ) {
		return 1;
	}
	/* Text selection may release outside the editor while the input still owns pointer capture. */
	pCapture = xuiGetPointerCapture(xuiWidgetGetContext(pWidget));
	return __xuiTableGridWidgetContains(pEditor, pCapture);
}

static void __xuiTableGridDefaultCell(xui_table_grid_data_t* pData, int iColumn, xui_table_view_cell_t* pCell)
{
	const xui_table_view_column_t* pColumn;

	if ( pCell == NULL ) return;
	memset(pCell, 0, sizeof(*pCell));
	pCell->iRowSpan = 1;
	pCell->iColSpan = 1;
	pCell->iType = XUI_TABLE_CELL_TYPE_TEXT;
	if ( (pData != NULL) && (pData->pTable != NULL) && (iColumn >= 0) ) {
		pColumn = xuiTableViewGetColumn(pData->pTable, iColumn);
		if ( pColumn != NULL ) {
			pCell->iType = pColumn->iType;
		}
	}
}

static void __xuiTableGridGetCell(xui_widget pWidget, xui_table_grid_data_t* pData, int iRow, int iColumn, xui_table_view_cell_t* pCell)
{
	__xuiTableGridDefaultCell(pData, iColumn, pCell);
	if ( (pData == NULL) || (pCell == NULL) || (iRow < 0) || (iColumn < 0) ) {
		return;
	}
	if ( pData->onCell != NULL ) {
		(void)pData->onCell(pWidget, iRow, iColumn, pCell, pData->pAdapterUser);
	}
	if ( pCell->iRowSpan <= 0 ) pCell->iRowSpan = 1;
	if ( pCell->iColSpan <= 0 ) pCell->iColSpan = 1;
}

static const char* __xuiTableGridCellText(xui_widget pWidget, xui_table_grid_data_t* pData, int iRow, int iColumn, const xui_table_view_cell_t* pCell, char* sBuffer, int iCapacity)
{
	xui_table_view_format_proc onFormat;
	void* pUser;
	const xui_table_view_column_t* pColumn;
	int iValue;

	if ( (sBuffer == NULL) || (iCapacity <= 0) ) return "";
	sBuffer[0] = '\0';
	if ( pCell == NULL ) return sBuffer;
	if ( pCell->sText != NULL ) return pCell->sText;
	onFormat = pCell->onFormat;
	pUser = pCell->pFormatUser;
	if ( onFormat == NULL && pData != NULL && pData->pTable != NULL ) {
		pColumn = xuiTableViewGetColumn(pData->pTable, iColumn);
		if ( pColumn != NULL ) {
			onFormat = pColumn->onFormat;
			pUser = pColumn->pFormatUser;
		}
	}
	if ( onFormat != NULL && onFormat(pWidget, iRow, iColumn, pCell, sBuffer, iCapacity, pUser) != 0 ) {
		sBuffer[iCapacity - 1] = '\0';
		return sBuffer;
	}
	if ( pCell->pValue != NULL ) {
		iValue = (int)(intptr_t)pCell->pValue;
		if ( pCell->iType == XUI_TABLE_CELL_TYPE_BOOL ) {
			snprintf(sBuffer, (size_t)iCapacity, "%s", iValue ? "true" : "false");
		} else {
			snprintf(sBuffer, (size_t)iCapacity, "%d", iValue);
		}
		sBuffer[iCapacity - 1] = '\0';
	}
	return sBuffer;
}

static void __xuiTableGridDefaultEditorConfig(xui_table_grid_editor_config_t* pConfig, int iType)
{
	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iEnumSelected = -1;
	pConfig->iDateMode = __xuiTableGridDateModeForType(iType);
	pConfig->bShowSecond = 1;
	pConfig->sRangeSeparator = " - ";
}

static int __xuiTableGridLoadEditorConfig(xui_widget pWidget, xui_table_grid_data_t* pData, int iRow, int iColumn, int iType)
{
	if ( pData == NULL ) return 0;
	__xuiTableGridDefaultEditorConfig(&pData->tEditorConfig, iType);
	if ( pData->onEditorConfig == NULL ) return 0;
	return pData->onEditorConfig(pWidget, iRow, iColumn, iType, &pData->tEditorConfig, pData->pEditorConfigUser);
}

static xtime __xuiTableGridParseDateValue(const char* sText, int iType, const char* sFormat)
{
	xtime tValue;
	int iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;

	if ( sText == NULL || sText[0] == '\0' ) {
		return xrtNow();
	}
	if ( sFormat != NULL && sFormat[0] != '\0' ) {
		tValue = xrtTimeParse((str)sText, (str)sFormat);
		if ( tValue > 0 ) return tValue;
	}
	iYear = 0;
	iMonth = 0;
	iDay = 0;
	iHour = 0;
	iMinute = 0;
	iSecond = 0;
	if ( iType == XUI_TABLE_CELL_TYPE_TIME ) {
		if ( sscanf(sText, "%d:%d:%d", &iHour, &iMinute, &iSecond) >= 2 ) {
			return (xtime)iHour * XRT_TIME_HOUR + (xtime)iMinute * XRT_TIME_MINUTE + (xtime)iSecond;
		}
		return xrtNow();
	}
	if ( iType == XUI_TABLE_CELL_TYPE_DATETIME ) {
		if ( sscanf(sText, "%d-%d-%d %d:%d:%d", &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond) >= 5 ) {
			return xrtDateSerial(iYear, iMonth, iDay) + (xtime)iHour * XRT_TIME_HOUR + (xtime)iMinute * XRT_TIME_MINUTE + (xtime)iSecond;
		}
	}
	if ( sscanf(sText, "%d-%d-%d", &iYear, &iMonth, &iDay) == 3 ) {
		return xrtDateSerial(iYear, iMonth, iDay);
	}
	return xrtNow();
}

static const char* __xuiTableGridCurrentEditorValue(xui_table_grid_data_t* pData, char* sBuffer, int iCapacity)
{
	const char* sText;
	const xui_combobox_item_t* pItem;
	int iIndex;

	if ( (sBuffer == NULL) || (iCapacity <= 0) ) return "";
	sBuffer[0] = '\0';
	if ( pData == NULL ) return sBuffer;
	if ( pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_TEXTAREA ) {
		sText = xuiTextEditGetText(pData->pTextEdit);
		return (sText != NULL) ? sText : "";
	}
	switch ( pData->iActiveEditor ) {
	case XUI_TABLE_GRID_EDITOR_COMBO:
		iIndex = xuiComboBoxGetSelected(pData->pCombo);
		if ( pData->tEditorConfig.bEnumUseValue ) {
			snprintf(sBuffer, (size_t)iCapacity, "%d", xuiComboBoxGetSelectedValue(pData->pCombo));
		} else {
			pItem = xuiComboBoxGetItem(pData->pCombo, iIndex);
			snprintf(sBuffer, (size_t)iCapacity, "%s", (pItem != NULL && pItem->sText != NULL) ? pItem->sText : "");
		}
		sBuffer[iCapacity - 1] = '\0';
		return sBuffer;
	case XUI_TABLE_GRID_EDITOR_COLOR:
		sText = xuiColorPickerGetHex(pData->pColor);
		return (sText != NULL) ? sText : "";
	case XUI_TABLE_GRID_EDITOR_DATE:
		sText = xuiDatePickerGetText(pData->pDate);
		return (sText != NULL) ? sText : "";
	case XUI_TABLE_GRID_EDITOR_NUMERIC:
		(void)xuiNumericInputCommit(pData->pNumeric);
		sText = xuiNumericInputGetText(pData->pNumeric);
		return (sText != NULL) ? sText : "";
	case XUI_TABLE_GRID_EDITOR_INPUT:
	default:
		sText = xuiInputGetText(pData->pInput);
		return (sText != NULL) ? sText : "";
	}
}

static int __xuiTableGridCommitValue(xui_widget pWidget, xui_table_grid_data_t* pData, const char* sValue)
{
	xui_widget pInput;

	if ( pData == NULL ) return 0;
	if ( sValue == NULL ) sValue = "";
	if ( pData->onValidate != NULL ) {
		if ( pData->onValidate(pWidget, pData->iEditingRow, pData->iEditingColumn, sValue, pData->iEditingType, pData->pValidateUser) == 0 ) {
			pData->iRejectCount++;
			if ( pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_INPUT && pData->pInput != NULL ) {
				(void)xuiInputSetError(pData->pInput, 1);
			} else if ( pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_NUMERIC && pData->pNumeric != NULL ) {
				pInput = xuiNumericInputGetInputWidget(pData->pNumeric);
				if ( pInput != NULL ) (void)xuiInputSetError(pInput, 1);
			}
			return 0;
		}
	}
	if ( strcmp(pData->sOriginalValue, sValue) != 0 ) {
		if ( pData->onSet != NULL ) {
			pData->onSet(pWidget, pData->iEditingRow, pData->iEditingColumn, sValue, pData->iEditingType, pData->pSetUser);
		}
		if ( pData->onChange != NULL ) {
			pData->onChange(pWidget, pData->iEditingRow, pData->iEditingColumn, sValue, pData->iEditingType, pData->pChangeUser);
		}
		pData->iCommitCount++;
		pData->iChangeCount++;
	}
	return 1;
}

static int __xuiTableGridSyncActiveEditorRect(xui_widget pWidget, xui_table_grid_data_t* pData)
{
	xui_widget pEditor;
	xui_rect_t tRect;

	if ( (pData == NULL) || (pData->iEditingRow < 0) || (pData->iEditingColumn < 0) ) {
		return 0;
	}
	pEditor = __xuiTableGridActiveEditor(pData);
	if ( pEditor == NULL ) {
		if ( (pData->iActiveEditor == XUI_TABLE_GRID_EDITOR_TEXTAREA) &&
		     (pData->pTextPopup != NULL) &&
		     xuiPopupIsOpen(pData->pTextPopup) &&
		     __xuiTableGridCellRectWorld(pWidget, pData, pData->iEditingRow, pData->iEditingColumn, &tRect) ) {
			(void)xuiPopupSetAnchorRect(pData->pTextPopup, tRect);
		}
		return 0;
	}
	if ( !__xuiTableGridEditingRect(pWidget, pData, &tRect) ) {
		(void)xuiWidgetSetVisible(pEditor, 0);
		return 0;
	}
	(void)__xuiTableGridPlaceEditor(pWidget, pData, pEditor, tRect);
	return 1;
}

static int __xuiTableGridCountProc(xui_widget pTable, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pWidget;

	pData = (xui_table_grid_data_t*)pUser;
	pWidget = (pTable != NULL) ? xuiWidgetGetParent(pTable) : NULL;
	if ( (pData == NULL) || (pData->onCount == NULL) ) {
		return 0;
	}
	return pData->onCount(pWidget, pData->pAdapterUser);
}

static int __xuiTableGridCellProc(xui_widget pTable, int iRow, int iColumn, xui_table_view_cell_t* pCell, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pWidget;
	int iRet;

	pData = (xui_table_grid_data_t*)pUser;
	pWidget = (pTable != NULL) ? xuiWidgetGetParent(pTable) : NULL;
	if ( (pData == NULL) || (pCell == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTableGridDefaultCell(pData, iColumn, pCell);
	iRet = 1;
	if ( pData->onCell != NULL ) {
		iRet = pData->onCell(pWidget, iRow, iColumn, pCell, pData->pAdapterUser);
	}
	if ( (pData->iEditingRow == iRow) && (pData->iEditingColumn == iColumn) ) {
		pCell->bEditing = 1;
	}
	return iRet;
}

static void __xuiTableGridComboSelect(xui_widget pWidget, int iIndex, int iValue, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;

	(void)pWidget;
	(void)iIndex;
	(void)iValue;
	pData = (xui_table_grid_data_t*)pUser;
	if ( pData == NULL || pData->iActiveEditor != XUI_TABLE_GRID_EDITOR_COMBO ) return;
	pGrid = xuiWidgetGetParent(pData->pCombo);
	(void)xuiTableGridEndEdit(pGrid, 1);
}

static void __xuiTableGridColorChange(xui_widget pWidget, uint32_t iColor, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;
	const char* sValue;

	(void)pWidget;
	(void)iColor;
	pData = (xui_table_grid_data_t*)pUser;
	if ( pData == NULL || pData->iActiveEditor != XUI_TABLE_GRID_EDITOR_COLOR ) return;
	pGrid = xuiWidgetGetParent(pData->pColor);
	sValue = xuiColorPickerGetHex(pData->pColor);
	if ( __xuiTableGridCommitValue(pGrid, pData, sValue) != 0 ) {
		snprintf(pData->sOriginalValue, sizeof(pData->sOriginalValue), "%s", sValue != NULL ? sValue : "");
		(void)xuiTableViewRefresh(pData->pTable);
	}
}

static void __xuiTableGridDateCommit(xui_widget pWidget, xtime tStart, xtime tEnd, int iMode, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;

	(void)pWidget;
	(void)tStart;
	(void)tEnd;
	(void)iMode;
	pData = (xui_table_grid_data_t*)pUser;
	if ( pData == NULL || pData->iActiveEditor != XUI_TABLE_GRID_EDITOR_DATE ) return;
	pGrid = xuiWidgetGetParent(pData->pDate);
	(void)xuiTableGridEndEdit(pGrid, 1);
}

static void __xuiTableGridDateCancel(xui_widget pWidget, xtime tStart, xtime tEnd, int iMode, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;

	(void)pWidget;
	(void)tStart;
	(void)tEnd;
	(void)iMode;
	pData = (xui_table_grid_data_t*)pUser;
	if ( pData == NULL || pData->iActiveEditor != XUI_TABLE_GRID_EDITOR_DATE ) return;
	pGrid = xuiWidgetGetParent(pData->pDate);
	(void)xuiTableGridEndEdit(pGrid, 0);
}

static void __xuiTableGridTextOk(xui_widget pWidget, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;

	(void)pWidget;
	pData = (xui_table_grid_data_t*)pUser;
	if ( pData == NULL ) return;
	pGrid = xuiPopupGetOwner(pData->pTextPopup);
	(void)xuiTableGridEndEdit(pGrid, 1);
}

static void __xuiTableGridTextCancel(xui_widget pWidget, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;

	(void)pWidget;
	pData = (xui_table_grid_data_t*)pUser;
	if ( pData == NULL ) return;
	pGrid = xuiPopupGetOwner(pData->pTextPopup);
	(void)xuiTableGridEndEdit(pGrid, 0);
}

static int __xuiTableGridTextEditEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_widget pGrid;

	(void)pWidget;
	pData = (xui_table_grid_data_t*)pUser;
	if ( (pData == NULL) || (pEvent == NULL) ) return XUI_OK;
	if ( (pEvent->iType == XUI_EVENT_KEY_DOWN) && (pEvent->iPhase != XUI_EVENT_PHASE_BUBBLE) && (pEvent->iKey == XUI_KEY_ESCAPE) ) {
		pGrid = xuiPopupGetOwner(pData->pTextPopup);
		(void)xuiTableGridEndEdit(pGrid, 0);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTableGridOpenTextArea(xui_widget pWidget, xui_table_grid_data_t* pData)
{
	xui_rect_t tAnchor;
	float fW;
	float fH;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pTextPopup == NULL) ) return 0;
	fW = 380.0f;
	fH = 238.0f;
	if ( !__xuiTableGridCellRectWorld(pWidget, pData, pData->iEditingRow, pData->iEditingColumn, &tAnchor) ) {
		tAnchor = xuiWidgetGetWorldRect(pWidget);
	}
	(void)xuiTextEditSetText(pData->pTextEdit, pData->sOriginalValue);
	(void)xuiPopupSetOwner(pData->pTextPopup, pWidget);
	(void)xuiPopupSetFocusRestore(pData->pTextPopup, pWidget);
	(void)xuiPopupSetAnchorRect(pData->pTextPopup, tAnchor);
	(void)xuiPopupSetAnchor(pData->pTextPopup, XUI_POPUP_ANCHOR_BOTTOM_LEFT);
	(void)xuiPopupSetDirection(pData->pTextPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	(void)xuiPopupSetContentSize(pData->pTextPopup, fW, fH);
	(void)xuiWidgetSetRect(pData->pTextEdit, (xui_rect_t){10.0f, 10.0f, fW - 20.0f, fH - 58.0f});
	(void)xuiWidgetSetRect(pData->pTextCancel, (xui_rect_t){fW - 172.0f, fH - 40.0f, 76.0f, 28.0f});
	(void)xuiWidgetSetRect(pData->pTextOk, (xui_rect_t){fW - 88.0f, fH - 40.0f, 76.0f, 28.0f});
	(void)xuiPopupSetOpen(pData->pTextPopup, 1);
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pTextEdit);
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_TEXTAREA;
	return 1;
}

static int __xuiTableGridOpenPicker(xui_widget pWidget, xui_table_grid_data_t* pData, xui_table_view_cell_t* pCell)
{
	xui_rect_t tRect;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->onEditor == NULL) || (pCell == NULL) ) {
		return 0;
	}
	if ( !__xuiTableGridCellRectWorld(pWidget, pData, pData->iEditingRow, pData->iEditingColumn, &tRect) ) {
		tRect = xuiWidgetGetWorldRect(pWidget);
	}
	if ( pData->onEditor(pWidget, pData->iEditingRow, pData->iEditingColumn, pCell, tRect, pData->pEditorUser) != 0 ) {
		pData->iPickerCount++;
		return 1;
	}
	return 0;
}

static int __xuiTableGridBeginComboEdit(xui_widget pWidget, xui_table_grid_data_t* pData, const char* sText, xui_rect_t tRect)
{
	const xui_combobox_item_t* pItem;
	int i;
	int iSelected;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pCombo == NULL) ) return 0;
	if ( pData->tEditorConfig.arrEnumItemData != NULL && pData->tEditorConfig.iEnumItemCount > 0 ) {
		(void)xuiComboBoxSetItemData(pData->pCombo, pData->tEditorConfig.arrEnumItemData, pData->tEditorConfig.iEnumItemCount);
		if ( pData->tEditorConfig.bEnumUseValue ) {
			(void)xuiComboBoxSetSelectedValue(pData->pCombo, pData->tEditorConfig.iEnumSelectedValue);
		}
	} else if ( pData->tEditorConfig.arrEnumItems != NULL && pData->tEditorConfig.iEnumItemCount > 0 ) {
		(void)xuiComboBoxSetItems(pData->pCombo, pData->tEditorConfig.arrEnumItems, pData->tEditorConfig.iEnumItemCount);
	} else {
		return 0;
	}
	if ( pData->tEditorConfig.arrEnumEnabled != NULL ) {
		(void)xuiComboBoxSetEnabledItems(pData->pCombo, pData->tEditorConfig.arrEnumEnabled, pData->tEditorConfig.iEnumItemCount);
	}
	if ( pData->tEditorConfig.bEnumUseValue && pData->tEditorConfig.arrEnumItemData != NULL ) {
		iSelected = xuiComboBoxGetSelected(pData->pCombo);
	} else {
		iSelected = pData->tEditorConfig.iEnumSelected;
		if ( iSelected < 0 && sText != NULL ) {
			for ( i = 0; i < xuiComboBoxGetItemCount(pData->pCombo); i++ ) {
				pItem = xuiComboBoxGetItem(pData->pCombo, i);
				if ( pItem != NULL && pItem->sText != NULL && strcmp(pItem->sText, sText) == 0 ) {
					iSelected = i;
					break;
				}
			}
		}
		(void)xuiComboBoxSetSelected(pData->pCombo, iSelected);
	}
	(void)__xuiTableGridPlaceEditor(pWidget, pData, pData->pCombo, tRect);
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_COMBO;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pCombo);
	return 1;
}

static int __xuiTableGridBeginColorEdit(xui_widget pWidget, xui_table_grid_data_t* pData, const char* sText, xui_rect_t tRect)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pColor == NULL) ) return 0;
	(void)xuiColorPickerSetAlphaEnabled(pData->pColor, pData->tEditorConfig.bAlphaEnabled);
	if ( pData->tEditorConfig.arrPalette != NULL && pData->tEditorConfig.iPaletteCount > 0 ) {
		(void)xuiColorPickerSetPalette(pData->pColor, pData->tEditorConfig.arrPalette, pData->tEditorConfig.iPaletteCount);
	}
	if ( xuiColorPickerSetHex(pData->pColor, sText != NULL ? sText : "") != XUI_OK ) {
		(void)xuiColorPickerSetColor(pData->pColor, XUI_COLOR_RGBA(67, 167, 124, 255));
	}
	(void)__xuiTableGridPlaceEditor(pWidget, pData, pData->pColor, tRect);
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_COLOR;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pColor);
	return 1;
}

static int __xuiTableGridBeginDateEdit(xui_widget pWidget, xui_table_grid_data_t* pData, const char* sText, xui_rect_t tRect, int iType)
{
	const char* sFormat;
	int iMode;
	xtime tValue;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pDate == NULL) ) return 0;
	iMode = pData->tEditorConfig.bDateModeSet ? pData->tEditorConfig.iDateMode : __xuiTableGridDateModeForType(iType);
	(void)xuiDatePickerSetMode(pData->pDate, iMode);
	(void)xuiDatePickerSetNullable(pData->pDate, pData->tEditorConfig.bNullable);
	(void)xuiDatePickerSetShowSecond(pData->pDate, pData->tEditorConfig.bShowSecond);
	sFormat = (pData->tEditorConfig.sDateFormat != NULL) ? pData->tEditorConfig.sDateFormat : __xuiTableGridDefaultDateFormat(iMode, pData->tEditorConfig.bShowSecond);
	(void)xuiDatePickerSetFormat(pData->pDate, sFormat);
	if ( pData->tEditorConfig.sRangeSeparator != NULL ) {
		(void)xuiDatePickerSetRangeSeparator(pData->pDate, pData->tEditorConfig.sRangeSeparator);
	}
	if ( pData->tEditorConfig.tDefaultRangeSpan > 0 ) {
		(void)xuiDatePickerSetDefaultRangeSpan(pData->pDate, pData->tEditorConfig.tDefaultRangeSpan);
	}
	if ( pData->tEditorConfig.bDateHasMin || pData->tEditorConfig.bDateHasMax ) {
		(void)xuiDatePickerSetLimits(pData->pDate,
			pData->tEditorConfig.bDateHasMin ? pData->tEditorConfig.tDateMin : 0,
			pData->tEditorConfig.bDateHasMax ? pData->tEditorConfig.tDateMax : (xtime)XRT_TIME_DAY * (xtime)365242);
	} else {
		(void)xuiDatePickerClearLimits(pData->pDate);
	}
	tValue = __xuiTableGridParseDateValue(sText, iType, sFormat);
	(void)xuiDatePickerSetValue(pData->pDate, tValue);
	(void)__xuiTableGridPlaceEditor(pWidget, pData, pData->pDate, tRect);
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_DATE;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pDate);
	return 1;
}

static int __xuiTableGridBeginNumericEdit(xui_widget pWidget, xui_table_grid_data_t* pData, const char* sText, xui_rect_t tRect, int iType)
{
	xui_widget pInput;
	float fValue;
	float fMin;
	float fMax;
	int iPrecision;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pNumeric == NULL) ) return 0;
	iPrecision = (pData->tEditorConfig.iPrecision >= 0) ? pData->tEditorConfig.iPrecision : __xuiTableGridInferPrecision(sText, iType);
	fMin = pData->tEditorConfig.fMin;
	fMax = pData->tEditorConfig.fMax;
	if ( fMin == fMax ) {
		fMin = -1000000000.0f;
		fMax = 1000000000.0f;
	}
	(void)xuiNumericInputSetRange(pData->pNumeric, fMin, fMax);
	(void)xuiNumericInputSetInteger(pData->pNumeric, iType == XUI_TABLE_CELL_TYPE_INT);
	(void)xuiNumericInputSetPrecision(pData->pNumeric, iPrecision);
	(void)xuiNumericInputSetStep(pData->pNumeric, (pData->tEditorConfig.fStep > 0.0f) ? pData->tEditorConfig.fStep : __xuiTableGridStepForPrecision(iPrecision));
	(void)xuiNumericInputSetSpinnerVisible(pData->pNumeric, 1);
	if ( sText != NULL && sText[0] != '\0' && __xuiTableGridValidateNumberText(sText, iType) ) {
		fValue = (float)strtod(sText, NULL);
		(void)xuiNumericInputSetValue(pData->pNumeric, fValue);
	}
	(void)xuiNumericInputSetText(pData->pNumeric, sText != NULL ? sText : "");
	(void)__xuiTableGridPlaceEditor(pWidget, pData, pData->pNumeric, tRect);
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NUMERIC;
	pInput = xuiNumericInputGetInputWidget(pData->pNumeric);
	if ( pInput != NULL ) {
		(void)xuiInputSetError(pInput, 0);
		(void)xuiInputSelectAll(pInput);
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pInput != NULL ? pInput : pData->pNumeric);
	return 1;
}

static int __xuiTableGridBeginInputEdit(xui_widget pWidget, xui_table_grid_data_t* pData, const char* sText, xui_rect_t tRect, int iColumn, int iType)
{
	const xui_table_view_column_t* pColumn;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pInput == NULL) ) return 0;
	(void)xuiInputSetReadonly(pData->pInput, 0);
	(void)xuiInputSetError(pData->pInput, 0);
	(void)xuiInputSetText(pData->pInput, sText != NULL ? sText : "");
	(void)xuiInputSelectAll(pData->pInput);
	if ( __xuiTableGridIsNumericType(iType) ) {
		(void)xuiInputSetTextAlign(pData->pInput, XUI_INPUT_ALIGN_RIGHT);
	} else {
		pColumn = xuiTableViewGetColumn(pData->pTable, iColumn);
		if ( pColumn != NULL && ((pColumn->iAlign & XUI_TEXT_ALIGN_RIGHT) != 0) ) {
			(void)xuiInputSetTextAlign(pData->pInput, XUI_INPUT_ALIGN_RIGHT);
		} else if ( pColumn != NULL && ((pColumn->iAlign & XUI_TEXT_ALIGN_CENTER) != 0) ) {
			(void)xuiInputSetTextAlign(pData->pInput, XUI_INPUT_ALIGN_CENTER);
		} else {
			(void)xuiInputSetTextAlign(pData->pInput, XUI_INPUT_ALIGN_LEFT);
		}
	}
	(void)__xuiTableGridPlaceEditor(pWidget, pData, pData->pInput, tRect);
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_INPUT;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pInput);
	return 1;
}

static int __xuiTableGridEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_table_grid_data_t* pData;
	xui_rect_t tTableWorld;
	float fLocalX;
	float fLocalY;
	int iRow;
	int iColumn;
	int iRet;

	(void)pUser;
	pData = __xuiTableGridGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iType == XUI_EVENT_KEY_DOWN ) {
		if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) {
			return XUI_OK;
		}
		if ( pData->iEditingRow >= 0 ) {
			if ( pEvent->iKey == XUI_KEY_ENTER ) {
				(void)xuiTableGridEndEdit(pWidget, 1);
				return XUI_EVENT_DISPATCH_STOP;
			}
			if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
				(void)xuiTableGridEndEdit(pWidget, 0);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		if ( pData->iEditingRow < 0 ) {
			if ( pEvent->iKey == XUI_KEY_ENTER || pEvent->iKey == XUI_KEY_SPACE ) {
				iRow = -1;
				iColumn = -1;
				(void)xuiTableViewGetSelectedCell(pData->pTable, &iRow, &iColumn);
				if ( iColumn < 0 ) {
					(void)xuiTableViewGetActiveCell(pData->pTable, &iRow, &iColumn);
				}
				if ( xuiTableGridBeginEdit(pWidget, iRow, iColumn) != 0 ) {
					return XUI_EVENT_DISPATCH_STOP;
				}
			}
		}
		return XUI_OK;
	}
	if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
		return XUI_OK;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_DOWN && pData->iEditingRow >= 0 ) {
		if ( __xuiTableGridEditorPopupOpen(pData) ) {
			return XUI_OK;
		}
		if ( !__xuiTableGridPointInWidget(__xuiTableGridActiveEditor(pData), pEvent->fX, pEvent->fY) ) {
			if ( xuiTableGridEndEdit(pWidget, 1) == 0 ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_WHEEL && pData->iEditingRow >= 0 ) {
		if ( __xuiTableGridEditorPopupOpen(pData) ||
		     __xuiTableGridPointInWidget(__xuiTableGridActiveEditor(pData), pEvent->fX, pEvent->fY) ) {
			return XUI_OK;
		}
		if ( xuiTableGridEndEdit(pWidget, 1) == 0 ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	if ( pData->iEditingRow >= 0 &&
	     (pEvent->iType == XUI_EVENT_POINTER_UP || pEvent->iType == XUI_EVENT_POINTER_DOUBLE_CLICK) &&
	     __xuiTableGridEventInActiveEditor(pWidget, pData, pEvent) ) {
		return XUI_OK;
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_UP && pData->iEditMode == XUI_TABLE_GRID_EDIT_QUICK) ||
	     (pEvent->iType == XUI_EVENT_POINTER_DOUBLE_CLICK && pData->iEditMode == XUI_TABLE_GRID_EDIT_DISPLAY) ) {
		tTableWorld = xuiWidgetGetWorldRect(pData->pTable);
		fLocalX = pEvent->fX - tTableWorld.fX;
		fLocalY = pEvent->fY - tTableWorld.fY;
		iRow = -1;
		iColumn = -1;
		iRet = xuiTableViewGetItemAt(pData->pTable, fLocalX, fLocalY, &iRow, &iColumn);
		if ( iRet == XUI_OK && iRow >= 0 && iColumn >= 0 ) {
			if ( xuiTableGridBeginEdit(pWidget, iRow, iColumn) != 0 ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
	}
	return XUI_OK;
}

static int __xuiTableGridArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_table_grid_data_t* pData;
	int iRet;

	(void)pUser;
	pData = __xuiTableGridGetData(pWidget);
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pTable == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiWidgetArrange(pData->pTable, tContentRect);
	(void)__xuiTableGridSyncActiveEditorRect(pWidget, pData);
	return iRet;
}

static int __xuiTableGridUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_table_grid_data_t* pData;

	(void)fDelta;
	(void)pUser;
	pData = __xuiTableGridGetData(pWidget);
	if ( pData != NULL ) {
		(void)__xuiTableGridSyncActiveEditorRect(pWidget, pData);
	}
	return XUI_OK;
}

static void __xuiTableGridDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = 0;
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

static int __xuiTableGridContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget;
	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSize->fX = 360.0f;
	pSize->fY = 220.0f;
	return XUI_OK;
}

static int __xuiTableGridInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTableGridEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTableGridEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiTableGridEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTableGridEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTableGridEvent, NULL);
	return iRet;
}

static int __xuiTableGridCreateEditors(xui_widget pWidget, xui_table_grid_data_t* pData, const xui_table_grid_desc_t* pDesc)
{
	xui_context pContext;
	xui_input_desc_t tInput;
	xui_numeric_input_desc_t tNumeric;
	xui_combobox_desc_t tCombo;
	xui_color_picker_desc_t tColor;
	xui_date_picker_desc_t tDate;
	xui_popup_desc_t tPopup;
	xui_text_edit_desc_t tTextEdit;
	xui_button_desc_t tButton;
	xui_widget pContent;
	int iRet;

	pContext = xuiWidgetGetContext(pWidget);
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.pFont = pData->pFont;
	tInput.iMaxLength = XUI_TABLE_GRID_VALUE_CAPACITY - 1;
	tInput.iTextAlign = XUI_INPUT_ALIGN_LEFT;
	tInput.iFocusBorderColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	tInput.iBorderColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	iRet = xuiInputCreate(pContext, &pData->pInput, &tInput);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tNumeric, 0, sizeof(tNumeric));
	tNumeric.iSize = sizeof(tNumeric);
	tNumeric.pFont = pData->pFont;
	tNumeric.fMin = -1000000000.0f;
	tNumeric.fMax = 1000000000.0f;
	tNumeric.fStep = 1.0f;
	tNumeric.iPrecision = 3;
	tNumeric.bSpinnerVisible = 1;
	iRet = xuiNumericInputCreate(pContext, &pData->pNumeric, &tNumeric);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tCombo, 0, sizeof(tCombo));
	tCombo.iSize = sizeof(tCombo);
	tCombo.pFont = pData->pFont;
	tCombo.fPopupMaxHeight = 180.0f;
	iRet = xuiComboBoxCreate(pContext, &pData->pCombo, &tCombo);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tColor, 0, sizeof(tColor));
	tColor.iSize = sizeof(tColor);
	tColor.pFont = pData->pFont;
	tColor.iColor = XUI_COLOR_RGBA(67, 167, 124, 255);
	iRet = xuiColorPickerCreate(pContext, &pData->pColor, &tColor);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tDate, 0, sizeof(tDate));
	tDate.iSize = sizeof(tDate);
	tDate.pFont = pData->pFont;
	tDate.iMode = XUI_DATE_PICKER_MODE_DATE;
	tDate.tValue = xrtNow();
	iRet = xuiDatePickerCreate(pContext, &pData->pDate, &tDate);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tPopup, 0, sizeof(tPopup));
	tPopup.iSize = sizeof(tPopup);
	tPopup.pOwner = pWidget;
	tPopup.fContentWidth = 380.0f;
	tPopup.fContentHeight = 238.0f;
	tPopup.fPadding = 0.0f;
	tPopup.fBorderWidth = 1.0f;
	tPopup.iOutsidePolicy = XUI_POPUP_OUTSIDE_IGNORE;
	tPopup.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tPopup.iEscapePolicy = XUI_POPUP_ESCAPE_IGNORE;
	tPopup.iFocusPolicy = XUI_POPUP_FOCUS_CUSTOM;
	iRet = xuiPopupCreate(pContext, &pData->pTextPopup, &tPopup);
	if ( iRet != XUI_OK ) return iRet;
	pContent = xuiPopupGetContentWidget(pData->pTextPopup);
	pData->pTextContent = pContent;

	memset(&tTextEdit, 0, sizeof(tTextEdit));
	tTextEdit.iSize = sizeof(tTextEdit);
	tTextEdit.pFont = pData->pFont;
	tTextEdit.bWordWrap = 1;
	iRet = xuiTextEditCreate(pContext, &pData->pTextEdit, &tTextEdit);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tButton, 0, sizeof(tButton));
	tButton.iSize = sizeof(tButton);
	tButton.pFont = pData->pFont;
	tButton.sText = "OK";
	iRet = xuiButtonCreate(pContext, &pData->pTextOk, &tButton);
	if ( iRet != XUI_OK ) return iRet;
	tButton.sText = "Cancel";
	iRet = xuiButtonCreate(pContext, &pData->pTextCancel, &tButton);
	if ( iRet != XUI_OK ) return iRet;

	(void)xuiWidgetSetVisible(pData->pInput, 0);
	(void)xuiWidgetSetVisible(pData->pNumeric, 0);
	(void)xuiWidgetSetVisible(pData->pCombo, 0);
	(void)xuiWidgetSetVisible(pData->pColor, 0);
	(void)xuiWidgetSetVisible(pData->pDate, 0);
	iRet = __xuiTableGridSetEditorLayerTree(pData->pInput, XUI_LAYER_NORMAL, 100);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableGridSetEditorLayerTree(pData->pNumeric, XUI_LAYER_NORMAL, 100);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableGridSetEditorLayerTree(pData->pCombo, XUI_LAYER_NORMAL, 100);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableGridSetEditorLayerTree(pData->pColor, XUI_LAYER_NORMAL, 100);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableGridSetEditorLayerTree(pData->pDate, XUI_LAYER_NORMAL, 100);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiComboBoxSetSelect(pData->pCombo, __xuiTableGridComboSelect, pData);
	(void)xuiColorPickerSetChange(pData->pColor, __xuiTableGridColorChange, pData);
	(void)xuiDatePickerSetCommit(pData->pDate, __xuiTableGridDateCommit, pData);
	(void)xuiDatePickerSetCancel(pData->pDate, __xuiTableGridDateCancel, pData);
	(void)xuiButtonSetClick(pData->pTextOk, __xuiTableGridTextOk, pData);
	(void)xuiButtonSetClick(pData->pTextCancel, __xuiTableGridTextCancel, pData);
	(void)xuiWidgetSetEventCallback(pData->pTextEdit, __xuiTableGridTextEditEvent, pData);
	(void)xuiPopupSetFocusPolicy(pData->pTextPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pTextEdit);
	(void)xuiWidgetSetLayoutType(pContent, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetAddChild(pContent, pData->pTextEdit);
	(void)xuiWidgetAddChild(pContent, pData->pTextCancel);
	(void)xuiWidgetAddChild(pContent, pData->pTextOk);

	iRet = xuiWidgetAddChild(pWidget, pData->pInput);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pData->pNumeric);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pData->pCombo);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pData->pColor);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pData->pDate);
	(void)pDesc;
	return iRet;
}

static int __xuiTableGridInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_table_grid_data_t* pData;
	const xui_table_grid_desc_t* pDesc;
	xui_table_view_desc_t tTableDesc;
	int iRet;

	(void)pUser;
	pData = (xui_table_grid_data_t*)pTypeData;
	pDesc = (const xui_table_grid_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTableGridDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pData->pFont = (pDesc != NULL) ? pDesc->pFont : NULL;
	pData->iEditMode = (pDesc != NULL && __xuiTableGridEditModeValid(pDesc->iEditMode)) ? pDesc->iEditMode : XUI_TABLE_GRID_EDIT_DISPLAY;
	pData->iEditingRow = -1;
	pData->iEditingColumn = -1;
	pData->iEditingType = XUI_TABLE_CELL_TYPE_TEXT;
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NONE;
	pData->onCount = (pDesc != NULL) ? pDesc->onCount : NULL;
	pData->onCell = (pDesc != NULL) ? pDesc->onCell : NULL;
	pData->onSet = (pDesc != NULL) ? pDesc->onSet : NULL;
	pData->pAdapterUser = (pDesc != NULL) ? pDesc->pAdapterUser : NULL;
	pData->pSetUser = pData->pAdapterUser;
	pData->onValidate = (pDesc != NULL) ? pDesc->onValidate : NULL;
	pData->pValidateUser = (pDesc != NULL) ? pDesc->pValidateUser : NULL;
	pData->onChange = (pDesc != NULL) ? pDesc->onChange : NULL;
	pData->pChangeUser = (pDesc != NULL) ? pDesc->pChangeUser : NULL;
	pData->onEditor = (pDesc != NULL) ? pDesc->onEditor : NULL;
	pData->pEditorUser = (pDesc != NULL) ? pDesc->pEditorUser : NULL;
	pData->onEditorConfig = (pDesc != NULL) ? pDesc->onEditorConfig : NULL;
	pData->pEditorConfigUser = (pDesc != NULL) ? pDesc->pEditorConfigUser : NULL;

	memset(&tTableDesc, 0, sizeof(tTableDesc));
	tTableDesc.iSize = sizeof(tTableDesc);
	if ( pDesc != NULL ) {
		tTableDesc.arrColumns = pDesc->arrColumns;
		tTableDesc.arrRows = pDesc->arrRows;
		tTableDesc.iColumnCount = pDesc->iColumnCount;
		tTableDesc.iRowCount = pDesc->iRowCount;
		tTableDesc.pFont = pDesc->pFont;
		tTableDesc.fDefaultColumnWidth = pDesc->fDefaultColumnWidth;
		tTableDesc.fDefaultRowHeight = pDesc->fDefaultRowHeight;
		tTableDesc.fHeaderHeight = pDesc->fHeaderHeight;
		tTableDesc.iSelectionMode = pDesc->iSelectionMode;
		tTableDesc.iScrollbarMode = pDesc->iScrollbarMode;
		tTableDesc.tColors = pDesc->tColors;
		tTableDesc.iTrackColor = pDesc->iTrackColor;
		tTableDesc.iThumbColor = pDesc->iThumbColor;
		tTableDesc.iScrollbarHoverColor = pDesc->iScrollbarHoverColor;
		tTableDesc.iScrollbarActiveColor = pDesc->iScrollbarActiveColor;
		tTableDesc.iScrollbarFocusColor = pDesc->iScrollbarFocusColor;
		tTableDesc.iScrollbarDisabledColor = pDesc->iScrollbarDisabledColor;
	}
	tTableDesc.onCount = (pData->onCount != NULL) ? __xuiTableGridCountProc : NULL;
	tTableDesc.onCell = (pData->onCell != NULL) ? __xuiTableGridCellProc : NULL;
	tTableDesc.pAdapterUser = pData;
	iRet = xuiTableViewCreate(xuiWidgetGetContext(pWidget), &pData->pTable, &tTableDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pData->pTable, (xui_rect_t){0.0f, 0.0f, 360.0f, 220.0f});
	iRet = xuiWidgetAddChild(pWidget, pData->pTable);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableGridCreateEditors(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	return __xuiTableGridInitEvents(pWidget);
}

static void __xuiTableGridDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_table_grid_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_table_grid_data_t*)pTypeData;
	if ( pData != NULL ) {
		if ( pData->pTextPopup != NULL ) {
			xuiWidgetDestroy(pData->pTextPopup);
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static xui_table_grid_data_t* __xuiTableGridGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "tablegrid");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_table_grid_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiTableGridGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "tablegrid");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "tablegrid";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iTypeDataSize = sizeof(xui_table_grid_data_t);
	tDesc.onInit = __xuiTableGridInit;
	tDesc.onDestroy = __xuiTableGridDestroy;
	tDesc.onContentMeasure = __xuiTableGridContentMeasure;
	tDesc.onLayoutArrange = __xuiTableGridArrange;
	tDesc.onUpdate = __xuiTableGridUpdate;
	__xuiTableGridDefaultLayout(&tDesc.tLayout);
	if ( xuiWidgetRegisterType(pContext, &pType, &tDesc) != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiTableGridCreate(xui_context pContext, xui_widget* ppWidget, const xui_table_grid_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiTableGridDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiTableGridGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_widget xuiTableGridGetTableView(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->pTable : NULL;
}

XUI_API int xuiTableGridSetColumns(xui_widget pWidget, const xui_table_view_column_t* arrColumns, int iCount)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL || pData->pTable == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableViewSetColumns(pData->pTable, arrColumns, iCount);
}

XUI_API int xuiTableGridSetRows(xui_widget pWidget, const xui_table_view_row_t* arrRows, int iCount)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL || pData->pTable == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableViewSetRows(pData->pTable, arrRows, iCount);
}

XUI_API int xuiTableGridSetAdapter(xui_widget pWidget, xui_table_view_count_proc onCount, xui_table_view_cell_proc onCell, xui_table_grid_set_proc onSet, void* pUser)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL || pData->pTable == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCount = onCount;
	pData->onCell = onCell;
	pData->onSet = onSet;
	pData->pAdapterUser = pUser;
	pData->pSetUser = pUser;
	return xuiTableViewSetAdapter(pData->pTable, __xuiTableGridCountProc, __xuiTableGridCellProc, pData);
}

XUI_API int xuiTableGridSetValidate(xui_widget pWidget, xui_table_grid_validate_proc onValidate, void* pUser)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onValidate = onValidate;
	pData->pValidateUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableGridSetChange(xui_widget pWidget, xui_table_grid_change_proc onChange, void* pUser)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableGridSetEditor(xui_widget pWidget, xui_table_grid_editor_proc onEditor, void* pUser)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onEditor = onEditor;
	pData->pEditorUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableGridSetEditorConfig(xui_widget pWidget, xui_table_grid_editor_config_proc onConfig, void* pUser)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onEditorConfig = onConfig;
	pData->pEditorConfigUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableGridSetEditMode(xui_widget pWidget, int iMode)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL || !__xuiTableGridEditModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iEditMode = iMode;
	return XUI_OK;
}

XUI_API int xuiTableGridGetEditMode(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->iEditMode : XUI_TABLE_GRID_EDIT_DISPLAY;
}

XUI_API int xuiTableGridSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	(void)xuiTableViewSetFont(pData->pTable, pFont);
	(void)xuiInputSetFont(pData->pInput, pFont);
	(void)xuiComboBoxSetFont(pData->pCombo, pFont);
	(void)xuiColorPickerSetFont(pData->pColor, pFont);
	(void)xuiDatePickerSetFont(pData->pDate, pFont);
	(void)xuiTextEditSetFont(pData->pTextEdit, pFont);
	(void)xuiButtonSetFont(pData->pTextOk, pFont);
	(void)xuiButtonSetFont(pData->pTextCancel, pFont);
	return XUI_OK;
}

XUI_API xui_font xuiTableGridGetFont(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiTableGridSetDefaultMetrics(xui_widget pWidget, float fColumnWidth, float fRowHeight, float fHeaderHeight)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableViewSetDefaultMetrics(pData->pTable, fColumnWidth, fRowHeight, fHeaderHeight);
}

XUI_API int xuiTableGridSetSelectionMode(xui_widget pWidget, int iMode)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableViewSetSelectionMode(pData->pTable, iMode);
}

XUI_API int xuiTableGridSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableViewSetScrollbarMode(pData->pTable, iMode);
}

XUI_API int xuiTableGridSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iHeader, uint32_t iRow, uint32_t iSelected, uint32_t iGrid, uint32_t iText)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiTableViewSetColors(pData->pTable, iBackground, iHeader, iRow, iSelected, iGrid, iText);
	(void)xuiInputSetColors(pData->pInput, XUI_COLOR_RGBA(255, 255, 255, 255), iText, XUI_COLOR_RGBA(47, 128, 237, 255), XUI_COLOR_RGBA(47, 128, 237, 255));
	(void)xuiComboBoxSetColors(pData->pCombo, iText, XUI_COLOR_RGBA(150, 160, 172, 255), XUI_COLOR_RGBA(255, 255, 255, 255), XUI_COLOR_RGBA(249, 252, 255, 255), XUI_COLOR_RGBA(238, 246, 255, 255), XUI_COLOR_RGBA(242, 245, 249, 255));
	return XUI_OK;
}

XUI_API int xuiTableGridSetColorStyle(xui_widget pWidget, const xui_table_view_colors_t* pColors)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiTableViewSetColorStyle(pData->pTable, pColors);
	(void)xuiInputSetColors(pData->pInput, XUI_COLOR_RGBA(255, 255, 255, 255), pColors->iTextColor, pColors->iFocusRingColor, pColors->iFocusRingColor);
	(void)xuiComboBoxSetColors(pData->pCombo, pColors->iTextColor, pColors->iDisabledTextColor, XUI_COLOR_RGBA(255, 255, 255, 255), pColors->iAltRowColor, pColors->iHoverColor, pColors->iDisabledColor);
	return XUI_OK;
}

XUI_API int xuiTableGridBeginEdit(xui_widget pWidget, int iRow, int iColumn)
{
	xui_table_grid_data_t* pData;
	xui_table_view_cell_t tCell;
	xui_rect_t tRect;
	char sValue[XUI_TABLE_GRID_VALUE_CAPACITY];
	const char* sText;
	int iType;
	int bOK;

	pData = __xuiTableGridGetData(pWidget);
	if ( (pData == NULL) || (pData->pTable == NULL) || (iRow < 0) || (iColumn < 0) ) {
		return 0;
	}
	if ( xuiTableViewGetRowCount(pData->pTable) <= iRow || xuiTableViewGetColumnCount(pData->pTable) <= iColumn ) {
		return 0;
	}
	if ( pData->iEditingRow >= 0 && xuiTableGridEndEdit(pWidget, 1) == 0 ) {
		return 0;
	}
	__xuiTableGridGetCell(pWidget, pData, iRow, iColumn, &tCell);
	if ( tCell.bDisabled ) {
		return 0;
	}
	iType = tCell.iType;
	sText = __xuiTableGridCellText(pWidget, pData, iRow, iColumn, &tCell, sValue, (int)sizeof(sValue));
	if ( iType == XUI_TABLE_CELL_TYPE_BOOL ) {
		pData->iEditingRow = iRow;
		pData->iEditingColumn = iColumn;
		pData->iEditingType = iType;
		snprintf(pData->sOriginalValue, sizeof(pData->sOriginalValue), "%s", __xuiTableGridBoolValue(sText, tCell.pValue) ? "true" : "false");
		bOK = __xuiTableGridCommitValue(pWidget, pData, __xuiTableGridBoolValue(sText, tCell.pValue) ? "false" : "true");
		pData->iEditingRow = -1;
		pData->iEditingColumn = -1;
		pData->iEditingType = XUI_TABLE_CELL_TYPE_TEXT;
		pData->sOriginalValue[0] = '\0';
		(void)xuiTableViewRefresh(pData->pTable);
		return bOK;
	}
	(void)xuiTableViewEnsureCellVisible(pData->pTable, iRow, iColumn);
	if ( !__xuiTableGridEditingRect(pWidget, pData, &tRect) ) {
		pData->iEditingRow = iRow;
		pData->iEditingColumn = iColumn;
		if ( !__xuiTableGridEditingRect(pWidget, pData, &tRect) ) {
			pData->iEditingRow = -1;
			pData->iEditingColumn = -1;
			return 0;
		}
	} else {
		pData->iEditingRow = iRow;
		pData->iEditingColumn = iColumn;
	}
	pData->iEditingType = iType;
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NONE;
	snprintf(pData->sOriginalValue, sizeof(pData->sOriginalValue), "%s", sText != NULL ? sText : "");
	(void)__xuiTableGridLoadEditorConfig(pWidget, pData, iRow, iColumn, iType);
	__xuiTableGridHideEditors(pData);
	if ( iType == XUI_TABLE_CELL_TYPE_TEXTAREA ) {
		(void)xuiTableViewRefresh(pData->pTable);
		return __xuiTableGridOpenTextArea(pWidget, pData);
	}
	if ( __xuiTableGridIsPickerType(iType) ) {
		(void)xuiTableViewRefresh(pData->pTable);
		bOK = __xuiTableGridOpenPicker(pWidget, pData, &tCell);
		pData->iEditingRow = -1;
		pData->iEditingColumn = -1;
		pData->iEditingType = XUI_TABLE_CELL_TYPE_TEXT;
		pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NONE;
		pData->sOriginalValue[0] = '\0';
		(void)xuiTableViewRefresh(pData->pTable);
		return bOK;
	}
	if ( iType == XUI_TABLE_CELL_TYPE_ENUM ) {
		if ( __xuiTableGridBeginComboEdit(pWidget, pData, pData->sOriginalValue, tRect) ) {
			(void)xuiTableViewRefresh(pData->pTable);
			if ( pData->iEditMode == XUI_TABLE_GRID_EDIT_QUICK ) {
				(void)xuiComboBoxOpen(pData->pCombo);
			}
			return 1;
		}
		if ( __xuiTableGridOpenPicker(pWidget, pData, &tCell) ) {
			pData->iEditingRow = -1;
			pData->iEditingColumn = -1;
			pData->iEditingType = XUI_TABLE_CELL_TYPE_TEXT;
			pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NONE;
			pData->sOriginalValue[0] = '\0';
			return 1;
		}
	}
	if ( iType == XUI_TABLE_CELL_TYPE_COLOR && __xuiTableGridBeginColorEdit(pWidget, pData, pData->sOriginalValue, tRect) ) {
		(void)xuiTableViewRefresh(pData->pTable);
		if ( pData->iEditMode == XUI_TABLE_GRID_EDIT_QUICK ) {
			(void)xuiColorPickerOpen(pData->pColor);
		}
		return 1;
	}
	if ( __xuiTableGridIsDateType(iType) && __xuiTableGridBeginDateEdit(pWidget, pData, pData->sOriginalValue, tRect, iType) ) {
		(void)xuiTableViewRefresh(pData->pTable);
		return 1;
	}
	if ( iType == XUI_TABLE_CELL_TYPE_CUSTOM && __xuiTableGridOpenPicker(pWidget, pData, &tCell) ) {
		pData->iEditingRow = -1;
		pData->iEditingColumn = -1;
		pData->iEditingType = XUI_TABLE_CELL_TYPE_TEXT;
		pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NONE;
		pData->sOriginalValue[0] = '\0';
		return 1;
	}
	if ( __xuiTableGridIsNumericType(iType) && __xuiTableGridBeginNumericEdit(pWidget, pData, pData->sOriginalValue, tRect, iType) ) {
		(void)xuiTableViewRefresh(pData->pTable);
		return 1;
	}
	if ( __xuiTableGridBeginInputEdit(pWidget, pData, pData->sOriginalValue, tRect, iColumn, iType) ) {
		(void)xuiTableViewRefresh(pData->pTable);
		return 1;
	}
	return 0;
}

XUI_API int xuiTableGridEndEdit(xui_widget pWidget, int bCommit)
{
	xui_table_grid_data_t* pData;
	char sBuffer[XUI_TABLE_GRID_VALUE_CAPACITY];
	const char* sValue;

	pData = __xuiTableGridGetData(pWidget);
	if ( (pData == NULL) || (pData->iEditingRow < 0) || (pData->iEditingColumn < 0) ) {
		return 1;
	}
	if ( bCommit ) {
		sValue = __xuiTableGridCurrentEditorValue(pData, sBuffer, (int)sizeof(sBuffer));
		if ( __xuiTableGridCommitValue(pWidget, pData, sValue) == 0 ) {
			return 0;
		}
	} else {
		pData->iCancelCount++;
	}
	pData->iEditingRow = -1;
	pData->iEditingColumn = -1;
	pData->iEditingType = XUI_TABLE_CELL_TYPE_TEXT;
	pData->iActiveEditor = XUI_TABLE_GRID_EDITOR_NONE;
	pData->sOriginalValue[0] = '\0';
	(void)xuiInputSetText(pData->pInput, "");
	(void)xuiInputSetError(pData->pInput, 0);
	(void)xuiNumericInputSetText(pData->pNumeric, "");
	__xuiTableGridHideEditors(pData);
	if ( pData->pTable != NULL ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pTable);
		(void)xuiTableViewRefresh(pData->pTable);
	}
	return 1;
}

XUI_API int xuiTableGridIsEditing(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) && (pData->iEditingRow >= 0) && (pData->iEditingColumn >= 0);
}

XUI_API int xuiTableGridGetEditingCell(xui_widget pWidget, int* pRow, int* pColumn)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRow != NULL ) *pRow = pData->iEditingRow;
	if ( pColumn != NULL ) *pColumn = pData->iEditingColumn;
	return XUI_OK;
}

XUI_API int xuiTableGridGetCommitCount(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->iCommitCount : 0;
}

XUI_API int xuiTableGridGetRejectCount(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->iRejectCount : 0;
}

XUI_API int xuiTableGridGetCancelCount(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->iCancelCount : 0;
}

XUI_API int xuiTableGridGetPickerCount(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->iPickerCount : 0;
}

XUI_API int xuiTableGridGetChangeCount(xui_widget pWidget)
{
	xui_table_grid_data_t* pData = __xuiTableGridGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
