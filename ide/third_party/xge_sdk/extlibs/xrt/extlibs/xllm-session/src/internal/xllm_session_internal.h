#ifndef XLLM_SESSION_INTERNAL_H
#define XLLM_SESSION_INTERNAL_H

#include <xllm-session.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xllm_session_entry {
    uint64_t uSequence;
    uint64_t uTurn;
    uint64_t uEstimatedTokens;
    uint32_t uFlags;
    xllm_message tMessage;
} xllm_session_entry;

typedef struct xllm_session_buf {
    char* pData;
    size_t iLen;
    size_t iCap;
} xllm_session_buf;

struct xllm_session {
    xllm_session_config tConfig;
    xllm_session_entry* pEntries;
    size_t iEntryCount;
    size_t iEntryCap;
    uint64_t uNextSequence;
    uint64_t uCurrentTurn;
    uint64_t uCompactedThrough;
    uint64_t uCompactionCount;
    uint64_t uJournalSequence;
    char* sSummary;
    char* sJournalPath;
    char* sStyleStorage;           /* owned copy of the loaded summary style */
    /* --- asset ledger (survives compaction by design) --- */
    char** psReadFiles;
    size_t iReadFileCount;
    size_t iReadFileCap;
    char** psModifiedFiles;
    size_t iModifiedFileCount;
    size_t iModifiedFileCap;
    /* --- v3 governance (exact feedback loop) --- */
    uint32_t uSummaryGeneration;       /* +1 per compaction or L2 truncation */
    uint64_t uSummaryPromptAtBirth;    /* meta-call usage, exact */
    uint64_t uSummaryOutputAtBirth;
    bool bFillSeen;                    /* any usage feedback ever received */
    bool bFillExactValid;              /* current fill_exact usable */
    uint64_t uFillExact;               /* last prompt+output tokens */
    uint64_t uCachedInputTokens;
    uint64_t uIncrementMax;            /* worst-case next-turn growth envelope */
    uint32_t uAutoCompactStreak;
    /* --- performance caches (尾账 #1/#3) ---
     * bStatsDirty: any mutation that stats reads (entries, watermarks,
     * summary, governance fill) sets it; GetStats rebuilds lazily. The
     * const API keeps its promise — the cache is lazy evaluation, not
     * observable state. uRenderGeneration: bumped by the same mutators;
     * wire-prefix caches key on it (owner pointer + generation). */
    bool bStatsDirty;
    xllm_session_stats tStatsCache;
    uint64_t uRenderGeneration;
    uint64_t uSessionNonce;        /* global unique-per-instance: prefix-cache
                                    * owner identity survives address reuse */
    uint64_t uLastUserSequence;        /* newest user entry at the last auto compaction */
    uint64_t uTailFloor;               /* L2: entries <= floor leave the rendered tail */
    xllm_session_pressure eLastPressure;
    /* --- v3 strategy and hooks (borrowed) --- */
    const xllm_compaction_ops* pOps;
    const xllm_session_hooks* pHooks;
    xllm_client* pClient;
    xllm_test_call_proc pTestCall;     /* test seam, preferred over pClient */
    void* pTestCallData;
    bool bInHook;                      /* re-entrancy guard for ops/hooks */
};

struct xllm_compaction {
    xllm_session* pSession;
    uint64_t uBaseCompactedThrough;
    uint64_t uThroughSequence;
    uint64_t uEstimatedTokens;
    uint64_t uUsagePromptTokens;       /* meta-call usage recorded by the host */
    uint64_t uUsageOutputTokens;
    char* sPrompt;
    bool bCommitted;
};

char* xllm_session__strdup(const char* sText);
bool xllm_session__message_clone(xllm_message* pDst, const xllm_message* pSrc);
bool xllm_session__buf_append(xllm_session_buf* pBuf, const void* pData, size_t iLen);
bool xllm_session__buf_cstr(xllm_session_buf* pBuf, const char* sText);
bool xllm_session__buf_char(xllm_session_buf* pBuf, char ch);
bool xllm_session__buf_u64(xllm_session_buf* pBuf, uint64_t uValue);
bool xllm_session__json_string(xllm_session_buf* pBuf, const char* sText);
char* xllm_session__buf_detach(xllm_session_buf* pBuf);
void xllm_session__buf_unit(xllm_session_buf* pBuf);
void xllm_session__error(xllm_error* pError, xllm_error_code eCode, const char* sMessage);
uint64_t xllm_session__input_budget(const xllm_session* pSession);
bool xllm_session__entry_is_active(const xllm_session* pSession, const xllm_session_entry* pEntry);
bool xllm_session__should_prune_tool(const xllm_session* pSession, const xllm_session_entry* pEntry);
uint32_t xllm_session__pending_tool_calls(const xllm_session* pSession);
bool xllm_session__journal_append_turn(xllm_session* pSession, uint64_t uTurn);
bool xllm_session__journal_append_entry(xllm_session* pSession, const xllm_session_entry* pEntry);
bool xllm_session__journal_append_compaction(xllm_session* pSession, uint64_t uThroughSequence,
    uint32_t uGeneration, uint64_t uPromptTokens, uint64_t uOutputTokens, const char* sSummary);
bool xllm_session__journal_append_truncate(xllm_session* pSession, uint64_t uFrom, uint64_t uTo);
bool xllm_session__write_entry(xllm_session_buf* pJson, const xllm_session_entry* pEntry);
xvalue* xllm_session__json_get(xvalue* pObject, const char* sKey);
const char* xllm_session__json_text(xvalue* pObject, const char* sKey);
uint64_t xllm_session__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault);
double xllm_session__json_double(xvalue* pObject, const char* sKey, double fDefault);
bool xllm_session__load_message(xllm_message* pMessage, xvalue* pEntry);

/* governance (govern.c) */
void xllm_session__record_usage(xllm_session* pSession, const xllm_usage* pUsage);
void xllm_session__invalidate_fill(xllm_session* pSession);
xllm_session_pressure xllm_session__pressure_exact(const xllm_session* pSession);
void xllm_session__event(xllm_session* pSession, xllm_session_event_type eType,
    uint64_t uSeqFrom, uint64_t uSeqTo, const char* sText);
void xllm_session__pressure_event(xllm_session* pSession);
bool xllm_session__hook_enter(xllm_session* pSession, xllm_error* pError, const char* sStage);
void xllm_session__hook_leave(xllm_session* pSession);

/* compaction pipeline (compact.c) */
typedef struct { uint32_t uFlag; const char* sHeading; } xllm_session_section;
typedef struct { const char* sId; const char* sInstruction;
    const xllm_session_section* pSections; size_t iSectionCount; } xllm_session_style;
const xllm_session_style* xllm_session__style(const xllm_session* pSession);
bool xllm_session__summary_text_ok(const xllm_session* pSession, const char* sSummary);
uint64_t xllm_session__tail_cut(const xllm_session* pSession, uint64_t uKeepTokens,
    uint64_t uFloor /* exclusive lower bound */);
char* xllm_session__serialize_candidates(const xllm_session* pSession, uint64_t uFrom, uint64_t uTo);
bool xllm_session__turn_is_safe(const xllm_session* pSession, uint64_t uTurn);
bool xllm_session__plan_pair_safe(const xllm_session* pSession, uint64_t uThrough);
bool xllm_session__auto_compact(xllm_session* pSession, xllm_error* pError);

/* render (render.c) */
char* xllm_session__pruned_content(const xllm_session* pSession, const xllm_session_entry* pEntry);

/* easy layer (easy.c) */
bool xllm_session__client_summarize(xllm_session* pSession, const char* sPrompt,
    char** psSummary, xllm_usage* pUsage, xllm_error* pError);
xllm_result xllm_session__dispatch_call(xllm_session* pSession, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError);

/* asset ledger (core.c) */
bool xllm_session__note_file(char*** ppsList, size_t* piCount, size_t* piCap,
    const char* sPath, bool* pbAdded);
bool xllm_session__append_ledger_blocks(xllm_session_buf* pBuf, const xllm_session* pSession);
bool xllm_session__journal_append_ledger(xllm_session* pSession, const char* sKind, const char* sPath);

#endif
