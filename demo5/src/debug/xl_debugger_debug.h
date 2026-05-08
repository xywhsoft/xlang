#ifndef XL_DEBUGGER_DEBUG_H
#define XL_DEBUGGER_DEBUG_H

#include "../common/xl_common.h"

#if XLANG_DEBUGMODE_ENABLED
#include "../../lib/libtcc.h"

#define XL_DEBUG_BREAKPOINT_MAX 512
#define XL_DEBUG_VARIABLE_MAX 256
#define XL_DEBUG_FRAME_MAX 128
#define XL_DEBUG_FRAME_VARIABLE_MAX 128
#define XL_DEBUG_EXECUTABLE_LINE_MAX 2048

typedef struct XL_DebugBreakpoint {
	char sSource[XL_PATH_MAX_LEN];
	int iLine;
	int iColumn;
	int bEnabled;
	int bVerified;
	int iActualLine;
	char sMessage[128];
} XL_DebugBreakpoint;

typedef struct XL_DebugVariable {
	char sName[64];
	char sType[32];
	char sValue[256];
	void* pValue;
} XL_DebugVariable;

typedef struct XL_DebugFrame {
	char sSource[XL_PATH_MAX_LEN];
	char sFunc[128];
	int iLine;
	int iColumn;
	int iFuncIndex;
	int iDepth;
	size_t iArgumentStart;
	size_t iArgumentCount;
	size_t iLocalStart;
	size_t iLocalCount;
} XL_DebugFrame;

typedef enum XL_DebugStepMode {
	XL_DEBUG_STEP_NONE = 0,
	XL_DEBUG_STEP_IN = 1,
	XL_DEBUG_STEP_NEXT = 2,
	XL_DEBUG_STEP_OUT = 3
} XL_DebugStepMode;

typedef enum XL_DebugState {
	XL_DEBUG_STATE_STARTING = 0,
	XL_DEBUG_STATE_RUNNING = 1,
	XL_DEBUG_STATE_STOPPED = 2,
	XL_DEBUG_STATE_TERMINATED = 3
} XL_DebugState;

typedef struct XL_DebugController {
	XL_DebugBreakpoint arrBreakpoints[XL_DEBUG_BREAKPOINT_MAX];
	size_t iBreakpointCount;
	char sExecutableSource[XL_PATH_MAX_LEN];
	int arrExecutableLines[XL_DEBUG_EXECUTABLE_LINE_MAX];
	int arrExecutableColumns[XL_DEBUG_EXECUTABLE_LINE_MAX];
	char* arrExecutableKinds[XL_DEBUG_EXECUTABLE_LINE_MAX];
	char* arrExecutableFunctions[XL_DEBUG_EXECUTABLE_LINE_MAX];
	size_t iExecutableLineCount;
	XL_DebugFrame arrFrames[XL_DEBUG_FRAME_MAX];
	size_t iFrameCount;
	XL_DebugFrame arrStopFrames[XL_DEBUG_FRAME_MAX];
	size_t iStopFrameCount;
	XL_DebugVariable arrFrameArguments[XL_DEBUG_FRAME_VARIABLE_MAX];
	size_t iFrameArgumentCount;
	XL_DebugVariable arrFrameLocals[XL_DEBUG_FRAME_VARIABLE_MAX];
	size_t iFrameLocalCount;
	XL_DebugVariable arrStopFrameArguments[XL_DEBUG_FRAME_VARIABLE_MAX];
	size_t iStopFrameArgumentCount;
	XL_DebugVariable arrStopFrameLocals[XL_DEBUG_FRAME_VARIABLE_MAX];
	size_t iStopFrameLocalCount;
	XL_DebugVariable arrArguments[XL_DEBUG_VARIABLE_MAX];
	size_t iArgumentCount;
	XL_DebugVariable arrLocals[XL_DEBUG_VARIABLE_MAX];
	size_t iLocalCount;
	XL_DebugVariable arrStopArguments[XL_DEBUG_VARIABLE_MAX];
	size_t iStopArgumentCount;
	XL_DebugVariable arrStopLocals[XL_DEBUG_VARIABLE_MAX];
	size_t iStopLocalCount;
	char sCurrentSource[XL_PATH_MAX_LEN];
	char sCurrentFunc[128];
	int iCurrentLine;
	int iCurrentColumn;
	int iFrameDepth;
	size_t iProbeCount;
	int bStopped;
	size_t iStopSequence;
	char sStopReason[32];
	char sStopSource[XL_PATH_MAX_LEN];
	char sStopFunc[128];
	int iStopLine;
	int iStopColumn;
	int iStopFrameDepth;
	size_t iStopProbeCount;
	XL_DebugState eState;
	XL_DebugStepMode eStepMode;
	char sStepSource[XL_PATH_MAX_LEN];
	int iStepLine;
	int iStepColumn;
	int iStepFrameDepth;
	size_t iStepProbeCount;
	int bStopOnEntry;
	int bPauseRequested;
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
	const int* arrUnsupported,
	size_t iCount
);
size_t xlDebugControllerBreakpointCount(const XL_DebugController* pController);
const XL_DebugBreakpoint* xlDebugControllerBreakpointAt(const XL_DebugController* pController, size_t iIndex);
int xlDebugSourceMatches(const char* sLeft, const char* sRight);
void xlDebugControllerClearExecutableLines(XL_DebugController* pController);
int xlDebugControllerAddExecutableLine(
	XL_DebugController* pController,
	const char* sSource,
	int iLine,
	int iColumn,
	const char* sKind,
	const char* sFunction
);
void xlDebugControllerBindBreakpoints(XL_DebugController* pController);
char* xlDebugControllerDumpExecutableLinesJson(const XL_DebugController* pController);
size_t xlDebugControllerProbeCount(const XL_DebugController* pController);
void xlDebugControllerClearBreakpoints(XL_DebugController* pController);
void xlDebugControllerClearStop(XL_DebugController* pController);
int xlDebugControllerHasStop(const XL_DebugController* pController);
XL_DebugState xlDebugControllerState(const XL_DebugController* pController);
void xlDebugControllerSetState(XL_DebugController* pController, XL_DebugState eState);
void xlDebugControllerStopAtException(
	XL_DebugController* pController,
	const char* sSource,
	int iLine,
	int iColumn
);
void xlDebugControllerSetBlocking(XL_DebugController* pController, int bEnabled);
void xlDebugControllerSetStopOnEntry(XL_DebugController* pController, int bEnabled);
void xlDebugControllerRequestPause(XL_DebugController* pController);
void xlDebugControllerSetStepMode(XL_DebugController* pController, XL_DebugStepMode eMode);
void xlDebugControllerResume(XL_DebugController* pController);

void xl_dbg_stmt(const char* sSource, int iLine, int iColumn);
void xl_dbg_enter_func(const char* sSource, int iLine, int iColumn, const char* sFuncName, int iFuncIndex);
void xl_dbg_leave_func(void);
void xl_dbg_arg_int(const char* sName, long long iValue);
void xl_dbg_arg_bool(const char* sName, int bValue);
void xl_dbg_arg_float(const char* sName, double fValue);
void xl_dbg_arg_string(const char* sName, const char* sValue);
void xl_dbg_arg_value(const char* sName, void* pValue);
void xl_dbg_local_int(const char* sName, long long iValue);
void xl_dbg_local_bool(const char* sName, int bValue);
void xl_dbg_local_float(const char* sName, double fValue);
void xl_dbg_local_string(const char* sName, const char* sValue);
void xl_dbg_local_value(const char* sName, void* pValue);
int xlDebugRegisterHostSymbols(TCCState* pTcc);

#endif

#endif
