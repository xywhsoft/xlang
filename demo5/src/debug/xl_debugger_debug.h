#ifndef XL_DEBUGGER_DEBUG_H
#define XL_DEBUGGER_DEBUG_H

#include "../common/xl_common.h"

#if XLANG_DEBUGMODE_ENABLED
#include "../../lib/libtcc.h"

#define XL_DEBUG_BREAKPOINT_MAX 512

typedef struct XL_DebugBreakpoint {
	char sSource[XL_PATH_MAX_LEN];
	int iLine;
	int iColumn;
	int bVerified;
	int iActualLine;
	char sMessage[128];
} XL_DebugBreakpoint;

typedef struct XL_DebugController {
	XL_DebugBreakpoint arrBreakpoints[XL_DEBUG_BREAKPOINT_MAX];
	size_t iBreakpointCount;
	char sCurrentSource[XL_PATH_MAX_LEN];
	int iCurrentLine;
	int iCurrentColumn;
	int iFrameDepth;
	size_t iProbeCount;
	int bStopped;
	char sStopReason[32];
	char sStopSource[XL_PATH_MAX_LEN];
	int iStopLine;
	int iStopColumn;
	int iStopFrameDepth;
	size_t iStopProbeCount;
	int bBlockingEnabled;
	void* hStoppedEvent;
	void* hResumeEvent;
} XL_DebugController;

void xlDebugControllerInit(XL_DebugController* pController);
void xlDebugControllerFree(XL_DebugController* pController);
void xlDebugControllerMakeCurrent(XL_DebugController* pController);
void xlDebugControllerClearCurrent(XL_DebugController* pController);
int xlDebugControllerSetBreakpoints(
	XL_DebugController* pController,
	const char* sSource,
	const int* arrLines,
	const int* arrColumns,
	size_t iCount
);
size_t xlDebugControllerBreakpointCount(const XL_DebugController* pController);
const XL_DebugBreakpoint* xlDebugControllerBreakpointAt(const XL_DebugController* pController, size_t iIndex);
size_t xlDebugControllerProbeCount(const XL_DebugController* pController);
void xlDebugControllerClearStop(XL_DebugController* pController);
int xlDebugControllerHasStop(const XL_DebugController* pController);
void xlDebugControllerSetBlocking(XL_DebugController* pController, int bEnabled);
void xlDebugControllerResume(XL_DebugController* pController);

void xl_dbg_stmt(const char* sSource, int iLine, int iColumn);
void xl_dbg_enter_func(const char* sSource, int iLine, int iColumn, const char* sFuncName, int iFuncIndex);
void xl_dbg_leave_func(void);
void xl_dbg_local_int(const char* sName, long long iValue);
void xl_dbg_local_bool(const char* sName, int bValue);
void xl_dbg_local_float(const char* sName, double fValue);
void xl_dbg_local_string(const char* sName, const char* sValue);
void xl_dbg_local_value(const char* sName, void* pValue);
int xlDebugRegisterHostSymbols(TCCState* pTcc);

#endif

#endif
