/*
 * Minimal windowsx.h compatibility surface for the embedded TCC SDK.
 * The including Win32 source must include windows.h first.
 */
#ifndef XL6_TCC_WINDOWSX_H
#define XL6_TCC_WINDOWSX_H

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

/* shellapi.h drag-and-drop declarations absent from the compact SDK. */
#ifndef HDROP_DEFINED
typedef HANDLE HDROP;
#define HDROP_DEFINED
#endif

#ifndef WINSHELLAPI
#define WINSHELLAPI DECLSPEC_IMPORT
#endif

WINSHELLAPI UINT WINAPI DragQueryFileA(HDROP, UINT, LPSTR, UINT);
WINSHELLAPI UINT WINAPI DragQueryFileW(HDROP, UINT, LPWSTR, UINT);
WINSHELLAPI void WINAPI DragFinish(HDROP);
WINSHELLAPI BOOL WINAPI DragQueryPoint(HDROP, LPPOINT);
#ifndef DragQueryFile
#ifdef UNICODE
#define DragQueryFile DragQueryFileW
#else
#define DragQueryFile DragQueryFileA
#endif
#endif

/* Input Method Manager definitions used by Sokol/XGE. */
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

typedef struct tagCOMPOSITIONFORM_SHIM {
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT rcArea;
} COMPOSITIONFORM_SHIM;

#ifndef COMPOSITIONFORM
#define COMPOSITIONFORM COMPOSITIONFORM_SHIM
#define LPCOMPOSITIONFORM COMPOSITIONFORM_SHIM*
#endif

#endif
