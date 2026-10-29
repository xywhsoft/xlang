/*
 * xui_native.c - XUI xlang OOP 桥接层
 *
 * 提供 XuiApp 框架：封装 XGE 引擎初始化 + XUI context/proxy/surface/font/root
 * 的完整 7 步初始化、帧管线和销毁。
 *
 * 以及回调 trampoline：将 XUI 控件回调转发到 xlang func 值。
 *
 * 手工编写，sync_xge.ps1 不覆盖此文件。
 */

/* TCC PE 链接器要求 XRT 导入符号使用 dllimport。 */
#if defined(_WIN32) && defined(__TINYC__)
#define XRT_API __declspec(dllimport)
#endif

#include "xrt.h"
#include "xruntime.h"
#include "xge.h"
#include "xui.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void __xl6_xuiCallbackClearAll(void);
static void __xl6_xuiCallbackClearWidget(xui_widget pWidget);

/* ========================================
 * XUI App 框架
 * ======================================== */

typedef struct xl6_xui_app_t {
    xge_render_target_t tTarget;
    xui_proxy_t         tProxy;
    xui_context         pContext;
    xui_surface         pTargetSurface;
    xui_font            pFont;
    xui_widget          pRootWidget;
    int                 iWidth;
    int                 iHeight;
} xl6_xui_app_t;

/* 搜索系统字体（Windows） */
static const char* xl6_xui_find_font(void)
{
	static const char* arrNames[] = {
		"msyh.ttc",
		"segoeui.ttf",
		"arial.ttf",
		"calibri.ttf",
		"simhei.ttf"
	};
	static char sPath[MAX_PATH];
	const char* sFontDir = "\\Fonts\\";
	size_t iFontDirLen = strlen(sFontDir);
	size_t i;
	UINT iRootLen;

	iRootLen = GetWindowsDirectoryA(sPath, sizeof(sPath));
	if ( (iRootLen == 0u) || (iRootLen >= sizeof(sPath)) ) { return NULL; }
	if ( (iRootLen != 0u) && (sPath[iRootLen - 1u] == '\\') ) { --iRootLen; }

	for ( i = 0u; i < (sizeof(arrNames) / sizeof(arrNames[0])); ++i ) {
		size_t iNameLen = strlen(arrNames[i]);
		DWORD iAttributes;

		if ( ((size_t)iRootLen + iFontDirLen + iNameLen + 1u) > sizeof(sPath) ) {
			continue;
		}
		memcpy(sPath + iRootLen, sFontDir, iFontDirLen);
		memcpy(sPath + iRootLen + iFontDirLen, arrNames[i], iNameLen + 1u);
		iAttributes = GetFileAttributesA(sPath);
		if ( (iAttributes != INVALID_FILE_ATTRIBUTES) &&
			((iAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) ) {
			return sPath;
		}
	}
	return NULL;
}

/* 创建 XUI App：一站式初始化 XGE + XUI */
xl6_xui_app_t* __xl6_xuiAppCreate(int iWidth, int iHeight, const char* sTitle)
{
    xl6_xui_app_t* pApp;
    xge_desc_t tDesc;
    xui_surface_desc_t tSurfaceDesc;
    const char* sFontPath;
    int iRet;

    pApp = (xl6_xui_app_t*)malloc(sizeof(xl6_xui_app_t));
    if (pApp == NULL) return NULL;
    memset(pApp, 0, sizeof(xl6_xui_app_t));
    pApp->iWidth = iWidth;
    pApp->iHeight = iHeight;

    /* 1. XGE 引擎初始化 */
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iWidth = iWidth;
    tDesc.iHeight = iHeight;
    tDesc.sTitle = sTitle;
    tDesc.iFlags = XGE_INIT_WINDOW | XGE_INIT_VSYNC;
    tDesc.iRunMode = XGE_RUN_GAME_LOOP;
    iRet = xgeInit(&tDesc);
    if (iRet != XGE_OK) { free(pApp); return NULL; }

    /* 2. Render Target */
    iRet = xgeRenderTargetCreate(&pApp->tTarget, iWidth, iHeight);
    if (iRet != XGE_OK) { xgeUnit(); free(pApp); return NULL; }

    /* 3. XUI Proxy + Context */
    pApp->tProxy = xuiProxyXge();
    iRet = xuiCreate(&pApp->pContext);
    if (iRet != XUI_OK) { xgeRenderTargetFree(&pApp->tTarget); xgeUnit(); free(pApp); return NULL; }
    iRet = xuiSetProxy(pApp->pContext, &pApp->tProxy);
    if (iRet != XUI_OK) goto fail;

    /* 4. 视口 */
    /* 像素坐标重构后 viewport/input 走整数像素尺寸。 */
    xuiInputViewport(pApp->pContext, iWidth, iHeight);
    xuiSetViewportSize(pApp->pContext, iWidth, iHeight);

    /* 5. Target Surface（XUI 渲染目标） */
    memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
    tSurfaceDesc.iKind = XUI_SURFACE_KIND_TEXTURE;
    tSurfaceDesc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
    tSurfaceDesc.iWidth = iWidth;
    tSurfaceDesc.iHeight = iHeight;
    tSurfaceDesc.iFlags = XUI_SURFACE_ALPHA_PREMULTIPLIED | XUI_SURFACE_USAGE_TARGET;
    iRet = pApp->tProxy.surfaceCreate(&pApp->tProxy, &pApp->pTargetSurface, &tSurfaceDesc);
    if (iRet != XUI_OK) goto fail;

    /* 6. 字体 */
    sFontPath = xl6_xui_find_font();
    if (sFontPath) {
        iRet = pApp->tProxy.fontLoadFile(&pApp->tProxy, &pApp->pFont, sFontPath, 16.0f, XUI_FONT_FORMAT_TTF);
        if (iRet == XUI_OK) {
            xuiSetDefaultFont(pApp->pContext, pApp->pFont);
        }
    }

    /* 7. Root Widget */
    iRet = xuiWidgetCreate(pApp->pContext, &pApp->pRootWidget);
    if (iRet != XUI_OK) goto fail;
    {
        xui_rect_t tRect;
        tRect.fX = 0; tRect.fY = 0; tRect.fW = (float)iWidth; tRect.fH = (float)iHeight;
        xuiWidgetSetRect(pApp->pRootWidget, tRect);
    }
    xuiWidgetSetLayoutType(pApp->pRootWidget, XUI_LAYOUT_COLUMN);
    xuiSetRootWidget(pApp->pContext, pApp->pRootWidget);

    return pApp;

fail:
    if (pApp->pFont) pApp->tProxy.fontDestroy(&pApp->tProxy, pApp->pFont);
    if (pApp->pTargetSurface) pApp->tProxy.surfaceDestroy(&pApp->tProxy, pApp->pTargetSurface);
    if (pApp->pContext) xuiDestroy(pApp->pContext);
    xgeRenderTargetFree(&pApp->tTarget);
    xgeUnit();
    free(pApp);
    return NULL;
}

/* 销毁 XUI App */
int64 __xl6_xuiAppDestroy(xl6_xui_app_t* pApp)
{
    if (pApp == NULL) return 0;
    if (pApp->pContext) xuiDestroy(pApp->pContext);
	__xl6_xuiCallbackClearAll();
    if (pApp->pFont) pApp->tProxy.fontDestroy(&pApp->tProxy, pApp->pFont);
    if (pApp->pTargetSurface) pApp->tProxy.surfaceDestroy(&pApp->tProxy, pApp->pTargetSurface);
    xgeRenderTargetFree(&pApp->tTarget);
    xgeUnit();
    free(pApp);
	return 0;
}

/* XUI 帧管线：输入注入 + 事件派发 + 布局 + 更新 + 渲染 + 贴图 */
int __xl6_xuiAppFrame(xl6_xui_app_t* pApp, uint32_t iBgColor)
{
    xui_rect_i_t tFullRect;
    int iRet;

    if (pApp == NULL) return XGE_ERROR_INVALID_ARGUMENT;

    iRet = xgeBegin();
    if (iRet != XGE_OK) return iRet;

    /* ESC 退出 */
    if (xgeKeyPressed(XGE_KEY_ESCAPE)) { xgeQuit(); }

    /* 输入注入（自动从 XGE 采集） */
    xuiProxyXgePumpInput(pApp->pContext);

    /* 事件派发 */
    xuiDispatchPendingEvents(pApp->pContext);

    /* 布局 + 更新 */
    xuiLayout(pApp->pContext);
    xuiUpdate(pApp->pContext, xgeGetDelta());

    /* 清 target surface + 渲染 XUI 到 target */
    pApp->tProxy.surfaceClear(&pApp->tProxy, pApp->pTargetSurface, iBgColor);
	    tFullRect.iX = 0; tFullRect.iY = 0; tFullRect.iW = pApp->iWidth; tFullRect.iH = pApp->iHeight;
    xuiRender(pApp->pContext, pApp->pTargetSurface, &tFullRect, 1);

    /* 贴到屏幕 */
    xgeClear(iBgColor);
    {
        xui_rect_t tSrc, tDst;
        uint32_t iDrawFlags = XUI_SURFACE_DRAW_SCREEN_SPACE;
        tSrc.fX = 0; tSrc.fY = 0; tSrc.fW = (float)pApp->iWidth; tSrc.fH = (float)pApp->iHeight;
        tDst.fX = 0; tDst.fY = 0; tDst.fW = (float)pApp->iWidth; tDst.fH = (float)pApp->iHeight;
        pApp->tProxy.surfaceDraw(&pApp->tProxy, pApp->pTargetSurface, tSrc, tDst, XUI_COLOR_WHITE, iDrawFlags);
    }

    xgeEnd();
    return XGE_OK;
}

/* 获取 context 句柄 */
xui_context __xl6_xuiAppContext(xl6_xui_app_t* pApp)
{
    if (pApp == NULL) return NULL;
    return pApp->pContext;
}

/* 获取 root widget 句柄 */
xui_widget __xl6_xuiAppRoot(xl6_xui_app_t* pApp)
{
    if (pApp == NULL) return NULL;
    return pApp->pRootWidget;
}

/* 获取 font 句柄 */
xui_font __xl6_xuiAppFont(xl6_xui_app_t* pApp)
{
    if (pApp == NULL) return NULL;
    return pApp->pFont;
}

/* 获取 proxy 句柄（用于控件创建时的 surface/font 操作） */
xui_proxy_t* __xl6_xuiAppProxy(xl6_xui_app_t* pApp)
{
    if (pApp == NULL) return NULL;
    return &pApp->tProxy;
}

/* ========================================
 * XGE 运行循环 bridge（XUI 版本，带 xvalue* 帧回调）
 * ======================================== */

static xvalue* g_xuiFrameFunc = NULL;

static int __xui_frame_trampoline(void* pUser)
{
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bInvoked;

	(void)pUser;
	if ( g_xuiFrameFunc == NULL ) {
		return 0;
	}
	memset(&Frame, 0, sizeof(Frame));
	bInvoked = xrtValueInvoke(g_xuiFrameFunc, &Frame, &Result);
	xrtCallResultUnit(&Result);
	return bInvoked ? 0 : -1;
}

/* 启动运行循环（XUI 版本） */
int __xl6_xuiAppRun(xl6_xui_app_t* pApp, void* pFunc)
{
	xvalue* pCallback;
	int iRet;

	(void)pApp;
	pCallback = (pFunc != NULL) ? xrtValueRetain((xvalue*)pFunc) : NULL;
	if ( (pFunc != NULL) && (pCallback == NULL) ) return XGE_ERROR_OUT_OF_MEMORY;
	xrtValueRelease(g_xuiFrameFunc);
	g_xuiFrameFunc = pCallback;
	iRet = xgeRun(__xui_frame_trampoline, NULL);
	xrtValueRelease(g_xuiFrameFunc);
	g_xuiFrameFunc = NULL;
	return iRet;
}

/* ========================================
 * 控件创建 bridge（out 参数 → 直接返回 handle）
 * ======================================== */

/* Button: 文本 + 字体 → 返回 widget */
xui_widget __xl6_xuiButtonCreate(xui_context pCtx, const char* sText, xui_font pFont)
{
    xui_button_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
	    tDesc.sText = sText;
    tDesc.pFont = pFont;
    tDesc.iTextFlags = XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE;
    xuiButtonCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Label: 文本 + 字体 → 返回 widget */
xui_widget __xl6_xuiLabelCreate(xui_context pCtx, const char* sText, xui_font pFont)
{
    xui_label_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sText = sText;
    tDesc.pFont = pFont;
    xuiLabelCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Panel: → 返回 widget */
xui_widget __xl6_xuiPanelCreate(xui_context pCtx)
{
    xui_panel_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    xuiPanelCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* CheckBox: 文本 + 字体 → 返回 widget */
xui_widget __xl6_xuiCheckBoxCreate(xui_context pCtx, const char* sText, xui_font pFont)
{
    xui_checkbox_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sText = sText;
    tDesc.pFont = pFont;
    tDesc.bUseBuiltinAtlas = 1;
    xuiCheckBoxCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Radio: 文本 + 字体 → 返回 widget */
xui_widget __xl6_xuiRadioCreate(xui_context pCtx, const char* sText, xui_font pFont)
{
    xui_radio_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sText = sText;
    tDesc.pFont = pFont;
    tDesc.bUseBuiltinAtlas = 1;
    xuiRadioCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Toggle: 文本 + 字体 → 返回 widget */
xui_widget __xl6_xuiToggleCreate(xui_context pCtx, const char* sText, xui_font pFont)
{
    xui_toggle_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sText = sText;
    tDesc.pFont = pFont;
    xuiToggleCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Slider: min/max/value → 返回 widget */
xui_widget __xl6_xuiSliderCreate(xui_context pCtx, float fMin, float fMax, float fValue)
{
    xui_slider_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.fMin = fMin;
    tDesc.fMax = fMax;
    tDesc.fValue = fValue;
    xuiSliderCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Progress: min/max/value → 返回 widget */
xui_widget __xl6_xuiProgressCreate(xui_context pCtx, float fMin, float fMax, float fValue)
{
    xui_progress_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.fMin = fMin;
    tDesc.fMax = fMax;
    tDesc.fValue = fValue;
    xuiProgressCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Separator → 返回 widget */
xui_widget __xl6_xuiSeparatorCreate(xui_context pCtx)
{
    xui_separator_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    xuiSeparatorCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Image → 返回 widget */
xui_widget __xl6_xuiImageCreate(xui_context pCtx)
{
    xui_image_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    xuiImageCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Hyperlink: 文本 + 字体 → 返回 widget */
xui_widget __xl6_xuiHyperlinkCreate(xui_context pCtx, const char* sText, xui_font pFont)
{
    xui_hyperlink_desc_t tDesc;
    xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sText = sText;
    tDesc.pFont = pFont;
    xuiHyperlinkCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* ========================================
 * 回调 trampoline bridge
 * ======================================== */

/* 每个控件独立持有回调，避免后设置的控件覆盖先前控件。 */
typedef struct xl6_xui_callback_slot_t {
	xui_widget pWidget;
	xvalue* pFunction;
	struct xl6_xui_callback_slot_t* pNext;
} xl6_xui_callback_slot_t;

static xl6_xui_callback_slot_t* g_xuiCallbackSlots = NULL;



/* 设置或删除控件回调槽；失败时保留原有槽位。 */
static bool __xl6_xuiCallbackSet(xui_widget pWidget, void* pFunc,
	xl6_xui_callback_slot_t** ppSlot)
{
	xl6_xui_callback_slot_t* pPrevious;
	xl6_xui_callback_slot_t* pSlot;
	xvalue* pFunction;

	if ( (pWidget == NULL) || (ppSlot == NULL) ) return false;
	*ppSlot = NULL;
	pFunction = (pFunc != NULL) ? xrtValueRetain((xvalue*)pFunc) : NULL;
	if ( (pFunc != NULL) && (pFunction == NULL) ) return false;
	pPrevious = NULL;
	pSlot = g_xuiCallbackSlots;
	while ( (pSlot != NULL) && (pSlot->pWidget != pWidget) ) {
		pPrevious = pSlot;
		pSlot = pSlot->pNext;
	}
	if ( pSlot != NULL ) {
		if ( pFunction != NULL ) {
			xrtValueRelease(pSlot->pFunction);
			pSlot->pFunction = pFunction;
			*ppSlot = pSlot;
			return true;
		}
		if ( pPrevious != NULL ) pPrevious->pNext = pSlot->pNext;
		else g_xuiCallbackSlots = pSlot->pNext;
		xrtValueRelease(pSlot->pFunction);
		xrtFree(pSlot);
		return true;
	}
	if ( pFunction == NULL ) return true;
	pSlot = (xl6_xui_callback_slot_t*)xrtMalloc(sizeof(*pSlot));
	if ( pSlot == NULL ) {
		xrtValueRelease(pFunction);
		return false;
	}
	pSlot->pWidget = pWidget;
	pSlot->pFunction = pFunction;
	pSlot->pNext = g_xuiCallbackSlots;
	g_xuiCallbackSlots = pSlot;
	*ppSlot = pSlot;
	return true;
}



/* 释放当前 XUI 应用持有的全部语言回调。 */
static void __xl6_xuiCallbackClearAll(void)
{
	xl6_xui_callback_slot_t* pNext;
	xl6_xui_callback_slot_t* pSlot;

	pSlot = g_xuiCallbackSlots;
	g_xuiCallbackSlots = NULL;
	while ( pSlot != NULL ) {
		pNext = pSlot->pNext;
		xrtValueRelease(pSlot->pFunction);
		xrtFree(pSlot);
		pSlot = pNext;
	}
}



/* 释放指定控件持有的语言回调。 */
static void __xl6_xuiCallbackClearWidget(xui_widget pWidget)
{
	xl6_xui_callback_slot_t* pPrevious;
	xl6_xui_callback_slot_t* pSlot;

	pPrevious = NULL;
	pSlot = g_xuiCallbackSlots;
	while ( pSlot != NULL ) {
		if ( pSlot->pWidget == pWidget ) {
			if ( pPrevious != NULL ) pPrevious->pNext = pSlot->pNext;
			else g_xuiCallbackSlots = pSlot->pNext;
			xrtValueRelease(pSlot->pFunction);
			xrtFree(pSlot);
			return;
		}
		pPrevious = pSlot;
		pSlot = pSlot->pNext;
	}
}



/* Widget 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiWidgetDestroyOwned(xui_widget pWidget)
{
	if ( pWidget != NULL ) {
		__xl6_xuiCallbackClearWidget(pWidget);
		xuiWidgetDestroy(pWidget);
	}
	return 0;
}



/* MsgBox 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiMsgBoxDestroyOwned(xui_msgbox pBox)
{
	if ( pBox != NULL ) xuiMsgBoxDestroy(pBox);
	return 0;
}



/* FileDialog 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiFileDialogDestroyOwned(xui_file_dialog pDialog)
{
	if ( pDialog != NULL ) xuiFileDialogDestroy(pDialog);
	return 0;
}



/* MsgTip 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiMsgTipDestroyOwned(xui_msgtip pTip)
{
	if ( pTip != NULL ) xuiMsgTipDestroy(pTip);
	return 0;
}



/* Toast 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiToastDestroyOwned(xui_toast pToast)
{
	if ( pToast != NULL ) xuiToastDestroy(pToast);
	return 0;
}



/* Path 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiPathDestroyOwned(xui_path pPath)
{
	if ( pPath != NULL ) xuiPathDestroy(pPath);
	return 0;
}



/* FlowGraph 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiFlowGraphDestroyOwned(xui_flow_graph pGraph)
{
	if ( pGraph != NULL ) xuiFlowGraphDestroy(pGraph);
	return 0;
}



/* CodeDocument 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiCodeDocumentDestroyOwned(xui_code_document pDocument)
{
	if ( pDocument != NULL ) xuiCodeDocumentDestroy(pDocument);
	return 0;
}



/* RichDocument 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xuiRichDocumentDestroyOwned(xui_rich_document pDocument)
{
	if ( pDocument != NULL ) xuiRichDocumentDestroy(pDocument);
	return 0;
}

/* Click 回调 trampoline: void(xui_widget, void*) */
static void __xui_click_trampoline(xui_widget pWidget, void* pUser)
{
	xl6_xui_callback_slot_t* pSlot;
	xvalue* Arguments[1];
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;

	pSlot = (xl6_xui_callback_slot_t*)pUser;
	if ( (pSlot == NULL) || (pSlot->pFunction == NULL) ) return;
	Arguments[0] = xrtValueInt((int64)(size_t)pWidget);
	if ( Arguments[0] == NULL ) {
		return;
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 1u;
	Frame.Arguments = Arguments;
	(void)xrtValueInvoke(pSlot->pFunction, &Frame, &Result);
	xrtCallResultUnit(&Result);
	xrtValueRelease(Arguments[0]);
}

/* Button 点击回调设置 */
void __xl6_xuiButtonSetClick(xui_widget pWidget, void* pFunc)
{
	xl6_xui_callback_slot_t* pSlot;

	if ( !__xl6_xuiCallbackSet(pWidget, pFunc, &pSlot) ) return;
	(void)xuiButtonSetClick(pWidget,
		(pSlot != NULL) ? __xui_click_trampoline : NULL, pSlot);
}

/* Hyperlink 点击回调设置（复用 click trampoline） */
void __xl6_xuiHyperlinkSetClick(xui_widget pWidget, void* pFunc)
{
	xl6_xui_callback_slot_t* pSlot;

	if ( !__xl6_xuiCallbackSet(pWidget, pFunc, &pSlot) ) return;
	(void)xuiHyperlinkSetClick(pWidget,
		(pSlot != NULL) ? __xui_click_trampoline : NULL, pSlot);
}

/* ========================================
 * Widget 基类结构体参数 bridge
 * 把 xui_rect_t / xui_vec2_t / xui_thickness_t 参数拆为标量
 * ======================================== */

/* SetRect: x, y, w, h → xui_rect_t */
int __xl6_xuiWidgetSetRect(xui_widget pWidget, float fX, float fY, float fW, float fH)
{
    xui_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    return xuiWidgetSetRect(pWidget, rc);
}

/* SetPreferredSize: w, h → xui_vec2_t */
int __xl6_xuiWidgetSetPreferredSize(xui_widget pWidget, float fW, float fH)
{
    xui_vec2_t sz; sz.fX = fW; sz.fY = fH;
    return xuiWidgetSetPreferredSize(pWidget, sz);
}

/* SetMinSize: w, h → xui_vec2_t */
int __xl6_xuiWidgetSetMinSize(xui_widget pWidget, float fW, float fH)
{
    xui_vec2_t sz; sz.fX = fW; sz.fY = fH;
    return xuiWidgetSetMinSize(pWidget, sz);
}

/* SetMaxSize: w, h → xui_vec2_t */
int __xl6_xuiWidgetSetMaxSize(xui_widget pWidget, float fW, float fH)
{
    xui_vec2_t sz; sz.fX = fW; sz.fY = fH;
    return xuiWidgetSetMaxSize(pWidget, sz);
}

/* SetMargin: left, top, right, bottom → xui_thickness_t */
int __xl6_xuiWidgetSetMargin(xui_widget pWidget, float fLeft, float fTop, float fRight, float fBottom)
{
    xui_thickness_t m; m.fLeft = fLeft; m.fTop = fTop; m.fRight = fRight; m.fBottom = fBottom;
    return xuiWidgetSetMargin(pWidget, m);
}

/* SetPadding: left, top, right, bottom → xui_thickness_t */
int __xl6_xuiWidgetSetPadding(xui_widget pWidget, float fLeft, float fTop, float fRight, float fBottom)
{
    xui_thickness_t p; p.fLeft = fLeft; p.fTop = fTop; p.fRight = fRight; p.fBottom = fBottom;
    return xuiWidgetSetPadding(pWidget, p);
}


/* ========================================
 * 批量控件创建 bridge（out 参数 → 直接返回 handle）
 * 每个控件用最简构造器：memset + iSize + Create
 * ======================================== */

/* --- 简单控件（仅 context，desc 全默认） --- */

#define XL6_XUI_SIMPLE_CREATE(FuncName, DescType, CreateFunc) \
xui_widget FuncName(xui_context pCtx) { \
    DescType tDesc; xui_widget pWidget = NULL; \
    memset(&tDesc, 0, sizeof(tDesc)); \
    tDesc.iSize = sizeof(tDesc); \
    CreateFunc(pCtx, &pWidget, &tDesc); \
    return pWidget; \
}

/* --- 带文本的控件（text + font） --- */

#define XL6_XUI_TEXT_CREATE(FuncName, DescType, CreateFunc, UseBuiltinAtlas) \
xui_widget FuncName(xui_context pCtx, const char* sText, xui_font pFont) { \
    DescType tDesc; xui_widget pWidget = NULL; \
    memset(&tDesc, 0, sizeof(tDesc)); \
    tDesc.iSize = sizeof(tDesc); \
    tDesc.sText = sText; tDesc.pFont = pFont; \
    (void)UseBuiltinAtlas; \
    CreateFunc(pCtx, &pWidget, &tDesc); \
    return pWidget; \
}

/* QrCode */
xui_widget __xl6_xuiQrCodeCreate(xui_context pCtx, const char* sText) {
    xui_qrcode_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
	    tDesc.sValue = sText;
    xuiQrCodeCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* StepBar */
xui_widget __xl6_xuiStepBarCreate(xui_context pCtx, int iStepCount, int iCurrent, xui_font pFont) {
    xui_step_bar_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.iStepCount = iStepCount;
    tDesc.iCurrent = iCurrent;
    tDesc.pFont = pFont;
    xuiStepBarCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* RangeSlider */
xui_widget __xl6_xuiRangeSliderCreate(xui_context pCtx, float fMin, float fMax, float fStart, float fEnd) {
    xui_range_slider_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.fMin = fMin; tDesc.fMax = fMax;
    tDesc.fStart = fStart; tDesc.fEnd = fEnd;
    xuiRangeSliderCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* ScrollBar */
xui_widget __xl6_xuiScrollBarCreate(xui_context pCtx, int iOrientation) {
    xui_scrollbar_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.iOrientation = iOrientation;
    xuiScrollBarCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* 创建选择卡片，并把便捷文本参数落实为标签子控件。 */
xui_widget __xl6_xuiCheckCardCreate(xui_context pCtx,
	const char* sText, xui_font pFont)
{
	xui_check_card_desc_t CardDesc;
	xui_label_desc_t LabelDesc;
	xui_widget pCard;
	xui_widget pLabel;
	int iRet;

	pCard = NULL;
	pLabel = NULL;
	memset(&CardDesc, 0, sizeof(CardDesc));
	CardDesc.iSize = sizeof(CardDesc);
	iRet = xuiCheckCardCreate(pCtx, &pCard, &CardDesc);
	if ( (iRet != XUI_OK) || (pCard == NULL) ) return NULL;
	if ( (sText == NULL) || (sText[0] == '\0') ) return pCard;

	memset(&LabelDesc, 0, sizeof(LabelDesc));
	LabelDesc.iSize = sizeof(LabelDesc);
	LabelDesc.sText = sText;
	LabelDesc.pFont = pFont;
	iRet = xuiLabelCreate(pCtx, &pLabel, &LabelDesc);
	if ( (iRet != XUI_OK) || (pLabel == NULL) ||
		 (xuiWidgetAddChild(pCard, pLabel) != XUI_OK) ) {
		if ( pLabel != NULL ) (void)xuiWidgetDestroy(pLabel);
		(void)xuiWidgetDestroy(pCard);
		return NULL;
	}
	return pCard;
}

/* Input */
XL6_XUI_TEXT_CREATE(__xl6_xuiInputCreate, xui_input_desc_t, xuiInputCreate, 0)

/* NumericInput */
xui_widget __xl6_xuiNumericInputCreate(xui_context pCtx, xui_font pFont) {
    xui_numeric_input_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiNumericInputCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* TagInput */
XL6_XUI_TEXT_CREATE(__xl6_xuiTagInputCreate, xui_tag_input_desc_t, xuiTagInputCreate, 0)

/* TextEdit */
xui_widget __xl6_xuiTextEditCreate(xui_context pCtx, const char* sText, xui_font pFont) {
    xui_text_edit_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sText = sText; tDesc.pFont = pFont;
    xuiTextEditCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* RichEdit */
xui_widget __xl6_xuiRichEditCreate(xui_context pCtx, xui_font pFont) {
    xui_rich_edit_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiRichEditCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Tabs */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiTabsCreate, xui_tabs_desc_t, xuiTabsCreate)

/* Window */
xui_widget __xl6_xuiWindowCreate(xui_context pCtx, const char* sTitle, xui_font pFont) {
    xui_window_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sTitle = sTitle; tDesc.pFont = pFont;
    xuiWindowCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Accordion */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiAccordionCreate, xui_accordion_desc_t, xuiAccordionCreate)

/* Page */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiPageCreate, xui_page_desc_t, xuiPageCreate)

/* Carousel */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiCarouselCreate, xui_carousel_desc_t, xuiCarouselCreate)

/* VirtualJoystick */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiVirtualJoystickCreate, xui_virtual_joystick_desc_t, xuiVirtualJoystickCreate)

/* Breadcrumb */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiBreadcrumbCreate, xui_breadcrumb_desc_t, xuiBreadcrumbCreate)

/* Canvas */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiCanvasCreate, xui_canvas_desc_t, xuiCanvasCreate)

/* Popup */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiPopupCreate, xui_popup_desc_t, xuiPopupCreate)

/* Menu */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiMenuCreate, xui_menu_desc_t, xuiMenuCreate)

/* MenuBar */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiMenuBarCreate, xui_menubar_desc_t, xuiMenuBarCreate)

/* Toolbar */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiToolbarCreate, xui_toolbar_desc_t, xuiToolbarCreate)

/* StatusBar */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiStatusBarCreate, xui_statusbar_desc_t, xuiStatusBarCreate)

/* ScrollFrame */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiScrollFrameCreate, xui_scroll_frame_desc_t, xuiScrollFrameCreate)

/* ScrollView */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiScrollViewCreate, xui_scroll_view_desc_t, xuiScrollViewCreate)

/* SplitLayout */
xui_widget __xl6_xuiSplitLayoutCreate(xui_context pCtx, int iOrientation) {
    xui_split_layout_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.iOrientation = iOrientation;
    xuiSplitLayoutCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* ListView */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiListViewCreate, xui_list_view_desc_t, xuiListViewCreate)

/* TreeView */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiTreeViewCreate, xui_tree_view_desc_t, xuiTreeViewCreate)

/* TableView */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiTableViewCreate, xui_table_view_desc_t, xuiTableViewCreate)

/* TableGrid */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiTableGridCreate, xui_table_grid_desc_t, xuiTableGridCreate)

/* PropertyGrid */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiPropertyGridCreate, xui_property_grid_desc_t, xuiPropertyGridCreate)

/* TimeLineView */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiTimeLineViewCreate, xui_timeline_view_desc_t, xuiTimeLineViewCreate)

/* MessageList */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiMessageListCreate, xui_message_list_desc_t, xuiMessageListCreate)

/* InventoryGrid */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiInventoryGridCreate, xui_inventory_grid_desc_t, xuiInventoryGridCreate)

/* Terminal */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiTerminalCreate, xui_terminal_desc_t, xuiTerminalCreate)

/* ComboBox */
xui_widget __xl6_xuiComboBoxCreate(xui_context pCtx, xui_font pFont) {
    xui_combobox_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiComboBoxCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* Cascader */
xui_widget __xl6_xuiCascaderCreate(xui_context pCtx, xui_font pFont) {
    xui_cascader_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiCascaderCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* ColorPicker */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiColorPickerCreate, xui_color_picker_desc_t, xuiColorPickerCreate)

/* IconPicker */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiIconPickerCreate, xui_icon_picker_desc_t, xuiIconPickerCreate)

/* DatePicker */
xui_widget __xl6_xuiDatePickerCreate(xui_context pCtx, xui_font pFont) {
    xui_date_picker_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiDatePickerCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* DockPanel */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiDockPanelCreate, xui_dock_panel_desc_t, xuiDockPanelCreate)

/* Chart */
XL6_XUI_SIMPLE_CREATE(__xl6_xuiChartCreate, xui_chart_desc_t, xuiChartCreate)

/* CodeEdit */
xui_widget __xl6_xuiCodeEditCreate(xui_context pCtx, xui_font pFont) {
    xui_code_edit_desc_t tDesc; xui_widget pWidget = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiCodeEditCreate(pCtx, &pWidget, &tDesc);
    return pWidget;
}

/* ========================================
 * 辅助对象创建 bridge（模式 B/C）
 * 模式 B：对话框/通知（context + 专属 handle 类型，独立 destroy）
 * 模式 C：独立辅助对象（无 context，仅 handle）
 * ======================================== */

/* --- 模式 B：MsgBox（标题 + 消息 + 字体） --- */
xui_msgbox __xl6_xuiMsgBoxCreate(xui_context pCtx, const char* sTitle, const char* sMessage, xui_font pFont) {
    xui_msgbox_desc_t tDesc; xui_msgbox pBox = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.sTitle = sTitle;
    tDesc.sMessage = sMessage;
    tDesc.pFont = pFont;
    xuiMsgBoxCreate(pCtx, &pBox, &tDesc);
    return pBox;
}

/* --- 模式 B：FileDialog --- */
xui_file_dialog __xl6_xuiFileDialogCreate(xui_context pCtx) {
    xui_file_dialog_desc_t tDesc; xui_file_dialog pDialog = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    xuiFileDialogCreate(pCtx, &pDialog, &tDesc);
    return pDialog;
}

/* --- 模式 B：MsgTip --- */
xui_msgtip __xl6_xuiMsgTipCreate(xui_context pCtx) {
    xui_msgtip_desc_t tDesc; xui_msgtip pTip = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    xuiMsgTipCreate(pCtx, &pTip, &tDesc);
    return pTip;
}

/* --- 模式 B：Toast（字体） --- */
xui_toast __xl6_xuiToastCreate(xui_context pCtx, xui_font pFont) {
    xui_toast_desc_t tDesc; xui_toast pToast = NULL;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.iSize = sizeof(tDesc);
    tDesc.pFont = pFont;
    xuiToastCreate(pCtx, &pToast, &tDesc);
    return pToast;
}

/* --- 模式 C：Path（矢量路径） --- */
xui_path __xl6_xuiPathCreate(void) {
    xui_path pPath = NULL;
    xuiPathCreate(&pPath);
    return pPath;
}

/* --- 模式 C：FlowGraph（流程图模型） --- */
xui_flow_graph __xl6_xuiFlowGraphCreate(void) {
    xui_flow_graph pGraph = NULL;
    xuiFlowGraphCreate(&pGraph);
    return pGraph;
}

/* --- 模式 C：CodeDocument（代码文档） --- */
xui_code_document __xl6_xuiCodeDocumentCreate(void) {
    xui_code_document pDoc = NULL;
    xuiCodeDocumentCreate(&pDoc);
    return pDoc;
}

/* --- 模式 C：RichDocument（富文本文档） --- */
xui_rich_document __xl6_xuiRichDocumentCreate(void) {
    xui_rich_document pDoc = NULL;
    xuiRichDocumentCreate(&pDoc);
    return pDoc;
}

/* ========================================
 * 辅助对象便捷 bridge（隐藏回调/结构体参数）
 * ======================================== */

/* Toast 显示（无点击回调版本） */
int __xl6_xuiToastShow(xui_toast pToast, int iType, const char* sTitle, const char* sMessage, float fDuration) {
    return xuiToastShow(pToast, iType, sTitle, sMessage, fDuration, NULL, NULL);
}
