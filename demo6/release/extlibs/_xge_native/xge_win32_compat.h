/*
 * xge_win32_compat.h - Windows SDK 兼容性补充
 *
 * TCC 嵌入式 VFS 的 Windows 头文件缺少部分定义，
 * 此文件补充 XGE/sokol 所需的缺失类型和宏。
 * 由 xge_compile.c 在包含 xge.h 之前 include。
 */
#ifndef XGE_WIN32_COMPAT_H
#define XGE_WIN32_COMPAT_H

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

/* windowsx.h 宏 */
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

/* shellapi.h 拖放类型 */
#ifndef HDROP_DEFINED
typedef HANDLE HDROP;
#define HDROP_DEFINED
#endif

#ifndef DragQueryFile
#ifdef UNICODE
#define DragQueryFile DragQueryFileW
#else
#define DragQueryFile DragQueryFileA
#endif
#endif

WINSHELLAPI UINT WINAPI DragQueryFileA(HDROP,UINT,LPSTR,UINT);
WINSHELLAPI UINT WINAPI DragQueryFileW(HDROP,UINT,LPWSTR,UINT);
WINSHELLAPI void WINAPI DragFinish(HDROP);
WINSHELLAPI BOOL WINAPI DragQueryPoint(HDROP,LPPOINT);

/* imm.h 类型 */
#ifndef HIMC_DEFINED
typedef HANDLE HIMC;
#define HIMC_DEFINED
#endif

#ifndef GCS_COMPSTR
#define GCS_COMPSTR 0x0008
#endif
#ifndef GCS_RESULTSTR
#define GCS_RESULTSTR 0x0800
#endif
#ifndef IMN_SETOPENSTATUS
#define IMN_SETOPENSTATUS 0x0004
#endif
#ifndef IMN_SETCOMPOSITIONWINDOW
#define IMN_SETCOMPOSITIONWINDOW 0x000C
#endif
#ifndef CFS_POINT
#define CFS_POINT 0x0002
#endif
#ifndef CFS_RECT
#define CFS_RECT 0x0001
#endif

typedef struct tagCOMPOSITIONFORM_XGE {
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT rcArea;
} COMPOSITIONFORM_XGE;

#ifndef COMPOSITIONFORM_DEFINED
#define COMPOSITIONFORM COMPOSITIONFORM_XGE
#define LPCOMPOSITIONFORM COMPOSITIONFORM_XGE*
#define COMPOSITIONFORM_DEFINED
#endif

#endif /* _WIN32 || _WIN64 */
#endif /* XGE_WIN32_COMPAT_H */
