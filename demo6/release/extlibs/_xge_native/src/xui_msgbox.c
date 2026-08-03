#include "xui_internal.h"

#include <string.h>

#define XUI_MSGBOX_MAGIC 0x584D5342u
#define XUI_MSGBOX_PRESET_BUTTON_TITLE_COUNT 4

struct xui_msgbox_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_widget pWindow;
	xui_widget pContent;
	xui_widget pBackdrop;
	xui_widget arrButtons[XUI_MSGBOX_BUTTON_CAPACITY];
	char* sTitle;
	char* sMessage;
	char* arrButtonText[XUI_MSGBOX_BUTTON_CAPACITY];
	char* arrPresetButtonTitle[XUI_MSGBOX_PRESET_BUTTON_TITLE_COUNT];
	int arrButtonResult[XUI_MSGBOX_BUTTON_CAPACITY];
	int arrButtonSemantic[XUI_MSGBOX_BUTTON_CAPACITY];
	xui_msgbox_metrics_t tMetrics;
	xui_msgbox_colors_t tColors;
	xui_msgbox_result_proc onResult;
	void* pResultUser;
	xui_widget_event_proc onWindowEvent;
	void* pWindowEventUser;
	xui_font pFont;
	xui_surface pIconSurface;
	xui_rect_t tIconSrc;
	xui_rect_t tIconRect;
	xui_rect_t tMessageRect;
	xui_rect_t tBackdropRect;
	xui_rect_t arrButtonRect[XUI_MSGBOX_BUTTON_CAPACITY];
	int iType;
	int iButtons;
	int iButtonCount;
	int iResult;
	int iWrapLineCount;
	int iResultCount;
	int iChangeCount;
	int bModal;
	int bOpen;
	int bCustomIcon;
	int bBuiltinIcon;
};

static int __xuiMsgBoxValid(xui_msgbox pBox)
{
	return (pBox != NULL) && (pBox->iMagic == XUI_MSGBOX_MAGIC);
}

static float __xuiMsgBoxMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiMsgBoxMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiMsgBoxClamp(float fValue, float fMin, float fMax)
{
	if ( fMax < fMin ) {
		return fMin;
	}
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static int __xuiMsgBoxFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiMsgBoxTypeValid(int iType)
{
	return (iType >= XUI_MSGBOX_ICON_NONE) && (iType <= XUI_MSGBOX_ICON_ERROR);
}

static int __xuiMsgBoxButtonsValid(int iButtons)
{
	return (iButtons >= XUI_MSGBOX_BUTTON_OK) && (iButtons <= XUI_MSGBOX_BUTTON_CUSTOM);
}

static int __xuiMsgBoxPresetButtonTitleIndex(int iTitle)
{
	switch ( iTitle ) {
	case XUI_MSGBOX_BUTTON_TITLE_OK:
		return 0;
	case XUI_MSGBOX_BUTTON_TITLE_CANCEL:
		return 1;
	case XUI_MSGBOX_BUTTON_TITLE_YES:
		return 2;
	case XUI_MSGBOX_BUTTON_TITLE_NO:
		return 3;
	default:
		return -1;
	}
}

static const char* __xuiMsgBoxDefaultPresetButtonTitle(int iTitle)
{
	switch ( iTitle ) {
	case XUI_MSGBOX_BUTTON_TITLE_CANCEL:
		return "Cancel";
	case XUI_MSGBOX_BUTTON_TITLE_YES:
		return "Yes";
	case XUI_MSGBOX_BUTTON_TITLE_NO:
		return "No";
	case XUI_MSGBOX_BUTTON_TITLE_OK:
	default:
		return "OK";
	}
}

static int __xuiMsgBoxSemanticValid(int iSemantic)
{
	return (iSemantic == XUI_BUTTON_SEMANTIC_DEFAULT) ||
	       (iSemantic == XUI_BUTTON_SEMANTIC_PRIMARY) ||
	       (iSemantic == XUI_BUTTON_SEMANTIC_DANGER);
}

static char* __xuiMsgBoxDupText(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static int __xuiMsgBoxReplaceText(char** ppText, const char* sText)
{
	char* sCopy;

	if ( ppText == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = __xuiMsgBoxDupText(sText);
	if ( sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( *ppText != NULL ) {
		xrtFree(*ppText);
	}
	*ppText = sCopy;
	return XUI_OK;
}

static void __xuiMsgBoxClearButtons(xui_msgbox pBox)
{
	int i;

	if ( pBox == NULL ) {
		return;
	}
	for ( i = 0; i < XUI_MSGBOX_BUTTON_CAPACITY; i++ ) {
		if ( pBox->arrButtonText[i] != NULL ) {
			xrtFree(pBox->arrButtonText[i]);
			pBox->arrButtonText[i] = NULL;
		}
		pBox->arrButtonResult[i] = XUI_MSGBOX_RESULT_CLOSE;
		pBox->arrButtonSemantic[i] = XUI_BUTTON_SEMANTIC_DEFAULT;
		if ( pBox->arrButtons[i] != NULL ) {
			(void)xuiWidgetSetVisible(pBox->arrButtons[i], 0);
			(void)xuiWidgetSetEnabled(pBox->arrButtons[i], 0);
		}
	}
	pBox->iButtonCount = 0;
}

static void __xuiMsgBoxDefaultMetrics(xui_msgbox_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fPaddingX = 18.0f;
	pMetrics->fPaddingY = 16.0f;
	pMetrics->fIconSize = 38.0f;
	pMetrics->fIconGap = 14.0f;
	pMetrics->fButtonWidth = 78.0f;
	pMetrics->fButtonHeight = 28.0f;
	pMetrics->fButtonGap = 8.0f;
	pMetrics->fFooterHeight = 50.0f;
	pMetrics->fMinMessageHeight = 38.0f;
	pMetrics->fMinWidth = 320.0f;
	pMetrics->fMaxWidth = 680.0f;
	pMetrics->fMaxWidthRatio = 0.8f;
}

static void __xuiMsgBoxDefaultColors(xui_msgbox_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackdropColor = XUI_COLOR_RGBA(18, 42, 66, 90);
	pColors->iClientColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pColors->iMessageColor = XUI_COLOR_RGBA(34, 52, 78, 255);
	pColors->iMutedTextColor = XUI_COLOR_RGBA(93, 113, 138, 255);
	pColors->iIconColor = XUI_COLOR_RGBA(35, 132, 214, 255);
	pColors->iButtonColor = XUI_COLOR_RGBA(247, 251, 255, 255);
	pColors->iButtonHoverColor = XUI_COLOR_RGBA(232, 243, 253, 255);
	pColors->iButtonActiveColor = XUI_COLOR_RGBA(212, 231, 248, 255);
	pColors->iButtonFocusColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	pColors->iButtonDisabledColor = XUI_COLOR_RGBA(226, 235, 243, 255);
}

static int __xuiMsgBoxMetricsValid(const xui_msgbox_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	return __xuiMsgBoxFloatValid(pMetrics->fPaddingX) &&
	       __xuiMsgBoxFloatValid(pMetrics->fPaddingY) &&
	       __xuiMsgBoxFloatValid(pMetrics->fIconSize) &&
	       __xuiMsgBoxFloatValid(pMetrics->fIconGap) &&
	       __xuiMsgBoxFloatValid(pMetrics->fButtonWidth) &&
	       __xuiMsgBoxFloatValid(pMetrics->fButtonHeight) &&
	       __xuiMsgBoxFloatValid(pMetrics->fButtonGap) &&
	       __xuiMsgBoxFloatValid(pMetrics->fFooterHeight) &&
	       __xuiMsgBoxFloatValid(pMetrics->fMinMessageHeight) &&
	       __xuiMsgBoxFloatValid(pMetrics->fMinWidth) &&
	       __xuiMsgBoxFloatValid(pMetrics->fMaxWidth) &&
	       (pMetrics->fMaxWidthRatio >= 0.1f) &&
	       (pMetrics->fMaxWidthRatio <= 1.0f);
}

static int __xuiMsgBoxColorsValid(const xui_msgbox_colors_t* pColors)
{
	if ( pColors == NULL ) return 0;
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static int __xuiMsgBoxDescValid(const xui_msgbox_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( !__xuiMsgBoxTypeValid(pDesc->iType) ) return 0;
	if ( !__xuiMsgBoxButtonsValid(pDesc->iButtons) ) return 0;
	if ( (pDesc->iCustomButtonCount < 0) || (pDesc->iCustomButtonCount > XUI_MSGBOX_BUTTON_CAPACITY) ) return 0;
	if ( (pDesc->iCustomButtonCount > 0) && (pDesc->pCustomButtons == NULL) ) return 0;
	if ( (pDesc->tIconSrc.fW < 0.0f) || (pDesc->tIconSrc.fH < 0.0f) ) return 0;
	if ( pDesc->bHasMetrics && !__xuiMsgBoxMetricsValid(&pDesc->tMetrics) ) return 0;
	if ( pDesc->bHasColors && !__xuiMsgBoxColorsValid(&pDesc->tColors) ) return 0;
	return 1;
}

static xui_font __xuiMsgBoxFont(xui_msgbox pBox)
{
	if ( pBox == NULL ) return NULL;
	return (pBox->pFont != NULL) ? pBox->pFont : xuiGetDefaultFont(pBox->pContext);
}

static const char* __xuiMsgBoxIconName(int iType)
{
	switch ( iType ) {
	case XUI_MSGBOX_ICON_INFO:
		return "msgbox_info";
	case XUI_MSGBOX_ICON_QUEST:
		return "msgbox_quest";
	case XUI_MSGBOX_ICON_WAR:
		return "msgbox_war";
	case XUI_MSGBOX_ICON_ERROR:
		return "msgbox_error";
	default:
		return NULL;
	}
}

static const char* __xuiMsgBoxIconFallbackText(int iType)
{
	switch ( iType ) {
	case XUI_MSGBOX_ICON_QUEST:
		return "?";
	case XUI_MSGBOX_ICON_WAR:
		return "!";
	case XUI_MSGBOX_ICON_ERROR:
		return "x";
	case XUI_MSGBOX_ICON_INFO:
	default:
		return "i";
	}
}

static uint32_t __xuiMsgBoxIconColor(int iType)
{
	switch ( iType ) {
	case XUI_MSGBOX_ICON_WAR:
		return XUI_COLOR_RGBA(219, 141, 36, 255);
	case XUI_MSGBOX_ICON_ERROR:
		return XUI_COLOR_RGBA(210, 70, 84, 255);
	case XUI_MSGBOX_ICON_QUEST:
		return XUI_COLOR_RGBA(59, 126, 204, 255);
	case XUI_MSGBOX_ICON_INFO:
	default:
		return XUI_COLOR_RGBA(35, 132, 214, 255);
	}
}

static xui_rect_t __xuiMsgBoxRootRect(xui_msgbox pBox)
{
	xui_widget pRoot;
	xui_rect_t tRect;

	tRect = (xui_rect_t){0.0f, 0.0f, 640.0f, 420.0f};
	if ( pBox == NULL ) {
		return tRect;
	}
	pRoot = xuiGetRootWidget(pBox->pContext);
	if ( pRoot != NULL ) {
		tRect = xuiWidgetGetRect(pRoot);
		if ( (tRect.fW <= 1.0f) || (tRect.fH <= 1.0f) ) {
			tRect = (xui_rect_t){0.0f, 0.0f, 640.0f, 420.0f};
		}
	}
	return tRect;
}

static int __xuiMsgBoxRectSame(xui_rect_t tA, xui_rect_t tB)
{
	float fDX;
	float fDY;
	float fDW;
	float fDH;

	fDX = tA.fX - tB.fX;
	fDY = tA.fY - tB.fY;
	fDW = tA.fW - tB.fW;
	fDH = tA.fH - tB.fH;
	if ( fDX < 0.0f ) fDX = -fDX;
	if ( fDY < 0.0f ) fDY = -fDY;
	if ( fDW < 0.0f ) fDW = -fDW;
	if ( fDH < 0.0f ) fDH = -fDH;
	return (fDX < 0.05f) && (fDY < 0.05f) && (fDW < 0.05f) && (fDH < 0.05f);
}

static int __xuiMsgBoxSetTreeLayer(xui_widget pWidget, int iLayer, int iZIndex)
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
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiMsgBoxSetTreeLayer(pChild, iLayer, iZIndex);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiMsgBoxMeasureText(xui_msgbox pBox, float fWidth, xui_vec2_t* pSize, int* pLineCount)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	xui_font pFont;
	int iRet;

	if ( (pBox == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSize->fX = 0.0f;
	pSize->fY = 0.0f;
	if ( pLineCount != NULL ) {
		*pLineCount = 0;
	}
	pFont = __xuiMsgBoxFont(pBox);
	if ( pFont == NULL ) {
		if ( pBox->sMessage != NULL ) {
			pSize->fX = __xuiMsgBoxMin((float)strlen(pBox->sMessage) * 7.0f, fWidth);
		}
		pSize->fY = 18.0f;
		if ( pLineCount != NULL ) *pLineCount = 1;
		return XUI_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = (pBox->sMessage != NULL) ? pBox->sMessage : "";
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = fWidth;
	tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pBox->pContext, &pLayout, &tDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	*pSize = xuiTextLayoutGetSize(pLayout);
	if ( pLineCount != NULL ) {
		*pLineCount = xuiTextLayoutGetLineCount(pLayout);
	}
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static int __xuiMsgBoxHasIcon(xui_msgbox pBox)
{
	if ( pBox == NULL ) return 0;
	if ( pBox->bCustomIcon && (pBox->pIconSurface != NULL) ) return 1;
	return pBox->iType != XUI_MSGBOX_ICON_NONE;
}

static const char* __xuiMsgBoxPresetButtonTitle(xui_msgbox pBox, int iTitle)
{
	int iIndex;

	iIndex = __xuiMsgBoxPresetButtonTitleIndex(iTitle);
	if ( (pBox != NULL) && (iIndex >= 0) && (pBox->arrPresetButtonTitle[iIndex] != NULL) ) {
		return pBox->arrPresetButtonTitle[iIndex];
	}
	return __xuiMsgBoxDefaultPresetButtonTitle(iTitle);
}

static int __xuiMsgBoxSetButtonData(xui_msgbox pBox, int iIndex, const char* sText, int iResult, int iSemantic)
{
	int iRet;

	if ( (pBox == NULL) || (iIndex < 0) || (iIndex >= XUI_MSGBOX_BUTTON_CAPACITY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiMsgBoxSemanticValid(iSemantic) ) {
		iSemantic = XUI_BUTTON_SEMANTIC_DEFAULT;
	}
	iRet = __xuiMsgBoxReplaceText(&pBox->arrButtonText[iIndex], sText);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pBox->arrButtonResult[iIndex] = iResult;
	pBox->arrButtonSemantic[iIndex] = iSemantic;
	if ( pBox->arrButtons[iIndex] != NULL ) {
		(void)xuiButtonSetText(pBox->arrButtons[iIndex], pBox->arrButtonText[iIndex]);
		(void)xuiButtonSetSemantic(pBox->arrButtons[iIndex], iSemantic);
		(void)xuiWidgetSetVisible(pBox->arrButtons[iIndex], 1);
		(void)xuiWidgetSetEnabled(pBox->arrButtons[iIndex], 1);
	}
	if ( iIndex + 1 > pBox->iButtonCount ) {
		pBox->iButtonCount = iIndex + 1;
	}
	return XUI_OK;
}

static int __xuiMsgBoxApplyPresetButtons(xui_msgbox pBox, int iButtons)
{
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) || !__xuiMsgBoxButtonsValid(iButtons) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiMsgBoxClearButtons(pBox);
	pBox->iButtons = iButtons;
	if ( iButtons == XUI_MSGBOX_BUTTON_OK ) {
		iRet = __xuiMsgBoxSetButtonData(pBox, 0, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_OK), XUI_MSGBOX_RESULT_OK, XUI_BUTTON_SEMANTIC_PRIMARY);
	} else if ( iButtons == XUI_MSGBOX_BUTTON_OK_CANCEL ) {
		iRet = __xuiMsgBoxSetButtonData(pBox, 0, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_OK), XUI_MSGBOX_RESULT_OK, XUI_BUTTON_SEMANTIC_PRIMARY);
		if ( iRet == XUI_OK ) iRet = __xuiMsgBoxSetButtonData(pBox, 1, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_CANCEL), XUI_MSGBOX_RESULT_CANCEL, XUI_BUTTON_SEMANTIC_DEFAULT);
	} else if ( iButtons == XUI_MSGBOX_BUTTON_YES_NO ) {
		iRet = __xuiMsgBoxSetButtonData(pBox, 0, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_YES), XUI_MSGBOX_RESULT_YES, XUI_BUTTON_SEMANTIC_PRIMARY);
		if ( iRet == XUI_OK ) iRet = __xuiMsgBoxSetButtonData(pBox, 1, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_NO), XUI_MSGBOX_RESULT_NO, XUI_BUTTON_SEMANTIC_DEFAULT);
	} else if ( iButtons == XUI_MSGBOX_BUTTON_YES_NO_CANCEL ) {
		iRet = __xuiMsgBoxSetButtonData(pBox, 0, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_YES), XUI_MSGBOX_RESULT_YES, XUI_BUTTON_SEMANTIC_PRIMARY);
		if ( iRet == XUI_OK ) iRet = __xuiMsgBoxSetButtonData(pBox, 1, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_NO), XUI_MSGBOX_RESULT_NO, XUI_BUTTON_SEMANTIC_DEFAULT);
		if ( iRet == XUI_OK ) iRet = __xuiMsgBoxSetButtonData(pBox, 2, __xuiMsgBoxPresetButtonTitle(pBox, XUI_MSGBOX_BUTTON_TITLE_CANCEL), XUI_MSGBOX_RESULT_CANCEL, XUI_BUTTON_SEMANTIC_DEFAULT);
	} else {
		iRet = XUI_OK;
	}
	return iRet;
}

static void __xuiMsgBoxApplyButtonVisual(xui_msgbox pBox)
{
	int i;

	if ( pBox == NULL ) {
		return;
	}
	for ( i = 0; i < XUI_MSGBOX_BUTTON_CAPACITY; i++ ) {
		if ( pBox->arrButtons[i] == NULL ) {
			continue;
		}
		(void)xuiButtonSetFont(pBox->arrButtons[i], __xuiMsgBoxFont(pBox));
		(void)xuiButtonSetBorder(pBox->arrButtons[i], 1.0f, XUI_COLOR_RGBA(166, 196, 224, 255));
		if ( pBox->arrButtonSemantic[i] == XUI_BUTTON_SEMANTIC_DEFAULT ) {
			(void)xuiButtonSetColors(pBox->arrButtons[i],
				pBox->tColors.iButtonColor,
				pBox->tColors.iButtonHoverColor,
				pBox->tColors.iButtonActiveColor,
				pBox->tColors.iButtonFocusColor,
				pBox->tColors.iButtonDisabledColor);
		}
	}
}

static int __xuiMsgBoxLayout(xui_msgbox pBox)
{
	xui_rect_t tRoot;
	xui_rect_t tWin;
	xui_vec2_t tTextSize;
	float fTitleBar;
	float fBorder;
	float fResizeGrip;
	float fButtonSize;
	float fRootMaxWidth;
	float fMaxWidth;
	float fWidth;
	float fClientW;
	float fClientH;
	float fIconBlock;
	float fMessageW;
	float fMessageH;
	float fRowH;
	float fFooterTop;
	float fButtonsW;
	float fButtonsX;
	float fButtonsY;
	float fWindowH;
	int bIcon;
	int i;
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tRoot = __xuiMsgBoxRootRect(pBox);
	fTitleBar = 30.0f;
	fBorder = 1.0f;
	fResizeGrip = 0.0f;
	fButtonSize = 28.0f;
	(void)xuiWindowGetChrome(pBox->pWindow, &fTitleBar, &fBorder, &fResizeGrip, &fButtonSize);
	(void)fResizeGrip;
	(void)fButtonSize;
	bIcon = __xuiMsgBoxHasIcon(pBox);
	fIconBlock = bIcon ? (pBox->tMetrics.fIconSize + pBox->tMetrics.fIconGap) : 0.0f;
	fRootMaxWidth = tRoot.fW * pBox->tMetrics.fMaxWidthRatio;
	fMaxWidth = __xuiMsgBoxMin(pBox->tMetrics.fMaxWidth, fRootMaxWidth);
	if ( fMaxWidth < 220.0f ) fMaxWidth = __xuiMsgBoxMin(tRoot.fW - 24.0f, pBox->tMetrics.fMaxWidth);
	if ( fMaxWidth < 220.0f ) fMaxWidth = 220.0f;
	fMessageW = __xuiMsgBoxMax(120.0f, fMaxWidth - pBox->tMetrics.fPaddingX * 2.0f - fIconBlock);
	iRet = __xuiMsgBoxMeasureText(pBox, fMessageW, &tTextSize, &pBox->iWrapLineCount);
	if ( iRet != XUI_OK ) {
		tTextSize.fX = fMessageW;
		tTextSize.fY = pBox->tMetrics.fMinMessageHeight;
		pBox->iWrapLineCount = 1;
	}
	fButtonsW = 0.0f;
	if ( pBox->iButtonCount > 0 ) {
		fButtonsW = (float)pBox->iButtonCount * pBox->tMetrics.fButtonWidth + (float)(pBox->iButtonCount - 1) * pBox->tMetrics.fButtonGap;
	}
	fWidth = pBox->tMetrics.fPaddingX * 2.0f + fIconBlock + __xuiMsgBoxMax(tTextSize.fX, fButtonsW);
	fWidth = __xuiMsgBoxClamp(fWidth, pBox->tMetrics.fMinWidth, fMaxWidth);
	fClientW = __xuiMsgBoxMax(1.0f, fWidth - fBorder * 2.0f);
	fMessageW = __xuiMsgBoxMax(80.0f, fClientW - pBox->tMetrics.fPaddingX * 2.0f - fIconBlock);
	iRet = __xuiMsgBoxMeasureText(pBox, fMessageW, &tTextSize, &pBox->iWrapLineCount);
	if ( iRet != XUI_OK ) {
		tTextSize.fX = fMessageW;
		tTextSize.fY = pBox->tMetrics.fMinMessageHeight;
		pBox->iWrapLineCount = 1;
	}
	fMessageH = __xuiMsgBoxMax(tTextSize.fY, pBox->tMetrics.fMinMessageHeight);
	fRowH = __xuiMsgBoxMax(fMessageH, bIcon ? pBox->tMetrics.fIconSize : 0.0f);
	fClientH = pBox->tMetrics.fPaddingY + fRowH + pBox->tMetrics.fFooterHeight;
	fWindowH = fTitleBar + fClientH + fBorder * 2.0f;
	tWin.fW = fWidth;
	tWin.fH = fWindowH;
	tWin.fX = tRoot.fX + (tRoot.fW - tWin.fW) * 0.5f;
	tWin.fY = tRoot.fY + (tRoot.fH - tWin.fH) * 0.5f;
	tWin = xuiInternalSnapRect(tWin);
	iRet = xuiWidgetSetRect(pBox->pWindow, tWin);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWindowSetMinSize(pBox->pWindow, pBox->tMetrics.fMinWidth, 120.0f);
	(void)xuiWidgetSetRect(pBox->pContent, (xui_rect_t){0.0f, 0.0f, fClientW, fClientH});
	if ( bIcon ) {
		pBox->tIconRect.fX = pBox->tMetrics.fPaddingX;
		pBox->tIconRect.fY = pBox->tMetrics.fPaddingY + (fRowH - pBox->tMetrics.fIconSize) * 0.5f;
		pBox->tIconRect.fW = pBox->tMetrics.fIconSize;
		pBox->tIconRect.fH = pBox->tMetrics.fIconSize;
	} else {
		pBox->tIconRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	pBox->tMessageRect.fX = pBox->tMetrics.fPaddingX + fIconBlock;
	pBox->tMessageRect.fY = pBox->tMetrics.fPaddingY + (fRowH - fMessageH) * 0.5f;
	pBox->tMessageRect.fW = fMessageW;
	pBox->tMessageRect.fH = fMessageH;
	fFooterTop = fClientH - pBox->tMetrics.fFooterHeight;
	fButtonsX = fClientW - pBox->tMetrics.fPaddingX - fButtonsW;
	fButtonsY = fFooterTop + (pBox->tMetrics.fFooterHeight - pBox->tMetrics.fButtonHeight) * 0.5f;
	for ( i = 0; i < XUI_MSGBOX_BUTTON_CAPACITY; i++ ) {
		if ( i < pBox->iButtonCount ) {
			pBox->arrButtonRect[i] = (xui_rect_t){
				fButtonsX + (float)i * (pBox->tMetrics.fButtonWidth + pBox->tMetrics.fButtonGap),
				fButtonsY,
				pBox->tMetrics.fButtonWidth,
				pBox->tMetrics.fButtonHeight
			};
			if ( pBox->arrButtons[i] != NULL ) {
				(void)xuiWidgetSetRect(pBox->arrButtons[i], pBox->arrButtonRect[i]);
				(void)xuiWidgetSetVisible(pBox->arrButtons[i], 1);
				(void)xuiWidgetSetEnabled(pBox->arrButtons[i], 1);
			}
		} else {
			pBox->arrButtonRect[i] = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
			if ( pBox->arrButtons[i] != NULL ) {
				(void)xuiWidgetSetVisible(pBox->arrButtons[i], 0);
				(void)xuiWidgetSetEnabled(pBox->arrButtons[i], 0);
			}
		}
	}
	pBox->tBackdropRect = tRoot;
	(void)xuiWidgetSetRect(pBox->pBackdrop, tRoot);
	(void)xuiWidgetInvalidate(pBox->pWindow, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	(void)xuiWidgetInvalidate(pBox->pContent, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	(void)xuiWidgetInvalidate(pBox->pBackdrop, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

static int __xuiMsgBoxSyncLayout(xui_msgbox pBox)
{
	xui_rect_t tRoot;

	if ( !__xuiMsgBoxValid(pBox) || !pBox->bOpen ) {
		return XUI_OK;
	}
	tRoot = __xuiMsgBoxRootRect(pBox);
	if ( __xuiMsgBoxRectSame(tRoot, pBox->tBackdropRect) ) {
		return XUI_OK;
	}
	return __xuiMsgBoxLayout(pBox);
}

static int __xuiMsgBoxWindowUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	(void)pWidget;
	(void)fDelta;
	return __xuiMsgBoxSyncLayout((xui_msgbox)pUser);
}

static int __xuiMsgBoxDrawTextLayout(xui_msgbox pBox, xui_draw_context pDraw)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	xui_text_line_t tLine;
	xui_proxy pProxy;
	xui_font pFont;
	const char* sText;
	char* sLine;
	xui_rect_t tLineRect;
	int i;
	int iRet;

	pProxy = xuiInternalContextGetProxy(pBox->pContext);
	pFont = __xuiMsgBoxFont(pBox);
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pDraw == NULL) || (pFont == NULL) ) {
		return XUI_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = (pBox->sMessage != NULL) ? pBox->sMessage : "";
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = pBox->tMessageRect.fW;
	tDesc.fMaxHeight = pBox->tMessageRect.fH;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pBox->pContext, &pLayout, &tDesc);
	if ( iRet != XUI_OK ) {
		return XUI_OK;
	}
	sText = xuiTextLayoutGetText(pLayout);
	for ( i = 0; i < xuiTextLayoutGetLineCount(pLayout); i++ ) {
		memset(&tLine, 0, sizeof(tLine));
		tLine.iSize = sizeof(tLine);
		if ( xuiTextLayoutGetLine(pLayout, i, &tLine) != XUI_OK || tLine.iTextSize <= 0 ) {
			continue;
		}
		sLine = (char*)xrtMalloc((size_t)tLine.iTextSize + 1u);
		if ( sLine == NULL ) {
			xuiTextLayoutDestroy(pLayout);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(sLine, sText + tLine.iTextOffset, (size_t)tLine.iTextSize);
		sLine[tLine.iTextSize] = 0;
		tLineRect.fX = pBox->tMessageRect.fX;
		tLineRect.fY = pBox->tMessageRect.fY + tLine.fY;
		tLineRect.fW = pBox->tMessageRect.fW;
		tLineRect.fH = tLine.fH;
		(void)pProxy->drawText(pProxy, pDraw, pFont, sLine, xuiInternalSnapRect(tLineRect), pBox->tColors.iMessageColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		xrtFree(sLine);
	}
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static int __xuiMsgBoxDrawIconFallback(xui_msgbox pBox, xui_draw_context pDraw, xui_proxy pProxy)
{
	xui_rect_t tIcon;
	xui_rect_t tText;
	uint32_t iColor;

	if ( (pBox == NULL) || (pProxy == NULL) || (pDraw == NULL) ) {
		return XUI_OK;
	}
	tIcon = xuiInternalSnapRect(pBox->tIconRect);
	iColor = pBox->tColors.iIconColor;
	if ( pProxy->drawCircleFill != NULL ) {
		(void)pProxy->drawCircleFill(pProxy, pDraw, tIcon.fX + tIcon.fW * 0.5f, tIcon.fY + tIcon.fH * 0.5f, tIcon.fW * 0.5f, iColor);
	} else if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, tIcon, iColor);
	} else if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, tIcon, iColor);
	}
	if ( pProxy->drawText != NULL ) {
		tText = tIcon;
		(void)pProxy->drawText(pProxy, pDraw, __xuiMsgBoxFont(pBox), __xuiMsgBoxIconFallbackText(pBox->iType), tText, XUI_COLOR_WHITE, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return XUI_OK;
}

static int __xuiMsgBoxDrawIcon(xui_msgbox pBox, xui_draw_context pDraw, xui_proxy pProxy)
{
	xui_surface pSurface;
	xui_rect_t tSrc;
	const char* sName;
	int iRet;

	if ( !__xuiMsgBoxHasIcon(pBox) ) {
		return XUI_OK;
	}
	if ( (pProxy == NULL) || (pProxy->drawSurface == NULL) ) {
		return __xuiMsgBoxDrawIconFallback(pBox, pDraw, pProxy);
	}
	pSurface = NULL;
	tSrc = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	if ( pBox->bCustomIcon && pBox->pIconSurface != NULL ) {
		pSurface = pBox->pIconSurface;
		tSrc = pBox->tIconSrc;
	} else if ( pBox->bBuiltinIcon ) {
		sName = __xuiMsgBoxIconName(pBox->iType);
		if ( sName != NULL ) {
			iRet = xuiBuiltinAssetGetAtlas(pBox->pContext, &pSurface);
			if ( iRet == XUI_OK ) {
				iRet = xuiBuiltinAssetGetRect(sName, &tSrc);
			}
			if ( iRet != XUI_OK ) {
				pSurface = NULL;
			}
		}
	}
	if ( pSurface == NULL ) {
		return __xuiMsgBoxDrawIconFallback(pBox, pDraw, pProxy);
	}
	return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, xuiInternalSnapRect(pBox->tIconRect), XUI_COLOR_WHITE, 0);
}

static int __xuiMsgBoxContentRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_msgbox pBox;
	xui_proxy pProxy;

	(void)pWidget;
	(void)iStateId;
	pBox = (xui_msgbox)pUser;
	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pBox->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	(void)__xuiMsgBoxDrawIcon(pBox, pDraw, pProxy);
	return __xuiMsgBoxDrawTextLayout(pBox, pDraw);
}

static int __xuiMsgBoxBackdropRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_msgbox pBox;
	xui_proxy pProxy;
	xui_rect_t tRect;

	(void)iStateId;
	pBox = (xui_msgbox)pUser;
	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pBox->pContext);
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) {
		return XUI_OK;
	}
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), pBox->tColors.iBackdropColor);
}

static int __xuiMsgBoxBackdropEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_msgbox pBox;

	(void)pWidget;
	pBox = (xui_msgbox)pUser;
	if ( !__xuiMsgBoxValid(pBox) || (pEvent == NULL) || !pBox->bOpen || !pBox->bModal ) {
		return XUI_OK;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_DOWN ) {
		(void)xuiSetFocusWidget(pBox->pContext, pBox->pWindow);
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_POINTER_DOWN:
	case XUI_EVENT_POINTER_UP:
	case XUI_EVENT_POINTER_CLICK:
	case XUI_EVENT_POINTER_WHEEL:
	case XUI_EVENT_KEY_DOWN:
	case XUI_EVENT_KEY_UP:
	case XUI_EVENT_TEXT:
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiMsgBoxWindowEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_msgbox pBox;
	int iRet;
	int bSkipWindowEvent;

	pBox = (xui_msgbox)pUser;
	iRet = XUI_OK;
	bSkipWindowEvent = (pEvent != NULL) &&
		(pEvent->iType == XUI_EVENT_POINTER_DOWN) &&
		(pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) &&
		(pEvent->pTarget != pWidget);
	if ( !bSkipWindowEvent && __xuiMsgBoxValid(pBox) && (pBox->onWindowEvent != NULL) ) {
		iRet = pBox->onWindowEvent(pWidget, pEvent, pBox->pWindowEventUser);
		if ( iRet < 0 ) {
			return iRet;
		}
	}
	if ( __xuiMsgBoxValid(pBox) && pBox->bOpen && pBox->bModal ) {
		(void)__xuiMsgBoxSetTreeLayer(pWidget, XUI_LAYER_MODAL, 1);
	}
	return iRet;
}

static void __xuiMsgBoxCloseResult(xui_msgbox pBox, int iResult)
{
	if ( !__xuiMsgBoxValid(pBox) ) {
		return;
	}
	pBox->iResult = iResult;
	pBox->iResultCount++;
	(void)xuiMsgBoxSetOpen(pBox, 0);
	if ( pBox->onResult != NULL ) {
		pBox->onResult(pBox, iResult, pBox->pResultUser);
	}
}

static void __xuiMsgBoxWindowClose(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	__xuiMsgBoxCloseResult((xui_msgbox)pUser, XUI_MSGBOX_RESULT_CLOSE);
}

static void __xuiMsgBoxDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_msgbox pBox;

	(void)pWidget;
	pBox = (xui_msgbox)pUser;
	if ( !__xuiMsgBoxValid(pBox) ) {
		return;
	}
	__xuiMsgBoxCloseResult(pBox, (pBox->iButtonCount > 0) ? pBox->arrButtonResult[0] : XUI_MSGBOX_RESULT_OK);
}

static void __xuiMsgBoxCancelAction(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	__xuiMsgBoxCloseResult((xui_msgbox)pUser, XUI_MSGBOX_RESULT_CLOSE);
}

static void __xuiMsgBoxButtonClick(xui_widget pWidget, void* pUser)
{
	xui_msgbox pBox;
	int i;

	pBox = (xui_msgbox)pUser;
	if ( !__xuiMsgBoxValid(pBox) ) {
		return;
	}
	for ( i = 0; i < pBox->iButtonCount; i++ ) {
		if ( pBox->arrButtons[i] == pWidget ) {
			__xuiMsgBoxCloseResult(pBox, pBox->arrButtonResult[i]);
			return;
		}
	}
}

static int __xuiMsgBoxApplyOverlay(xui_msgbox pBox)
{
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pBox->bOpen && pBox->bModal ) {
		iRet = xuiWidgetSetVisible(pBox->pBackdrop, 1);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetSetEnabled(pBox->pBackdrop, 1);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiOverlayAttach(pBox->pContext, pBox->pWindow, pBox->pBackdrop, XUI_LAYER_MODAL, 0);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiMsgBoxSetTreeLayer(pBox->pWindow, XUI_LAYER_MODAL, 1);
		if ( iRet != XUI_OK ) return iRet;
		return xuiOverlayBringToFront(pBox->pWindow);
	}
	(void)xuiWidgetSetVisible(pBox->pBackdrop, 0);
	(void)xuiWidgetSetEnabled(pBox->pBackdrop, 0);
	if ( xuiWidgetGetParent(pBox->pBackdrop) != NULL ) {
		(void)xuiOverlayDetach(pBox->pBackdrop);
	}
	return XUI_OK;
}

XUI_API int xuiMsgBoxCreate(xui_context pContext, xui_msgbox* ppBox, const xui_msgbox_desc_t* pDesc)
{
	xui_msgbox pBox;
	xui_window_desc_t tWindowDesc;
	xui_button_desc_t tButtonDesc;
	int i;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppBox == NULL) || !__xuiMsgBoxDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppBox = NULL;
	pBox = (xui_msgbox)xrtCalloc(1, sizeof(*pBox));
	if ( pBox == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pBox->iMagic = XUI_MSGBOX_MAGIC;
	pBox->pContext = pContext;
	pBox->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	__xuiMsgBoxDefaultMetrics(&pBox->tMetrics);
	__xuiMsgBoxDefaultColors(&pBox->tColors);
	if ( pDesc != NULL && pDesc->bHasMetrics ) pBox->tMetrics = pDesc->tMetrics;
	if ( pDesc != NULL && pDesc->bHasColors ) pBox->tColors = pDesc->tColors;
	pBox->tMetrics.iSize = sizeof(pBox->tMetrics);
	pBox->tColors.iSize = sizeof(pBox->tColors);
	pBox->iType = (pDesc != NULL) ? pDesc->iType : XUI_MSGBOX_ICON_INFO;
	pBox->iButtons = (pDesc != NULL) ? pDesc->iButtons : XUI_MSGBOX_BUTTON_OK;
	pBox->iResult = XUI_MSGBOX_RESULT_CLOSE;
	pBox->bModal = (pDesc != NULL && pDesc->bHasModal) ? (pDesc->bModal ? 1 : 0) : 1;
	pBox->bBuiltinIcon = 1;
	if ( pDesc != NULL && pDesc->bHasCustomIcon && pDesc->pIconSurface != NULL ) {
		pBox->pIconSurface = pDesc->pIconSurface;
		pBox->tIconSrc = pDesc->tIconSrc;
		pBox->bCustomIcon = 1;
		pBox->bBuiltinIcon = 0;
	}
	pBox->tColors.iIconColor = __xuiMsgBoxIconColor(pBox->iType);
	iRet = __xuiMsgBoxReplaceText(&pBox->sTitle, (pDesc != NULL && pDesc->sTitle != NULL) ? pDesc->sTitle : "Message");
	if ( iRet == XUI_OK ) iRet = __xuiMsgBoxReplaceText(&pBox->sMessage, (pDesc != NULL && pDesc->sMessage != NULL) ? pDesc->sMessage : "");
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	memset(&tWindowDesc, 0, sizeof(tWindowDesc));
	tWindowDesc.iSize = sizeof(tWindowDesc);
	tWindowDesc.sTitle = pBox->sTitle;
	tWindowDesc.pFont = pBox->pFont;
	tWindowDesc.bClosed = 1;
	tWindowDesc.bTopMost = 1;
	tWindowDesc.bNotResizable = 1;
	tWindowDesc.bHideCollapse = 1;
	tWindowDesc.bHideMaximize = 1;
	tWindowDesc.fTitleBarHeight = 30.0f;
	tWindowDesc.fBorderWidth = 1.0f;
	tWindowDesc.fButtonSize = 20.0f;
	tWindowDesc.iBackgroundColor = XUI_COLOR_RGBA(245, 250, 254, 255);
	tWindowDesc.iClientColor = pBox->tColors.iClientColor;
	tWindowDesc.iTitleBarColor = XUI_COLOR_RGBA(231, 241, 250, 255);
	tWindowDesc.iInactiveTitleBarColor = XUI_COLOR_RGBA(236, 244, 251, 255);
	tWindowDesc.iTitleTextColor = XUI_COLOR_RGBA(35, 54, 80, 255);
	tWindowDesc.iInactiveTitleTextColor = XUI_COLOR_RGBA(91, 108, 130, 255);
	tWindowDesc.iBorderColor = XUI_COLOR_RGBA(135, 173, 211, 255);
	tWindowDesc.iActiveBorderColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	tWindowDesc.iButtonColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tWindowDesc.iButtonHoverColor = XUI_COLOR_RGBA(218, 234, 249, 255);
	tWindowDesc.iButtonActiveColor = XUI_COLOR_RGBA(200, 222, 244, 255);
	iRet = xuiWindowCreate(pContext, &pBox->pWindow, &tWindowDesc);
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	(void)xuiWindowSetClose(pBox->pWindow, __xuiMsgBoxWindowClose, pBox);
	(void)xuiWidgetSetDefaultAction(pBox->pWindow, __xuiMsgBoxDefaultAction, pBox);
	(void)xuiWidgetSetCancelAction(pBox->pWindow, __xuiMsgBoxCancelAction, pBox);
	(void)xuiWidgetGetEventCallback(pBox->pWindow, &pBox->onWindowEvent, &pBox->pWindowEventUser);
	(void)xuiWidgetSetEventCallback(pBox->pWindow, __xuiMsgBoxWindowEvent, pBox);
	pBox->pWindow->onUpdate = __xuiMsgBoxWindowUpdate;
	pBox->pWindow->pUpdateUser = pBox;
	(void)xuiWidgetSetLayoutType(xuiWindowGetClientWidget(pBox->pWindow), XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(xuiWindowGetClientWidget(pBox->pWindow), XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetPadding(xuiWindowGetClientWidget(pBox->pWindow), (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetGap(xuiWindowGetClientWidget(pBox->pWindow), 0.0f);
	iRet = xuiWidgetCreate(pContext, &pBox->pContent);
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	(void)xuiWidgetSetHitTestVisible(pBox->pContent, 0);
	(void)xuiWidgetSetCacheRenderCallback(pBox->pContent, __xuiMsgBoxContentRender, pBox);
	iRet = xuiWindowAddChild(pBox->pWindow, pBox->pContent);
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	memset(&tButtonDesc, 0, sizeof(tButtonDesc));
	tButtonDesc.iSize = sizeof(tButtonDesc);
	tButtonDesc.pFont = pBox->pFont;
	tButtonDesc.fBorderWidth = 1.0f;
	tButtonDesc.iBorderColor = XUI_COLOR_RGBA(166, 196, 224, 255);
	for ( i = 0; i < XUI_MSGBOX_BUTTON_CAPACITY; i++ ) {
		tButtonDesc.sText = "";
		iRet = xuiButtonCreate(pContext, &pBox->arrButtons[i], &tButtonDesc);
		if ( iRet != XUI_OK ) {
			xuiMsgBoxDestroy(pBox);
			return iRet;
		}
		(void)xuiButtonSetClick(pBox->arrButtons[i], __xuiMsgBoxButtonClick, pBox);
		(void)xuiWidgetSetVisible(pBox->arrButtons[i], 0);
		(void)xuiWidgetSetEnabled(pBox->arrButtons[i], 0);
		iRet = xuiWindowAddChild(pBox->pWindow, pBox->arrButtons[i]);
		if ( iRet != XUI_OK ) {
			xuiMsgBoxDestroy(pBox);
			return iRet;
		}
	}
	iRet = xuiWidgetCreate(pContext, &pBox->pBackdrop);
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	(void)xuiWidgetSetVisible(pBox->pBackdrop, 0);
	(void)xuiWidgetSetEnabled(pBox->pBackdrop, 0);
	(void)xuiWidgetSetFocusable(pBox->pBackdrop, 0);
	(void)xuiWidgetSetFlowMode(pBox->pBackdrop, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetCacheRenderCallback(pBox->pBackdrop, __xuiMsgBoxBackdropRender, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_ENTER, __xuiMsgBoxBackdropEvent, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_LEAVE, __xuiMsgBoxBackdropEvent, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_MOVE, __xuiMsgBoxBackdropEvent, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_DOWN, __xuiMsgBoxBackdropEvent, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_UP, __xuiMsgBoxBackdropEvent, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_CLICK, __xuiMsgBoxBackdropEvent, pBox);
	(void)xuiWidgetSetEventHandler(pBox->pBackdrop, XUI_EVENT_POINTER_WHEEL, __xuiMsgBoxBackdropEvent, pBox);
	iRet = (pDesc != NULL && (pDesc->iButtons == XUI_MSGBOX_BUTTON_CUSTOM || pDesc->iCustomButtonCount > 0)) ?
		xuiMsgBoxSetCustomButtons(pBox, pDesc->pCustomButtons, pDesc->iCustomButtonCount) :
		__xuiMsgBoxApplyPresetButtons(pBox, pBox->iButtons);
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	__xuiMsgBoxApplyButtonVisual(pBox);
	iRet = __xuiMsgBoxLayout(pBox);
	if ( iRet != XUI_OK ) {
		xuiMsgBoxDestroy(pBox);
		return iRet;
	}
	*ppBox = pBox;
	return XUI_OK;
}

XUI_API void xuiMsgBoxDestroy(xui_msgbox pBox)
{
	int i;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return;
	}
	(void)xuiMsgBoxSetOpen(pBox, 0);
	if ( pBox->pBackdrop != NULL ) {
		xuiWidgetDestroy(pBox->pBackdrop);
		pBox->pBackdrop = NULL;
	}
	if ( pBox->pWindow != NULL ) {
		xuiWidgetDestroy(pBox->pWindow);
		pBox->pWindow = NULL;
	}
	for ( i = 0; i < XUI_MSGBOX_BUTTON_CAPACITY; i++ ) {
		if ( pBox->arrButtonText[i] != NULL ) {
			xrtFree(pBox->arrButtonText[i]);
			pBox->arrButtonText[i] = NULL;
		}
	}
	for ( i = 0; i < XUI_MSGBOX_PRESET_BUTTON_TITLE_COUNT; i++ ) {
		if ( pBox->arrPresetButtonTitle[i] != NULL ) {
			xrtFree(pBox->arrPresetButtonTitle[i]);
			pBox->arrPresetButtonTitle[i] = NULL;
		}
	}
	if ( pBox->sTitle != NULL ) xrtFree(pBox->sTitle);
	if ( pBox->sMessage != NULL ) xrtFree(pBox->sMessage);
	pBox->iMagic = 0;
	xrtFree(pBox);
}

XUI_API int xuiMsgBoxSetText(xui_msgbox pBox, const char* sTitle, const char* sMessage)
{
	int iRet;

	iRet = xuiMsgBoxSetTitle(pBox, sTitle);
	if ( iRet != XUI_OK ) return iRet;
	return xuiMsgBoxSetMessage(pBox, sMessage);
}

XUI_API int xuiMsgBoxSetTitle(xui_msgbox pBox, const char* sTitle)
{
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiMsgBoxReplaceText(&pBox->sTitle, sTitle);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWindowSetTitle(pBox->pWindow, pBox->sTitle);
	if ( iRet != XUI_OK ) return iRet;
	pBox->iChangeCount++;
	return XUI_OK;
}

XUI_API const char* xuiMsgBoxGetTitle(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->sTitle : NULL;
}

XUI_API int xuiMsgBoxSetMessage(xui_msgbox pBox, const char* sMessage)
{
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiMsgBoxReplaceText(&pBox->sMessage, sMessage);
	if ( iRet != XUI_OK ) return iRet;
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API const char* xuiMsgBoxGetMessage(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->sMessage : NULL;
}

XUI_API int xuiMsgBoxSetType(xui_msgbox pBox, int iType)
{
	if ( !__xuiMsgBoxValid(pBox) || !__xuiMsgBoxTypeValid(iType) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->iType = iType;
	pBox->bBuiltinIcon = 1;
	pBox->bCustomIcon = 0;
	pBox->pIconSurface = NULL;
	pBox->tColors.iIconColor = __xuiMsgBoxIconColor(iType);
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxGetType(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->iType : XUI_MSGBOX_ICON_NONE;
}

XUI_API int xuiMsgBoxSetIconSurface(xui_msgbox pBox, xui_surface pSurface, xui_rect_t tSrc)
{
	if ( !__xuiMsgBoxValid(pBox) || (tSrc.fW < 0.0f) || (tSrc.fH < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->pIconSurface = pSurface;
	pBox->tIconSrc = tSrc;
	pBox->bCustomIcon = (pSurface != NULL);
	pBox->bBuiltinIcon = (pSurface == NULL);
	if ( pSurface == NULL ) {
		pBox->iType = XUI_MSGBOX_ICON_NONE;
	}
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxUseBuiltinIcon(xui_msgbox pBox)
{
	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->pIconSurface = NULL;
	pBox->bCustomIcon = 0;
	pBox->bBuiltinIcon = 1;
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxSetButtons(xui_msgbox pBox, int iButtons)
{
	int iRet;

	iRet = __xuiMsgBoxApplyPresetButtons(pBox, iButtons);
	if ( iRet != XUI_OK ) return iRet;
	__xuiMsgBoxApplyButtonVisual(pBox);
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxSetCustomButtons(xui_msgbox pBox, const xui_msgbox_button_t* pButtons, int iCount)
{
	int i;
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) || (iCount < 0) || (iCount > XUI_MSGBOX_BUTTON_CAPACITY) || ((iCount > 0) && (pButtons == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiMsgBoxClearButtons(pBox);
	pBox->iButtons = XUI_MSGBOX_BUTTON_CUSTOM;
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xuiMsgBoxSetButtonData(pBox, i, pButtons[i].sText, pButtons[i].iResult, pButtons[i].iSemantic);
		if ( iRet != XUI_OK ) return iRet;
	}
	__xuiMsgBoxApplyButtonVisual(pBox);
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxSetPresetButtonTitle(xui_msgbox pBox, int iTitle, const char* sText)
{
	int iIndex;
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiMsgBoxPresetButtonTitleIndex(iTitle);
	if ( iIndex < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (sText == NULL) || (sText[0] == 0) ) {
		if ( pBox->arrPresetButtonTitle[iIndex] != NULL ) {
			xrtFree(pBox->arrPresetButtonTitle[iIndex]);
			pBox->arrPresetButtonTitle[iIndex] = NULL;
		}
	} else {
		iRet = __xuiMsgBoxReplaceText(&pBox->arrPresetButtonTitle[iIndex], sText);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pBox->iButtons != XUI_MSGBOX_BUTTON_CUSTOM ) {
		iRet = __xuiMsgBoxApplyPresetButtons(pBox, pBox->iButtons);
		if ( iRet != XUI_OK ) return iRet;
		__xuiMsgBoxApplyButtonVisual(pBox);
		pBox->iChangeCount++;
		return __xuiMsgBoxLayout(pBox);
	}
	pBox->iChangeCount++;
	return XUI_OK;
}

XUI_API const char* xuiMsgBoxGetPresetButtonTitle(xui_msgbox pBox, int iTitle)
{
	if ( !__xuiMsgBoxValid(pBox) || (__xuiMsgBoxPresetButtonTitleIndex(iTitle) < 0) ) {
		return NULL;
	}
	return __xuiMsgBoxPresetButtonTitle(pBox, iTitle);
}

XUI_API int xuiMsgBoxSetResult(xui_msgbox pBox, xui_msgbox_result_proc onResult, void* pUser)
{
	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->onResult = onResult;
	pBox->pResultUser = pUser;
	return XUI_OK;
}

XUI_API int xuiMsgBoxSetModal(xui_msgbox pBox, int bModal)
{
	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->bModal = bModal ? 1 : 0;
	pBox->iChangeCount++;
	return __xuiMsgBoxApplyOverlay(pBox);
}

XUI_API int xuiMsgBoxIsModal(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->bModal : 0;
}

XUI_API int xuiMsgBoxSetOpen(xui_msgbox pBox, int bOpen)
{
	int iRet;

	if ( !__xuiMsgBoxValid(pBox) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bOpen = bOpen ? 1 : 0;
	if ( pBox->bOpen == bOpen ) {
		if ( bOpen ) {
			(void)__xuiMsgBoxLayout(pBox);
			(void)xuiWindowSetOpen(pBox->pWindow, 1);
			return __xuiMsgBoxApplyOverlay(pBox);
		}
		return XUI_OK;
	}
	if ( bOpen ) {
		pBox->iResult = XUI_MSGBOX_RESULT_CLOSE;
		iRet = __xuiMsgBoxLayout(pBox);
		if ( iRet != XUI_OK ) return iRet;
		pBox->bOpen = 1;
		iRet = xuiWindowSetOpen(pBox->pWindow, 1);
		if ( iRet != XUI_OK ) {
			pBox->bOpen = 0;
			return iRet;
		}
		iRet = __xuiMsgBoxApplyOverlay(pBox);
		if ( iRet != XUI_OK ) return iRet;
		(void)xuiSetFocusWidget(pBox->pContext, pBox->pWindow);
	} else {
		pBox->bOpen = 0;
		(void)xuiWindowSetOpen(pBox->pWindow, 0);
		iRet = __xuiMsgBoxApplyOverlay(pBox);
		if ( iRet != XUI_OK ) return iRet;
	}
	pBox->iChangeCount++;
	return XUI_OK;
}

XUI_API int xuiMsgBoxIsOpen(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->bOpen : 0;
}

XUI_API int xuiMsgBoxGetResult(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->iResult : XUI_MSGBOX_RESULT_CLOSE;
}

XUI_API int xuiMsgBoxSetMetrics(xui_msgbox pBox, const xui_msgbox_metrics_t* pMetrics)
{
	if ( !__xuiMsgBoxValid(pBox) || !__xuiMsgBoxMetricsValid(pMetrics) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->tMetrics = *pMetrics;
	pBox->tMetrics.iSize = sizeof(pBox->tMetrics);
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxGetMetrics(xui_msgbox pBox, xui_msgbox_metrics_t* pMetrics)
{
	if ( !__xuiMsgBoxValid(pBox) || (pMetrics == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pMetrics = pBox->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiMsgBoxSetColors(xui_msgbox pBox, const xui_msgbox_colors_t* pColors)
{
	if ( !__xuiMsgBoxValid(pBox) || !__xuiMsgBoxColorsValid(pColors) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pBox->tColors = *pColors;
	pBox->tColors.iSize = sizeof(pBox->tColors);
	(void)xuiWindowSetColors(pBox->pWindow,
		XUI_COLOR_RGBA(245, 250, 254, 255),
		pBox->tColors.iClientColor,
		XUI_COLOR_RGBA(231, 241, 250, 255),
		XUI_COLOR_RGBA(35, 54, 80, 255),
		XUI_COLOR_RGBA(135, 173, 211, 255),
		XUI_COLOR_RGBA(232, 242, 251, 255),
		XUI_COLOR_RGBA(218, 234, 249, 255),
		XUI_COLOR_RGBA(200, 222, 244, 255));
	__xuiMsgBoxApplyButtonVisual(pBox);
	pBox->iChangeCount++;
	return __xuiMsgBoxLayout(pBox);
}

XUI_API int xuiMsgBoxGetColors(xui_msgbox pBox, xui_msgbox_colors_t* pColors)
{
	if ( !__xuiMsgBoxValid(pBox) || (pColors == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pColors = pBox->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API xui_widget xuiMsgBoxGetWindowWidget(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->pWindow : NULL;
}

XUI_API xui_widget xuiMsgBoxGetContentWidget(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->pContent : NULL;
}

XUI_API xui_widget xuiMsgBoxGetBackdropWidget(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->pBackdrop : NULL;
}

XUI_API xui_widget xuiMsgBoxGetButtonWidget(xui_msgbox pBox, int iIndex)
{
	if ( !__xuiMsgBoxValid(pBox) || (iIndex < 0) || (iIndex >= pBox->iButtonCount) ) {
		return NULL;
	}
	return pBox->arrButtons[iIndex];
}

XUI_API int xuiMsgBoxGetButtonCount(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->iButtonCount : 0;
}

XUI_API xui_rect_t xuiMsgBoxGetIconRect(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->tIconRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiMsgBoxGetMessageRect(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->tMessageRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiMsgBoxGetButtonRect(xui_msgbox pBox, int iIndex)
{
	if ( !__xuiMsgBoxValid(pBox) || (iIndex < 0) || (iIndex >= pBox->iButtonCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return pBox->arrButtonRect[iIndex];
}

XUI_API xui_rect_t xuiMsgBoxGetBackdropRect(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->tBackdropRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiMsgBoxGetWrapLineCount(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->iWrapLineCount : 0;
}

XUI_API int xuiMsgBoxGetResultCount(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->iResultCount : 0;
}

XUI_API int xuiMsgBoxGetChangeCount(xui_msgbox pBox)
{
	return __xuiMsgBoxValid(pBox) ? pBox->iChangeCount : 0;
}
