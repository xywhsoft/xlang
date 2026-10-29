/* Minimal Input Method Manager declarations for the embedded TCC SDK. */
#ifndef XL6_TCC_IMM_H
#define XL6_TCC_IMM_H

#ifndef HIMC_DEFINED
typedef HANDLE HIMC;
#define HIMC_DEFINED
#endif

#ifndef HIMCMAX
#define HIMCMAX 0xFFFF
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

#ifndef COMPOSITIONFORM
typedef struct tagCOMPOSITIONFORM {
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT rcArea;
} COMPOSITIONFORM, *LPCOMPOSITIONFORM;
#endif

#ifndef CANDIDATEFORM
typedef struct tagCANDIDATEFORM {
    DWORD dwIndex;
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT rcArea;
} CANDIDATEFORM, *LPCANDIDATEFORM;
#endif

#endif
