#ifndef XWORK_API_H
#define XWORK_API_H

#include <xwork/features.h>
#include <xllm-session/features.h>
#include <xllm/features.h>
#include <xrt.h>

#if defined(XWORK_FEATURE_XWORK)

/* The selected product requires its complete declared dependency set. */
#if !defined(XLLM_SESSION_FEATURE_XLLM_SESSION)
#error "xwork requires xllm_session (XLLM_SESSION_FEATURE_XLLM_SESSION)"
#endif
#if !defined(XRT_FEATURE_JSON_WRITE)
#error "xwork requires json_write (XRT_FEATURE_JSON_WRITE)"
#endif
#if !defined(XRT_FEATURE_REGEX)
#error "xwork requires regex (XRT_FEATURE_REGEX)"
#endif
#if !defined(XRT_FEATURE_FILE_TEMP)
#error "xwork requires file_temp (XRT_FEATURE_FILE_TEMP)"
#endif
#if !defined(XRT_FEATURE_FILE_TREE)
#error "xwork requires file_tree (XRT_FEATURE_FILE_TREE)"
#endif
#if !defined(XRT_FEATURE_PROCESS_RUN)
#error "xwork requires process_run (XRT_FEATURE_PROCESS_RUN)"
#endif


/*
 * xwork v2: the agent/tool-loop boundary above xllm and xllm-session.
 *
 * xwork owns orchestration, workspace policy, tool execution, artifacts and
 * compaction scheduling. It does not own provider protocols or CLI rendering.
 */

#include <xllm.h>
#include <xllm/executor.h>
#include <xllm-session.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XWORK_VERSION_MAJOR 2
#define XWORK_VERSION_MINOR 3
#define XWORK_VERSION_PATCH 1

typedef struct xwork_agent xwork_agent;
typedef struct xwork_mcp_client xwork_mcp_client;

/* Top-level result of an agent or tool call (ok / error / cancelled / limit). */
typedef enum xwork_result {
    XWORK_RESULT_OK = 0,
    XWORK_RESULT_ERROR = -1,
    XWORK_RESULT_CANCELLED = -2,
    XWORK_RESULT_LIMIT = -3,
    XWORK_RESULT_TIMEOUT = -4
} xwork_result;

/* Stable error categories for agent and tool failures. */
typedef enum xwork_error_code {
    XWORK_ERROR_NONE = 0,
    XWORK_ERROR_INVALID_ARGUMENT,
    XWORK_ERROR_OUT_OF_MEMORY,
    XWORK_ERROR_MODEL,
    XWORK_ERROR_TOOL,
    XWORK_ERROR_POLICY,
    XWORK_ERROR_IO,
    XWORK_ERROR_CONTEXT,
    XWORK_ERROR_LOOP_GUARD,
    XWORK_ERROR_CANCELLED,
    XWORK_ERROR_TIMEOUT
} xwork_error_code;

/* Error detail: code, message buffer, and the underlying model error when present. */
typedef struct xwork_error {
    xwork_error_code eCode;
    char sMessage[1024];
    xllm_error tModelError;
} xwork_error;

/* What a tool may do (read-only / workspace write / process spawn). */
typedef enum xwork_tool_effect {
    XWORK_TOOL_EFFECT_READ_ONLY = 0,
    XWORK_TOOL_EFFECT_WORKSPACE_WRITE,
    XWORK_TOOL_EFFECT_PROCESS
} xwork_tool_effect;

/* How tool permission is granted (auto / callback / read-only). */
typedef enum xwork_approval_mode {
    /* Execute tools automatically inside the configured workspace sandbox. */
    XWORK_APPROVAL_AUTO = 0,
    /* Ask the host callback before workspace writes and process execution. */
    XWORK_APPROVAL_CALLBACK,
    /* Allow reads but reject all mutating tools. */
    XWORK_APPROVAL_READ_ONLY
} xwork_approval_mode;

/* Line-ending discipline for text tools. The model always works in LF
 * space; storage converts per policy. AUTO keeps each file's dominant
 * ending (new files LF), which heals mixed endings on first write-back. */
typedef enum xwork_eol_policy {
    XWORK_EOL_AUTO = 0,
    XWORK_EOL_FORCE_LF,
    XWORK_EOL_FORCE_CRLF,
    XWORK_EOL_PRESERVE   /* legacy strict mode: raw bytes, exact matching */
} xwork_eol_policy;

/* Host decision for a permission request (default / allow / deny). */
typedef enum xwork_permission_decision {
    XWORK_PERMISSION_DEFAULT = 0,
    XWORK_PERMISSION_ALLOW,
    XWORK_PERMISSION_DENY
} xwork_permission_decision;

/* The kind of resource a permission request refers to. */
typedef enum xwork_resource_kind {
    XWORK_RESOURCE_NONE = 0,
    XWORK_RESOURCE_PATH,
    XWORK_RESOURCE_COMMAND,
    XWORK_RESOURCE_PROCESS
} xwork_resource_kind;

/* Coarse risk grading attached to tools and requests. */
typedef enum xwork_risk_level {
    XWORK_RISK_LOW = 0,
    XWORK_RISK_MEDIUM,
    XWORK_RISK_HIGH
} xwork_risk_level;

/* A permission request: tool, effect, risk, resource, and arguments. */
typedef struct xwork_permission_request {
    const char* sToolName;
    xwork_tool_effect eEffect;
    xwork_risk_level eRisk;
    xwork_resource_kind eResourceKind;
    const char* sResource;
    const char* sArgumentsJson;
    const char* sWorkspaceRoot;
    uint64_t uAgentTurn;
} xwork_permission_request;

/* Hook timing (before or after a tool call). */
typedef enum xwork_hook_phase {
    XWORK_HOOK_BEFORE_TOOL = 0,
    XWORK_HOOK_AFTER_TOOL
} xwork_hook_phase;

/* Hook verdict (continue / deny / cancel). */
typedef enum xwork_hook_action {
    XWORK_HOOK_CONTINUE = 0,
    XWORK_HOOK_DENY,
    XWORK_HOOK_CANCEL
} xwork_hook_action;

/* Hook input: phase, turn, tool name, effect, and arguments. */
typedef struct xwork_hook_event {
    xwork_hook_phase ePhase;
    uint64_t uAgentTurn;
    const char* sToolName;
    xwork_tool_effect eEffect;
    const char* sArgumentsJson;
    const char* sOutput;
    bool bSuccess;
} xwork_hook_event;

/* Per-call context handed to tool implementations. */
typedef struct xwork_tool_context {
    xwork_agent* pAgent;
    const char* sWorkspaceRoot;
    const char* sToolCallId;
    uint64_t uAgentTurn;
} xwork_tool_context;

/* Tool result: text plus an optional owned image payload. */
typedef struct xwork_tool_output {
    char* sContent;
    bool bSuccess;
    /* Image passthrough (read): owned bytes + mime; a text summary rides
     * sContent. Images bypass the text truncation/spill path. */
    unsigned char* pImageBytes;   /* owned */
    size_t iImageSize;
    char sImageMime[32];          /* "image/png" etc.; empty when no image */
} xwork_tool_output;

typedef xwork_result (*xwork_tool_execute_fn)(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
);

/* A host tool registration (name, description, JSON schema, strictness). */
typedef struct xwork_tool_definition {
    const char* sName;
    const char* sDescription;
    const char* sParametersJson;
    bool bStrict;
    xwork_tool_effect eEffect;
    xwork_tool_execute_fn OnExecute;
    void* pUserData;
    /* Stable owner namespace used for discovery and bulk replacement. When
     * omitted the registry records "application". The agent copies it. */
    const char* sSource;
} xwork_tool_definition;

/* Read view of a registry entry, adding the source tag. */
typedef struct xwork_tool_info {
    const char* sName;
    const char* sDescription;
    const char* sParametersJson;
    const char* sSource;
    bool bStrict;
    xwork_tool_effect eEffect;
} xwork_tool_info;

/* Agent and run event kinds (turns, model deltas, tool calls, delegation). */
typedef enum xwork_event_kind {
    XWORK_EVENT_AGENT_START = 0,
    XWORK_EVENT_MODEL_START,
    XWORK_EVENT_MODEL_TEXT_DELTA,
    XWORK_EVENT_MODEL_REASONING_DELTA,
    XWORK_EVENT_MODEL_DONE,
    XWORK_EVENT_TOOL_START,
    XWORK_EVENT_TOOL_DONE,
    XWORK_EVENT_COMPACTION_START,
    XWORK_EVENT_COMPACTION_REJECTED,
    XWORK_EVENT_COMPACTION_DONE,
    XWORK_EVENT_AGENT_DONE,
    XWORK_EVENT_ERROR
} xwork_event_kind;

/* One agent event; the union payload is selected by kind. */
typedef struct xwork_event {
    xwork_event_kind eKind;
    uint64_t uAgentTurn;
    uint32_t uAgentDepth;
    uint64_t uDelegationId;
    uint64_t uParentAgentTurn;
    const char* sText;
    size_t iTextLength;
    const char* sToolName;
    const char* sToolCallId;
    const char* sArtifactPath;
    const char* sModel;
    const char* sProviderRequestId;
    const char* sProviderCode;
    const char* sProviderMessage;
    const char* sFinishReason;
    const char* sRequestFingerprint;
    size_t iMessageCount;
    size_t iToolDefinitionCount;
    size_t iResponseToolCallCount;
    uint32_t uMaxOutputTokens;
    uint32_t uHttpStatus;
    xllm_error_code eModelErrorCode;
    bool bSuccess;
    uint32_t uCompactionAttempt;
    xllm_compaction_quality tCompactionQuality;
    xllm_usage tUsage;
    xllm_diagnostics tDiagnostics;
    xllm_session_stats tSessionStats;
} xwork_event;

/* Return false to request cooperative cancellation. Callbacks may be invoked
 * from a background delegation thread (agent tool, background=true): hosts
 * must treat these callbacks as thread-safe and must not mutate the agent
 * or its session inside them. The child's cancellation does not propagate
 * to the parent agent. */
typedef bool (*xwork_event_fn)(void* pUserData, const xwork_event* pEvent);

/* Return true to approve the requested side effect. */
typedef bool (*xwork_approval_fn)(
    void* pUserData,
    const char* sToolName,
    xwork_tool_effect eEffect,
    const char* sArgumentsJson
);

/* Structured per-call policy. DEFAULT falls back to eApprovalMode/OnApproval. */
typedef xwork_permission_decision (*xwork_permission_fn)(
    void* pUserData,
    const xwork_permission_request* pRequest
);

/* DENY is a tool-level rejection before execution. After execution it marks
 * the tool result failed because an already-completed side effect cannot be undone. */
typedef xwork_hook_action (*xwork_hook_fn)(void* pUserData, const xwork_hook_event* pEvent);

/* Injectable model boundary used by tests and offline hosts. */
typedef xllm_result (*xwork_model_complete_fn)(
    void* pUserData,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
);

/* Agent wiring: client, session, workspace, system prompt, approvals, hooks. */
typedef struct xwork_agent_config {
    /* Borrowed dependencies; they must outlive the agent. */
    xllm_client* pClient;
    xllm_session* pSession;

    const char* sWorkspaceRoot;
    const char* sSystemPrompt;
    const char* sSessionPath;
    const char* sArtifactDirectory;
    const char* sModel;
    const char* sReasoningEffort;
    xcancel* pCancel;
    int64_t iTimeout;

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
    uint32_t uMaxAgentTurns;          /* 0 means unlimited. */
    uint32_t uRepeatedToolBatchLimit;
    uint32_t uConsecutiveFailureLimit;
    uint32_t uMaxManagedProcesses;
    uint32_t uCompletionVerificationRetries; /* Premature final answers after a write; default 2. */
    uint32_t uCompactionQualityRetries;       /* Retries after a structurally rejected summary. */
    size_t iMaxInlineToolBytes;
    size_t iMaxCapturedCommandBytes;
    bool bRegisterBuiltinTools;
    bool bAutoSaveSession;
    bool bAllowArtifactWrites;
    bool bRequireVerificationAfterWrite;      /* Require successful exec_command after latest write. */
    /* Opt-in identity injection: when true and the session is empty, creation
     * pins sSystemPrompt as the system message. The host owns identity by
     * default (see xllmSessionSetSystemPrompt); only loop-style hosts that
     * want xwork's default persona enable this. */
    bool bInjectSystemPrompt;
} xwork_agent_config;

/* Final run outcome: text plus turn, model, and tool counters. */
typedef struct xwork_run_result {
    char* sFinalText;
    uint64_t uAgentTurns;
    uint64_t uModelCalls;
    uint64_t uToolCalls;
    uint64_t uCompactions;
    uint64_t uRejectedCompactionSummaries;
    uint32_t uAgentDepth;
    uint64_t uDelegationId;
    xllm_usage tLastUsage;
    xllm_session_stats tFinalSessionStats;
} xwork_run_result;

/* Read-only sub-agent wiring (prompt, permission callback, event sink). */
typedef struct xwork_readonly_subagent_config {
    const char* sSystemPrompt;
    xwork_permission_fn OnPermission;
    void* pPermissionUserData;
    xwork_event_fn OnEvent;
    void* pEventUserData;
    uint64_t uParentAgentTurn;
    uint32_t uTimeoutMs;
    uint32_t uMaxAgentTurns;
    uint32_t uMaxOutputTokens;
    size_t iMaxFinalBytes;
} xwork_readonly_subagent_config;

/* Zero an error struct; reusable across failure paths. */
XRT_API void xworkErrorInit(xwork_error* pError);
/* Stable name for an error code (never NULL). */
XRT_API const char* xworkErrorCodeName(xwork_error_code eCode);
/* Zero a tool output. */
XRT_API void xworkToolOutputInit(xwork_tool_output* pOutput);
/* Free the contents of a tool output. */
XRT_API void xworkToolOutputUnit(xwork_tool_output* pOutput);
/* Set the success flag and text content (copied). */
XRT_API bool xworkToolOutputSet(xwork_tool_output* pOutput, bool bSuccess, const char* sContent);
/* Attach an image payload (copies); the text summary should already be in
 * sContent via xworkToolOutputSet. */
XRT_API bool xworkToolOutputSetImage(xwork_tool_output* pOutput,
    const unsigned char* pData, size_t iSize, const char* sMime);

/* Zero an agent config (default tools, approvals, loop bounds). */
XRT_API void xworkAgentConfigInit(xwork_agent_config* pConfig);
/* Zero a read-only sub-agent config. */
XRT_API void xworkReadOnlySubagentConfigInit(xwork_readonly_subagent_config* pConfig);
/* Create an agent; NULL with pError on failure. */
XRT_API xwork_agent* xworkAgentCreate(const xwork_agent_config* pConfig, xwork_error* pError);
/* Destroy the agent after in-flight work has finished. */
XRT_API void xworkAgentDestroy(xwork_agent* pAgent);
/* Register a custom tool (replaces any tool with the same name). */
XRT_API bool xworkAgentRegisterTool(xwork_agent* pAgent, const xwork_tool_definition* pDefinition, xwork_error* pError);
/* Unregister a tool by name (built-ins cannot be removed). */
XRT_API bool xworkAgentUnregisterTool(xwork_agent* pAgent, const char* sName, xwork_error* pError);
/* Unregister tools by source in bulk (e.g. one MCP server). */
XRT_API bool xworkAgentUnregisterToolsBySource(
    xwork_agent* pAgent,
    const char* sSource,
    size_t* piRemoved,
    xwork_error* pError
);
/* Number of tools currently in the registry. */
XRT_API size_t xworkAgentToolCount(const xwork_agent* pAgent);
/* Fetch tool info by index (name, description, source). */
XRT_API bool xworkAgentToolAt(const xwork_agent* pAgent, size_t iIndex, xwork_tool_info* pInfo);
/* Registry generation; bumped on every change (cache invalidation). */
XRT_API uint64_t xworkAgentToolRegistryGeneration(const xwork_agent* pAgent);

/* MCP stdio client. The client owns the subprocess and discovered proxy
 * definitions. It must outlive every agent whose registry refers to those
 * proxies. Transport messages use newline-delimited UTF-8 JSON-RPC. */
typedef struct xwork_mcp_stdio_config {
    const char* sServerName;
    const char* sProgram;
    const char* const* psArguments;
    size_t iArgumentCount;
    const char* sWorkingDirectory;
    const char* sProtocolVersion;
    uint32_t uRequestTimeoutMs;
    size_t iMaxMessageBytes;
    size_t iMaxTools;
    xwork_tool_effect eDefaultToolEffect;
    bool bTrustReadOnlyAnnotations;
    xcancel* pCancel;
    int64_t iTimeout;
} xwork_mcp_stdio_config;

/* MCP connection snapshot (server, protocol, tool count, liveness). */
typedef struct xwork_mcp_info {
    const char* sServerName;
    const char* sProtocolVersion;
    const char* sToolSource;
    size_t iToolCount;
    uint64_t uRequestsCompleted;
    bool bConnected;
    bool bServerSupportsToolListChanges;
} xwork_mcp_info;

/* Zero a stdio MCP config (command line, env, trust flags). */
XRT_API void xworkMcpStdioConfigInit(xwork_mcp_stdio_config* pConfig);
/* Create a stdio MCP client without connecting yet. */
XRT_API xwork_mcp_client* xworkMcpClientCreate(const xwork_mcp_stdio_config* pConfig, xwork_error* pError);
/* Spawn the child process and finish the MCP initialize handshake. */
XRT_API bool xworkMcpClientConnect(xwork_mcp_client* pClient, xwork_error* pError);
/* Fetch the remote tool list into the agent (replacing that source). */
XRT_API bool xworkMcpClientRefreshTools(xwork_mcp_client* pClient, xwork_agent* pAgent, xwork_error* pError);
/* Call a remote tool directly, bypassing the agent loop; cancellable with timeout. */
XRT_API xwork_result xworkMcpClientCallTool(
    xwork_mcp_client* pClient,
    const char* sRemoteToolName,
    const char* sArgumentsJson,
    xcancel* pCancel,
    int64_t iTimeout,
    xwork_tool_output* pOutput,
    xwork_error* pError
);
/* Copy connection info (server name, protocol version, tool count, state). */
XRT_API bool xworkMcpClientGetInfo(const xwork_mcp_client* pClient, xwork_mcp_info* pInfo);
/* Disconnect and destroy the client after in-flight calls finish. */
XRT_API void xworkMcpClientDestroy(xwork_mcp_client* pClient);
/* Cooperatively cancel the current run (visible to tools and the loop). */
XRT_API bool xworkAgentCancel(xwork_agent* pAgent);
/* The agent's workspace root (base for tool-relative paths). */
XRT_API const char* xworkAgentWorkspaceRoot(const xwork_agent* pAgent);

/* True when any path component is an internal control directory (.git,
 * .xcode), compared case-insensitively across both separators. Hosts use it
 * inside permission callbacks to keep internal state off-limits; the
 * readonly subagent enforces it by default. */
XRT_API bool xworkPathIsProtected(const char* sPath);

/* Run one agent turn synchronously (prompt in, result out; cancellable). */
XRT_API xwork_result xworkAgentRun(xwork_agent* pAgent, const char* sPrompt, xwork_run_result* pResult, xwork_error* pError);
/* Declare an externally driven run window: while open, registry mutation and
 * every other run entry (built-in loop, compact, another window) are rejected
 * — the same rule the built-in loop enforces on itself. Pair with RunEnd;
 * hosts driving xllmSessionRunWithTools over an xwork executor wrap the call
 * in this pair so the tool registry stays stable for the whole run. */
XRT_API bool xworkAgentRunBegin(xwork_agent* pAgent, xwork_error* pError);
/* End the run state and release run-scoped resources; call after Run. */
XRT_API void xworkAgentRunEnd(xwork_agent* pAgent);

/* ------------------------------------------------------------------ */
/* Unified task table: notices and the model-clock watchdog.           */
/*                                                                     */
/* Task completion is pushed at turn boundaries: TakeTaskNotices       */
/* returns each finished, not-yet-consumed task (with the model's      */
/* notify message, if any) and marks it consumed; the host injects     */
/* the notices into the session as synthetic entries. The watchdog     */
/* digest reports model-scheduled reminders (spawn remind_after_ms)    */
/* and a one-shot uncollected-notice nudge — the harness executes the  */
/* clocks the model set; it never invents its own schedule.            */
/* ------------------------------------------------------------------ */

/* Task kinds the unified task system tracks (process; agent reserved). */
typedef enum xwork_task_kind {
    XWORK_TASK_PROCESS = 0,
    XWORK_TASK_AGENT      /* reserved: subagent delegation batch */
} xwork_task_kind;

/* Completion notice from a background task (id, exit code, clean flag). */
typedef struct xwork_task_notice {
    uint64_t uTaskId;
    xwork_task_kind eKind;
    int32_t iExitCode;
    bool bExitedCleanly;
    const char* sNotify;    /* borrowed from the task entry */
    const char* sPreview;   /* borrowed command preview */
} xwork_task_notice;

/* Watchdog state: wake hint, next deadline, running and stalled counts. */
typedef struct xwork_watchdog_digest {
    bool bShouldWake;
    uint64_t uNextWakeMs;         /* 0 = no timer needed */
    size_t iRunningTasks;
    size_t iStalledTasks;         /* past their remind_after_ms */
    size_t iUncollectedNotices;
} xwork_watchdog_digest;

/* Call from the thread that owns the agent (the same thread that drives
 * runs); the notice payloads borrow entry storage and are not safe to read
 * across a concurrent registry or task mutation. */
XRT_API size_t xworkAgentTakeTaskNotices(xwork_agent* pAgent,
    xwork_task_notice* pNotices, size_t iCapacity);
/* Copy the task-system watchdog digest (model clock, hung tasks). */
XRT_API bool xworkTaskWatchdog(xwork_agent* pAgent, xwork_watchdog_digest* pDigest);

/* ------------------------------------------------------------------ */
/* Subagent delegation: the conditional `agent` tool.                   */
/*                                                                     */
/* Hosts register specialist archetypes; the model sees ONE `agent`    */
/* tool whose description carries the roster (one affordance line per  */
/* type). Execution composes the three-piece public APIs — a fresh      */
/* session (cloned config, no parent history), a child agent with the   */
/* archetype's tool whitelist, and xllmSessionRunWithTools under the   */
/* archetype's budgets — so delegation breaks no layer boundary.       */
/* Depth is locked at one: subagents cannot delegate further.          */
/* Permissions inherit the parent chain; archetypes may only tighten.  */
/* ------------------------------------------------------------------ */

/* A registered sub-agent type (name, prompt, allowed tools). */
typedef struct xwork_subagent_type {
    const char* sName;            /* roster key, e.g. "probe" */
    const char* sDescription;     /* one line: when to choose me */
    const char* sSystemPrompt;    /* identity injected into the child */
    const char* const* psTools;   /* tool-name whitelist; NULL = all parent tools */
    size_t iToolCount;
    const char* sModel;           /* optional lighter model override */
    uint32_t uMaxTurns;           /* 0 = 8 */
    uint32_t uTimeoutMs;          /* 0 = 120000 */
    uint32_t uMaxOutputTokens;    /* 0 = keep parent config */
    size_t iMaxFinalBytes;        /* 0 = 64 KiB */
    bool bReadOnly;               /* clamp approval to READ_ONLY (tighten only) */
} xwork_subagent_type;

/* Register an archetype (deep copy). The `agent` tool appears with the
 * first registration and its roster description is rebuilt on every
 * change. Returns false while a run is active (registry stability). */
XRT_API bool xworkAgentRegisterSubagentType(xwork_agent* pAgent,
    const xwork_subagent_type* pType, xwork_error* pError);
/* Remove one archetype by name (tool persists while others remain). */
XRT_API bool xworkAgentUnregisterSubagentType(xwork_agent* pAgent, const char* sName,
    xwork_error* pError);
/* Number of registered sub-agent types. */
XRT_API size_t xworkAgentSubagentTypeCount(const xwork_agent* pAgent);
/* Run a read-only sub-agent on a subtask (depth lock prevents recursion). */
XRT_API xwork_result xworkAgentRunReadOnlySubagent(
    xwork_agent* pParent,
    const xwork_readonly_subagent_config* pConfig,
    const char* sTask,
    xwork_run_result* pResult,
    xwork_error* pError
);
/* Resume an interrupted run without appending another user prompt. Pending
 * tool calls are completed first; an interrupted model call is retried from
 * the durable session tail. */
XRT_API xwork_result xworkAgentResume(xwork_agent* pAgent, xwork_run_result* pResult, xwork_error* pError);
/* Force one safe-prefix summary compaction and persist the committed session. */
XRT_API xwork_result xworkAgentCompact(xwork_agent* pAgent, xwork_error* pError);
/* Free the contents of a run result. */
XRT_API void xworkRunResultUnit(xwork_run_result* pResult);

/* Registers filesystem, transactional edit, synchronous command, and managed process tools. */
XRT_API bool xworkAgentRegisterBuiltinTools(xwork_agent* pAgent, xwork_error* pError);
/* Registers only filesystem inspection tools: read_file, list_files, and search_text. */
XRT_API bool xworkAgentRegisterBuiltinReadOnlyTools(xwork_agent* pAgent, xwork_error* pError);

/* ------------------------------------------------------------------ */
/* Executor adapter: expose an agent's tool machinery as xllm's hands.  */
/*                                                                     */
/* The binding serves the xllm_executor contract from the agent's       */
/* registry (list), and its full execution path (execute): permission   */
/* gate, hooks, executor, truncation and artifact spill — the same      */
/* path the built-in loop uses. Hosts driving their own loop (or        */
/* xllmSessionRunWithTools) consume this binding; the built-in          */
/* xworkAgentRun stays available as a convenience.                      */
/* ------------------------------------------------------------------ */

typedef struct xwork_executor_state xwork_executor_state;

/* Bind an agent as an xllm executor (the mdo OnWire seam). */
XRT_API bool xworkExecutorBind(xllm_executor* pOut, xwork_agent* pAgent, xwork_error* pError);
/* Unbind the executor without destroying the agent. */
XRT_API void xworkExecutorUnbind(xllm_executor* pExecutor);

#ifdef __cplusplus
}
#endif

#endif /* selected xwork */

#endif
