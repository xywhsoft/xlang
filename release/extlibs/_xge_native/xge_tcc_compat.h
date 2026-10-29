/*
 * xge_tcc_compat.h - TCC 编译器 Windows SDK 兼容性补充
 *
 * TCC 嵌入式 VFS 的 Windows 头文件缺少部分较新的 API 定义。
 * 此文件在 #include <windows.h> 之后包含，补充缺失类型。
 *
 * 由 sync_xge.ps1 自动 patch 到需要的源文件中。
 */
#ifndef XGE_TCC_COMPAT_H
#define XGE_TCC_COMPAT_H

#if defined(_WIN32) || defined(_WIN64)

/* === ConPTY (Pseudo Console) === */
#ifndef HPCON_DEFINED
typedef void* HPCON;
#define HPCON_DEFINED
#endif

/* PROC_THREAD_ATTRIBUTE_LIST (opaque) */
#ifndef PROC_THREAD_ATTRIBUTE_LIST_DEFINED
typedef struct _PROC_THREAD_ATTRIBUTE_LIST *PPROC_THREAD_ATTRIBUTE_LIST;
typedef PPROC_THREAD_ATTRIBUTE_LIST LPPROC_THREAD_ATTRIBUTE_LIST;
#define PROC_THREAD_ATTRIBUTE_LIST_DEFINED
#endif

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

/* ProcThreadAttribute functions (kernel32.dll) */
#ifndef _PROC_THREAD_ATTRIBUTE_FUNCS_DECLARED
#define _PROC_THREAD_ATTRIBUTE_FUNCS_DECLARED
WINBASEAPI BOOL WINAPI InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST,DWORD,DWORD,PSIZE_T);
WINBASEAPI BOOL WINAPI UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST,DWORD,DWORD_PTR,PVOID,SIZE_T,PVOID,PSIZE_T);
WINBASEAPI VOID WINAPI DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST);
#endif

/* STARTUPINFOEXA (extended startup info for ConPTY) */
#ifndef STARTUPINFOEXA_DEFINED
typedef struct _STARTUPINFOEXA {
    STARTUPINFOA StartupInfo;
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEXA, *LPSTARTUPINFOEXA;
#define STARTUPINFOEXA_DEFINED
#endif

#ifndef EXTENDED_STARTUPINFO_PRESENT
#define EXTENDED_STARTUPINFO_PRESENT 0x00080000
#endif

/* === IME types (imm.h supplement) === */
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
#ifndef GCS_CURSORPOS
#define GCS_CURSORPOS 0x0040
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
#ifndef CFS_EXCLUDE
#define CFS_EXCLUDE 0x0004
#endif

#ifndef COMPOSITIONFORM_DEFINED_XGE
typedef struct tagCOMPOSITIONFORM_XGE2 {
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT rcArea;
} COMPOSITIONFORM_XGE2;
#define COMPOSITIONFORM COMPOSITIONFORM_XGE2
#define LPCOMPOSITIONFORM COMPOSITIONFORM_XGE2*
#define COMPOSITIONFORM_DEFINED_XGE
#endif

#ifndef CANDIDATEFORM_DEFINED_XGE
typedef struct tagCANDIDATEFORM_XGE {
    DWORD dwIndex;
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT rcArea;
} CANDIDATEFORM_XGE;
#define CANDIDATEFORM CANDIDATEFORM_XGE
#define LPCANDIDATEFORM CANDIDATEFORM_XGE*
#define CANDIDATEFORM_DEFINED_XGE
#endif

/* === windowsx.h macros === */
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

/* === shellapi.h drag/drop === */
#ifndef HDROP_DEFINED
typedef HANDLE HDROP;
#define HDROP_DEFINED
#endif

#ifndef WINSHELLAPI
#define WINSHELLAPI DECLSPEC_IMPORT
#endif

#ifndef _DRAG_DROP_FUNCS_DECLARED
#define _DRAG_DROP_FUNCS_DECLARED
WINSHELLAPI UINT WINAPI DragQueryFileA(HDROP,UINT,LPSTR,UINT);
WINSHELLAPI UINT WINAPI DragQueryFileW(HDROP,UINT,LPWSTR,UINT);
WINSHELLAPI void WINAPI DragFinish(HDROP);
WINSHELLAPI BOOL WINAPI DragQueryPoint(HDROP,LPPOINT);
#endif
#ifndef DragQueryFile
#ifdef UNICODE
#define DragQueryFile DragQueryFileW
#else
#define DragQueryFile DragQueryFileA
#endif
#endif

#endif /* _WIN32 || _WIN64 */
#endif /* XGE_TCC_COMPAT_H */
