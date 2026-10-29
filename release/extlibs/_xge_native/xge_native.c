/*
 * xge_native.c - XGE/XUI xlang 扩展库 C 桥接层
 *
 * 本文件提供：
 * 1. 回调 trampoline：将 C 回调转发到 xlang func 值
 * 2. 结构体辅助：按值传递的结构体拆分为标量参数
 * 3. 错误传播：统一 xrtSetError 包装
 *
 * 由 sync_xge.ps1 管理，手工编写部分不被 sync 覆盖。
 */

/* TCC PE 链接器要求 XRT 导入符号使用 dllimport。 */
#if defined(_WIN32) && defined(__TINYC__)
#define XRT_API __declspec(dllimport)
#endif

#include "xrt.h"
#include "xruntime.h"
#include "xge.h"
#include "xui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ========================================
 * 回调 trampoline
 * 模式：xlang 传入 xvalue* 函数，C 侧存储并通过 trampoline 调用
 * ======================================== */

/* 全局回调存储（单例模式，XGE 只有一个运行循环） */
static xvalue* g_xgeFrameFunc = NULL;  /* xgeRun 帧回调 */

/* 帧回调 trampoline: int (*)(void* pUser) */
static int __xge_frame_trampoline(void* pUser)
{
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bInvoked;

	(void)pUser;
	if ( g_xgeFrameFunc == NULL ) {
		return 0;
	}
	memset(&Frame, 0, sizeof(Frame));
	bInvoked = xrtValueInvoke(g_xgeFrameFunc, &Frame, &Result);
	xrtCallResultUnit(&Result);
	return bInvoked ? 0 : -1;
}

/* Bridge: xlang 调用此函数启动 XGE 运行循环
 * xlang 侧: xgeRunBridge(func) -> 传入 xlang func 值 */
int __xl6_xgeRunBridge(void* pFunc)
{
	xvalue* pCallback;
	int iRet;

	pCallback = (pFunc != NULL) ? xrtValueRetain((xvalue*)pFunc) : NULL;
	if ( (pFunc != NULL) && (pCallback == NULL) ) return XGE_ERROR_OUT_OF_MEMORY;
	xrtValueRelease(g_xgeFrameFunc);
	g_xgeFrameFunc = pCallback;
	iRet = xgeRun(__xge_frame_trampoline, NULL);
	xrtValueRelease(g_xgeFrameFunc);
	g_xgeFrameFunc = NULL;
	return iRet;
}

/* 通用回调 trampoline 工厂（用于 XUI 控件回调）
 * 模式：void (*callback)(xui_widget pWidget, void* pUser) */
static int __xui_widget_trampoline(xui_widget pWidget, void* pUser)
{
	xvalue* pFunction = (xvalue*)pUser;
	xvalue* Arguments[1];
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bInvoked;

	if ( pFunction == NULL ) {
		return 0;
	}
	Arguments[0] = xrtValueInt((int64)(size_t)pWidget);
	if ( Arguments[0] == NULL ) {
		return -1;
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 1u;
	Frame.Arguments = Arguments;
	bInvoked = xrtValueInvoke(pFunction, &Frame, &Result);
	xrtCallResultUnit(&Result);
	xrtValueRelease(Arguments[0]);
	return bInvoked ? 0 : -1;
}

/* ========================================
 * 结构体辅助函数
 * ======================================== */

/* xge_rect_t 按值传递辅助 */
xge_rect_t __xge_rect_make(float x, float y, float w, float h) {
    xge_rect_t rc;
    rc.fX = x;
    rc.fY = y;
    rc.fW = w;
    rc.fH = h;
    return rc;
}

/* xui_vec2_t 按值传递辅助 */
xui_vec2_t __xui_vec2_make(float x, float y) {
    xui_vec2_t v;
    v.fX = x;
    v.fY = y;
    return v;
}

/* xui_rect_t 按值传递辅助 */
xui_rect_t __xui_rect_make(float x, float y, float w, float h) {
    xui_rect_t rc;
    rc.fX = x;
    rc.fY = y;
    rc.fW = w;
    rc.fH = h;
    return rc;
}

/* ========================================
 * 结构体按值传递 bridge 函数
 * 将拆分的标量参数重组为结构体，调用原始 C 函数
 * ======================================== */

/* --- xge_rect_t 按值 bridge --- */
void __xl6_xgeShapeRectFill(float fX, float fY, float fW, float fH, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRectFill(rc, iColor);
}
void __xl6_xgeShapeRectFillPx(float fX, float fY, float fW, float fH, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRectFillPx(rc, iColor);
}
void __xl6_xgeShapeRectStroke(float fX, float fY, float fW, float fH, float fWidth, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRectStroke(rc, fWidth, iColor);
}
void __xl6_xgeShapeRectStrokePx(float fX, float fY, float fW, float fH, float fWidth, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRectStrokePx(rc, fWidth, iColor);
}
void __xl6_xgeShapeRoundRectFill(float fX, float fY, float fW, float fH, float fRadius, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRoundRectFill(rc, fRadius, iColor);
}
void __xl6_xgeShapeRoundRectStroke(float fX, float fY, float fW, float fH, float fRadius, float fWidth, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRoundRectStroke(rc, fRadius, fWidth, iColor);
}
void __xl6_xgeClipSet(float fX, float fY, float fW, float fH) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeClipSet(rc);
}
void __xl6_xgeViewportSet(float fX, float fY, float fW, float fH) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeViewportSet(rc);
}
void __xl6_xgeInvalidateRect(float fX, float fY, float fW, float fH) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeInvalidateRect(rc);
}

/* --- capsule bridge --- */
void __xl6_xgeShapeCapsuleFill(float fX, float fY, float fW, float fH, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeCapsuleFill(rc, iColor);
}
void __xl6_xgeShapeCapsuleStroke(float fX, float fY, float fW, float fH, float fWidth, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeCapsuleStroke(rc, fWidth, iColor);
}

/* --- roundrect draw/ex bridge --- */
void __xl6_xgeShapeRoundRectDraw(float fX, float fY, float fW, float fH, float fRadius, unsigned int iFillColor, float fStrokeWidth, unsigned int iStrokeColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xgeShapeRoundRectDraw(rc, fRadius, iFillColor, fStrokeWidth, iStrokeColor);
}
void __xl6_xgeShapeRoundRectFillEx(float fX, float fY, float fW, float fH, float fTL, float fTR, float fBR, float fBL, int iMode, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xge_shape_round_rect_t radii; radii.fTopLeft = fTL; radii.fTopRight = fTR; radii.fBottomRight = fBR; radii.fBottomLeft = fBL;
    xgeShapeRoundRectFillEx(rc, radii, iMode, iColor);
}
void __xl6_xgeShapeRoundRectStrokeEx(float fX, float fY, float fW, float fH, float fTL, float fTR, float fBR, float fBL, int iMode, float fWidth, unsigned int iColor) {
    xge_rect_t rc; rc.fX = fX; rc.fY = fY; rc.fW = fW; rc.fH = fH;
    xge_shape_round_rect_t radii; radii.fTopLeft = fTL; radii.fTopRight = fTR; radii.fBottomRight = fBR; radii.fBottomLeft = fBL;
    xgeShapeRoundRectStrokeEx(rc, radii, iMode, fWidth, iColor);
}

/* --- camera bridge --- */
void __xl6_xgeCameraSetSimple(float fPosX, float fPosY, float fScaleX, float fScaleY, float fWidth, float fHeight) {
    xge_camera_t cam = xgeCameraDefault(fWidth, fHeight);
    cam.tPosition.fX = fPosX;
    cam.tPosition.fY = fPosY;
    cam.tScale.fX = fScaleX;
    cam.tScale.fY = fScaleY;
    xgeCameraSet(&cam);
}

/* --- xge_vec2_t 按值 bridge --- */
void __xl6_xgeShapeTriangleFill(float fX0, float fY0, float fX1, float fY1, float fX2, float fY2, unsigned int iColor) {
    xge_vec2_t v0, v1, v2;
    v0.fX = fX0; v0.fY = fY0;
    v1.fX = fX1; v1.fY = fY1;
    v2.fX = fX2; v2.fY = fY2;
    xgeShapeTriangleFill(v0, v1, v2, iColor);
}
void __xl6_xgeShapeTriangleStroke(float fX0, float fY0, float fX1, float fY1, float fX2, float fY2, float fWidth, unsigned int iColor) {
    xge_vec2_t v0, v1, v2;
    v0.fX = fX0; v0.fY = fY0;
    v1.fX = fX1; v1.fY = fY1;
    v2.fX = fX2; v2.fY = fY2;
    xgeShapeTriangleStroke(v0, v1, v2, fWidth, iColor);
}

/* ========================================
 * 教程框架 bridge
 * 简化 XGE 初始化 + 帧循环，供范例使用
 * ======================================== */

static xvalue* g_tutFrameFunc = NULL;

/* 帧数上限（>0 时达到上限自动 xgeQuit，用于无头/CI 冒烟验证） */
static int g_tutMaxFrames = 0;
static int g_tutFrameCount = 0;

static int __tut_frame_cb(void* pUser)
{
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bInvoked;

	(void)pUser;
	++g_tutFrameCount;
	if ( (g_tutMaxFrames > 0) &&
		(g_tutFrameCount >= g_tutMaxFrames) ) {
		xgeQuit();
	}
	if ( g_tutFrameFunc == NULL ) {
		return 0;
	}
	memset(&Frame, 0, sizeof(Frame));
	bInvoked = xrtValueInvoke(g_tutFrameFunc, &Frame, &Result);
	xrtCallResultUnit(&Result);
	return bInvoked ? 0 : -1;
}

/* 初始化 XGE 窗口 */
int __xl6_xgeTutorialInit(int iWidth, int iHeight, const char* sTitle) {
    xge_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.iWidth = iWidth;
    desc.iHeight = iHeight;
    desc.sTitle = sTitle;
    desc.iFlags = XGE_INIT_WINDOW | XGE_INIT_VSYNC;
    desc.iRunMode = XGE_RUN_GAME_LOOP;
    return xgeInit(&desc);
}

/* 运行帧循环（传入 xlang func 作为帧回调） */
int __xl6_xgeTutorialRun(void* pFunc)
{
	xvalue* pCallback;
	int iRet;

	pCallback = (pFunc != NULL) ? xrtValueRetain((xvalue*)pFunc) : NULL;
	if ( (pFunc != NULL) && (pCallback == NULL) ) return XGE_ERROR_OUT_OF_MEMORY;
	xrtValueRelease(g_tutFrameFunc);
	g_tutFrameFunc = pCallback;
	iRet = xgeRun(__tut_frame_cb, NULL);
	xrtValueRelease(g_tutFrameFunc);
	g_tutFrameFunc = NULL;
	return iRet;
}

/* 帧开始 + 清屏 */
int __xl6_xgeTutorialBeginFrame(unsigned int iClearColor) {
    int ret = xgeBegin();
    if (ret == 0) xgeClear(iClearColor);
    return ret;
}

/* 帧结束 */
int __xl6_xgeTutorialEndFrame(void) {
    return xgeEnd();
}

/* ========================================
 * 错误传播辅助
 * ======================================== */

/* 通用 XGE 错误检查 */
void __xge_check_error(int ret, const char* funcName) {
    if (ret != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s failed with code %d", funcName, ret);
        xrtSetErrorInfo(XERR_STATE, "xlang.extlib.xge", ret, buf);
    }
}

/* ========================================
 * XGE OOP 引擎层 bridge
 * 为 xge_oop.xl 提供 OOP 友好的 C bridge
 * ======================================== */

/* --- 帧增量 --- */
float __xl6_xgeGetDelta(void) {
    return xgeGetDelta();
}

/* --- Render Target 管理（结构体需要 malloc，不能在 xlang 侧分配） --- */

/* 创建 render target，返回堆分配的结构体指针 */
xge_render_target __xl6_xgeRenderTargetCreate(int iWidth, int iHeight) {
    xge_render_target pTarget = (xge_render_target)malloc(sizeof(xge_render_target_t));
    if (pTarget == NULL) return NULL;
    memset(pTarget, 0, sizeof(xge_render_target_t));
    if (xgeRenderTargetCreate(pTarget, iWidth, iHeight) != 0) {
        free(pTarget);
        return NULL;
    }
    return pTarget;
}

/* 释放 render target */
void __xl6_xgeRenderTargetFree(xge_render_target pTarget) {
    if (pTarget == NULL) return;
    xgeRenderTargetFree(pTarget);
    free(pTarget);
}

/* 获取 render target 的纹理句柄 */
xge_texture __xl6_xgeRenderTargetTexture(xge_render_target pTarget) {
    if (pTarget == NULL) return NULL;
    return xgeRenderTargetTexture(pTarget);
}

/* --- Render Pass 管理 --- */

/* 创建 pass，返回堆分配的结构体指针 */
xge_pass __xl6_xgePassCreate(xge_render_target pTarget, uint32_t iClearFlags, uint32_t iClearColor) {
    xge_pass pPass = (xge_pass)malloc(sizeof(xge_pass_t));
    if (pPass == NULL) return NULL;
    memset(pPass, 0, sizeof(xge_pass_t));
    xgePassInit(pPass, pTarget, iClearFlags, iClearColor);
    return pPass;
}

/* 销毁 pass */
void __xl6_xgePassDestroy(xge_pass pPass) {
    if (pPass == NULL) return;
    free(pPass);
}

/* --- 鼠标坐标获取（两个 out 参数） --- */
void __xl6_xgeMouseGetPos(float* pOutX, float* pOutY) {
    float x = 0.0f, y = 0.0f;
    xgeMouseGet(&x, &y);
    if (pOutX) *pOutX = x;
    if (pOutY) *pOutY = y;
}

/* --- 帧统计 --- */
float __xl6_xgeGetFPS(void) {
    xge_frame_stats_t stats = xgeFrameStatsGet();
    if (stats.fFrameTimeMs > 0.0f) {
        return 1000.0f / stats.fFrameTimeMs;
    }
    return 0.0f;
}


/* ========================================
 * ShapeEx / Svg 对象 bridge（out 参数 → 直接返回 handle）
 * ======================================== */

/* ShapeEx 创建 */
xge_shape_ex __xl6_xgeShapeExCreate(void) {
    xge_shape_ex pShape = NULL;
    xgeShapeExCreate(&pShape);
    return pShape;
}

/* ShapeEx 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xgeShapeExDestroyOwned(xge_shape_ex pShape)
{
	xgeShapeExDestroy(pShape);
	return 0;
}

/* Svg 创建 */
xge_svg __xl6_xgeSvgCreate(void) {
    xge_svg pSvg = NULL;
    xgeSvgCreate(&pSvg);
    return pSvg;
}

/* Svg 类析构遵循 xlang native-backed class 的整数返回契约。 */
int64 __xl6_xgeSvgDestroyOwned(xge_svg pSvg)
{
	xgeSvgDestroy(pSvg);
	return 0;
}

/* Svg 从 URI 加载（创建 + 加载一步完成） */
xge_svg __xl6_xgeSvgLoadFile(const char* sURI) {
    xge_svg pSvg = NULL;
    if (xgeSvgLoadCached(sURI, &pSvg) != XGE_OK) {
        return NULL;
    }
    return pSvg;
}

/* Svg 获取尺寸（out 参数拆为返回结构不便，用两个 out float 指针） */
void __xl6_xgeSvgGetSize(xge_svg pSvg, float* pOutW, float* pOutH) {
    float w = 0.0f, h = 0.0f;
    if (pSvg == NULL) {
        if (pOutW) *pOutW = 0.0f;
        if (pOutH) *pOutH = 0.0f;
        return;
    }
    xgeSvgGetSize(pSvg, &w, &h);
    if (pOutW) *pOutW = w;
    if (pOutH) *pOutH = h;
}

/* Svg 绘制（dst rect 拆标量） */
int __xl6_xgeSvgDraw(xge_svg pSvg, float fX, float fY, float fW, float fH, float fTolerance) {
    xge_rect_t tDst;
    tDst.fX = fX; tDst.fY = fY; tDst.fW = fW; tDst.fH = fH;
    return xgeSvgDraw(pSvg, tDst, fTolerance);
}

/* ========================================
 * 像素精确几何 bridge（整数像素坐标，半开区间矩形）
 * XGE 工作区像素坐标重构新增的 xge_rect_i_t / xge_edges_i_t 按值参数拆标量
 * ======================================== */

/* 像素对齐矩形填充 */
void __xl6_xgeShapeRectFillPixels(int iX, int iY, int iW, int iH, unsigned int iColor) {
    xge_rect_i_t rc; rc.iX = iX; rc.iY = iY; rc.iW = iW; rc.iH = iH;
    xgeShapeRectFillPixels(rc, iColor);
}

/* 像素对齐矩形边框（四边独立宽度） */
void __xl6_xgeShapeRectBorderPixels(int iX, int iY, int iW, int iH, int iLeft, int iTop, int iRight, int iBottom, unsigned int iColor) {
    xge_rect_i_t rc; rc.iX = iX; rc.iY = iY; rc.iW = iW; rc.iH = iH;
    xge_edges_i_t e; e.iLeft = iLeft; e.iTop = iTop; e.iRight = iRight; e.iBottom = iBottom;
    xgeShapeRectBorderPixels(rc, e, iColor);
}

/* 像素视口设置 */
void __xl6_xgeViewportSetPixels(int iX, int iY, int iW, int iH) {
    xge_rect_i_t rc; rc.iX = iX; rc.iY = iY; rc.iW = iW; rc.iH = iH;
    xgeViewportSetPixels(rc);
}

/* 像素裁剪设置 */
void __xl6_xgeClipSetPixels(int iX, int iY, int iW, int iH) {
    xge_rect_i_t rc; rc.iX = iX; rc.iY = iY; rc.iW = iW; rc.iH = iH;
    xgeClipSetPixels(rc);
}

/* ========================================
 * 游戏基础设施 bridge
 * 鼠标位置 / 字体 / 纹理 / 声音 / 帧数上限
 * ======================================== */

/* --- 鼠标位置（原 API 是 out float* 指针，拆成两个返回函数） --- */
float __xl6_xgeMouseX(void) {
    float x = 0.0f, y = 0.0f;
    xgeMouseGet(&x, &y);
    return x;
}

float __xl6_xgeMouseY(void) {
    float x = 0.0f, y = 0.0f;
    xgeMouseGet(&x, &y);
    return y;
}

/* --- 字体（xge_font_t 为公开结构体，调用方分配；bridge 堆分配后返回） --- */
xge_font __xl6_xgeFontLoadFile(const char* sPath, float fSize) {
    xge_font pFont = (xge_font)malloc(sizeof(xge_font_t));
    if (pFont == NULL) return NULL;
    memset(pFont, 0, sizeof(xge_font_t));
    if (xgeFontLoad(pFont, sPath, fSize) != XGE_OK) {
        free(pFont);
        return NULL;
    }
    return pFont;
}

void __xl6_xgeFontFreeFile(xge_font pFont) {
    if (pFont == NULL) return;
    xgeFontFree(pFont);
    free(pFont);
}

/* --- 纹理（同上分配模式） --- */
xge_texture __xl6_xgeTextureLoadFile(const char* sPath) {
    xge_texture pTex = (xge_texture)malloc(sizeof(xge_texture_t));
    if (pTex == NULL) return NULL;
    memset(pTex, 0, sizeof(xge_texture_t));
    if (xgeTextureLoad(pTex, sPath) != XGE_OK) {
        free(pTex);
        return NULL;
    }
    return pTex;
}

void __xl6_xgeTextureFreeFile(xge_texture pTex) {
    if (pTex == NULL) return;
    xgeTextureFree(pTex);
    free(pTex);
}

/* 缩放+着色绘制（xgeDrawEx 的 xge_draw_t 结构体拆标量） */
void __xl6_xgeDrawScaled(xge_texture pTex, float fX, float fY, float fW, float fH, unsigned int iColor) {
    xge_draw_t tDraw;
    if (pTex == NULL) return;
    memset(&tDraw, 0, sizeof(tDraw));
    tDraw.pTexture = pTex;
    tDraw.tSrc.fX = 0.0f; tDraw.tSrc.fY = 0.0f; tDraw.tSrc.fW = 1.0f; tDraw.tSrc.fH = 1.0f;
    tDraw.tDst.fX = fX; tDraw.tDst.fY = fY; tDraw.tDst.fW = fW; tDraw.tDst.fH = fH;
    tDraw.tOrigin.fX = 0.0f; tDraw.tOrigin.fY = 0.0f;
    tDraw.fRotation = 0.0f;
    tDraw.iColor = iColor;
    tDraw.iFlags = 0;
    xgeDrawEx(&tDraw);
}

/* --- 声音（xge_sound_t 公开结构体） --- */
xge_sound __xl6_xgeSoundLoadFile(const char* sPath) {
    xge_sound pSound = (xge_sound)malloc(sizeof(xge_sound_t));
    if (pSound == NULL) return NULL;
    memset(pSound, 0, sizeof(xge_sound_t));
    if (xgeSoundLoad(pSound, sPath) != XGE_OK) {
        free(pSound);
        return NULL;
    }
    return pSound;
}

void __xl6_xgeSoundFreeFile(xge_sound pSound) {
    if (pSound == NULL) return;
    xgeSoundFree(pSound);
    free(pSound);
}

/* --- 音乐 --- */
xge_music __xl6_xgeMusicLoadFile(const char* sPath) {
    xge_music pMusic = (xge_music)malloc(sizeof(xge_music_t));
    if (pMusic == NULL) return NULL;
    memset(pMusic, 0, sizeof(xge_music_t));
    if (xgeMusicLoad(pMusic, sPath) != XGE_OK) {
        free(pMusic);
        return NULL;
    }
    return pMusic;
}

void __xl6_xgeMusicFreeFile(xge_music pMusic) {
    if (pMusic == NULL) return;
    xgeMusicFree(pMusic);
    free(pMusic);
}

/* --- 帧数上限（g_tutMaxFrames 定义在教程帧回调区，此处只导出设置函数） --- */
void __xl6_xgeSetMaxFrames(int iMaxFrames) {
    g_tutMaxFrames = iMaxFrames;
    g_tutFrameCount = 0;
}
