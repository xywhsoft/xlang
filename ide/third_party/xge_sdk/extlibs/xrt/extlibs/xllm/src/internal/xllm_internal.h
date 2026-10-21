#ifndef XLLM_INTERNAL_H
#define XLLM_INTERNAL_H

#include <xllm.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define XLLM_MAX_FALLBACK_BODY (64u * 1024u * 1024u)
#define XLLM_MAX_AUTH_HEADERS 4u
#define XLLM_MAX_IDLE_CONNECTIONS 8u
#define XLLM_HTTP_HEAD_LIMIT (64u * 1024u)
#define XLLM_HTTP_FIELD_LIMIT 100u
#define XLLM_HTTP_TRAILER_LIMIT 32u
#define XLLM_HTTP_IO_CHUNK (64u * 1024u)

/* Engine-driven transport phases; advanced only from op-watch callbacks. */
typedef enum xllm_async_phase {
    XLLM_ASYNC_DIAL = 0,
    XLLM_ASYNC_SEND,
    XLLM_ASYNC_READ,
    XLLM_ASYNC_DONE
} xllm_async_phase;

typedef struct xllm_buf {
    char* pData;
    size_t iLen;
    size_t iCap;
} xllm_buf;

typedef enum xllm_transport_result {
    XLLM_TRANSPORT_OK = 0,
    XLLM_TRANSPORT_ERROR,
    XLLM_TRANSPORT_TIMEOUT,
    XLLM_TRANSPORT_CANCELLED
} xllm_transport_result;

typedef struct xllm_transport_diagnostics {
    xllm_transport_result eResult;
    int32_t iSystemError;
    double uStartedMs;
    double uConnectedMs;
    double uRequestSentMs;
    double uFirstByteMs;
    double uFirstTokenMs;
    double uHeadersMs;
    double uCompletedMs;
    uint64_t uRequestBytes;
    uint64_t uResponseBodyBytes;
    int64 uEffectiveTimeoutMs;
    bool bReusedConnection;
    bool bToolCallDropped;
    char sError[32];
    char sPhase[32];
} xllm_transport_diagnostics;

typedef struct xllm_connection xllm_connection;

/* Owned auth header produced by a dialect; the transport wipes and frees it. */
typedef struct xllm_auth_header {
    char sName[32];
    char* sValue;
} xllm_auth_header;

/* Dialect scratch: provider streaming item id -> unified tool index. */
typedef struct xllm_item_map {
    char sId[64];
    size_t iTool;
} xllm_item_map;

/* One assembled SSE event: field set collected by the framing layer. */
typedef struct xllm_sse_fields {
    xstrview tEvent;   /* event: line value; empty when absent */
    xstrview tData;    /* data: lines joined with \n */
    xstrview tId;      /* id: line value; empty when absent */
} xllm_sse_fields;

struct xllm_client {
    char* sBaseUrl;
    char* sApiKey;
    char* sModel;
    char* sReasoningEffort;
    char* sUserAgent;
    uint32_t uMaxOutputTokens;
    uint32_t uTimeoutMs;
    uint32_t uIdleTimeoutMs;
    uint32_t uMaxAttempts;
    uint32_t uRetryBaseDelayMs;
    uint32_t uRetryMaxDelayMs;
    bool bVerifyPeer;
    char* sCaPem;              /* owned copy; private CA chain */
    xx509store* pX509Store;    /* borrowed custom trust store */
    xllm_provider eProvider;
    xllm_model_profile tModelProfile;
    char* sProfileId;
    bool bHasModelProfile;
    bool bTls;
    char* sHost;
    char* sTarget;
    char* sHostHeader;
    uint16_t uPort;
    const struct xllm_dialect_ops* pDialect;
    bool bEngineOwned;
    xnetengine* pNetEngine;
    xnetresolver* pResolver;
    xtlsverifier* pVerifier;
    xmutex* pConnectionMutex;
    const xllm_hooks* pHooks;   /* borrowed client-level lifecycle hooks */
    xllm_connection* pIdleConnections[XLLM_MAX_IDLE_CONNECTIONS];
    uint32_t uIdleConnectionCount;
    uint32_t uMaxIdleConnections;
    /* --- wire-prefix cache (尾账 #3): inner bytes of the serialized
     * messages array for [0..iPrefixCacheMessages). Keyed by the
     * view-render stamp; the accumulator only grows between misses. */
    void* pPrefixCacheOwner;
    uint64_t uPrefixCacheStamp;
    size_t iPrefixCacheMessages;
    char* sPrefixCacheInner;
    size_t iPrefixCacheLen;
    size_t iPrefixCacheCap;
};

/* Assembly bookkeeping parallel to the response arrays so appends stay O(n). */
typedef struct xllm_tool_state {
    size_t iIdLen;
    size_t iNameLen;
    size_t iArgsLen;
} xllm_tool_state;

typedef struct xllm_block_state {
    size_t iTextLen;
} xllm_block_state;

struct xllm_call {
    xllm_client* pClient;
    const struct xllm_dialect_ops* pDialect;
    xcancel* pCancel;
    /* async engine-driven transport */
    xpromise* pPromise;
    xfuture* pFuture;
    xfuture* pOpFuture;
    void* pOpWatchNode;   /* armed heap watch node (owned by its release) */
    uint64 uTimerId;
    xcancelwatch* pCancelWatch;
    volatile long iTransportActive;
    /* Terminal latch: 0 = running, first finisher wins via atomic add.
     * Diagnostics and connection release run only in the winner. */
    volatile long iTerminal;
    /* Timer lifetime: the engine timer callback runs exactly once per
     * accepted schedule and drops its reference there; Destroy waits for
     * zero after cancelling, closing the async-cancel use-after-free. */
    volatile long iTimerRefs;
    xllm_async_phase ePhase;
    bool bHeadReady;
    bool bBodyDone;
    bool bWireEnd;
    bool bReusable;
    size_t iSendOffset;
    size_t iSendPending;   /* bytes in the in-flight TLS send chunk */
    size_t iWireOffset;
    const xllm_hooks* pHooks;      /* resolved for this call (request over client) */
    xllm_request* pClonedRequest;   /* deep copy when pOnRequest rewrote it */
    bool bDeferParse;               /* pOnResponseBody armed: buffer, parse at wait */
    bool bOfflineBody;               /* pre-send injection: canned response, no dial */
    xllm_connection* pConnection;
    xllm_buf tWire;
    xhttpfield tHeadFields[XLLM_HTTP_FIELD_LIMIT];
    xhttpfield tTrailers[XLLM_HTTP_TRAILER_LIMIT];
    xhttp1head tHead;
    xhttp1limits tHeadLimits;
    xhttp1bodyplan tPlan;
    xhttp1bodylimits tBodyLimits;
    xhttp1body tBody;
    xhttp1errorinfo tProtoErr;
    char* sRequestBody;
    char* sRequestHeader;
    size_t iRequestHeaderSize;
    double uDeadline;
    double uScopeDeadline;
    bool bScopeAttached;
    bool bStreamWanted;
    xllm_transport_result eTransportResult;
    xllm_response* pResponse;
    xllm_stream_callbacks tCallbacks;
    xllm_error tError;
    xllm_transport_diagnostics tHttpDiagnostics;
    /* owned per-request extra headers */
    char** pExtraHeaderNames;
    char** pExtraHeaderValues;
    size_t iExtraHeaderCount;
    /* SSE framing state */
    xllm_buf tLine;
    xllm_buf tEventData;
    xllm_buf tEventName;
    bool bHaveEventName;
    /* non-SSE fallback body */
    xllm_buf tRawBody;
    /* assembly bookkeeping */
    xllm_tool_state* pToolState;
    size_t iToolStateCap;
    xllm_block_state* pBlockState;
    size_t iBlockStateCap;
    /* dialect decoder scratch */
    size_t* pBlockMap;        /* wire block index -> tool index + 1 (0 = none) */
    size_t iBlockMapCap;
    struct xllm_item_map* pItemMap;   /* provider item id -> tool index */
    size_t iItemCount;
    size_t iItemCap;
    uint32_t uHttpStatus;
    uint32_t uAttempt;
    uint32_t uRetryAfterMs;
    char sRequestId[160];
    char sContentType[160];
    char* sSelectedModel;
    volatile long iCallbackActive;
    volatile long iClosing;
    bool bSse;
    bool bDone;
    bool bSawEvent;
    bool bCallbackCancelled;
    bool bWaited;
    bool bResponseTaken;
};

/* ------------------------------------------------------------------ */
/* Dialect layer                                                       */
/* ------------------------------------------------------------------ */

typedef struct xllm_dialect_ops {
    const char* sName;
    const char* sPathSuffix;   /* appended to the base URL when absent */
    /* Auth headers (owned values); returns count, 0 on allocation failure. */
    size_t (*BuildAuth)(const xllm_client* pClient, xllm_auth_header* pOut, size_t iCap);
    /* Serialize the unified request into a provider JSON body (owned). */
    char* (*BuildRequest)(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError);
    /* Incremental serialization (尾账 #3): append messages
     * [iPrefixCount..iMessageCount) into pInner (which already holds the
     * cached prefix bytes) and assemble the full body around it. Only
     * dialects whose per-message serialization is stateless implement
     * this; others leave it NULL and the client falls back. */
    char* (*BuildRequestCached)(xllm_client* pClient, const xllm_request* pRequest,
        xllm_buf* pInner, size_t iPrefixCount, xllm_error* pError);
    /* Decode one assembled SSE event into unified events/response. */
    bool (*DecodeSseEvent)(xllm_call* pCall, const xllm_sse_fields* pFields);
    /* Decode a complete non-streaming JSON body. */
    bool (*DecodeJsonBody)(xllm_call* pCall, xstrview tBody);
    /* Extract provider error details from an error response body. */
    void (*FillProviderError)(xllm_call* pCall, xstrview tBody);
    /* Provider-specific retry classification beyond the common policy. */
    bool (*IsRetryableProviderError)(const xllm_error* pError);
} xllm_dialect_ops;

const xllm_dialect_ops* xllm__dialect_ops_for(xllm_provider eProvider);
const xllm_dialect_ops* xllm__dialect_completions(void);
const xllm_dialect_ops* xllm__dialect_anthropic(void);
const xllm_dialect_ops* xllm__dialect_responses(void);

/* ------------------------------------------------------------------ */
/* Shared utilities (single copy for all layers)                       */
/* ------------------------------------------------------------------ */

/* Swappable allocator. The default is the CRT; tests substitute a fault
 * injector through xllm__set_allocator, which is not public API. */
typedef struct xllm_allocator {
    void* (*Alloc)(size_t iSize);
    void* (*Realloc)(void* pMemory, size_t iSize);
    void (*Free)(void* pMemory);
} xllm_allocator;

void xllm__set_allocator(const xllm_allocator* pAllocator);
size_t xllm__allocation_count(void);
void* xllm__malloc(size_t iSize);
void* xllm__calloc(size_t iCount, size_t iSize);
void* xllm__realloc(void* pMemory, size_t iSize);
void xllm__free(void* pMemory);

char* xllm__strdup(const char* sText);
bool xllm__replace(char** ppDst, const char* sText);
bool xllm__buf_reserve(xllm_buf* pBuf, size_t iNeed);
bool xllm__buf_append(xllm_buf* pBuf, const void* pData, size_t iLen);
bool xllm__buf_append_cstr(xllm_buf* pBuf, const char* sText);
bool xllm__buf_append_char(xllm_buf* pBuf, char ch);
void xllm__buf_reset(xllm_buf* pBuf);
char* xllm__buf_detach(xllm_buf* pBuf);
bool xllm__json_string(xllm_buf* pBuf, const char* sText);
void xllm__error_set(xllm_error* pError, xllm_error_code eCode, const char* sMessage);
void xllm__error_copy(xllm_error* pDst, const xllm_error* pSrc);
void xllm__copy_text(char* sDst, size_t iCap, const char* sSrc);
bool xllm__utf8_valid(const char* sText);
void xllm__copy_view(char* sDst, size_t iCap, xstrview tValue);
bool xllm__contains_ci(const char* sText, const char* sNeedle);
/* Length-tracked owned-string append (no strlen on the existing tail). */
bool xllm__append_tracked(char** ppText, size_t* piLen, const char* sDelta, size_t iDeltaLen);

bool xllm__tool_call_clone(xllm_tool_call* pDst, const xllm_tool_call* pSrc);
void xllm__tool_call_unit(xllm_tool_call* pCall);
bool xllm__message_clone(xllm_message* pDst, const xllm_message* pSrc);
bool xllm__request_clone(xllm_request* pDst, const xllm_request* pSrc);
bool xllm__part_clone(xllm_part* pDst, const xllm_part* pSrc);
void xllm__part_unit(xllm_part* pPart);
void xllm__tool_unit(xllm_tool* pTool);

/* ------------------------------------------------------------------ */
/* Response assembly                                                   */
/* ------------------------------------------------------------------ */

xllm_response* xllm__assemble_ensure(xllm_call* pCall);
bool xllm__assemble_text(xllm_call* pCall, xllm_block_kind eKind, xstrview tText, char* sNative);
bool xllm__assemble_native(xllm_call* pCall, xllm_block_kind eKind, xstrview tNative);
bool xllm__assemble_tool(xllm_call* pCall, size_t iIndex, xstrview tId, xstrview tName, xstrview tArguments);
bool xllm__assemble_block_mark_tool(xllm_call* pCall, size_t iToolIndex);
size_t xllm__assemble_map_block_tool(xllm_call* pCall, size_t iWireIndex);
bool xllm__assemble_set_block_tool(xllm_call* pCall, size_t iWireIndex, size_t iToolIndex);
size_t xllm__assemble_find_item_tool(xllm_call* pCall, const char* sItemId);
size_t xllm__assemble_add_item_tool(xllm_call* pCall, const char* sItemId, xstrview tId, xstrview tName);
bool xllm__assemble_usage(xllm_call* pCall, const xllm_usage* pUsage);
void xllm__assemble_finish(xllm_call* pCall, xstrview tRaw);
bool xllm__assemble_finalize(xllm_call* pCall);
bool xllm__assemble_refusal(xllm_call* pCall, xstrview tRefusal);
void xllm__assemble_first_token(xllm_call* pCall);

/* Generic helpers shared by dialect decoders (JSON navigation on xvalue). */
xvalue* xllm__json_get(xvalue* pObject, const char* sKey);
xstrview xllm__json_text(xvalue* pObject, const char* sKey);
uint64_t xllm__json_u64(xvalue* pObject, const char* sKey);

/* ------------------------------------------------------------------ */
/* SSE framing                                                         */
/* ------------------------------------------------------------------ */

bool xllm__sse_feed(xllm_call* pCall, const void* pData, size_t iLen);
bool xllm__sse_finish(xllm_call* pCall);
void xllm__sse_reset(xllm_call* pCall);

/* ------------------------------------------------------------------ */
/* Transport / call                                                    */
/* ------------------------------------------------------------------ */

bool xllm__emit(xllm_call* pCall, const xllm_event* pEvent);
bool xllm__transport_headers(xllm_call* pCall, const xhttp1head* pHead);
bool xllm__transport_body(xllm_call* pCall, const void* pData, size_t iLen);
bool xllm__transport_client_init(xllm_client* pClient, xllm_error* pError);
void xllm__transport_client_unit(xllm_client* pClient);
/* Submits the transport onto the client engine; the call promise reaches a
 * terminal state when the exchange completes, fails, or is cancelled. */
void xllm__transport_begin(xllm_call* pCall);
/* Abort in-flight transport and wait until the call reaches a terminal
 * state; safe from any thread, used by Wait-timeout and Destroy. */
void xllm__transport_abort(xllm_call* pCall);

static inline long xllm__atomic_add(volatile long* pValue, long iDelta)
{
#if defined(_MSC_VER)
    return _InterlockedExchangeAdd(pValue, iDelta) + iDelta;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(pValue, iDelta, __ATOMIC_SEQ_CST);
#else
    *pValue += iDelta;
    return *pValue;
#endif
}

static inline long xllm__atomic_load(volatile long* pValue)
{
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(pValue, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
#else
    return *pValue;
#endif
}

static inline void xllm__atomic_store(volatile long* pValue, long iValue)
{
#if defined(_MSC_VER)
    (void)_InterlockedExchange(pValue, iValue);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(pValue, iValue, __ATOMIC_SEQ_CST);
#else
    *pValue = iValue;
#endif
}


/* Internal probes also link through the modular build. */
char* xllm__client_serialize_body(xllm_client* pClient,
    const xllm_request* pRequest, xllm_error* pError);

#endif
