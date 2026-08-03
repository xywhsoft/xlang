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

/* TCC PE 链接器要求外部变量必须声明 dllimport (xCore) */
#if defined(_WIN32) && defined(__TINYC__)
#define XXAPI __declspec(dllimport)
#endif

#include "xrt.h"
#include "xge.h"
#include "xui.h"
#include <string.h>

/* ========================================
 * 回调 trampoline
 * 模式：xlang 传入 xvalue 函数，C 侧存储并通过 trampoline 调用
 * ======================================== */

/* 全局回调存储（单例模式，XGE 只有一个运行循环） */
static xvalue g_xgeFrameFunc = NULL;  /* xgeRun 帧回调 */

/* 帧回调 trampoline: int (*)(void* pUser) */
static int __xge_frame_trampoline(void* pUser) {
    xrt_call_frame frame;
    xrt_call_result result;
    (void)pUser;
    if (g_xgeFrameFunc == NULL) return 0;
    memset(&frame, 0, sizeof(frame));
    memset(&result, 0, sizeof(result));
    frame.Argc = 0;
    frame.Argv = NULL;
    xvoInvoke(g_xgeFrameFunc, &frame, &result);
    return 0;
}

/* Bridge: xlang 调用此函数启动 XGE 运行循环
 * xlang 侧: xgeRunBridge(func) -> 传入 xlang func 值 */
int __xl6_xgeRunBridge(void* pFunc) {
    if (g_xgeFrameFunc != NULL) {
        xvoUnref(g_xgeFrameFunc);
    }
    g_xgeFrameFunc = (xvalue)pFunc;
    if (g_xgeFrameFunc != NULL) {
        xvoAddRef(g_xgeFrameFunc);
    }
    return xgeRun(__xge_frame_trampoline, NULL);
}

/* 通用回调 trampoline 工厂（用于 XUI 控件回调）
 * 模式：void (*callback)(xui_widget pWidget, void* pUser) */
static int __xui_widget_trampoline(xui_widget pWidget, void* pUser) {
    xvalue pFunc = (xvalue)pUser;
    xrt_call_frame frame;
    xrt_call_result result;
    xvalue args[1];
    if (pFunc == NULL) return 0;
    memset(&frame, 0, sizeof(frame));
    memset(&result, 0, sizeof(result));
    args[0] = xvoCreateInt((int64)(size_t)pWidget);
    frame.Argc = 1;
    frame.Argv = args;
    xvoInvoke(pFunc, &frame, &result);
    xvoUnref(args[0]);
    return 0;
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

static xvalue g_tutFrameFunc = NULL;

static int __tut_frame_cb(void* pUser) {
    xrt_call_frame frame;
    xrt_call_result result;
    (void)pUser;
    if (g_tutFrameFunc == NULL) return 0;
    memset(&frame, 0, sizeof(frame));
    memset(&result, 0, sizeof(result));
    xvoInvoke(g_tutFrameFunc, &frame, &result);
    return 0;
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
int __xl6_xgeTutorialRun(void* pFunc) {
    if (g_tutFrameFunc != NULL) xvoUnref(g_tutFrameFunc);
    g_tutFrameFunc = (xvalue)pFunc;
    if (g_tutFrameFunc != NULL) xvoAddRef(g_tutFrameFunc);
    return xgeRun(__tut_frame_cb, NULL);
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
        xrtSetError(buf, 0);
    }
}
