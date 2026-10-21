#include <xrt/detail/wait.h>
#ifndef XWORK_INTERNAL_H
#define XWORK_INTERNAL_H

#include <xwork.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#else
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>
#endif

typedef struct xwork_buf {
    char* pData;
    size_t iLen;
    size_t iCap;
} xwork_buf;

typedef struct xwork_tool_entry {
    char* sName;
    char* sDescription;
    char* sParametersJson;
    char* sSource;
    bool bStrict;
    xwork_tool_effect eEffect;
    xwork_tool_execute_fn OnExecute;
    void* pUserData;
} xwork_tool_entry;

/* Unified task kinds live in xwork.h (xwork_task_kind): one table, one id
 * space, one tool family shared by processes and subagents. */

typedef struct xwork_process_entry {
    uint64_t uId;
    xwork_task_kind eKind;
    xprocess* pProcess;
    struct xwork_process_capture* pCapture;
    char* sCommand;            /* argv preview for status display */
    uint64_t uStdoutOffset;
    uint64_t uStderrOffset;
    bool bStdinClosed;
    /* Model-driven timing and notifications. */
    char* sNotify;             /* message delivered with the completion notice */
    uint64_t uRemindAfterMs;   /* model-set soft deadline; 0 = none */
    double uStartedUs;       /* xrtTimer() at start */
    double uExitedUs;        /* first observed exit; 0 while running */
    bool bNoticeTaken;         /* completion notice consumed by the host */
    bool bNudged;              /* uncollected-notice nudge already sent */
    /* Agent-task fields (eKind == XWORK_TASK_AGENT). The delegate thread
     * owns its child objects; the entry owns the cancel token and result. */
    xcancel* pChildCancel;
    xthread* pThread;
    xmutex* pStateLock;
    char* sResult;             /* final report text (locked by pStateLock) */
    bool bDone;                /* thread finished (locked by pStateLock) */
    bool bSuccess;             /* run result (locked by pStateLock) */
    bool bStopRequested;       /* cooperative stop asked */
} xwork_process_entry;

typedef struct xwork_process_capture_stream {
    struct xwork_process_capture* pOwner;
    xprocessstream eStream;
    xthread* pThread;
    xwork_buf tData;
    uint64_t uBaseOffset;
    size_t iLimit;
    bool bDone;
} xwork_process_capture_stream;

typedef struct xwork_process_capture {
    xprocess* pProcess;
    xmutex* pLock;
    xwork_process_capture_stream tStdout;
    xwork_process_capture_stream tStderr;
} xwork_process_capture;

typedef enum xwork_operation_status {
    XWORK_OPERATION_ACTIVE = 0,
    XWORK_OPERATION_CANCELLED,
    XWORK_OPERATION_TIMED_OUT
} xwork_operation_status;

struct xwork_agent {
    xllm_client* pClient;
    xllm_session* pSession;
    char* sWorkspaceRoot;
    char* sSystemPrompt;
    char* sSessionPath;
    char* sArtifactDirectory;
    char* sModel;
    char* sReasoningEffort;
    xcancel* pCancel;
    double uDeadline;

    xwork_approval_mode eApprovalMode;
    xwork_approval_fn OnApproval;
    void* pApprovalUserData;
    xwork_permission_fn OnPermission;
    void* pPermissionUserData;
    xwork_hook_fn OnHook;
    void* pHookUserData;
    xwork_eol_policy eEolPolicy;
    xwork_event_fn OnEvent;
    void* pEventUserData;
    xwork_model_complete_fn OnModelComplete;
    void* pModelUserData;

    uint32_t uCommandTimeoutMs;
    uint32_t uMaxAgentTurns;
    uint32_t uRepeatedToolBatchLimit;
    uint32_t uConsecutiveFailureLimit;
    uint32_t uMaxManagedProcesses;
    uint32_t uCompletionVerificationRetries;
    uint32_t uCompactionQualityRetries;
    size_t iMaxInlineToolBytes;
    size_t iMaxCapturedCommandBytes;
    bool bAutoSaveSession;
    bool bAllowArtifactWrites;
    bool bRequireVerificationAfterWrite;
    bool bRegisterExploreTools;
    bool bExploreExternal;
    char sLsProgram[64];
    char sGlobProgram[64];
    char sGrepProgram[64];
    bool bRegisterPythonTool;
    char sPythonPath[280];
    volatile long iCancelled;
    bool bRunning;

    xwork_tool_entry* pTools;
    size_t iToolCount;
    size_t iToolCap;
    uint64_t uToolRegistryGeneration;
    xwork_process_entry* pProcesses;
    size_t iProcessCount;
    size_t iProcessCap;
    uint64_t uNextProcessId;
    xwork_subagent_type* pSubagentTypes;   /* owned deep copies */
    size_t iSubagentTypeCount;
    size_t iSubagentTypeCap;
    uint64_t uArtifactSequence;
    uint64_t uRunSequence;
    uint32_t uAgentDepth;
    uint64_t uDelegationId;
    uint64_t uParentAgentTurn;
    uint64_t uSubagentSequence;
    /* python persistent REPL (python tool; state lives for the agent's life) */
    xprocess* pPyProc;
    xmutex* pPyLock;
    xcond* pPyCond;
    xthread* pPyReader;
    char* pPyBuf;              /* accumulated stdout (tail-capped) */
    size_t iPyLen;
    size_t iPyCap;
    bool bPyEof;               /* reader saw EOF: interpreter exited */
    uint32_t uPySeq;           /* sentinel sequence counter */
};

char* xwork__strdup(const char* sText);
char* xwork__strndup(const char* sText, size_t iLen);
bool xwork__replace(char** ppDst, const char* sText);
void xwork__set_error(xwork_error* pError, xwork_error_code eCode, const char* sMessage);
xwork_result xwork__tool_fail(xwork_tool_output* pOutput, const char* sMessage);
bool xwork__task_running(xwork_process_entry* pEntry);   /* unified: process or agent */
xwork_process_entry* xwork__task_add(xwork_agent* pAgent, xwork_task_kind eKind);
xwork_process_entry* xwork__process_add(xwork_agent* pAgent);
void xwork__subagent_type_unit(xwork_subagent_type* pType);
void xwork__copy_model_error(xwork_error* pError, const xllm_error* pModelError);
bool xwork__buf_reserve(xwork_buf* pBuf, size_t iNeed);
bool xwork__buf_append(xwork_buf* pBuf, const void* pData, size_t iLen);
bool xwork__buf_append_cstr(xwork_buf* pBuf, const char* sText);
bool xwork__buf_append_char(xwork_buf* pBuf, char ch);
bool xwork__buf_appendf(xwork_buf* pBuf, const char* sFormat, ...);
char* xwork__buf_detach(xwork_buf* pBuf);
void xwork__buf_unit(xwork_buf* pBuf);
bool xwork__json_string(xwork_buf* pBuf, const char* sText);

xvalue* xwork__json_parse_object(const char* sJson);
xvalue* xwork__json_get(xvalue* pObject, const char* sKey);
const char* xwork__json_text(xvalue* pObject, const char* sKey);
bool xwork__json_bool(xvalue* pObject, const char* sKey, bool bDefault, bool* pValid);
uint64_t xwork__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault, bool* pValid);

char* xwork__resolve_path(const xwork_agent* pAgent, const char* sPath, xwork_error* pError);
char* xwork__relative_path(const xwork_agent* pAgent, const char* sPath);
bool xwork__ensure_parent(const char* sPath);
bool xwork__parent_exists(const char* sPath);
bool xwork__emit(xwork_agent* pAgent, const xwork_event* pEvent);
bool xwork__save(xwork_agent* pAgent, xwork_error* pError);
const xwork_tool_entry* xwork__find_tool(const xwork_agent* pAgent, const char* sName);
void xwork__processes_unit(xwork_agent* pAgent);
void xwork__python_unit(xwork_agent* pAgent);   /* python REPL teardown (python tool) */
bool xwork__register_explore_tools(xwork_agent* pAgent, xwork_error* pError);
bool xwork__register_python_tool(xwork_agent* pAgent, xwork_error* pError);
bool xwork__list_directory(const char* sDir, bool bLong, bool bAll, xwork_buf* pOut);

xwork_result xwork__execute_tool(
    xwork_agent* pAgent,
    const xllm_tool_call* pCall,
    uint64_t uTurn,
    char** ppSessionContent,
    bool* pbSuccess,
    bool* pbEffectApplied,
    unsigned char** ppImageBytes,
    size_t* piImageSize,
    char* psImageMime,
    xwork_error* pError
);

xllm_result xwork__model_complete(
    xwork_agent* pAgent,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
);

static inline uint64_t xwork__atomic_add_u64(volatile uint64_t* pValue, uint64_t uAdd)
{
#if defined(_MSC_VER)
    return (uint64_t)_InterlockedExchangeAdd64((volatile LONG64*)pValue, (LONG64)uAdd) + uAdd;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(pValue, uAdd, __ATOMIC_SEQ_CST);
#else
    *pValue += uAdd;   /* best effort on unknown compilers */
    return *pValue;
#endif
}

static inline long xwork__atomic_load(volatile long* pValue)
{
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(pValue, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
#else
    return *pValue;
#endif
}

static inline void xwork__atomic_store(volatile long* pValue, long iValue)
{
#if defined(_MSC_VER)
    (void)_InterlockedExchange(pValue, iValue);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(pValue, iValue, __ATOMIC_SEQ_CST);
#else
    *pValue = iValue;
#endif
}

static inline bool xwork__is_cancelled(xwork_agent* pAgent)
{
    return pAgent && (xwork__atomic_load(&pAgent->iCancelled) != 0 ||
        (pAgent->pCancel && xrtCancelRequested(pAgent->pCancel)) ||
        (pAgent->uDeadline != INFINITY && __xrtWaitExpired(pAgent->uDeadline)));
}

static inline xwork_operation_status xwork__operation_status(const xwork_agent* pAgent)
{
    if ( !pAgent ) return XWORK_OPERATION_ACTIVE;
    if ( pAgent->pCancel && xrtCancelRequested(pAgent->pCancel) ) {
        return XWORK_OPERATION_CANCELLED;
    }
    if ( pAgent->uDeadline != INFINITY && __xrtWaitExpired(pAgent->uDeadline) ) {
        return XWORK_OPERATION_TIMED_OUT;
    }
    return XWORK_OPERATION_ACTIVE;
}


/* Shared process helpers; each source is an independent translation unit. */
bool xwork__process_running(const xprocess* pProcess);
xwork_process_capture* xwork__process_capture_create(
    xprocess* pProcess,
    size_t iLimit,
    bool bCaptureStderr
);
void xwork__process_capture_destroy(xwork_process_capture* pCapture);
void* xwork__process_capture_since(
    xwork_process_capture* pCapture,
    bool bStderr,
    uint64_t uOffset,
    size_t iMaxBytes,
    size_t* pSize,
    uint64_t* pBaseOffset,
    uint64_t* pNextOffset
);
void xwork__process_remove(xwork_agent* pAgent, size_t iIndex);
xwork_result xwork__tool_spawn(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
);


/* Internal probes also link through the modular build. */
bool xwork__buf_append_process_text(xwork_buf* pBuf, const void* pData, size_t iSize);
xwork_process_entry* xwork__process_find(xwork_agent* pAgent, uint64_t uId, size_t* piIndex);

#endif
