#ifndef XLLM_SESSION_API_H
#define XLLM_SESSION_API_H

#include <xllm-session/features.h>
#include <xllm/features.h>
#include <xrt.h>

#if defined(XLLM_SESSION_FEATURE_XLLM_SESSION)

/* The selected product requires its complete declared dependency set. */
#if !defined(XLLM_FEATURE_XLLM)
#error "xllm-session requires xllm (XLLM_FEATURE_XLLM)"
#endif
#if !defined(XRT_FEATURE_JSONL_READ)
#error "xllm-session requires jsonl_read (XRT_FEATURE_JSONL_READ)"
#endif
#if !defined(XRT_FEATURE_FILE_WHOLE)
#error "xllm-session requires file_whole (XRT_FEATURE_FILE_WHOLE)"
#endif
#if !defined(XRT_FEATURE_DIR)
#error "xllm-session requires dir (XRT_FEATURE_DIR)"
#endif
#if !defined(XRT_FEATURE_PATH)
#error "xllm-session requires path (XRT_FEATURE_PATH)"
#endif
#if !defined(XRT_FEATURE_RANDOM_SECURE)
#error "xllm-session requires random_secure (XRT_FEATURE_RANDOM_SECURE)"
#endif


/* TCC hosts mount this header in a flat VFS (/xs/xllm-session.h); native
 * builds use the on-disk sibling tree. Both spellings resolve to the same
 * xllm.h so vendored copies stay byte-identical to this file. */
#include <xllm.h>
#include <xllm/executor.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm_session xllm_session;
typedef struct xllm_compaction xllm_compaction;

#define XLLM_SESSION_ENTRY_PINNED     0x00000001u
#define XLLM_SESSION_ENTRY_SYNTHETIC  0x00000002u

#define XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS 204800ull
#define XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS     131072u

/* Legacy (durable style) summary sections; kept for hosts that pin them. */
#define XLLM_COMPACTION_SECTION_OBJECTIVE          (1u << 0)
#define XLLM_COMPACTION_SECTION_CONSTRAINTS        (1u << 1)
#define XLLM_COMPACTION_SECTION_ARCHITECTURE       (1u << 2)
#define XLLM_COMPACTION_SECTION_COMPLETED          (1u << 3)
#define XLLM_COMPACTION_SECTION_REPOSITORY_STATE   (1u << 4)
#define XLLM_COMPACTION_SECTION_VERIFICATION       (1u << 5)
#define XLLM_COMPACTION_SECTION_OPEN_ISSUES        (1u << 6)
#define XLLM_COMPACTION_SECTION_NEXT_ACTIONS       (1u << 7)
#define XLLM_COMPACTION_SECTION_ALL                0x000000ffu

/* Default (coding style, Pi) summary sections. */
#define XLLM_COMPACTION_SECTION_GOAL                (1u << 8)
#define XLLM_COMPACTION_SECTION_PREFERENCES         (1u << 9)
#define XLLM_COMPACTION_SECTION_PROGRESS            (1u << 10)
#define XLLM_COMPACTION_SECTION_KEY_DECISIONS       (1u << 11)
#define XLLM_COMPACTION_SECTION_NEXT_STEPS          (1u << 12)
#define XLLM_COMPACTION_SECTION_CRITICAL_CONTEXT    (1u << 13)
#define XLLM_COMPACTION_SECTION_PI_ALL              0x00003f00u

/* Window and budget knobs (context window, output cap, safety reserve). */
typedef struct xllm_session_config {
    uint64_t uContextWindowTokens;
    uint32_t uMaxOutputTokens;
    /* Minimum output room protected from input growth; 0 selects a dynamic default. */
    uint32_t uOutputReserveTokens;
    uint32_t uSafetyReserveTokens;
    uint32_t uRecentTurnsToKeep;
    uint32_t uToolPruneBytes;
    uint32_t uSummaryMaxTokens;
    uint32_t uSummaryMinTokens;
    uint32_t uCompactionRequiredSections;
    double fPruneTrigger;
    double fCompactTrigger;
    /* --- v3 additions (appended; ConfigInit assigns defaults) --- */
    uint32_t uKeepRecentTokens;      /* tail-window cut budget; 0 = derived min(20000, window/4) */
    uint32_t uSummaryMaxBytes;       /* summary byte cap (quality gate); 0 = 32768, clamped to window */
    uint32_t uUserMessageCapBytes;   /* user message byte cap at Add time; 0 = unlimited */
    uint32_t uToolResultCapBytes;    /* tool result byte cap at Add time; 0 = unlimited */
    uint32_t uToolResultTotalCapBytes; /* per-turn cumulative tool result cap; 0 = unlimited */
    uint64_t uJournalMaxBytes;       /* journal replay budget; 0 = 64 MiB */
    const char* sSummaryStyle;       /* NULL/"coding" = Pi coding, "general", "durable" (v2 8-section); borrowed */
    const char* sSnapshotPath;       /* optional default snapshot path for the easy layer; borrowed */
} xllm_session_config;

/* Budget pressure state (none / prune / compact / overflow). */
typedef enum xllm_session_pressure {
    XLLM_SESSION_PRESSURE_NONE = 0,
    XLLM_SESSION_PRESSURE_PRUNE,
    XLLM_SESSION_PRESSURE_COMPACT,
    XLLM_SESSION_PRESSURE_OVERFLOW
} xllm_session_pressure;

/* Budget accounting: raw vs billed tokens and compaction counters. */
typedef struct xllm_session_stats {
    uint64_t uContextWindowTokens;
    uint64_t uInputBudgetTokens;
    uint64_t uOutputReserveTokens;
    uint64_t uRawActiveTokens;
    uint64_t uRenderedActiveTokens;
    uint64_t uPruneThresholdTokens;
    uint64_t uCompactThresholdTokens;
    uint64_t uCompactedThroughSequence;
    uint64_t uCurrentTurn;
    uint64_t uEntryCount;
    uint64_t uCompactionCount;
    uint64_t uJournalSequence;
    uint32_t uNextMaxOutputTokens;
    uint32_t uPendingToolCalls;
    bool bJournalEnabled;
    xllm_session_pressure ePressure;
    /* --- v3 additions --- */
    uint64_t uFillExact;            /* last observed prompt+output tokens; UINT64_MAX when unknown */
    uint64_t uIncrementMax;         /* worst-case next-turn growth envelope (tokens) */
    uint64_t uCachedInputTokens;    /* last observed provider cache hit (observation only) */
    uint64_t uSummaryTokensExact;   /* current summary cost: meta-call output tokens */
    uint32_t uSummaryGeneration;    /* summary generation; +1 per compaction or L2 truncation */
    uint32_t uAutoCompactStreak;    /* consecutive auto-compactions without a user entry */
    bool bFillExactValid;           /* false until the first real call reports usage (or after restore) */
} xllm_session_stats;

/* Section coverage of a candidate summary. */
typedef struct xllm_compaction_quality {
    uint32_t uRequiredSections;
    uint32_t uPresentSections;
    uint32_t uMissingSections;
    uint32_t uMinimumSummaryTokens;
    uint32_t uMaximumSummaryTokens;
    uint64_t uSourceTokens;
    uint64_t uSummaryTokens;
    bool bAccepted;
    /* --- v3 additions (the decision fields; token figures stay informational) --- */
    size_t uSummaryBytes;
    size_t uMaximumSummaryBytes;
} xllm_compaction_quality;

/* Borrowed view into an unresolved assistant tool call. The strings remain
 * valid until the session is mutated or destroyed. */
typedef struct xllm_pending_tool_call {
    uint64_t uTurn;
    const char* sId;
    const char* sName;
    const char* sArgumentsJson;
} xllm_pending_tool_call;

/* Tail snapshot: turn, role, and last-message presence. */
typedef struct xllm_session_tail {
    uint64_t uTurn;
    xllm_role eRole;
    bool bHasMessage;
} xllm_session_tail;

/* Rolling summary object (Pi CompactionEntry with exact meta-call usage). */
typedef struct xllm_session_summary {
    const char* sText;              /* borrowed from the session */
    uint64_t uThroughSequence;
    uint32_t uGeneration;
    uint64_t uPromptTokensAtBirth;
    uint64_t uOutputTokensAtBirth;
} xllm_session_summary;

/* ------------------------------------------------------------------ */
/* Asset ledger (pi: the conversation compacts, the ledger does not).   */
/*                                                                      */
/* Hosts note files as tools touch them; entries dedup by exact path.   */
/* The ledger survives compaction, rides the compaction prompt as       */
/* context, renders appended to the summary bridge, and persists in     */
/* the snapshot and journal.                                            */
/* ------------------------------------------------------------------ */

/* Borrowed file lists (read and modified) for replay and audits. */
typedef struct xllm_file_ledger {
    const char* const* psReadFiles;     /* borrowed until the next note */
    size_t iReadFileCount;
    const char* const* psModifiedFiles; /* borrowed until the next note */
    size_t iModifiedFileCount;
} xllm_file_ledger;

/* Record a file read into the session ledger (tool visibility tracking). */
XRT_API bool xllmSessionNoteFileRead(xllm_session* pSession, const char* sPath);
/* Record a file modification into the session ledger. */
XRT_API bool xllmSessionNoteFileModified(xllm_session* pSession, const char* sPath);
/* Copy out the file ledger (read/modified state per path). */
XRT_API bool xllmSessionGetFileLedger(const xllm_session* pSession, xllm_file_ledger* pLedger);

/* ------------------------------------------------------------------ */
/* Compaction strategy table (D10): per-stage NULL = built-in default. */
/* ------------------------------------------------------------------ */

/* Compaction operator verdict (yes or no). */
typedef enum xllm_compact_decision {
    XLLM_COMPACT_NO = 0,
    XLLM_COMPACT_YES
} xllm_compact_decision;

/* Compaction plan: the sequence span and split-turn prefix. */
typedef struct xllm_compaction_plan {
    uint64_t uThroughSequence;      /* complete-turn candidates = (previous through, this value] */
    uint64_t uPrefixThroughSequence; /* split-turn prefix upper bound; 0 = no split (default).
                                      * Set when the retained-window-start turn alone exceeds the
                                      * keep-recent budget: entries (uThroughSequence, this value]
                                      * are that turn's prefix, summarized separately. */
    uint32_t uReserved[3];
} xllm_compaction_plan;

struct xllm_session_stats;

/* Compaction operator vtable (decide, plan, prompt, evaluate). */
typedef struct xllm_compaction_ops {
    /* Trigger ruling; threshold auto path only (overflow/manual bypass it). */
    xllm_compact_decision (*pShouldCompact)(xllm_session*,
        const xllm_session_stats* pStats, void* pUserData);
    /* Cut point: pick the candidate range ending sequence (pair-complete). */
    bool (*pPlan)(xllm_session*, uint64_t uPrevThrough,
        xllm_compaction_plan* pPlan, void* pUserData);
    /* Serialize candidates (uFrom exclusive, uTo inclusive) into a NUL-terminated
     * malloc'd text; the session frees it with free(). */
    bool (*pSerialize)(xllm_session*, uint64_t uFrom, uint64_t uTo,
        char** psText, void* pUserData);
    /* Build the summarizer prompt from the previous summary and serialized
     * candidates; malloc'd result. */
    bool (*pBuildPrompt)(xllm_session*, const char* sPrevSummary,
        const char* sCandidates, char** psPrompt, void* pUserData);
    /* Meta call: produce a malloc'd summary from the prompt; report usage for
     * exact accounting. Default (easy layer) = bound client; NULL with no
     * bound client means the auto path is unavailable (drive it manually). */
    bool (*pSummarize)(xllm_session*, const char* sPrompt,
        char** psSummary, xllm_usage* pUsageOut, void* pUserData);
    /* Quality gate; fills *pQuality and returns acceptance in bAccepted. */
    bool (*pEvaluate)(xllm_session*, const char* sSummary,
        xllm_compaction_quality* pQuality, void* pUserData);
    /* Read-only commit notification. */
    void (*pOnCommitted)(xllm_session*, const xllm_session_summary*, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_compaction_ops;

/* Return the default compaction operator table (read-only borrow). */
XRT_API const xllm_compaction_ops* xllmSessionDefaultCompactionOps(void);
/* Install custom compaction operators (NULL restores the defaults). */
XRT_API bool xllmSessionSetCompactionOps(xllm_session*, const xllm_compaction_ops* pOps /* NULL = default */);

/* ------------------------------------------------------------------ */
/* Render and event hooks (D11). Borrowed; not persisted; fork inherits. */
/* ------------------------------------------------------------------ */

/* Host render verdict per message (keep / modified / skip). */
typedef enum xllm_render_action {
    XLLM_RENDER_KEEP = 0,
    XLLM_RENDER_MODIFIED,
    XLLM_RENDER_SKIP
} xllm_render_action;

/* Session event types (turn begin and end, entries, compaction). */
typedef enum xllm_session_event_type {
    XLLM_SESSION_EVENT_TURN_BEGIN = 1,
    XLLM_SESSION_EVENT_TURN_END,
    XLLM_SESSION_EVENT_ENTRY_ADDED,
    XLLM_SESSION_EVENT_FILL_UPDATED,
    XLLM_SESSION_EVENT_PRESSURE_CHANGED,
    XLLM_SESSION_EVENT_COMPACT_PREPARE,
    XLLM_SESSION_EVENT_COMPACT_PLAN,
    XLLM_SESSION_EVENT_COMPACT_PROMPT,
    XLLM_SESSION_EVENT_COMPACT_SUMMARY,
    XLLM_SESSION_EVENT_COMPACT_EVALUATE,
    XLLM_SESSION_EVENT_COMPACT_COMMIT,
    XLLM_SESSION_EVENT_COMPACT_ABORT,
    XLLM_SESSION_EVENT_LADDER_TRUNCATE,
    XLLM_SESSION_EVENT_JOURNAL_RECORD,
    XLLM_SESSION_EVENT_CHECKPOINT_SAVED,
    XLLM_SESSION_EVENT_SESSION_RECOVERED,
    XLLM_SESSION_EVENT_SESSION_FORKED
} xllm_session_event_type;

/* One session event with its sequence range. */
typedef struct xllm_session_event {
    xllm_session_event_type eType;
    uint64_t uTurn;
    uint64_t uSeqFrom;
    uint64_t uSeqTo;
    const xllm_session_stats* pStats;   /* stack snapshot; valid only during the callback */
    const char* sText;                  /* optional: summary preview / stage label */
} xllm_session_event;

/* Host hooks: render filter and event sink. */
typedef struct xllm_session_hooks {
    /* Per-entry transform on a cloned work message. Same entry should map to
     * the same output (violations cost cache hits, not correctness). SKIPping
     * an entry that breaks tool-call pairing fails the render. */
    xllm_render_action (*pRenderMessage)(xllm_session*, uint64_t uSequence,
        uint64_t uTurn, uint32_t uEntryFlags, xllm_message* pWork, void* pUserData);
    /* Summary message construction; pWork arrives pre-filled with the default
     * user-bridge form. Return false to omit the summary this render. */
    bool (*pRenderSummary)(xllm_session*, const xllm_session_summary*,
        xllm_message* pWork, void* pUserData);
    /* Final request post-processing (ephemeral content allowed; append at the
     * end to protect prefix caching). Return false to fail the render. */
    bool (*pRenderComplete)(xllm_session*, xllm_request* pRequest, void* pUserData);
    /* Pure observation; return value ignored. */
    void (*pOnEvent)(xllm_session*, const xllm_session_event* pEvent, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_session_hooks;

/* Install session hooks (NULL removes them). */
XRT_API bool xllmSessionSetHooks(xllm_session*, const xllm_session_hooks* pHooks /* NULL = remove */);

/* Zero a session config with usable defaults (window, budgets, compaction). */
XRT_API void xllmSessionConfigInit(xllm_session_config*);
/* Recommended safety-reserve tokens for a context window (overflow margin). */
XRT_API uint64_t xllmSessionComputeSafetyReserve(uint64_t uContextWindowTokens);
/* Recommended output-reserve tokens from window and max output. */
XRT_API uint32_t xllmSessionComputeOutputReserve(uint64_t uContextWindowTokens, uint32_t uMaxOutputTokens);
/* Rough text token estimate; no model call. */
XRT_API uint64_t xllmEstimateTextTokens(const char* sText);
/* Rough message token estimate (same measure as xllm). */
XRT_API uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage);

/* Create a session from a config; NULL with pError on failure. */
XRT_API xllm_session* xllmSessionCreate(const xllm_session_config* pConfig, xllm_error* pError);
/* Deep-copy a session, including history and budget snapshots. */
XRT_API xllm_session* xllmSessionFork(const xllm_session* pSession, xllm_error* pError);
/* Destroy the session and release everything it owns. */
XRT_API void xllmSessionDestroy(xllm_session* pSession);
/* Copy out the session's effective configuration. */
XRT_API bool xllmSessionGetConfig(const xllm_session* pSession, xllm_session_config* pConfig);

/* Begin a new turn and return its number. */
XRT_API uint64_t xllmSessionBeginTurn(xllm_session* pSession);
/* Current turn number (0 before the first turn). */
XRT_API uint64_t xllmSessionCurrentTurn(const xllm_session* pSession);
/* Append a full message to a turn; flags control billing and history. */
XRT_API bool xllmSessionAddMessage(xllm_session* pSession, uint64_t uTurn, const xllm_message* pMessage, uint32_t uFlags);
/* Convenience: append a plain text message to a turn. */
XRT_API bool xllmSessionAddText(xllm_session* pSession, uint64_t uTurn, xllm_role eRole, const char* sContent, uint32_t uFlags);

/* Idempotent pinned identity: appends a PINNED system entry when none exists
 * and no-ops when the newest pinned system text is unchanged. A changed text
 * appends a new pinned entry; rendering shows only the newest pinned system
 * message, so identity upgrades stay append-only (the journal records them)
 * without stacking blocks. The host owns identity; nothing here injects one. */
XRT_API bool xllmSessionSetSystemPrompt(xllm_session* pSession, const char* sText, xllm_error* pError);
/* Fold a model response (with tool calls) into the turn ledger. */
XRT_API bool xllmSessionAddAssistantResponse(xllm_session* pSession, uint64_t uTurn, const xllm_response* pResponse);
/* Fold a tool result into the ledger, answering a pending call. */
XRT_API bool xllmSessionAddToolResult(xllm_session* pSession, uint64_t uTurn, const char* sToolCallId, const char* sContent);
/* Tool result with an image attachment (read passthrough): the text stays
 * the tool message content, the image rides as an IMAGE part. */
XRT_API bool xllmSessionAddToolResultWithImage(xllm_session* pSession, uint64_t uTurn,
    const char* sToolCallId, const char* sContent,
    const unsigned char* pImageBytes, size_t iImageSize, const char* sImageMime);

/* Append retrieved reference material (search results, notes, fetched docs)
 * as a synthetic user entry wrapped in the untrusted-reference frame, so
 * instructions hidden inside the content cannot override host policy.
 * sSource may be NULL; when present it is recorded as a provenance line.
 * Heritage: xllm-memory RenderContext, retired with that library. */
XRT_API bool xllmSessionAddReference(xllm_session* pSession, uint64_t uTurn,
    const char* sSource, const char* sContent);

/* Exact-feedback channel: records server usage and refreshes governance.
 * xllmSessionAddAssistantResponse calls this automatically. */
XRT_API bool xllmSessionRecordUsage(xllm_session* pSession, const xllm_usage* pUsage);

/* Copy a tail snapshot (last message, turn, pending state). */
XRT_API bool xllmSessionGetTail(const xllm_session* pSession, xllm_session_tail* pTail);
/* Number of pending tool calls awaiting results. */
XRT_API size_t xllmSessionPendingToolCallCount(const xllm_session* pSession);
/* Fetch a pending call by index (id, name, arguments). */
XRT_API bool xllmSessionPendingToolCallAt(const xllm_session* pSession, size_t iIndex, xllm_pending_tool_call* pCall);

/* Copy session statistics (tokens, turns, compactions). */
XRT_API bool xllmSessionGetStats(const xllm_session* pSession, xllm_session_stats* pStats);
/* Assemble a request from the session (budgets, window, tools, system prompt). */
XRT_API bool xllmSessionBuildRequest(const xllm_session* pSession, xllm_request* pRequest, xllm_error* pError);
/* Borrowed-view variant (改造 A): plain ledger entries enter the request as
 * shallow copies pointing into the ledger — zero per-message allocations.
 * Hooks, pruned tool output, and the summary bridge still take owned clones.
 * The request must not outlive the session or span a session mutation. */
XRT_API bool xllmSessionBuildRequestView(const xllm_session* pSession, xllm_request* pRequest, xllm_error* pError);
/* Copy the session summary (turns, budget usage, compaction state). */
XRT_API bool xllmSessionGetSummary(const xllm_session* pSession, xllm_session_summary* pSummary);

/*
 * A compaction object is a transaction: prepare selects a safe prefix and
 * builds a summarizer prompt; commit advances the checkpoint only after a
 * valid summary was obtained. Destroying it without commit aborts safely.
 */
XRT_API xllm_compaction* xllmSessionPrepareCompaction(xllm_session* pSession, bool bForce, xllm_error* pError);
/* The prompt that triggered this compaction (verifiable replay). */
XRT_API const char* xllmCompactionPrompt(const xllm_compaction* pCompaction);
/* Sequence number the compaction covers through. */
XRT_API uint64_t xllmCompactionThroughSequence(const xllm_compaction* pCompaction);
/* Estimated tokens of the compacted span. */
XRT_API uint64_t xllmCompactionEstimatedTokens(const xllm_compaction* pCompaction);
/* Record the meta-call usage before committing (exact summary accounting). */
XRT_API bool xllmCompactionSetUsage(xllm_compaction* pCompaction, const xllm_usage* pUsage);
/* Grade a summary (length, coverage) for compaction operators. */
XRT_API bool xllmCompactionEvaluateSummary(const xllm_compaction* pCompaction, const char* sSummary,
    xllm_compaction_quality* pQuality, xllm_error* pError);
/* Commit a compaction: replace the span with the summary and record it. */
XRT_API bool xllmSessionCommitCompaction(xllm_session* pSession, xllm_compaction* pCompaction, const char* sSummary, xllm_error* pError);
/* Free a compaction object. */
XRT_API void xllmCompactionDestroy(xllm_compaction* pCompaction);

/* Threshold auto-compaction consult: runs the full ops pipeline (meta call via
 * the bound client or a custom pSummarize) when due. */
XRT_API bool xllmSessionMaybeCompact(xllm_session* pSession, bool* pbCompact, xllm_error* pError);

/* Overflow ladder: L1 full compaction via the current ops, then L2 structural
 * tail truncation (turn boundaries, pair-safe, floor keepRecent/2) journaling
 * a truncate event. L3 (per-message cap rejection) happens at Add time. */
XRT_API bool xllmSessionOverflowLadder(xllm_session* pSession, xllm_error* pError);

/* Serialize the session to a JSONL archive (atomic write). */
XRT_API bool xllmSessionSave(const xllm_session* pSession, const char* sPath, xllm_error* pError);
/* Restore a session from a JSONL archive. */
XRT_API xllm_session* xllmSessionLoad(const char* sPath, xllm_error* pError);

/*
 * Journaling is single-writer. Enable it only on a new/recovered session.
 * Mutations are appended before they become visible in memory. A checkpoint
 * atomically writes the full state and then removes covered journal records.
 */
XRT_API bool xllmSessionEnableJournal(xllm_session* pSession, const char* sJournalPath, xllm_error* pError);
/* Disable the automatic journal (debugging aid). */
XRT_API void xllmSessionDisableJournal(xllm_session* pSession);
/* Journal path, or NULL when disabled. */
XRT_API const char* xllmSessionJournalPath(const xllm_session* pSession);
/* Write a standalone snapshot without interrupting the session. */
XRT_API bool xllmSessionCheckpoint(xllm_session* pSession, const char* sSnapshotPath, xllm_error* pError);
/* Both paths are required and must be non-empty; a snapshot FILE that does
 * not exist yet selects the journal-only replay: the session is created from
 * pConfigIfNew (defaults when NULL) and every entry is replayed from the
 * journal. Recovery always re-attaches the journal for continued append. */
XRT_API xllm_session* xllmSessionRecover(const char* sSnapshotPath, const char* sJournalPath,
    const xllm_session_config* pConfigIfNew, xllm_error* pError);

/* ------------------------------------------------------------------ */
/* Easy layer (D1): optional bound client driving the call loop.       */
/* ------------------------------------------------------------------ */

/* Create a session bound to a real client (borrowed; must outlive the session). */
XRT_API xllm_session* xllmSessionCreateBound(const xllm_session_config* pConfig,
    xllm_client* pClient /* borrowed, must outlive the session */, xllm_error* pError);

/* Full convenience turn: begin turn, add user text (cap-checked), render,
 * call, add the assistant response (usage recorded), then MaybeCompact.
 * The response ownership moves to the caller. */
XRT_API xllm_result xllmSessionSend(xllm_session* pSession, const char* sUserText,
    const xllm_stream_callbacks* pCallbacks /* optional */, xllm_response** ppResponse,
    xllm_error* pError);

/* Render and call only; nothing is appended to the ledger. */
XRT_API xllm_result xllmSessionComplete(xllm_session* pSession,
    const xllm_stream_callbacks* pCallbacks /* optional */, xllm_response** ppResponse,
    xllm_error* pError);

/* Test seam: scriptable model call replacing the bound client (usage fully
 * controllable for offline governance lifecycle tests). */
typedef xllm_result (*xllm_test_call_proc)(void* pUserData, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError);
/* Create a session over an injectable call function (offline, deterministic). */
XRT_API xllm_session* xllmSessionCreateForTest(const xllm_session_config* pConfig,
    xllm_test_call_proc pCall, void* pUserData, xllm_error* pError);

/* Bind (or rebind) a client on an existing session: the durable-run path is
 * Recover() -> BindClient() -> RunWithTools(NULL, ...). */
XRT_API bool xllmSessionBindClient(xllm_session* pSession, xllm_client* pClient /* borrowed */);
/* Attach, replace, or remove (NULL) the test seam on an existing session. */
XRT_API bool xllmSessionSetTestCall(xllm_session* pSession, xllm_test_call_proc pCall, void* pUserData);
/* Forward the source session's model driver (client or test seam) onto an
 * existing destination session — the subagent composition path. */
XRT_API bool xllmSessionForwardDriver(xllm_session* pDst, const xllm_session* pSrc);

/* ------------------------------------------------------------------ */
/* Bounded tool round-trips: the loop as a library function.           */
/*                                                                     */
/* One call runs prompt -> model rounds -> executor tool calls -> final */
/* text, with every step recorded in the ledger. This is a convenience, */
/* not a framework: hosts with their own policy (guards, gates, gates,  */
/* prompts) drive BuildRequest/dispatch/AddAssistantResponse manually   */
/* and use the same executor contract.                                  */
/* ------------------------------------------------------------------ */

/* Run guard policy (round cap, model override). */
typedef struct xllm_run_policy {
    /* Model-round budget; 0 selects the default (32); UINT32_MAX disables
     * the round bound entirely (mdo-style hosts guard via pOnRound instead). */
    uint32_t uMaxRounds;
    /* Optional per-run model override (subagent archetypes on a lighter
     * model); borrowed, applied to every request in this run. */
    const char* sModel;
    /* Borrowed cooperative cancel token and absolute deadline (milliseconds;
     * 0 and UINT64_MAX mean none). Applied to every model request and
     * forwarded to each executor context so one tree governs the run. */
    xcancel* pCancel;
    int64_t iTimeout;
    /* Guard seam: invoked after each assistant response is recorded and
     * before its tool calls execute. Return false to stop the run; the
     * unresolved tool calls stay pending in the ledger for a later resume. */
    bool (*pOnRound)(xllm_session* pSession, uint32_t uRound,
        const xllm_response* pResponse, size_t iPendingToolCalls, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
} xllm_run_policy;

/* Run outcome: rounds, tool calls, stop reason, final text. */
typedef struct xllm_run_summary {
    uint32_t uRounds;        /* model rounds consumed */
    uint32_t uToolCalls;     /* executor calls completed (tool-level failures included) */
    bool bStoppedByPolicy;   /* the guard seam stopped the run; calls left pending */
    char* sFinalText;        /* final assistant text; NULL when the run stopped without one */
    xllm_usage tLastUsage;
    uint32_t uReserved[4];
} xllm_run_summary;

/* Zero a run policy (sentinel defaults for unbounded loops). */
XRT_API void xllmRunPolicyInit(xllm_run_policy* pPolicy);
/* Free the contents of a run summary. */
XRT_API void xllmRunSummaryUnit(xllm_run_summary* pSummary);

/* Run a bounded tool round-trip loop. sPrompt == NULL resumes an interrupted
 * run: pending tool calls are completed first, then the loop continues from
 * the durable tail without appending another user prompt. The executor is
 * borrowed and must outlive the call. pCallbacks (optional) stream every
 * model round. On success with bStoppedByPolicy == false the run ended with
 * an assistant final answer.
 *
 * Observer model — three channels, one run:
 *   1. pCallbacks          model text/reasoning deltas (UI streaming);
 *   2. session OnEvent     ledger lifecycle (entries, pressure, compaction);
 *   3. executor/xwork OnEvent  tool start/done, artifact paths, permissions.
 * They are deliberately separate seams; a host UI subscribes to each at its
 * own granularity. Cancellation flows through the policy token.
 *
 * Pairing with xwork: while this loop drives an xwork executor, wrap the run
 * in xworkAgentRunBegin()/xworkAgentRunEnd() so registry mutation stays
 * rejected for the duration (the built-in loop does this on its own). */
XRT_API xllm_result xllmSessionRunWithTools(xllm_session* pSession, const char* sPrompt,
    const xllm_executor* pExecutor, const xllm_stream_callbacks* pCallbacks,
    const xllm_run_policy* pPolicy /* NULL = defaults */, xllm_run_summary* pSummary /* optional */,
    xllm_error* pError);

#ifdef __cplusplus
}
#endif

#endif /* selected xllm_session */

#endif
