#include "xui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>

typedef struct xui_message_node_data_t {
	char* sId;
	char* sSender;
	char* sTime;
	char* sText;
	char* sParentId;
	char* sTitle;
	int iType;
	int iFlags;
	int iAuxiliaryKind;
	void* pUser;
	xui_rect_t tNodeRect;
	xui_rect_t tBubbleRect;
	xui_rect_t tHeaderRect;
	xui_rect_t tTextRect;
	xui_text_layout pTextLayout;
	xui_font pTextLayoutFont;
	const char* sTextLayoutSource;
	float fTextLayoutWidth;
} xui_message_node_data_t;

typedef struct xui_message_list_data_t {
	xui_message_node_data_t* arrNodes;
	int iNodeCount;
	int iNodeCapacity;
	xui_font pFont;
	xui_message_list_metrics_t tMetrics;
	xui_message_list_colors_t tColors;
	xui_message_list_event_proc onEvent;
	void* pEventUser;
	xui_message_list_node_renderer_proc onRenderNode;
	void* pRenderNodeUser;
	float fScrollY;
	float fContentHeight;
	int iHover;
	int iSelected;
	int iSelectCount;
	int iClickCount;
	int iChangeCount;
	int bAutoScroll;
	int iSelectionAnchorNode;
	int iSelectionAnchorOffset;
	int iSelectionActiveNode;
	int iSelectionActiveOffset;
	int bSelecting;
	xui_widget pContextMenu;
	char* sLineScratch;
	int iLineScratchCapacity;
} xui_message_list_data_t;

static xui_message_list_data_t* __xuiMessageListGetData(xui_widget pWidget)
{
	if ( pWidget == NULL ) return NULL;
	if ( strcmp(xuiWidgetTypeGetName(xuiWidgetGetType(pWidget)), "messagelist") != 0 ) return NULL;
	return (xui_message_list_data_t*)xuiWidgetGetTypeData(pWidget);
}

static const char* __xuiMessageText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static char* __xuiMessageDup(const char* sText)
{
	char* sCopy;
	size_t iLen;
	sText = __xuiMessageText(sText);
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) return NULL;
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static int __xuiMessageReplace(char** ppText, const char* sText)
{
	char* sCopy;
	if ( ppText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sCopy = __xuiMessageDup(sText);
	if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( *ppText != NULL ) xrtFree(*ppText);
	*ppText = sCopy;
	return XUI_OK;
}

static float __xuiMessageMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiMessageMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static int __xuiMessageAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static int __xuiMessageDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (pProxy == NULL) || (pDraw == NULL) || (pProxy->drawRectFill == NULL) || (__xuiMessageAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiMessageDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiMessageAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return __xuiMessageDrawFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiMessageDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiMessageAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy == NULL) || (pDraw == NULL) || (pProxy->drawRectStroke == NULL) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiMessageDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	if ( (pProxy == NULL) || (pDraw == NULL) || (pFont == NULL) || (pProxy->drawText == NULL) || (sText == NULL) || (sText[0] == 0) ||
	     (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiMessageAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawText(pProxy, pDraw, pFont, sText, tRect, iColor, iFlags);
}

static float __xuiMessageClamp(float fValue, float fMin, float fMax)
{
	if ( fMax < fMin ) fMax = fMin;
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static int __xuiMessageFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiMessageNodeTypeValid(int iType)
{
	return iType == XUI_MESSAGE_NODE_SELF || iType == XUI_MESSAGE_NODE_OTHER || iType == XUI_MESSAGE_NODE_SYSTEM || iType == XUI_MESSAGE_NODE_AUXILIARY;
}

static int __xuiMessageNodeHasExtension(const xui_message_node_t* pNode)
{
	return pNode != NULL && pNode->iSize >= offsetof(xui_message_node_t, iAuxiliaryKind) + sizeof(pNode->iAuxiliaryKind);
}

static int __xuiMessageDescValid(const xui_message_list_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( pDesc->iNodeCount < 0 ) return 0;
	if ( (pDesc->iNodeCount > 0) && (pDesc->arrNodes == NULL) ) return 0;
	return 1;
}

static void __xuiMessageDefaultMetrics(xui_message_list_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fPaddingX = 16.0f;
	pMetrics->fPaddingY = 14.0f;
	pMetrics->fNodeGap = 14.0f;
	pMetrics->fAvatarSize = 34.0f;
	pMetrics->fAvatarGap = 8.0f;
	pMetrics->fBubbleMaxWidth = 360.0f;
	pMetrics->fBubblePaddingX = 12.0f;
	pMetrics->fBubblePaddingY = 8.0f;
	pMetrics->fSystemPaddingX = 8.0f;
	pMetrics->fSystemPaddingY = 4.0f;
	pMetrics->fMetaHeight = 18.0f;
	pMetrics->fMinBubbleHeight = 36.0f;
	pMetrics->fWheelStep = 48.0f;
}

static void __xuiMessageDefaultColors(xui_message_list_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iBackgroundColor = XUI_COLOR_RGBA(242, 243, 245, 255);
	pColors->iSelfBubbleColor = XUI_COLOR_RGBA(188, 232, 255, 255);
	pColors->iOtherBubbleColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pColors->iSystemBubbleColor = XUI_COLOR_RGBA(255, 255, 255, 180);
	pColors->iSelfTextColor = XUI_COLOR_RGBA(20, 28, 38, 255);
	pColors->iOtherTextColor = XUI_COLOR_RGBA(20, 28, 38, 255);
	pColors->iSystemTextColor = XUI_COLOR_RGBA(154, 158, 166, 255);
	pColors->iMetaTextColor = XUI_COLOR_RGBA(148, 153, 162, 255);
	pColors->iAvatarSelfColor = XUI_COLOR_RGBA(64, 82, 104, 255);
	pColors->iAvatarOtherColor = XUI_COLOR_RGBA(160, 198, 218, 255);
	pColors->iHoverColor = XUI_COLOR_RGBA(75, 138, 208, 40);
	pColors->iSelectedColor = XUI_COLOR_RGBA(47, 128, 237, 70);
	pColors->iBorderColor = XUI_COLOR_RGBA(214, 220, 228, 255);
}

static int __xuiMessageMetricsValid(const xui_message_list_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	return __xuiMessageFloatValid(pMetrics->fPaddingX) &&
	       __xuiMessageFloatValid(pMetrics->fPaddingY) &&
	       __xuiMessageFloatValid(pMetrics->fNodeGap) &&
	       __xuiMessageFloatValid(pMetrics->fAvatarSize) &&
	       __xuiMessageFloatValid(pMetrics->fAvatarGap) &&
	       __xuiMessageFloatValid(pMetrics->fBubbleMaxWidth) &&
	       __xuiMessageFloatValid(pMetrics->fBubblePaddingX) &&
	       __xuiMessageFloatValid(pMetrics->fBubblePaddingY) &&
	       __xuiMessageFloatValid(pMetrics->fSystemPaddingX) &&
	       __xuiMessageFloatValid(pMetrics->fSystemPaddingY) &&
	       __xuiMessageFloatValid(pMetrics->fMetaHeight) &&
	       __xuiMessageFloatValid(pMetrics->fMinBubbleHeight) &&
	       __xuiMessageFloatValid(pMetrics->fWheelStep);
}

static void __xuiMessageFreeNode(xui_message_node_data_t* pNode)
{
	if ( pNode == NULL ) return;
	if ( pNode->pTextLayout != NULL ) xuiTextLayoutDestroy(pNode->pTextLayout);
	if ( pNode->sId != NULL ) xrtFree(pNode->sId);
	if ( pNode->sSender != NULL ) xrtFree(pNode->sSender);
	if ( pNode->sTime != NULL ) xrtFree(pNode->sTime);
	if ( pNode->sText != NULL ) xrtFree(pNode->sText);
	if ( pNode->sParentId != NULL ) xrtFree(pNode->sParentId);
	if ( pNode->sTitle != NULL ) xrtFree(pNode->sTitle);
	memset(pNode, 0, sizeof(*pNode));
}

static void __xuiMessageInvalidateNodeTextLayout(xui_message_node_data_t* pNode)
{
	if ( pNode == NULL ) return;
	if ( pNode->pTextLayout != NULL ) xuiTextLayoutDestroy(pNode->pTextLayout);
	pNode->pTextLayout = NULL;
	pNode->pTextLayoutFont = NULL;
	pNode->sTextLayoutSource = NULL;
	pNode->fTextLayoutWidth = 0.0f;
}

static int __xuiMessageCopyNode(xui_message_node_data_t* pDst, const xui_message_node_t* pSrc)
{
	int iType;
	int iRet;
	if ( (pDst == NULL) || (pSrc == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pDst, 0, sizeof(*pDst));
	iType = __xuiMessageNodeTypeValid(pSrc->iType) ? pSrc->iType : XUI_MESSAGE_NODE_OTHER;
	pDst->iType = iType;
	pDst->iFlags = pSrc->iFlags;
	pDst->pUser = pSrc->pUser;
	pDst->iAuxiliaryKind = __xuiMessageNodeHasExtension(pSrc) ? pSrc->iAuxiliaryKind : 0;
	iRet = __xuiMessageReplace(&pDst->sId, pSrc->sId);
	if ( iRet == XUI_OK ) iRet = __xuiMessageReplace(&pDst->sSender, pSrc->sSender);
	if ( iRet == XUI_OK ) iRet = __xuiMessageReplace(&pDst->sTime, pSrc->sTime);
	if ( iRet == XUI_OK ) iRet = __xuiMessageReplace(&pDst->sText, pSrc->sText);
	if ( iRet == XUI_OK ) iRet = __xuiMessageReplace(&pDst->sParentId, __xuiMessageNodeHasExtension(pSrc) ? pSrc->sParentId : NULL);
	if ( iRet == XUI_OK ) iRet = __xuiMessageReplace(&pDst->sTitle, __xuiMessageNodeHasExtension(pSrc) ? pSrc->sTitle : NULL);
	if ( iRet != XUI_OK ) __xuiMessageFreeNode(pDst);
	return iRet;
}

static int __xuiMessageReserve(xui_message_list_data_t* pData, int iCapacity)
{
	xui_message_node_data_t* pNewNodes;
	int iNewCapacity;
	if ( pData == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pData->iNodeCapacity ) return XUI_OK;
	iNewCapacity = (pData->iNodeCapacity > 0) ? pData->iNodeCapacity * 2 : 16;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	pNewNodes = (xui_message_node_data_t*)xrtMalloc(sizeof(*pNewNodes) * (size_t)iNewCapacity);
	if ( pNewNodes == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pNewNodes, 0, sizeof(*pNewNodes) * (size_t)iNewCapacity);
	if ( pData->arrNodes != NULL ) {
		memcpy(pNewNodes, pData->arrNodes, sizeof(*pNewNodes) * (size_t)pData->iNodeCount);
		xrtFree(pData->arrNodes);
	}
	pData->arrNodes = pNewNodes;
	pData->iNodeCapacity = iNewCapacity;
	return XUI_OK;
}

static void __xuiMessageClearData(xui_message_list_data_t* pData)
{
	int i;
	if ( pData == NULL ) return;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		__xuiMessageFreeNode(&pData->arrNodes[i]);
	}
	pData->iNodeCount = 0;
	pData->iHover = -1;
	pData->iSelected = -1;
	pData->iSelectionAnchorNode = -1;
	pData->iSelectionAnchorOffset = 0;
	pData->iSelectionActiveNode = -1;
	pData->iSelectionActiveOffset = 0;
	pData->bSelecting = 0;
	pData->fScrollY = 0.0f;
	pData->fContentHeight = 0.0f;
}

static xui_font __xuiMessageFont(xui_widget pWidget, xui_message_list_data_t* pData)
{
	if ( pData == NULL ) return NULL;
	return (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
}

static xui_message_node_t __xuiMessagePublicNode(const xui_message_node_data_t* pNode)
{
	xui_message_node_t tNode;
	memset(&tNode, 0, sizeof(tNode));
	if ( pNode == NULL ) return tNode;
	tNode.iSize = sizeof(tNode);
	tNode.sId = __xuiMessageText(pNode->sId);
	tNode.sSender = __xuiMessageText(pNode->sSender);
	tNode.sTime = __xuiMessageText(pNode->sTime);
	tNode.sText = __xuiMessageText(pNode->sText);
	tNode.iType = pNode->iType;
	tNode.iFlags = pNode->iFlags;
	tNode.pUser = pNode->pUser;
	tNode.sParentId = __xuiMessageText(pNode->sParentId);
	tNode.sTitle = __xuiMessageText(pNode->sTitle);
	tNode.iAuxiliaryKind = pNode->iAuxiliaryKind;
	return tNode;
}

static float __xuiMessageTextWidth(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_proxy pProxy;
	xui_vec2_t tSize;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) ) {
		tSize = (xui_vec2_t){0.0f, 0.0f};
		if ( pProxy->textMeasure(pProxy, pFont, __xuiMessageText(sText), &tSize) == XUI_OK ) {
			return tSize.fX;
		}
	}
	return (float)strlen(__xuiMessageText(sText)) * 7.0f;
}

static float __xuiMessageLineHeight(xui_context pContext, xui_font pFont)
{
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (pFont != NULL) ) {
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK && tMetrics.fLineHeight > 0.0f ) {
			return tMetrics.fLineHeight;
		}
	}
	return 18.0f;
}

static int __xuiMessageEnsureLineScratch(xui_message_list_data_t* pData, int iCapacity)
{
	char* sNew;
	int iNewCapacity;

	if ( pData == NULL || iCapacity <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pData->iLineScratchCapacity ) return XUI_OK;
	iNewCapacity = (pData->iLineScratchCapacity > 0) ? pData->iLineScratchCapacity : 128;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	sNew = (char*)xrtRealloc(pData->sLineScratch, (size_t)iNewCapacity);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pData->sLineScratch = sNew;
	pData->iLineScratchCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiMessageEnsureNodeTextLayout(xui_widget pWidget, xui_message_list_data_t* pData,
	xui_message_node_data_t* pNode, float fMaxWidth, xui_text_layout* ppLayout)
{
	xui_text_layout_desc_t tDesc;
	xui_font pFont;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pNode == NULL || ppLayout == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppLayout = NULL;
	pFont = __xuiMessageFont(pWidget, pData);
	fMaxWidth = __xuiMessageMax(1.0f, fMaxWidth);
	if ( pNode->pTextLayout != NULL && pNode->pTextLayoutFont == pFont &&
	     pNode->sTextLayoutSource == pNode->sText && pNode->fTextLayoutWidth == fMaxWidth ) {
		*ppLayout = pNode->pTextLayout;
		return XUI_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = __xuiMessageText(pNode->sText);
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = fMaxWidth;
	tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
	tDesc.fLineGap = 2.0f;
	if ( pNode->pTextLayout != NULL ) iRet = xuiTextLayoutReset(pNode->pTextLayout, &tDesc);
	else iRet = xuiTextLayoutCreate(xuiWidgetGetContext(pWidget), &pNode->pTextLayout, &tDesc);
	if ( iRet != XUI_OK ) {
		if ( pNode->pTextLayout != NULL ) xuiTextLayoutDestroy(pNode->pTextLayout);
		pNode->pTextLayout = NULL;
		pNode->pTextLayoutFont = NULL;
		pNode->sTextLayoutSource = NULL;
		pNode->fTextLayoutWidth = 0.0f;
		return iRet;
	}
	pNode->pTextLayoutFont = pFont;
	pNode->sTextLayoutSource = pNode->sText;
	pNode->fTextLayoutWidth = fMaxWidth;
	*ppLayout = pNode->pTextLayout;
	return XUI_OK;
}

static int __xuiMessageMeasureNodeWrapped(xui_widget pWidget, xui_message_list_data_t* pData,
	xui_message_node_data_t* pNode, float fMaxWidth, xui_vec2_t* pSize)
{
	xui_text_layout pLayout;
	int iRet;

	iRet = __xuiMessageEnsureNodeTextLayout(pWidget, pData, pNode, fMaxWidth, &pLayout);
	if ( iRet == XUI_OK ) {
		*pSize = xuiTextLayoutGetSize(pLayout);
		return XUI_OK;
	}
	pSize->fX = __xuiMessageMin(__xuiMessageTextWidth(pWidget, __xuiMessageFont(pWidget, pData), pNode->sText), fMaxWidth);
	pSize->fY = __xuiMessageLineHeight(xuiWidgetGetContext(pWidget), __xuiMessageFont(pWidget, pData));
	return iRet;
}

static int __xuiMessageMeasureWrapped(xui_widget pWidget, xui_message_list_data_t* pData, const char* sText, float fMaxWidth, xui_vec2_t* pSize)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	int iRet;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = __xuiMessageText(sText);
	tDesc.iTextSize = -1;
	tDesc.pFont = __xuiMessageFont(pWidget, pData);
	tDesc.fMaxWidth = __xuiMessageMax(1.0f, fMaxWidth);
	tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(xuiWidgetGetContext(pWidget), &pLayout, &tDesc);
	if ( iRet != XUI_OK ) {
		pSize->fX = __xuiMessageMin(__xuiMessageTextWidth(pWidget, tDesc.pFont, sText), fMaxWidth);
		pSize->fY = __xuiMessageLineHeight(xuiWidgetGetContext(pWidget), tDesc.pFont);
		return iRet;
	}
	*pSize = xuiTextLayoutGetSize(pLayout);
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static const char* __xuiMessageAuxiliaryTitle(xui_widget pWidget, const xui_message_node_data_t* pNode)
{
	if ( pNode != NULL && pNode->sTitle != NULL && pNode->sTitle[0] != 0 ) return pNode->sTitle;
	return xuiTranslate(xuiWidgetGetContext(pWidget),
		(pNode != NULL && pNode->iAuxiliaryKind == XUI_MESSAGE_AUXILIARY_TOOL) ? XUI_TR_MESSAGE_TOOL : XUI_TR_MESSAGE_THINKING);
}

static int __xuiMessageLayoutNodes(xui_widget pWidget, xui_message_list_data_t* pData)
{
	xui_rect_t tContent;
	xui_message_node_data_t* pNode;
	float fY;
	float fAvailable;
	float fMaxBubble;
	float fTextMax;
	float fBubbleW;
	float fBubbleH;
	float fTextW;
	float fRowH;
	float fAuxIndent;
	float fHeaderH;
	xui_vec2_t tTextSize;
	xui_vec2_t tTitleSize;
	xui_font pFont;
	int i;
	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tContent = xuiWidgetGetContentRect(pWidget);
	fAvailable = __xuiMessageMax(0.0f, tContent.fW - pData->tMetrics.fPaddingX * 2.0f);
	fMaxBubble = __xuiMessageMin(pData->tMetrics.fBubbleMaxWidth, fAvailable - pData->tMetrics.fAvatarSize - pData->tMetrics.fAvatarGap);
	if ( fMaxBubble < 80.0f ) fMaxBubble = __xuiMessageMax(40.0f, fAvailable);
	fTextMax = __xuiMessageMax(16.0f, fMaxBubble - pData->tMetrics.fBubblePaddingX * 2.0f);
	pFont = __xuiMessageFont(pWidget, pData);
	fY = pData->tMetrics.fPaddingY;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		pNode = &pData->arrNodes[i];
		memset(&pNode->tNodeRect, 0, sizeof(pNode->tNodeRect));
		memset(&pNode->tBubbleRect, 0, sizeof(pNode->tBubbleRect));
		memset(&pNode->tHeaderRect, 0, sizeof(pNode->tHeaderRect));
		memset(&pNode->tTextRect, 0, sizeof(pNode->tTextRect));
		if ( pNode->iType == XUI_MESSAGE_NODE_SYSTEM ) {
			(void)__xuiMessageMeasureNodeWrapped(pWidget, pData, pNode, __xuiMessageMax(16.0f, fAvailable - pData->tMetrics.fSystemPaddingX * 2.0f), &tTextSize);
			fTextW = tTextSize.fX;
			fBubbleW = __xuiMessageMin(fAvailable, __xuiMessageMax(56.0f, fTextW + pData->tMetrics.fSystemPaddingX * 2.0f));
			fBubbleH = __xuiMessageMax(__xuiMessageLineHeight(xuiWidgetGetContext(pWidget), pFont), tTextSize.fY) + pData->tMetrics.fSystemPaddingY * 2.0f;
			pNode->tNodeRect = (xui_rect_t){0.0f, fY, tContent.fW, fBubbleH};
			pNode->tBubbleRect = (xui_rect_t){(tContent.fW - fBubbleW) * 0.5f, fY, fBubbleW, fBubbleH};
			pNode->tTextRect = (xui_rect_t){pNode->tBubbleRect.fX + pData->tMetrics.fSystemPaddingX, fY + pData->tMetrics.fSystemPaddingY, fBubbleW - pData->tMetrics.fSystemPaddingX * 2.0f, fBubbleH - pData->tMetrics.fSystemPaddingY * 2.0f};
			fY += fBubbleH + pData->tMetrics.fNodeGap;
			continue;
		}
		if ( pNode->iType == XUI_MESSAGE_NODE_AUXILIARY ) {
			fAuxIndent = (pNode->sParentId != NULL && pNode->sParentId[0] != 0) ? 12.0f : 0.0f;
			fBubbleW = __xuiMessageMax(48.0f, fAvailable - fAuxIndent);
			fHeaderH = __xuiMessageLineHeight(xuiWidgetGetContext(pWidget), pFont) + 10.0f;
			(void)__xuiMessageMeasureWrapped(pWidget, pData, __xuiMessageAuxiliaryTitle(pWidget, pNode), __xuiMessageMax(16.0f, fBubbleW - pData->tMetrics.fBubblePaddingX * 2.0f - 20.0f), &tTitleSize);
			fHeaderH = __xuiMessageMax(fHeaderH, tTitleSize.fY + 8.0f);
			memset(&tTextSize, 0, sizeof(tTextSize));
			if ( (pNode->iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSED) == 0 ) {
				(void)__xuiMessageMeasureNodeWrapped(pWidget, pData, pNode, __xuiMessageMax(16.0f, fBubbleW - pData->tMetrics.fBubblePaddingX * 2.0f), &tTextSize);
			}
			fBubbleH = fHeaderH + ((pNode->iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSED) ? 0.0f : (tTextSize.fY + pData->tMetrics.fBubblePaddingY * 2.0f));
			pNode->tNodeRect = (xui_rect_t){0.0f, fY, tContent.fW, fBubbleH};
			pNode->tBubbleRect = (xui_rect_t){pData->tMetrics.fPaddingX + fAuxIndent, fY, fBubbleW, fBubbleH};
			pNode->tHeaderRect = (xui_rect_t){pNode->tBubbleRect.fX, fY, fBubbleW, fHeaderH};
			pNode->tTextRect = (xui_rect_t){pNode->tBubbleRect.fX + pData->tMetrics.fBubblePaddingX, fY + fHeaderH + pData->tMetrics.fBubblePaddingY, fBubbleW - pData->tMetrics.fBubblePaddingX * 2.0f, tTextSize.fY};
			fY += fBubbleH + pData->tMetrics.fNodeGap;
			continue;
		}
		(void)__xuiMessageMeasureNodeWrapped(pWidget, pData, pNode, fTextMax, &tTextSize);
		fTextW = tTextSize.fX;
		fBubbleW = __xuiMessageMin(fMaxBubble, __xuiMessageMax(48.0f, fTextW + pData->tMetrics.fBubblePaddingX * 2.0f));
		fBubbleH = __xuiMessageMax(pData->tMetrics.fMinBubbleHeight, tTextSize.fY + pData->tMetrics.fBubblePaddingY * 2.0f);
		fRowH = __xuiMessageMax(pData->tMetrics.fAvatarSize, pData->tMetrics.fMetaHeight + fBubbleH);
		pNode->tNodeRect = (xui_rect_t){0.0f, fY, tContent.fW, fRowH};
		if ( pNode->iType == XUI_MESSAGE_NODE_SELF ) {
			pNode->tBubbleRect = (xui_rect_t){tContent.fW - pData->tMetrics.fPaddingX - pData->tMetrics.fAvatarSize - pData->tMetrics.fAvatarGap - fBubbleW, fY + pData->tMetrics.fMetaHeight, fBubbleW, fBubbleH};
		} else {
			pNode->tBubbleRect = (xui_rect_t){pData->tMetrics.fPaddingX + pData->tMetrics.fAvatarSize + pData->tMetrics.fAvatarGap, fY + pData->tMetrics.fMetaHeight, fBubbleW, fBubbleH};
		}
		pNode->tTextRect = (xui_rect_t){pNode->tBubbleRect.fX + pData->tMetrics.fBubblePaddingX, pNode->tBubbleRect.fY + pData->tMetrics.fBubblePaddingY, fBubbleW - pData->tMetrics.fBubblePaddingX * 2.0f, fBubbleH - pData->tMetrics.fBubblePaddingY * 2.0f};
		fY += fRowH + pData->tMetrics.fNodeGap;
	}
	pData->fContentHeight = __xuiMessageMax(0.0f, fY - pData->tMetrics.fNodeGap + pData->tMetrics.fPaddingY);
	pData->fScrollY = __xuiMessageClamp(pData->fScrollY, 0.0f, __xuiMessageMax(0.0f, pData->fContentHeight - tContent.fH));
	return XUI_OK;
}

static int __xuiMessageInvalidate(xui_widget pWidget, xui_message_list_data_t* pData)
{
	if ( pData != NULL ) pData->iChangeCount++;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiMessageNotify(xui_widget pWidget, xui_message_list_data_t* pData, int iEvent, int iIndex, const xui_event_t* pInput)
{
	xui_message_list_event_t tEvent;
	xui_message_node_t tNode;
	if ( (pData == NULL) || (pData->onEvent == NULL) ) return XUI_OK;
	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iSize = sizeof(tEvent);
	tEvent.iEvent = iEvent;
	tEvent.iIndex = iIndex;
	tEvent.iNodeType = -1;
	if ( (iIndex >= 0) && (iIndex < pData->iNodeCount) ) {
		tNode = __xuiMessagePublicNode(&pData->arrNodes[iIndex]);
		tEvent.pNode = &tNode;
		tEvent.iNodeType = tNode.iType;
	}
	if ( pInput != NULL ) {
		tEvent.fX = pInput->fX;
		tEvent.fY = pInput->fY;
		tEvent.iButton = pInput->iButton;
		tEvent.iModifiers = pInput->iModifiers;
	}
	pData->onEvent(pWidget, &tEvent, pData->pEventUser);
	return XUI_OK;
}

static int __xuiMessageGetIndexAtData(xui_widget pWidget, xui_message_list_data_t* pData, float fX, float fY)
{
	xui_rect_t tContent;
	xui_rect_t tRect;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	int i;
	if ( (pWidget == NULL) || (pData == NULL) ) return -1;
	tContent = xuiWidgetGetContentRect(pWidget);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = fX - tWorld.fX - tContent.fX;
	fLocalY = fY - tWorld.fY - tContent.fY + pData->fScrollY;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		tRect = pData->arrNodes[i].tNodeRect;
		if ( fLocalX >= tRect.fX && fLocalX <= tRect.fX + tRect.fW && fLocalY >= tRect.fY && fLocalY <= tRect.fY + tRect.fH ) {
			return i;
		}
	}
	return -1;
}

static int __xuiMessageNodeCanSelectText(const xui_message_node_data_t* pNode)
{
	if ( pNode == NULL || pNode->iType == XUI_MESSAGE_NODE_SYSTEM ) return 0;
	if ( pNode->iType == XUI_MESSAGE_NODE_AUXILIARY && (pNode->iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSED) != 0 ) return 0;
	return pNode->sText != NULL && pNode->sText[0] != 0;
}

static int __xuiMessagePositionCompare(int iNodeA, int iOffsetA, int iNodeB, int iOffsetB)
{
	if ( iNodeA != iNodeB ) return (iNodeA < iNodeB) ? -1 : 1;
	if ( iOffsetA != iOffsetB ) return (iOffsetA < iOffsetB) ? -1 : 1;
	return 0;
}

static int __xuiMessageGetTextSelectionForNode(const xui_message_list_data_t* pData, int iNode, int* pStart, int* pEnd)
{
	int iStartNode;
	int iStartOffset;
	int iEndNode;
	int iEndOffset;
	int iLength;
	if ( pStart != NULL ) *pStart = 0;
	if ( pEnd != NULL ) *pEnd = 0;
	if ( (pData == NULL) || (iNode < 0) || (iNode >= pData->iNodeCount) ||
	     __xuiMessagePositionCompare(pData->iSelectionAnchorNode, pData->iSelectionAnchorOffset, pData->iSelectionActiveNode, pData->iSelectionActiveOffset) == 0 ) return 0;
	if ( __xuiMessagePositionCompare(pData->iSelectionAnchorNode, pData->iSelectionAnchorOffset, pData->iSelectionActiveNode, pData->iSelectionActiveOffset) <= 0 ) {
		iStartNode = pData->iSelectionAnchorNode;
		iStartOffset = pData->iSelectionAnchorOffset;
		iEndNode = pData->iSelectionActiveNode;
		iEndOffset = pData->iSelectionActiveOffset;
	} else {
		iStartNode = pData->iSelectionActiveNode;
		iStartOffset = pData->iSelectionActiveOffset;
		iEndNode = pData->iSelectionAnchorNode;
		iEndOffset = pData->iSelectionAnchorOffset;
	}
	if ( iNode < iStartNode || iNode > iEndNode ) return 0;
	iLength = (int)strlen(__xuiMessageText(pData->arrNodes[iNode].sText));
	if ( pStart != NULL ) *pStart = (iNode == iStartNode) ? iStartOffset : 0;
	if ( pEnd != NULL ) *pEnd = (iNode == iEndNode) ? iEndOffset : iLength;
	if ( pStart != NULL && *pStart < 0 ) *pStart = 0;
	if ( pEnd != NULL && *pEnd > iLength ) *pEnd = iLength;
	return pStart != NULL && pEnd != NULL && *pEnd > *pStart;
}

static int __xuiMessageUtf8Next(const char* sText, int iLength, int iOffset)
{
	unsigned char ch;
	int iMore;
	if ( sText == NULL || iOffset >= iLength ) return iLength;
	ch = (unsigned char)sText[iOffset++];
	if ( ch < 0x80u ) return iOffset;
	iMore = ((ch & 0xe0u) == 0xc0u) ? 1 : (((ch & 0xf0u) == 0xe0u) ? 2 : (((ch & 0xf8u) == 0xf0u) ? 3 : 0));
	while ( iMore-- > 0 && iOffset < iLength && ((unsigned char)sText[iOffset] & 0xc0u) == 0x80u ) iOffset++;
	return iOffset;
}

static float __xuiMessagePrefixWidth(xui_widget pWidget, xui_font pFont, const char* sText, int iOffset, int iSize)
{
	char* sPrefix;
	float fWidth;
	if ( sText == NULL || iSize <= 0 ) return 0.0f;
	sPrefix = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( sPrefix == NULL ) return 0.0f;
	memcpy(sPrefix, sText + iOffset, (size_t)iSize);
	sPrefix[iSize] = 0;
	fWidth = __xuiMessageTextWidth(pWidget, pFont, sPrefix);
	xrtFree(sPrefix);
	return fWidth;
}

static int __xuiMessageHitTextOffset(xui_widget pWidget, xui_message_list_data_t* pData, float fX, float fY, int* pNodeIndex, int* pOffset)
{
	xui_message_node_data_t* pNode;
	xui_text_layout pLayout;
	xui_text_line_t tLine;
	xui_rect_t tContent;
	xui_rect_t tText;
	xui_rect_t tWorld;
	xui_font pFont;
	const char* sText;
	float fLocalX;
	float fLocalY;
	float fPrevious;
	float fCurrent;
	int iIndex;
	int iLine;
	int iOffset;
	int iNext;
	int iLength;
	if ( pNodeIndex != NULL ) *pNodeIndex = -1;
	if ( pOffset != NULL ) *pOffset = 0;
	if ( (pWidget == NULL) || (pData == NULL) ) return 0;
	iIndex = __xuiMessageGetIndexAtData(pWidget, pData, fX, fY);
	if ( iIndex < 0 ) return 0;
	pNode = &pData->arrNodes[iIndex];
	if ( !__xuiMessageNodeCanSelectText(pNode) ) return 0;
	tContent = xuiWidgetGetContentRect(pWidget);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tText = pNode->tTextRect;
	fLocalX = fX - tWorld.fX - tContent.fX;
	fLocalY = fY - tWorld.fY - tContent.fY + pData->fScrollY;
	if ( fLocalX < tText.fX || fLocalX > tText.fX + tText.fW || fLocalY < tText.fY || fLocalY > tText.fY + tText.fH ) return 0;
	sText = __xuiMessageText(pNode->sText);
	pFont = __xuiMessageFont(pWidget, pData);
	pLayout = NULL;
	if ( __xuiMessageEnsureNodeTextLayout(pWidget, pData, pNode, tText.fW, &pLayout) != XUI_OK ) return 0;
	iLength = (int)strlen(sText);
	for ( iLine = 0; iLine < xuiTextLayoutGetLineCount(pLayout); iLine++ ) {
		memset(&tLine, 0, sizeof(tLine));
		tLine.iSize = sizeof(tLine);
		if ( xuiTextLayoutGetLine(pLayout, iLine, &tLine) != XUI_OK ) continue;
		if ( fLocalY < tText.fY + tLine.fY || fLocalY > tText.fY + tLine.fY + tLine.fH ) continue;
		fPrevious = 0.0f;
		for ( iOffset = tLine.iTextOffset; iOffset < tLine.iTextOffset + tLine.iTextSize; iOffset = iNext ) {
			iNext = __xuiMessageUtf8Next(sText, iLength, iOffset);
			fCurrent = __xuiMessagePrefixWidth(pWidget, pFont, sText, tLine.iTextOffset, iNext - tLine.iTextOffset);
			if ( fLocalX - tText.fX <= (fPrevious + fCurrent) * 0.5f ) {
				if ( pNodeIndex != NULL ) *pNodeIndex = iIndex;
				if ( pOffset != NULL ) *pOffset = iOffset;
				return 1;
			}
			fPrevious = fCurrent;
		}
		if ( pNodeIndex != NULL ) *pNodeIndex = iIndex;
		if ( pOffset != NULL ) *pOffset = tLine.iTextOffset + tLine.iTextSize;
		return 1;
	}
	return 0;
}

static int __xuiMessageResolveSelectionOffset(xui_widget pWidget, xui_message_list_data_t* pData, float fX, float fY, int iAnchorNode, int* pNodeIndex, int* pOffset)
{
	xui_rect_t tContent;
	xui_rect_t tWorld;
	xui_rect_t tText;
	xui_message_node_data_t* pNode;
	float fLocalX;
	float fLocalY;
	int iCandidate;
	int iBefore;
	int iAfter;
	int iLength;
	int i;
	int bBeforeAnchor;
	if ( pNodeIndex != NULL ) *pNodeIndex = -1;
	if ( pOffset != NULL ) *pOffset = 0;
	if ( pWidget == NULL || pData == NULL || iAnchorNode < 0 || iAnchorNode >= pData->iNodeCount ) return 0;
	tContent = xuiWidgetGetContentRect(pWidget);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = fX - tWorld.fX - tContent.fX;
	fLocalY = fY - tWorld.fY - tContent.fY + pData->fScrollY;
	iCandidate = -1;
	iBefore = -1;
	iAfter = -1;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		pNode = &pData->arrNodes[i];
		if ( !__xuiMessageNodeCanSelectText(pNode) ) continue;
		if ( fLocalY >= pNode->tNodeRect.fY && fLocalY <= pNode->tNodeRect.fY + pNode->tNodeRect.fH ) {
			iCandidate = i;
			break;
		}
		if ( pNode->tNodeRect.fY + pNode->tNodeRect.fH <= fLocalY ) iBefore = i;
		if ( iAfter < 0 && pNode->tNodeRect.fY >= fLocalY ) iAfter = i;
	}
	bBeforeAnchor = fLocalY < pData->arrNodes[iAnchorNode].tNodeRect.fY;
	if ( iCandidate < 0 ) {
		if ( bBeforeAnchor ) iCandidate = (iBefore >= 0) ? iBefore : iAfter;
		else iCandidate = (iAfter >= 0) ? iAfter : iBefore;
	}
	if ( iCandidate < 0 ) return 0;
	pNode = &pData->arrNodes[iCandidate];
	iLength = (int)strlen(__xuiMessageText(pNode->sText));
	if ( pNodeIndex != NULL ) *pNodeIndex = iCandidate;
	if ( pOffset != NULL ) {
		if ( iCandidate < iAnchorNode ) *pOffset = 0;
		else if ( iCandidate > iAnchorNode ) *pOffset = iLength;
		else {
			tText = pNode->tTextRect;
			* pOffset = (fLocalY <= tText.fY || fLocalX <= tText.fX) ? 0 : iLength;
		}
	}
	return 1;
}

static void __xuiMessageSetTextSelection(xui_message_list_data_t* pData, int iAnchorNode, int iAnchorOffset, int iActiveNode, int iActiveOffset)
{
	if ( pData == NULL ) return;
	pData->iSelectionAnchorNode = iAnchorNode;
	pData->iSelectionAnchorOffset = iAnchorOffset;
	pData->iSelectionActiveNode = iActiveNode;
	pData->iSelectionActiveOffset = iActiveOffset;
}

static int __xuiMessagePointInRect(xui_rect_t tRect, float fX, float fY)
{
	return fX >= tRect.fX && fX <= tRect.fX + tRect.fW && fY >= tRect.fY && fY <= tRect.fY + tRect.fH;
}

static int __xuiMessageAppendSelectionBytes(char** ppText, int* pLength, int* pCapacity, const char* sText, int iLength)
{
	char* sNew;
	int iRequired;
	int iCapacity;
	if ( ppText == NULL || pLength == NULL || pCapacity == NULL || iLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iRequired = *pLength + iLength + 1;
	if ( iRequired > *pCapacity ) {
		iCapacity = (*pCapacity > 0) ? *pCapacity * 2 : 128;
		while ( iCapacity < iRequired ) iCapacity *= 2;
		sNew = (char*)xrtMalloc((size_t)iCapacity);
		if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		if ( *ppText != NULL && *pLength > 0 ) memcpy(sNew, *ppText, (size_t)*pLength);
		if ( *ppText != NULL ) xrtFree(*ppText);
		*ppText = sNew;
		*pCapacity = iCapacity;
	}
	if ( iLength > 0 && sText != NULL ) memcpy(*ppText + *pLength, sText, (size_t)iLength);
	*pLength += iLength;
	(*ppText)[*pLength] = 0;
	return XUI_OK;
}

static int __xuiMessageBuildSelectedText(xui_message_list_data_t* pData, char** ppText)
{
	char* sText;
	const char* sNodeText;
	int iStartNode;
	int iStartOffset;
	int iEndNode;
	int iEndOffset;
	int iLength;
	int iCapacity;
	int i;
	int iNodeStart;
	int iNodeEnd;
	int iRet;
	if ( ppText != NULL ) *ppText = NULL;
	if ( pData == NULL || ppText == NULL ||
	     __xuiMessagePositionCompare(pData->iSelectionAnchorNode, pData->iSelectionAnchorOffset, pData->iSelectionActiveNode, pData->iSelectionActiveOffset) == 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiMessagePositionCompare(pData->iSelectionAnchorNode, pData->iSelectionAnchorOffset, pData->iSelectionActiveNode, pData->iSelectionActiveOffset) <= 0 ) {
		iStartNode = pData->iSelectionAnchorNode;
		iStartOffset = pData->iSelectionAnchorOffset;
		iEndNode = pData->iSelectionActiveNode;
		iEndOffset = pData->iSelectionActiveOffset;
	} else {
		iStartNode = pData->iSelectionActiveNode;
		iStartOffset = pData->iSelectionActiveOffset;
		iEndNode = pData->iSelectionAnchorNode;
		iEndOffset = pData->iSelectionAnchorOffset;
	}
	if ( iStartNode < 0 || iEndNode >= pData->iNodeCount ) return XUI_ERROR_INVALID_ARGUMENT;
	sText = NULL;
	iLength = 0;
	iCapacity = 0;
	for ( i = iStartNode; i <= iEndNode; i++ ) {
		if ( !__xuiMessageNodeCanSelectText(&pData->arrNodes[i]) ) continue;
		sNodeText = __xuiMessageText(pData->arrNodes[i].sText);
		iNodeStart = (i == iStartNode) ? iStartOffset : 0;
		iNodeEnd = (i == iEndNode) ? iEndOffset : (int)strlen(sNodeText);
		if ( iNodeStart < 0 ) iNodeStart = 0;
		if ( iNodeEnd > (int)strlen(sNodeText) ) iNodeEnd = (int)strlen(sNodeText);
		if ( iNodeEnd < iNodeStart ) iNodeEnd = iNodeStart;
		if ( iLength > 0 ) {
			iRet = __xuiMessageAppendSelectionBytes(&sText, &iLength, &iCapacity, "\n", 1);
			if ( iRet != XUI_OK ) goto failed;
		}
		iRet = __xuiMessageAppendSelectionBytes(&sText, &iLength, &iCapacity, sNodeText + iNodeStart, iNodeEnd - iNodeStart);
		if ( iRet != XUI_OK ) goto failed;
	}
	if ( sText == NULL || iLength == 0 ) {
		if ( sText != NULL ) xrtFree(sText);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppText = sText;
	return XUI_OK;
failed:
	if ( sText != NULL ) xrtFree(sText);
	return iRet;
}

static void __xuiMessageContextMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pWidget = (xui_widget)pUser;
	(void)pMenu;
	(void)iIndex;
	if ( pWidget == NULL || iValue != 1 ) return;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	(void)xuiMessageListCopySelection(pWidget);
}

static int __xuiMessageOpenContextMenu(xui_widget pWidget, xui_message_list_data_t* pData, float fX, float fY)
{
	xui_menu_item_t tItem;
	if ( pWidget == NULL || pData == NULL || pData->pContextMenu == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	memset(&tItem, 0, sizeof(tItem));
	tItem.sText = xuiTranslate(xuiWidgetGetContext(pWidget), XUI_TR_MESSAGE_COPY);
	tItem.sShortcut = "Ctrl+C";
	tItem.iType = XUI_MENU_ITEM_NORMAL;
	tItem.iState = (xuiMessageListGetSelectedText(pWidget, NULL, 0) > 1) ? XUI_MENU_ITEM_ENABLED : 0u;
	tItem.iValue = 1;
	if ( xuiMenuSetItems(pData->pContextMenu, &tItem, 1) != XUI_OK ) return XUI_ERROR;
	return xuiMenuOpenAt(pData->pContextMenu, pWidget, fX, fY);
}

static int __xuiMessageEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_message_list_data_t* pData;
	xui_message_node_data_t* pNode;
	xui_rect_t tContent;
	xui_rect_t tHeader;
	xui_rect_t tWorld;
	float fMaxScroll;
	float fOldScroll;
	int iIndex;
	int iTextNode;
	int iTextOffset;
	int bChanged;
	(void)pUser;
	pData = __xuiMessageListGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iType == XUI_EVENT_POINTER_MOVE ) {
		iIndex = __xuiMessageGetIndexAtData(pWidget, pData, pEvent->fX, pEvent->fY);
		bChanged = 0;
		if ( iIndex != pData->iHover ) {
			pData->iHover = iIndex;
			(void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_HOVER, iIndex, pEvent);
			bChanged = 1;
		}
		if ( pData->bSelecting ) {
			tContent = xuiWidgetGetContentRect(pWidget);
			tWorld = xuiWidgetGetWorldRect(pWidget);
			fMaxScroll = __xuiMessageMax(0.0f, pData->fContentHeight - tContent.fH);
			fOldScroll = pData->fScrollY;
			if ( pEvent->fY < tWorld.fY + tContent.fY + 12.0f ) pData->fScrollY -= pData->tMetrics.fWheelStep * 0.35f;
			else if ( pEvent->fY > tWorld.fY + tContent.fY + tContent.fH - 12.0f ) pData->fScrollY += pData->tMetrics.fWheelStep * 0.35f;
			pData->fScrollY = __xuiMessageClamp(pData->fScrollY, 0.0f, fMaxScroll);
			if ( pData->fScrollY != fOldScroll ) bChanged = 1;
		}
		if ( pData->bSelecting &&
		     (__xuiMessageHitTextOffset(pWidget, pData, pEvent->fX, pEvent->fY, &iTextNode, &iTextOffset) ||
		      __xuiMessageResolveSelectionOffset(pWidget, pData, pEvent->fX, pEvent->fY, pData->iSelectionAnchorNode, &iTextNode, &iTextOffset)) ) {
			if ( iTextNode != pData->iSelectionActiveNode || iTextOffset != pData->iSelectionActiveOffset ) {
				pData->iSelectionActiveNode = iTextNode;
				pData->iSelectionActiveOffset = iTextOffset;
				bChanged = 1;
			}
		}
		if ( bChanged ) return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	} else if ( pEvent->iType == XUI_EVENT_POINTER_LEAVE ) {
		if ( pData->iHover != -1 && !pData->bSelecting ) {
			pData->iHover = -1;
			(void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_HOVER, -1, pEvent);
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
	} else if ( pEvent->iType == XUI_EVENT_POINTER_DOWN ) {
		iIndex = __xuiMessageGetIndexAtData(pWidget, pData, pEvent->fX, pEvent->fY);
		if ( iIndex >= 0 ) {
			pData->iSelected = iIndex;
			pData->iSelectCount++;
			(void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_SELECT, iIndex, pEvent);
			pNode = &pData->arrNodes[iIndex];
			tContent = xuiWidgetGetContentRect(pWidget);
			tWorld = xuiWidgetGetWorldRect(pWidget);
			tHeader = pNode->tHeaderRect;
			if ( pNode->iType == XUI_MESSAGE_NODE_AUXILIARY &&
			     (pNode->iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSIBLE) != 0 &&
			     __xuiMessagePointInRect(tHeader,
			         pEvent->fX - tWorld.fX - tContent.fX,
			         pEvent->fY - tWorld.fY - tContent.fY + pData->fScrollY) ) {
				pNode->iFlags ^= XUI_MESSAGE_NODE_FLAG_COLLAPSED;
				(void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_TOGGLE, iIndex, pEvent);
				return __xuiMessageInvalidate(pWidget, pData);
			}
			if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT && __xuiMessageHitTextOffset(pWidget, pData, pEvent->fX, pEvent->fY, &iTextNode, &iTextOffset) ) {
				(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
				__xuiMessageSetTextSelection(pData, iTextNode, iTextOffset, iTextNode, iTextOffset);
				pData->bSelecting = 1;
				(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
				return XUI_EVENT_DISPATCH_STOP;
			}
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
	} else if ( pEvent->iType == XUI_EVENT_POINTER_UP || pEvent->iType == XUI_EVENT_POINTER_CAPTURE_LOST ) {
		if ( pData->bSelecting || xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
			pData->bSelecting = 0;
			if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) (void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
	} else if ( pEvent->iType == XUI_EVENT_POINTER_CLICK || pEvent->iType == XUI_EVENT_POINTER_DOUBLE_CLICK ) {
		iIndex = __xuiMessageGetIndexAtData(pWidget, pData, pEvent->fX, pEvent->fY);
		if ( iIndex >= 0 ) {
			pData->iClickCount++;
			(void)__xuiMessageNotify(pWidget, pData, pEvent->iType == XUI_EVENT_POINTER_CLICK ? XUI_MESSAGE_EVENT_CLICK : XUI_MESSAGE_EVENT_DOUBLE_CLICK, iIndex, pEvent);
		}
	} else if ( pEvent->iType == XUI_EVENT_CONTEXT_MENU ) {
		iIndex = __xuiMessageGetIndexAtData(pWidget, pData, pEvent->fX, pEvent->fY);
		if ( iIndex >= 0 ) {
			pNode = &pData->arrNodes[iIndex];
			pData->iSelected = iIndex;
			if ( __xuiMessageNodeCanSelectText(pNode) && __xuiMessagePositionCompare(pData->iSelectionAnchorNode, pData->iSelectionAnchorOffset, pData->iSelectionActiveNode, pData->iSelectionActiveOffset) == 0 ) {
				__xuiMessageSetTextSelection(pData, iIndex, 0, iIndex, (int)strlen(__xuiMessageText(pNode->sText)));
			}
			(void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_CONTEXT_MENU, iIndex, pEvent);
			(void)__xuiMessageOpenContextMenu(pWidget, pData, pEvent->fX, pEvent->fY);
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
	} else if ( pEvent->iType == XUI_EVENT_POINTER_WHEEL ) {
		tContent = xuiWidgetGetContentRect(pWidget);
		fMaxScroll = __xuiMessageMax(0.0f, pData->fContentHeight - tContent.fH);
		fOldScroll = pData->fScrollY;
		pData->fScrollY = __xuiMessageClamp(pData->fScrollY - pEvent->fWheelY * pData->tMetrics.fWheelStep, 0.0f, fMaxScroll);
		if ( pData->fScrollY != fOldScroll ) {
			(void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_SCROLL, -1, pEvent);
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
	} else if ( pEvent->iType == XUI_EVENT_BOUNDS_CHANGED ) {
		return __xuiMessageInvalidate(pWidget, pData);
	} else if ( pEvent->iType == XUI_EVENT_KEY_DOWN ) {
		if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 && (pEvent->iKey == 'c' || pEvent->iKey == 'C') ) {
			(void)xuiMessageListCopySelection(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	return XUI_OK;
}

static int __xuiMessageInitEvents(xui_widget pWidget)
{
	int iRet;
	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiMessageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiMessageEvent, NULL);
	return iRet;
}

static int __xuiMessageContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_message_list_data_t* pData;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	pSize->fX = (tConstraint.fX > 0.0f && tConstraint.fX < XUI_LAYOUT_UNBOUNDED) ? tConstraint.fX : 360.0f;
	pSize->fY = __xuiMessageMin(__xuiMessageMax(160.0f, pData->fContentHeight), (tConstraint.fY > 0.0f) ? tConstraint.fY : XUI_LAYOUT_UNBOUNDED);
	return XUI_OK;
}

static int __xuiMessageDrawWrappedText(xui_widget pWidget, xui_message_list_data_t* pData, xui_proxy pProxy, xui_draw_context pDraw, const char* sText, int iNodeIndex, xui_rect_t tRect, uint32_t iColor, int bCenter)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	xui_text_line_t tLine;
	xui_font pFont;
	xui_rect_t tLineRect;
	xui_rect_t tSelectionRect;
	const char* sLayoutText;
	float fStart;
	float fEnd;
	int iStart;
	int iEnd;
	int iLine;
	int iRet;
	int bOwnedLayout;
	if ( pWidget == NULL || pData == NULL || pProxy == NULL || pDraw == NULL || tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XUI_OK;
	pFont = __xuiMessageFont(pWidget, pData);
	/* Headless callers may intentionally omit a font; preserve the old no-op rendering behavior. */
	if ( pFont == NULL ) return XUI_OK;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = __xuiMessageText(sText);
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = tRect.fW;
	tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	bOwnedLayout = 0;
	if ( iNodeIndex >= 0 && iNodeIndex < pData->iNodeCount &&
	     sText == pData->arrNodes[iNodeIndex].sText ) {
		iRet = __xuiMessageEnsureNodeTextLayout(pWidget, pData, &pData->arrNodes[iNodeIndex], tRect.fW, &pLayout);
	} else {
		iRet = xuiTextLayoutCreate(xuiWidgetGetContext(pWidget), &pLayout, &tDesc);
		bOwnedLayout = (iRet == XUI_OK);
	}
	if ( iRet != XUI_OK ) return iRet;
	sLayoutText = xuiTextLayoutGetText(pLayout);
	for ( iLine = 0; iLine < xuiTextLayoutGetLineCount(pLayout); iLine++ ) {
		memset(&tLine, 0, sizeof(tLine));
		tLine.iSize = sizeof(tLine);
		if ( xuiTextLayoutGetLine(pLayout, iLine, &tLine) != XUI_OK || tLine.iTextSize <= 0 ) continue;
		tLineRect = (xui_rect_t){tRect.fX + (bCenter ? __xuiMessageMax(0.0f, (tRect.fW - tLine.fW) * 0.5f) : 0.0f), tRect.fY + tLine.fY, bCenter ? tLine.fW : tRect.fW, tLine.fH};
		if ( iNodeIndex >= 0 && __xuiMessageGetTextSelectionForNode(pData, iNodeIndex, &iStart, &iEnd) ) {
			iStart = (iStart > tLine.iTextOffset) ? iStart : tLine.iTextOffset;
			iEnd = (iEnd < tLine.iTextOffset + tLine.iTextSize) ? iEnd : tLine.iTextOffset + tLine.iTextSize;
			if ( iEnd > iStart ) {
				fStart = __xuiMessagePrefixWidth(pWidget, pFont, sLayoutText, tLine.iTextOffset, iStart - tLine.iTextOffset);
				fEnd = __xuiMessagePrefixWidth(pWidget, pFont, sLayoutText, tLine.iTextOffset, iEnd - tLine.iTextOffset);
				tSelectionRect = (xui_rect_t){tLineRect.fX + fStart, tLineRect.fY, __xuiMessageMax(1.0f, fEnd - fStart), tLineRect.fH};
				(void)__xuiMessageDrawFill(pProxy, pDraw, tSelectionRect, XUI_COLOR_RGBA(68, 130, 205, 104));
			}
		}
		iRet = __xuiMessageEnsureLineScratch(pData, tLine.iTextSize + 1);
		if ( iRet != XUI_OK ) {
			if ( bOwnedLayout ) xuiTextLayoutDestroy(pLayout);
			return iRet;
		}
		memcpy(pData->sLineScratch, sLayoutText + tLine.iTextOffset, (size_t)tLine.iTextSize);
		pData->sLineScratch[tLine.iTextSize] = 0;
		iRet = __xuiMessageDrawText(pProxy, pDraw, pFont, pData->sLineScratch, tLineRect, iColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) {
			if ( bOwnedLayout ) xuiTextLayoutDestroy(pLayout);
			return iRet;
		}
	}
	if ( bOwnedLayout ) xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static int __xuiMessageCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_message_list_data_t* pData;
	xui_message_node_data_t* pNode;
	xui_message_node_t tPublicNode;
	xui_proxy pProxy;
	xui_font pFont;
	xui_rect_t tContent;
	xui_rect_t tNode;
	xui_rect_t tBubble;
	xui_rect_t tText;
	xui_rect_t tHeader;
	xui_rect_t tAvatar;
	xui_rect_t tMeta;
	uint32_t iBubbleColor;
	uint32_t iAvatarColor;
	uint32_t iTextColor;
	int i;
	int iHandled;
	int iRet;
	(void)iStateId;
	(void)pUser;
	pData = __xuiMessageListGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	pFont = __xuiMessageFont(pWidget, pData);
	tContent = xuiWidgetGetContentRect(pWidget);
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	(void)__xuiMessageDrawFill(pProxy, pDraw, tContent, pData->tColors.iBackgroundColor);
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		pNode = &pData->arrNodes[i];
		tNode = pNode->tNodeRect;
		tNode.fX += tContent.fX;
		tNode.fY += tContent.fY - pData->fScrollY;
		if ( tNode.fY + tNode.fH < tContent.fY || tNode.fY > tContent.fY + tContent.fH ) continue;
		tPublicNode = __xuiMessagePublicNode(pNode);
		if ( pData->onRenderNode != NULL ) {
			iHandled = pData->onRenderNode(pWidget, i, &tPublicNode, pDraw, tNode, (i == pData->iSelected ? XUI_WIDGET_STATE_ACTIVE : 0) | (i == pData->iHover ? XUI_WIDGET_STATE_HOVER : 0), pData->pRenderNodeUser);
			if ( iHandled < 0 ) return iHandled;
			if ( iHandled ) continue;
		}
		if ( i == pData->iHover ) (void)__xuiMessageDrawFill(pProxy, pDraw, tNode, pData->tColors.iHoverColor);
		if ( i == pData->iSelected ) (void)__xuiMessageDrawFill(pProxy, pDraw, tNode, pData->tColors.iSelectedColor);
		tBubble = pNode->tBubbleRect;
		tBubble.fX += tContent.fX;
		tBubble.fY += tContent.fY - pData->fScrollY;
		if ( pNode->iType == XUI_MESSAGE_NODE_SYSTEM ) {
			(void)__xuiMessageDrawRectFill(pProxy, pDraw, tBubble, pData->tColors.iSystemBubbleColor);
			tText = pNode->tTextRect;
			tText.fX += tContent.fX;
			tText.fY += tContent.fY - pData->fScrollY;
			(void)__xuiMessageDrawWrappedText(pWidget, pData, pProxy, pDraw, pNode->sText, i, tText, pData->tColors.iSystemTextColor, 1);
			continue;
		}
		if ( pNode->iType == XUI_MESSAGE_NODE_AUXILIARY ) {
			(void)__xuiMessageDrawRectFill(pProxy, pDraw, tBubble, XUI_COLOR_RGBA(255, 255, 255, 226));
			(void)__xuiMessageDrawRectStroke(pProxy, pDraw, tBubble, 1.0f, pData->tColors.iBorderColor);
			tHeader = pNode->tHeaderRect;
			tHeader.fX += tContent.fX;
			tHeader.fY += tContent.fY - pData->fScrollY;
			(void)__xuiMessageDrawFill(pProxy, pDraw, tHeader, XUI_COLOR_RGBA(232, 237, 243, 220));
			(void)__xuiMessageDrawText(pProxy, pDraw, pFont, (pNode->iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSED) ? ">" : "v", (xui_rect_t){tHeader.fX + 7.0f, tHeader.fY, 12.0f, tHeader.fH}, pData->tColors.iMetaTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			(void)__xuiMessageDrawText(pProxy, pDraw, pFont, __xuiMessageAuxiliaryTitle(pWidget, pNode), (xui_rect_t){tHeader.fX + 23.0f, tHeader.fY, tHeader.fW - 30.0f, tHeader.fH}, pData->tColors.iMetaTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( (pNode->iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSED) == 0 ) {
				tText = pNode->tTextRect;
				tText.fX += tContent.fX;
				tText.fY += tContent.fY - pData->fScrollY;
				iRet = __xuiMessageDrawWrappedText(pWidget, pData, pProxy, pDraw, pNode->sText, i, tText, pData->tColors.iOtherTextColor, 0);
				if ( iRet != XUI_OK ) return iRet;
			}
			continue;
		}
		iBubbleColor = (pNode->iType == XUI_MESSAGE_NODE_SELF) ? pData->tColors.iSelfBubbleColor : pData->tColors.iOtherBubbleColor;
		iTextColor = (pNode->iType == XUI_MESSAGE_NODE_SELF) ? pData->tColors.iSelfTextColor : pData->tColors.iOtherTextColor;
		tAvatar.fY = tNode.fY + pData->tMetrics.fMetaHeight;
		tAvatar.fW = pData->tMetrics.fAvatarSize;
		tAvatar.fH = pData->tMetrics.fAvatarSize;
		tAvatar.fX = (pNode->iType == XUI_MESSAGE_NODE_SELF) ? (tContent.fX + tContent.fW - pData->tMetrics.fPaddingX - pData->tMetrics.fAvatarSize) : (tContent.fX + pData->tMetrics.fPaddingX);
		iAvatarColor = (pNode->iType == XUI_MESSAGE_NODE_SELF) ? pData->tColors.iAvatarSelfColor : pData->tColors.iAvatarOtherColor;
		if ( __xuiMessageAlpha(iAvatarColor) != 0 ) {
			if ( pProxy->drawCircleFill != NULL ) {
				(void)pProxy->drawCircleFill(pProxy, pDraw, tAvatar.fX + tAvatar.fW * 0.5f, tAvatar.fY + tAvatar.fH * 0.5f, pData->tMetrics.fAvatarSize * 0.5f, iAvatarColor);
			} else {
				(void)__xuiMessageDrawRectFill(pProxy, pDraw, tAvatar, iAvatarColor);
			}
		}
		tMeta.fY = tNode.fY;
		tMeta.fH = pData->tMetrics.fMetaHeight;
		tMeta.fX = tBubble.fX;
		tMeta.fW = tBubble.fW;
		if ( pNode->iType == XUI_MESSAGE_NODE_SELF ) {
			(void)__xuiMessageDrawText(pProxy, pDraw, pFont, __xuiMessageText(pNode->sSender), tMeta, pData->tColors.iMetaTextColor, XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		} else {
			(void)__xuiMessageDrawText(pProxy, pDraw, pFont, __xuiMessageText(pNode->sSender), tMeta, pData->tColors.iMetaTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
		(void)__xuiMessageDrawRectFill(pProxy, pDraw, tBubble, iBubbleColor);
		(void)__xuiMessageDrawRectStroke(pProxy, pDraw, tBubble, 1.0f, pData->tColors.iBorderColor);
		tText = pNode->tTextRect;
		tText.fX += tContent.fX;
		tText.fY += tContent.fY - pData->fScrollY;
		iRet = __xuiMessageDrawWrappedText(pWidget, pData, pProxy, pDraw, pNode->sText, i, tText, iTextColor, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiMessageInitContextMenu(xui_widget pWidget, xui_message_list_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;
	if ( pWidget == NULL || pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pContextMenu, &tDesc);
	if ( iRet != XUI_OK ) {
		pData->pContextMenu = NULL;
		return iRet;
	}
	return xuiMenuSetSelect(pData->pContextMenu, __xuiMessageContextMenuSelect, pWidget);
}

static int __xuiMessageInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_message_list_data_t* pData;
	const xui_message_list_desc_t* pDesc;
	int iRet;
	(void)pUser;
	pData = (xui_message_list_data_t*)pTypeData;
	pDesc = (const xui_message_list_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiMessageDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData, 0, sizeof(*pData));
	__xuiMessageDefaultMetrics(&pData->tMetrics);
	__xuiMessageDefaultColors(&pData->tColors);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( pDesc != NULL && pDesc->bHasMetrics ) {
		if ( !__xuiMessageMetricsValid(&pDesc->tMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->tMetrics = pDesc->tMetrics;
	}
	if ( pDesc != NULL && pDesc->bHasColors ) pData->tColors = pDesc->tColors;
	pData->iHover = -1;
	pData->iSelected = -1;
	pData->iSelectionAnchorNode = -1;
	pData->iSelectionActiveNode = -1;
	pData->bAutoScroll = (pDesc == NULL) ? 1 : (pDesc->bAutoScroll ? 1 : 0);
	if ( pDesc != NULL && pDesc->iNodeCount > 0 ) {
		iRet = xuiMessageListSetNodes(pWidget, pDesc->arrNodes, pDesc->iNodeCount);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiMessageInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	/* Clipboard remains available even if a platform cannot create a popup menu. */
	(void)__xuiMessageInitContextMenu(pWidget, pData);
	return XUI_OK;
}

static void __xuiMessageDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_message_list_data_t* pData;
	(void)pWidget;
	(void)pUser;
	pData = (xui_message_list_data_t*)pTypeData;
	if ( pData == NULL ) return;
	if ( pData->pContextMenu != NULL ) {
		xui_widget pPopup = xuiMenuGetPopupWidget(pData->pContextMenu);
		if ( pPopup != NULL ) xuiWidgetDestroy(pPopup);
		else xuiWidgetDestroy(pData->pContextMenu);
		pData->pContextMenu = NULL;
	}
	__xuiMessageClearData(pData);
	if ( pData->arrNodes != NULL ) xrtFree(pData->arrNodes);
	if ( pData->sLineScratch != NULL ) xrtFree(pData->sLineScratch);
	memset(pData, 0, sizeof(*pData));
}

static void __xuiMessageDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FILL;
	pLayout->iHeightMode = XUI_SIZE_FILL;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_HIDDEN;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiMessageDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

XUI_API xui_widget_type xuiMessageListGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "messagelist");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "messagelist";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_message_list_data_t);
	tDesc.onInit = __xuiMessageInit;
	tDesc.onDestroy = __xuiMessageDestroy;
	tDesc.onContentMeasure = __xuiMessageContentMeasure;
	tDesc.onCacheRender = __xuiMessageCacheRender;
	__xuiMessageDefaultLayout(&tDesc.tLayout);
	__xuiMessageDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	return pType;
}

XUI_API int xuiMessageListCreate(xui_context pContext, xui_widget* ppWidget, const xui_message_list_desc_t* pDesc)
{
	xui_widget_type pType;
	if ( (ppWidget == NULL) || !__xuiMessageDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiMessageListGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiMessageListSetEvent(xui_widget pWidget, xui_message_list_event_proc onEvent, void* pUser)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onEvent = onEvent;
	pData->pEventUser = pUser;
	return XUI_OK;
}

XUI_API int xuiMessageListSetNodeRenderer(xui_widget pWidget, xui_message_list_node_renderer_proc onRender, void* pUser)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onRenderNode = onRender;
	pData->pRenderNodeUser = pUser;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMessageListSetNodes(xui_widget pWidget, const xui_message_node_t* pNodes, int iCount)
{
	xui_message_list_data_t* pData;
	int i;
	int iRet;
	if ( (iCount < 0) || (iCount > 0 && pNodes == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiMessageClearData(pData);
	iRet = __xuiMessageReserve(pData, iCount);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xuiMessageCopyNode(&pData->arrNodes[i], &pNodes[i]);
		if ( iRet != XUI_OK ) {
			pData->iNodeCount = i;
			__xuiMessageClearData(pData);
			return iRet;
		}
	}
	pData->iNodeCount = iCount;
	if ( pData->bAutoScroll ) (void)xuiMessageListScrollToEnd(pWidget);
	return __xuiMessageInvalidate(pWidget, pData);
}

XUI_API int xuiMessageListAddNode(xui_widget pWidget, const xui_message_node_t* pNode)
{
	xui_message_list_data_t* pData;
	int iRet;
	if ( pNode == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMessageReserve(pData, pData->iNodeCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiMessageCopyNode(&pData->arrNodes[pData->iNodeCount], pNode);
	if ( iRet != XUI_OK ) return iRet;
	pData->iNodeCount++;
	if ( pData->bAutoScroll ) (void)xuiMessageListScrollToEnd(pWidget);
	return __xuiMessageInvalidate(pWidget, pData);
}

static int __xuiMessageFindNodeById(const xui_message_list_data_t* pData, const char* sId)
{
	int i;
	if ( pData == NULL || sId == NULL ) return -1;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		if ( strcmp(__xuiMessageText(pData->arrNodes[i].sId), sId) == 0 ) return i;
	}
	return -1;
}

static int __xuiMessageAppendText(char** ppText, const char* sText)
{
	char* sNew;
	const char* sOld;
	size_t iOldLen;
	size_t iAddLen;
	if ( ppText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sOld = __xuiMessageText(*ppText);
	sText = __xuiMessageText(sText);
	iOldLen = strlen(sOld);
	iAddLen = strlen(sText);
	sNew = (char*)xrtMalloc(iOldLen + iAddLen + 1u);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(sNew, sOld, iOldLen);
	memcpy(sNew + iOldLen, sText, iAddLen + 1u);
	if ( *ppText != NULL ) xrtFree(*ppText);
	*ppText = sNew;
	return XUI_OK;
}

static int __xuiMessageInvalidateAfterNodeUpdate(xui_widget pWidget, xui_message_list_data_t* pData)
{
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMessageInvalidate(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->bAutoScroll ) return xuiMessageListScrollToEnd(pWidget);
	return XUI_OK;
}

XUI_API int xuiMessageListUpdateNodeText(xui_widget pWidget, const char* sId, const char* sText)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	int iIndex;
	int iRet;
	if ( pData == NULL || sId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiMessageFindNodeById(pData, sId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiMessageInvalidateNodeTextLayout(&pData->arrNodes[iIndex]);
	iRet = __xuiMessageReplace(&pData->arrNodes[iIndex].sText, sText);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiMessageInvalidateAfterNodeUpdate(pWidget, pData);
}

XUI_API int xuiMessageListAppendNodeText(xui_widget pWidget, const char* sId, const char* sText)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	int iIndex;
	int iRet;
	if ( pData == NULL || sId == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiMessageFindNodeById(pData, sId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiMessageInvalidateNodeTextLayout(&pData->arrNodes[iIndex]);
	iRet = __xuiMessageAppendText(&pData->arrNodes[iIndex].sText, sText);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiMessageInvalidateAfterNodeUpdate(pWidget, pData);
}

XUI_API int xuiMessageListSetNodeTitle(xui_widget pWidget, const char* sId, const char* sTitle)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	int iIndex;
	int iRet;
	if ( pData == NULL || sId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiMessageFindNodeById(pData, sId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMessageReplace(&pData->arrNodes[iIndex].sTitle, sTitle);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiMessageInvalidateAfterNodeUpdate(pWidget, pData);
}

XUI_API int xuiMessageListSetNodeCollapsed(xui_widget pWidget, const char* sId, int bCollapsed)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	int iIndex;
	if ( pData == NULL || sId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiMessageFindNodeById(pData, sId);
	if ( iIndex < 0 || pData->arrNodes[iIndex].iType != XUI_MESSAGE_NODE_AUXILIARY ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bCollapsed ) pData->arrNodes[iIndex].iFlags |= XUI_MESSAGE_NODE_FLAG_COLLAPSED;
	else pData->arrNodes[iIndex].iFlags &= ~XUI_MESSAGE_NODE_FLAG_COLLAPSED;
	return __xuiMessageInvalidateAfterNodeUpdate(pWidget, pData);
}

XUI_API int xuiMessageListGetNodeCollapsed(xui_widget pWidget, const char* sId)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	int iIndex;
	if ( pData == NULL || sId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiMessageFindNodeById(pData, sId);
	if ( iIndex < 0 || pData->arrNodes[iIndex].iType != XUI_MESSAGE_NODE_AUXILIARY ) return XUI_ERROR_INVALID_ARGUMENT;
	return (pData->arrNodes[iIndex].iFlags & XUI_MESSAGE_NODE_FLAG_COLLAPSED) ? 1 : 0;
}

XUI_API int xuiMessageListClear(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiMessageClearData(pData);
	return __xuiMessageInvalidate(pWidget, pData);
}

XUI_API int xuiMessageListGetNodeCount(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->iNodeCount : 0;
}

XUI_API const xui_message_node_t* xuiMessageListGetNode(xui_widget pWidget, int iIndex)
{
	static xui_message_node_t tNode;
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || iIndex < 0 || iIndex >= pData->iNodeCount ) return NULL;
	tNode = __xuiMessagePublicNode(&pData->arrNodes[iIndex]);
	return &tNode;
}

XUI_API int xuiMessageListSetSelected(xui_widget pWidget, int iIndex)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iIndex < -1 || iIndex >= pData->iNodeCount ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSelected = iIndex;
	pData->iSelectCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMessageListGetSelected(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->iSelected : -1;
}

XUI_API int xuiMessageListGetHoverIndex(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiMessageListGetNodeAt(xui_widget pWidget, float fX, float fY)
{
	return __xuiMessageGetIndexAtData(pWidget, __xuiMessageListGetData(pWidget), fX, fY);
}

XUI_API xui_rect_t xuiMessageListGetNodeRect(xui_widget pWidget, int iIndex)
{
	xui_rect_t tRect = {0.0f, 0.0f, 0.0f, 0.0f};
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || iIndex < 0 || iIndex >= pData->iNodeCount ) return tRect;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	return pData->arrNodes[iIndex].tNodeRect;
}

XUI_API xui_rect_t xuiMessageListGetBubbleRect(xui_widget pWidget, int iIndex)
{
	xui_rect_t tRect = {0.0f, 0.0f, 0.0f, 0.0f};
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || iIndex < 0 || iIndex >= pData->iNodeCount ) return tRect;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	return pData->arrNodes[iIndex].tBubbleRect;
}

XUI_API int xuiMessageListSetScroll(xui_widget pWidget, float fOffsetY)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	xui_rect_t tContent;
	if ( pData == NULL || fOffsetY != fOffsetY ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	tContent = xuiWidgetGetContentRect(pWidget);
	pData->fScrollY = __xuiMessageClamp(fOffsetY, 0.0f, __xuiMessageMax(0.0f, pData->fContentHeight - tContent.fH));
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiMessageListGetScroll(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->fScrollY : 0.0f;
}

XUI_API int xuiMessageListScrollBy(xui_widget pWidget, float fDeltaY)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiMessageListSetScroll(pWidget, pData->fScrollY + fDeltaY);
}

XUI_API int xuiMessageListEnsureVisible(xui_widget pWidget, int iIndex)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	xui_rect_t tContent;
	xui_rect_t tRect;
	if ( pData == NULL || iIndex < 0 || iIndex >= pData->iNodeCount ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	tContent = xuiWidgetGetContentRect(pWidget);
	tRect = pData->arrNodes[iIndex].tNodeRect;
	if ( tRect.fY < pData->fScrollY ) return xuiMessageListSetScroll(pWidget, tRect.fY);
	if ( tRect.fY + tRect.fH > pData->fScrollY + tContent.fH ) return xuiMessageListSetScroll(pWidget, tRect.fY + tRect.fH - tContent.fH);
	return XUI_OK;
}

XUI_API int xuiMessageListScrollToEnd(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	xui_rect_t tContent;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMessageLayoutNodes(pWidget, pData);
	tContent = xuiWidgetGetContentRect(pWidget);
	pData->fScrollY = __xuiMessageMax(0.0f, pData->fContentHeight - tContent.fH);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMessageListSetAutoScroll(xui_widget pWidget, int bAutoScroll)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bAutoScroll = bAutoScroll ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiMessageListGetAutoScroll(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->bAutoScroll : 0;
}

XUI_API int xuiMessageListSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiMessageInvalidate(pWidget, pData);
}

XUI_API xui_font xuiMessageListGetFont(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiMessageListSetMetrics(xui_widget pWidget, const xui_message_list_metrics_t* pMetrics)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || !__xuiMessageMetricsValid(pMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics = *pMetrics;
	return __xuiMessageInvalidate(pWidget, pData);
}

XUI_API int xuiMessageListGetMetrics(xui_widget pWidget, xui_message_list_metrics_t* pMetrics)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || pMetrics == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pMetrics = pData->tMetrics;
	return XUI_OK;
}

XUI_API int xuiMessageListSetColors(xui_widget pWidget, const xui_message_list_colors_t* pColors)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || pColors == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMessageListGetColors(xui_widget pWidget, xui_message_list_colors_t* pColors)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL || pColors == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	return XUI_OK;
}

XUI_API int xuiMessageListClearTextSelection(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSelectionAnchorNode == -1 && pData->iSelectionActiveNode == -1 ) return XUI_OK;
	__xuiMessageSetTextSelection(pData, -1, 0, -1, 0);
	pData->bSelecting = 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMessageListGetSelectedText(xui_widget pWidget, char* sBuffer, int iCapacity)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	char* sText;
	int iNeed;
	if ( pData == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiMessageBuildSelectedText(pData, &sText) != XUI_OK ) {
		if ( sBuffer != NULL && iCapacity > 0 ) sBuffer[0] = 0;
		return 0;
	}
	iNeed = (int)strlen(sText) + 1;
	if ( sBuffer != NULL && iCapacity > 0 ) {
		strncpy(sBuffer, sText, (size_t)iCapacity - 1u);
		sBuffer[iCapacity - 1] = 0;
	}
	xrtFree(sText);
	return iNeed;
}

XUI_API int xuiMessageListCopySelection(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	xui_proxy pProxy;
	char* sText;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMessageBuildSelectedText(pData, &sText);
	if ( iRet != XUI_OK ) return iRet;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL || pProxy->clipboardSetText == NULL ) {
		xrtFree(sText);
		return XUI_ERROR_UNSUPPORTED;
	}
	iRet = pProxy->clipboardSetText(pProxy, sText);
	xrtFree(sText);
	if ( iRet == XUI_OK ) (void)__xuiMessageNotify(pWidget, pData, XUI_MESSAGE_EVENT_COPY, pData->iSelected, NULL);
	return iRet;
}

static int __xuiMessageAppendEscaped(char* sBuffer, int iCapacity, int iPos, const char* sText)
{
	const unsigned char* p;
	if ( sText == NULL ) sText = "";
	for ( p = (const unsigned char*)sText; *p != 0; p++ ) {
		char ch = (char)*p;
		if ( ch == '\\' || ch == '\t' || ch == '\n' || ch == '\r' ) {
			if ( sBuffer != NULL && iPos + 2 < iCapacity ) {
				sBuffer[iPos] = '\\';
				sBuffer[iPos + 1] = (ch == '\t') ? 't' : ((ch == '\n') ? 'n' : ((ch == '\r') ? 'r' : '\\'));
			}
			iPos += 2;
		} else {
			if ( sBuffer != NULL && iPos + 1 < iCapacity ) sBuffer[iPos] = ch;
			iPos++;
		}
	}
	return iPos;
}

static int __xuiMessageAppendRaw(char* sBuffer, int iCapacity, int iPos, const char* sText)
{
	int iLen = (int)strlen(__xuiMessageText(sText));
	if ( sBuffer != NULL && iPos < iCapacity ) {
		int iCopy = iLen;
		if ( iCopy > iCapacity - iPos - 1 ) iCopy = iCapacity - iPos - 1;
		if ( iCopy > 0 ) memcpy(sBuffer + iPos, sText, (size_t)iCopy);
	}
	return iPos + iLen;
}

XUI_API int xuiMessageListExportText(xui_widget pWidget, char* sBuffer, int iCapacity)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	xui_message_node_data_t* pNode;
	int i;
	int iPos;
	char sNum[64];
	if ( pData == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iPos = 0;
	iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "MESSAGELIST2\n");
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		pNode = &pData->arrNodes[i];
		sprintf(sNum, "N\t%d\t%d\t%d\t", pNode->iType, pNode->iFlags, pNode->iAuxiliaryKind);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, sNum);
		iPos = __xuiMessageAppendEscaped(sBuffer, iCapacity, iPos, pNode->sId);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "\t");
		iPos = __xuiMessageAppendEscaped(sBuffer, iCapacity, iPos, pNode->sParentId);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "\t");
		iPos = __xuiMessageAppendEscaped(sBuffer, iCapacity, iPos, pNode->sTitle);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "\t");
		iPos = __xuiMessageAppendEscaped(sBuffer, iCapacity, iPos, pNode->sSender);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "\t");
		iPos = __xuiMessageAppendEscaped(sBuffer, iCapacity, iPos, pNode->sTime);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "\t");
		iPos = __xuiMessageAppendEscaped(sBuffer, iCapacity, iPos, pNode->sText);
		iPos = __xuiMessageAppendRaw(sBuffer, iCapacity, iPos, "\n");
	}
	if ( sBuffer != NULL && iCapacity > 0 ) {
		sBuffer[(iPos < iCapacity) ? iPos : (iCapacity - 1)] = 0;
	}
	return iPos + 1;
}

static char* __xuiMessageUnescapeField(const char* sStart, const char* sEnd)
{
	char* sOut;
	char* sWrite;
	const char* p;
	size_t iLen;
	if ( sStart == NULL || sEnd == NULL || sEnd < sStart ) return NULL;
	iLen = (size_t)(sEnd - sStart);
	sOut = (char*)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) return NULL;
	sWrite = sOut;
	for ( p = sStart; p < sEnd; p++ ) {
		if ( *p == '\\' && p + 1 < sEnd ) {
			p++;
			*sWrite++ = (*p == 't') ? '\t' : ((*p == 'n') ? '\n' : ((*p == 'r') ? '\r' : *p));
		} else {
			*sWrite++ = *p;
		}
	}
	*sWrite = 0;
	return sOut;
}

static int __xuiMessageParseIntField(const char* sStart, const char* sEnd, int* pValue)
{
	char sNumber[32];
	char* sParseEnd;
	long iValue;
	size_t iLength;
	if ( sStart == NULL || sEnd == NULL || pValue == NULL || sEnd <= sStart ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = (size_t)(sEnd - sStart);
	if ( iLength >= sizeof(sNumber) ) return XUI_ERROR_INVALID_ARGUMENT;
	memcpy(sNumber, sStart, iLength);
	sNumber[iLength] = 0;
	iValue = strtol(sNumber, &sParseEnd, 10);
	if ( sParseEnd == sNumber || *sParseEnd != 0 || iValue < INT_MIN || iValue > INT_MAX ) return XUI_ERROR_INVALID_ARGUMENT;
	*pValue = (int)iValue;
	return XUI_OK;
}

static void __xuiMessageDestroyTemporaryData(xui_message_list_data_t* pData)
{
	if ( pData == NULL ) return;
	__xuiMessageClearData(pData);
	if ( pData->arrNodes != NULL ) xrtFree(pData->arrNodes);
	pData->arrNodes = NULL;
	pData->iNodeCapacity = 0;
}

static int __xuiMessageAppendImportedNode(xui_message_list_data_t* pData, const xui_message_node_t* pNode)
{
	int iRet;
	iRet = __xuiMessageReserve(pData, pData->iNodeCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiMessageCopyNode(&pData->arrNodes[pData->iNodeCount], pNode);
	if ( iRet != XUI_OK ) return iRet;
	pData->iNodeCount++;
	return XUI_OK;
}

XUI_API int xuiMessageListImportText(xui_widget pWidget, const char* sText)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	xui_message_list_data_t tImported;
	xui_message_node_t tNode;
	xui_message_node_data_t* arrOldNodes;
	int iOldNodeCount;
	const char* p;
	const char* e;
	const char* n;
	const char* f[10];
	char* sId;
	char* sParentId;
	char* sTitle;
	char* sSender;
	char* sTime;
	char* sBody;
	int iField;
	int iRet;
	int bVersion2;
	if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tImported, 0, sizeof(tImported));
	p = sText;
	bVersion2 = strncmp(p, "MESSAGELIST2", 12) == 0;
	if ( bVersion2 || strncmp(p, "MESSAGELIST1", 12) == 0 ) {
		e = strchr(p, '\n');
		p = (e != NULL) ? e + 1 : p + strlen(p);
	}
	while ( *p != 0 ) {
		e = strchr(p, '\n');
		if ( e == NULL ) e = p + strlen(p);
		n = (*e == '\n') ? e + 1 : e;
		if ( e > p && e[-1] == '\r' ) e--;
		if ( e > p && p[0] == 'N' && p[1] == '\t' ) {
			f[0] = p;
			iField = 1;
			for ( ; iField < (bVersion2 ? 10 : 7); iField++ ) {
				f[iField] = strchr(f[iField - 1] + 1, '\t');
				if ( f[iField] == NULL || f[iField] > e ) break;
			}
			if ( iField == (bVersion2 ? 10 : 7) ) {
				memset(&tNode, 0, sizeof(tNode));
				tNode.iSize = sizeof(tNode);
				iRet = __xuiMessageParseIntField(f[1] + 1, f[2], &tNode.iType);
				if ( iRet == XUI_OK ) iRet = __xuiMessageParseIntField(f[2] + 1, f[3], &tNode.iFlags);
				if ( iRet == XUI_OK && bVersion2 ) iRet = __xuiMessageParseIntField(f[3] + 1, f[4], &tNode.iAuxiliaryKind);
				if ( iRet != XUI_OK || !__xuiMessageNodeTypeValid(tNode.iType) ) {
					__xuiMessageDestroyTemporaryData(&tImported);
					return XUI_ERROR_INVALID_ARGUMENT;
				}
				sId = __xuiMessageUnescapeField(f[bVersion2 ? 4 : 3] + 1, f[bVersion2 ? 5 : 4]);
				sParentId = bVersion2 ? __xuiMessageUnescapeField(f[5] + 1, f[6]) : __xuiMessageDup("");
				sTitle = bVersion2 ? __xuiMessageUnescapeField(f[6] + 1, f[7]) : __xuiMessageDup("");
				sSender = __xuiMessageUnescapeField(f[bVersion2 ? 7 : 4] + 1, f[bVersion2 ? 8 : 5]);
				sTime = __xuiMessageUnescapeField(f[bVersion2 ? 8 : 5] + 1, f[bVersion2 ? 9 : 6]);
				sBody = __xuiMessageUnescapeField(f[bVersion2 ? 9 : 6] + 1, e);
				if ( sId == NULL || sParentId == NULL || sTitle == NULL || sSender == NULL || sTime == NULL || sBody == NULL ) {
					if ( sId != NULL ) xrtFree(sId);
					if ( sParentId != NULL ) xrtFree(sParentId);
					if ( sTitle != NULL ) xrtFree(sTitle);
					if ( sSender != NULL ) xrtFree(sSender);
					if ( sTime != NULL ) xrtFree(sTime);
					if ( sBody != NULL ) xrtFree(sBody);
					__xuiMessageDestroyTemporaryData(&tImported);
					return XUI_ERROR_OUT_OF_MEMORY;
				}
				tNode.sId = sId;
				tNode.sParentId = sParentId;
				tNode.sTitle = sTitle;
				tNode.sSender = sSender;
				tNode.sTime = sTime;
				tNode.sText = sBody;
				iRet = __xuiMessageAppendImportedNode(&tImported, &tNode);
				xrtFree(sId);
				xrtFree(sParentId);
				xrtFree(sTitle);
				xrtFree(sSender);
				xrtFree(sTime);
				xrtFree(sBody);
				if ( iRet != XUI_OK ) {
					__xuiMessageDestroyTemporaryData(&tImported);
					return iRet;
				}
			} else {
				__xuiMessageDestroyTemporaryData(&tImported);
				return XUI_ERROR_INVALID_ARGUMENT;
			}
		} else if ( e > p ) {
			__xuiMessageDestroyTemporaryData(&tImported);
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		p = n;
	}
	arrOldNodes = pData->arrNodes;
	iOldNodeCount = pData->iNodeCount;
	pData->arrNodes = NULL;
	pData->iNodeCount = 0;
	pData->iNodeCapacity = 0;
	while ( iOldNodeCount > 0 ) __xuiMessageFreeNode(&arrOldNodes[--iOldNodeCount]);
	if ( arrOldNodes != NULL ) xrtFree(arrOldNodes);
	pData->arrNodes = tImported.arrNodes;
	pData->iNodeCount = tImported.iNodeCount;
	pData->iNodeCapacity = tImported.iNodeCapacity;
	pData->iHover = -1;
	pData->iSelected = -1;
	pData->iSelectionAnchorNode = -1;
	pData->iSelectionAnchorOffset = 0;
	pData->iSelectionActiveNode = -1;
	pData->iSelectionActiveOffset = 0;
	pData->bSelecting = 0;
	pData->fScrollY = 0.0f;
	pData->fContentHeight = 0.0f;
	return __xuiMessageInvalidate(pWidget, pData);
}

XUI_API int xuiMessageListSaveFile(xui_widget pWidget, const char* sPath)
{
	FILE* fp;
	char* sBuffer;
	int iNeed;
	int iRet;
	if ( sPath == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNeed = xuiMessageListExportText(pWidget, NULL, 0);
	if ( iNeed < 0 ) return iNeed;
	sBuffer = (char*)xrtMalloc((size_t)iNeed);
	if ( sBuffer == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiMessageListExportText(pWidget, sBuffer, iNeed);
	if ( iRet < 0 ) {
		xrtFree(sBuffer);
		return iRet;
	}
	fp = fopen(sPath, "wb");
	if ( fp == NULL ) {
		xrtFree(sBuffer);
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	if ( fwrite(sBuffer, 1u, strlen(sBuffer), fp) != strlen(sBuffer) || fclose(fp) != 0 ) {
		xrtFree(sBuffer);
		return XUI_ERROR;
	}
	xrtFree(sBuffer);
	return XUI_OK;
}

XUI_API int xuiMessageListLoadFile(xui_widget pWidget, const char* sPath)
{
	FILE* fp;
	char* sBuffer;
	long iSize;
	int iRet;
	if ( sPath == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	fp = fopen(sPath, "rb");
	if ( fp == NULL ) return XUI_ERROR_FILE_NOT_FOUND;
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		return XUI_ERROR;
	}
	iSize = ftell(fp);
	if ( iSize < 0 || iSize > INT_MAX || fseek(fp, 0, SEEK_SET) != 0 ) {
		fclose(fp);
		return XUI_ERROR;
	}
	sBuffer = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( sBuffer == NULL ) {
		fclose(fp);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( fread(sBuffer, 1u, (size_t)iSize, fp) != (size_t)iSize ) {
		fclose(fp);
		xrtFree(sBuffer);
		return XUI_ERROR;
	}
	fclose(fp);
	sBuffer[iSize] = 0;
	iRet = xuiMessageListImportText(pWidget, sBuffer);
	xrtFree(sBuffer);
	return iRet;
}

XUI_API int xuiMessageListGetSelectCount(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiMessageListGetClickCount(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->iClickCount : 0;
}

XUI_API int xuiMessageListGetChangeCount(xui_widget pWidget)
{
	xui_message_list_data_t* pData = __xuiMessageListGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
