#ifndef XLLM_API_H
#define XLLM_API_H

#include <xllm/features.h>
#include <xrt.h>

#if defined(XLLM_FEATURE_XLLM)

/* The selected product requires its complete declared dependency set. */
#if !defined(XRT_FEATURE_JSON_READ)
#error "xllm requires json_read (XRT_FEATURE_JSON_READ)"
#endif
#if !defined(XRT_FEATURE_FILE_WHOLE)
#error "xllm requires file_whole (XRT_FEATURE_FILE_WHOLE)"
#endif
#if !defined(XRT_FEATURE_DIR)
#error "xllm requires dir (XRT_FEATURE_DIR)"
#endif
#if !defined(XRT_FEATURE_CODEC_BASE64)
#error "xllm requires codec_base64 (XRT_FEATURE_CODEC_BASE64)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)
#error "xllm requires net_tcp_dial_sync (XRT_FEATURE_NET_TCP_DIAL_SYNC)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE)
#error "xllm requires net_tcp_dial_future (XRT_FEATURE_NET_TCP_DIAL_FUTURE)"
#endif
#if !defined(XRT_FEATURE_NET_PROXY_DIAL)
#error "xllm requires net_proxy_dial (XRT_FEATURE_NET_PROXY_DIAL)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_FUTURE)
#error "xllm requires net_tcp_future (XRT_FEATURE_NET_TCP_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
#error "xllm requires tls_stream_dial_future (XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_FUTURE)
#error "xllm requires tls_stream_future (XRT_FEATURE_TLS_STREAM_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_LISTENER)
#error "xllm requires tls_stream_listener (XRT_FEATURE_TLS_STREAM_LISTENER)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)
#error "xllm requires tls_stream_listener_sync (XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)"
#endif
#if !defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
#error "xllm requires tls_client_verify (XRT_FEATURE_TLS_CLIENT_VERIFY)"
#endif
#if !defined(XRT_FEATURE_TLS_RECORD_AES)
#error "xllm requires tls_record_aes (XRT_FEATURE_TLS_RECORD_AES)"
#endif
#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
#error "xllm requires tls_schedule_sha256 (XRT_FEATURE_TLS_SCHEDULE_SHA256)"
#endif
#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
#error "xllm requires tls_schedule_sha384 (XRT_FEATURE_TLS_SCHEDULE_SHA384)"
#endif
#if !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
#error "xllm requires tls_key_exchange_x25519 (XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)"
#endif
#if !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256)
#error "xllm requires tls_key_exchange_p256 (XRT_FEATURE_TLS_KEY_EXCHANGE_P256)"
#endif
#if !defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#error "xllm requires tls_identity_rsa (XRT_FEATURE_TLS_IDENTITY_RSA)"
#endif
#if !defined(XRT_FEATURE_TLS_VERIFY)
#error "xllm requires tls_verify (XRT_FEATURE_TLS_VERIFY)"
#endif
#if !defined(XRT_FEATURE_X509_STORE_SYSTEM)
#error "xllm requires x509_store_system (XRT_FEATURE_X509_STORE_SYSTEM)"
#endif
#if !defined(XRT_FEATURE_HTTP1_BODY)
#error "xllm requires http1_body (XRT_FEATURE_HTTP1_BODY)"
#endif
#if !defined(XRT_FEATURE_THREAD)
#error "xllm requires thread (XRT_FEATURE_THREAD)"
#endif
#if !defined(XRT_FEATURE_MUTEX)
#error "xllm requires mutex (XRT_FEATURE_MUTEX)"
#endif
#if !defined(XRT_FEATURE_CANCEL)
#error "xllm requires cancel (XRT_FEATURE_CANCEL)"
#endif
#if !defined(XRT_FEATURE_TIME)
#error "xllm requires time (XRT_FEATURE_TIME)"
#endif


/*
 * xllm v3: the shared foundation for agent workloads.
 *
 * One model call, one API, three provider wire dialects (Chat Completions,
 * OpenAI Responses, Anthropic Messages). The library owns request
 * serialization, HTTP transport, SSE decoding, tool-call assembly and
 * diagnostics. It deliberately does not execute tools, manage conversation
 * history, compact context, or run an agent loop.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLLM_VERSION_MAJOR 3
#define XLLM_VERSION_MINOR 0
#define XLLM_VERSION_PATCH 0

typedef struct xllm_client xllm_client;
typedef struct xllm_call xllm_call;
typedef struct xllm_hooks xllm_hooks;
typedef struct xcancel xcancel;
typedef struct xllm_request xllm_request;
/* Borrowed XRT runtime handles; only meaningful when building against XRT. */
typedef struct xnetengine xnetengine;
typedef struct xfuture xfuture;
typedef struct xx509store xx509store;

/* Call outcome (ok / error / timeout / cancelled). */
typedef enum xllm_result {
    XLLM_RESULT_OK = 0,
    XLLM_RESULT_ERROR = -1,
    XLLM_RESULT_TIMEOUT = -2,
    XLLM_RESULT_CANCELLED = -3
} xllm_result;

/* Stable error categories (argument, OOM, network, protocol, auth, ...). */
typedef enum xllm_error_code {
    XLLM_ERROR_NONE = 0,
    XLLM_ERROR_INVALID_ARGUMENT,
    XLLM_ERROR_OUT_OF_MEMORY,
    XLLM_ERROR_NETWORK,
    XLLM_ERROR_TIMEOUT,
    XLLM_ERROR_CANCELLED,
    XLLM_ERROR_AUTH,
    XLLM_ERROR_RATE_LIMIT,
    XLLM_ERROR_MODEL_NOT_FOUND,
    XLLM_ERROR_UPSTREAM,
    XLLM_ERROR_PROTOCOL,
    XLLM_ERROR_PARSE,
    /* Session-layer additions (appended: existing values stay stable). */
    XLLM_ERROR_LIMIT,   /* a single message exceeds the configured byte cap */
    XLLM_ERROR_HOOK,    /* a host-supplied session hook failed or re-entered */
    XLLM_ERROR_INCOMPLETE_RESPONSE, /* valid stream ended before model termination */
    XLLM_ERROR_OUTPUT_LIMIT /* output cap interrupted a tool-call generation */
} xllm_error_code;

/* Retry diagnostics (attempt, limit, retry-after, retryable). */
typedef struct xllm_diagnostics {
    uint32_t uAttemptCount;
    uint32_t uMaxAttempts;
    uint32_t uRetryAfterMs;
    bool bRetryable;
    bool bRetryExhausted;
    bool bResponseStarted;
    bool bModelDataDelivered;
    bool bReusedConnection;
    bool bContextAttached;
    bool bToolCallDropped;  /* a lifecycle hook removed a tool call */
    int32_t iTransportStatus;
    int32_t iSystemError;
    uint64_t uStartedMs;
    uint64_t uConnectedMs;
    uint64_t uRequestSentMs;
    uint64_t uFirstByteMs;
    uint64_t uFirstTokenMs;
    uint64_t uHeadersMs;
    uint64_t uCompletedMs;
    uint64_t uConnectDurationMs;
    uint64_t uTimeToFirstByteMs;
    uint64_t uTransferDurationMs;
    uint64_t uTotalDurationMs;
    uint64_t uRequestBytes;
    uint64_t uResponseBodyBytes;
    uint64_t uContextDeadlineMs;
    int64 uEffectiveTimeoutMs;
    char sTransportError[32];
    char sTransportPhase[32];
    char sContextStatus[32];
    char sDialect[24];
} xllm_diagnostics;

/* Full error detail with transport and HTTP status plus message. */
typedef struct xllm_error {
    xllm_error_code eCode;
    int32_t iTransportStatus;
    int32_t iHttpStatus;
    char sMessage[512];
    char sProviderMessage[2048];
    char sProviderCode[64];
    char sProviderType[64];
    char sRequestId[160];
    xllm_diagnostics tDiagnostics;
} xllm_error;

/* Chat roles (system, user, assistant, tool). */
typedef enum xllm_role {
    XLLM_ROLE_SYSTEM = 0,
    XLLM_ROLE_USER,
    XLLM_ROLE_ASSISTANT,
    XLLM_ROLE_TOOL
} xllm_role;

/* Wire dialect selection (OpenAI-compatible, GLM, Responses, Anthropic). */
typedef enum xllm_provider {
    XLLM_PROVIDER_OPENAI_COMPAT = 0,
    XLLM_PROVIDER_GLM,
    XLLM_PROVIDER_OPENAI_RESPONSES,
    XLLM_PROVIDER_ANTHROPIC
} xllm_provider;

/* ------------------------------------------------------------------ */
/* Multimodal content parts                                            */
/* ------------------------------------------------------------------ */

/* Multimodal part kinds (text, reasoning, image, audio, file, native). */
typedef enum xllm_part_kind {
    XLLM_PART_TEXT = 0,   /* UTF-8 text */
    XLLM_PART_REASONING,  /* replayed assistant reasoning text */
    XLLM_PART_IMAGE,      /* image bytes or URL reference */
    XLLM_PART_AUDIO,      /* audio bytes */
    XLLM_PART_FILE,       /* generic file/document bytes */
    XLLM_PART_NATIVE      /* provider-native JSON part, replayed verbatim */
} xllm_part_kind;

/* One multimodal part; fields are owned copies set by the Set* helpers. */
typedef struct xllm_part {
    xllm_part_kind eKind;
    char* sText;          /* TEXT/REASONING: content; NATIVE: raw JSON object */
    char* sNativeType;    /* NATIVE: provider block type for exact replay */
    char* sMediaType;     /* "image/png", "audio/wav", ... */
    char* sSourceUrl;     /* remote URL reference when the dialect supports it */
    char* sDetail;        /* image detail hint: auto/low/high */
    uint8_t* pData;       /* owned bytes for IMAGE/AUDIO/FILE */
    size_t iDataSize;
} xllm_part;

/* Zero-init a part of the given kind; fill it with the xllmPartSet* helpers. */
XRT_API void xllmPartInit(xllm_part* pPart, xllm_part_kind eKind);
/* Release a part constructed with the xllmPartSet* helpers (deep frees). */
XRT_API void xllmPartUnit(xllm_part* pPart);
/* Set plain-text content; the string is copied. */
XRT_API bool xllmPartSetText(xllm_part* pPart, const char* sText);
/* Attach inline image bytes (copied); mediaType e.g. "image/png". */
XRT_API bool xllmPartSetImageData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
/* Attach an image by remote URL; mediaType e.g. "image/jpeg". */
XRT_API bool xllmPartSetImageUrl(xllm_part* pPart, const char* sUrl, const char* sMediaType);
/* Attach inline audio bytes (copied); mediaType e.g. "audio/wav". */
XRT_API bool xllmPartSetAudioData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
/* Attach inline file bytes (copied); mediaType carries the document type. */
XRT_API bool xllmPartSetFileData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
/* Attach a provider-native part (raw JSON); replayed verbatim on the wire. */
XRT_API bool xllmPartSetNative(xllm_part* pPart, const char* sNativeType, const char* sJson);

/* ------------------------------------------------------------------ */
/* Messages: text fast path + optional parts                           */
/* ------------------------------------------------------------------ */

/* A message carries either a plain text content (sContent, the fast path
 * session and memory build on) or a parts array. When iPartCount > 0 the
 * parts are authoritative and sContent must be NULL; every setter keeps
 * that invariant. */
typedef struct xllm_tool_call {
    char* sId;
    char* sName;
    char* sArgumentsJson;
} xllm_tool_call;

/* A chat message: role, text fast path, reasoning, tool calls, parts. */
typedef struct xllm_message {
    xllm_role eRole;
    /* Text content must be valid UTF-8; the setters reject anything else
     * before it can ride into provider JSON. */
    char* sContent;            /* text fast path (NULL once parts are used) */
    char* sReasoningContent;   /* assistant reasoning replay text */
    char* sToolCallId;         /* tool role: result correlation */
    xllm_tool_call* pToolCalls;
    size_t iToolCallCount;
    size_t iToolCallCap;
    xllm_part* pParts;
    size_t iPartCount;
    size_t iPartCap;
    char* sNative;             /* whole-message provider-native replay blob */
} xllm_message;

/* ------------------------------------------------------------------ */
/* Tools                                                               */
/* ------------------------------------------------------------------ */

/* A tool registration (name, description, JSON schema, strict flag). */
typedef struct xllm_tool {
    char* sName;
    char* sDescription;
    char* sParametersJson;
    bool bStrict;
} xllm_tool;

/* Tool selection mode (auto / none / required / named). */
typedef enum xllm_tool_choice {
    XLLM_TOOL_CHOICE_AUTO = 0,
    XLLM_TOOL_CHOICE_NONE,
    XLLM_TOOL_CHOICE_REQUIRED,
    XLLM_TOOL_CHOICE_NAMED
} xllm_tool_choice;

/* ------------------------------------------------------------------ */
/* Usage, finish, response                                             */
/* ------------------------------------------------------------------ */

/* Token usage reported by the provider (input, output, cached, reasoning). */
typedef struct xllm_usage {
    uint64_t uInputTokens;
    uint64_t uOutputTokens;
    uint64_t uTotalTokens;
    uint64_t uCachedInputTokens;
    uint64_t uCacheWriteTokens;
    uint64_t uReasoningTokens;
} xllm_usage;

/* Why generation stopped (stop, length, tool calls, content filter). */
typedef enum xllm_finish {
    XLLM_FINISH_STOP = 0,
    XLLM_FINISH_LENGTH,
    XLLM_FINISH_TOOL_CALLS,
    XLLM_FINISH_CONTENT_FILTER,
    XLLM_FINISH_REFUSAL,
    XLLM_FINISH_OTHER
} xllm_finish;

/* Response block kinds (text, reasoning, tool call) in arrival order. */
typedef enum xllm_block_kind {
    XLLM_BLOCK_TEXT = 0,
    XLLM_BLOCK_REASONING,
    XLLM_BLOCK_TOOL_CALL
} xllm_block_kind;

/* Responses arrive as ordered blocks so callers can render interleaved
 * text, reasoning, and tool calls in arrival order. */
typedef struct xllm_block {
    xllm_block_kind eKind;
    char* sText;          /* TEXT / REASONING */
    size_t iToolIndex;    /* TOOL_CALL: index into xllm_response.pToolCalls */
    char* sNative;        /* provider-native replay blob for this block */
} xllm_block;

/* Call statistics: usage plus timing milestones and byte counts. */
typedef struct xllm_stats {
    xllm_usage tUsage;
    uint64_t uConnectMs;
    uint64_t uFirstByteMs;
    uint64_t uFirstTokenMs;
    uint64_t uTotalMs;
    double fOutputTokensPerSec;
    uint64_t uRequestBytes;
    uint64_t uResponseBytes;
    uint32_t uAttempts;
    bool bReusedConnection;
} xllm_stats;

/* Owned response: joined text plus ordered blocks and tool calls. */
typedef struct xllm_response {
    char* sId;
    char* sModel;
    char* sContent;            /* convenience: all TEXT blocks joined */
    char* sReasoningContent;   /* convenience: all REASONING blocks joined */
    char* sRefusal;            /* provider safety refusal, when reported */
    char* sFinishReason;       /* raw provider finish value */
    xllm_finish eFinish;       /* normalized finish */
    char* sRequestId;
    xllm_block* pBlocks;
    size_t iBlockCount;
    xllm_tool_call* pToolCalls;
    size_t iToolCallCount;
    size_t iToolCallCap;
    xllm_usage tUsage;
    uint32_t uHttpStatus;
    xllm_diagnostics tDiagnostics;
    xllm_stats tStats;
} xllm_response;

/* ------------------------------------------------------------------ */
/* Streaming events                                                    */
/* ------------------------------------------------------------------ */

/* Streaming event kinds (response start, deltas, tool arguments, usage, done). */
typedef enum xllm_event_kind {
    XLLM_EVENT_RESPONSE_START = 0,
    XLLM_EVENT_TEXT_DELTA,
    XLLM_EVENT_REASONING_DELTA,
    XLLM_EVENT_TOOL_CALL_DELTA,
    XLLM_EVENT_BLOCK_META,   /* block opened or closed (arrival order) */
    XLLM_EVENT_USAGE,
    XLLM_EVENT_RESPONSE_DONE
} xllm_event_kind;

/* One streaming event; the union payload is selected by kind. */
typedef struct xllm_event {
    xllm_event_kind eKind;
    union {
        struct {
            size_t iBlock;          /* block the delta belongs to */
            const char* sData;
            size_t iLen;
        } tText;
        struct {
            size_t iIndex;          /* tool-call index */
            size_t iBlock;
            const char* sIdDelta;
            const char* sNameDelta;
            const char* sArgumentsDelta;
        } tToolCall;
        struct {
            size_t iBlock;
            xllm_block_kind eKind;
            bool bEnd;
        } tBlockMeta;
        xllm_usage tUsage;
        struct {
            uint32_t uHttpStatus;
            const char* sRequestId;
        } tResponse;
    } as;
} xllm_event;

typedef bool (*xllm_event_fn)(void* pUserData, const xllm_event* pEvent);

/* Stream callbacks fire on the client engine's background workers, never on
 * the thread that started or waits on the call. Callbacks must be thread
 * safe with respect to any state they touch, must not block (they occupy an
 * engine worker), and returning false cancels the call. */
typedef struct xllm_stream_callbacks {
    void* pUserData;
    xllm_event_fn OnEvent;
} xllm_stream_callbacks;

/* ------------------------------------------------------------------ */
/* Model profiles                                                      */
/* ------------------------------------------------------------------ */

typedef uint64_t xllm_capability_flags;

#define XLLM_CAP_TEXT_IN              (1ull << 0)
#define XLLM_CAP_TOOL_RESULT_IN       (1ull << 1)
#define XLLM_CAP_TEXT_OUT             (1ull << 2)
#define XLLM_CAP_JSON_OUT             (1ull << 3)
#define XLLM_CAP_TOOL_CALL_OUT        (1ull << 4)
#define XLLM_CAP_REASONING_OUT        (1ull << 5)
#define XLLM_CAP_STREAM               (1ull << 6)
#define XLLM_CAP_REASONING_CONTROL    (1ull << 7)
#define XLLM_CAP_PARALLEL_TOOL_CALL   (1ull << 8)
/* Chat Completions wire conventions of reasoning-first models: the endpoint
 * takes max_completion_tokens instead of max_tokens, and top-level instructions
 * ride the developer role instead of system. Mirrors of the legacy protocol
 * keep the legacy spellings, so these bits are opt-in through a profile. */
#define XLLM_CAP_MAX_COMPLETION_TOKENS (1ull << 9)
#define XLLM_CAP_DEVELOPER_ROLE        (1ull << 10)
/* The dialect accepts binary media input parts. Despite the historical
 * name this bit gates image, audio, and file parts alike. */
#define XLLM_CAP_IMAGE_IN              (1ull << 11)

/* Context window shape (shared context vs split input/output). */
typedef enum xllm_window_mode {
    XLLM_WINDOW_UNSPECIFIED = 0,
    XLLM_WINDOW_SHARED_CONTEXT,
    XLLM_WINDOW_SPLIT_INPUT_OUTPUT
} xllm_window_mode;

/* A model profile is a non-secret, inspectable capability contract. Connection
 * URLs and credentials remain client configuration. Built-in profiles are
 * immutable snapshots; hosts may provide an explicit custom profile instead. */
typedef struct xllm_model_profile {
    const char* sId;
    const char* sModel;
    xllm_provider eProvider;
    xllm_capability_flags uCapabilities;
    xllm_window_mode eWindowMode;
    uint64_t uContextWindowTokens;
    uint64_t uMaxInputTokens;
    uint32_t uMaxOutputTokens;
    uint32_t uRecommendedOutputReserveTokens;
    uint32_t uRecommendedSummaryTokens;
} xllm_model_profile;

/* Fill a model profile with safe defaults (no caps, no limits). */
XRT_API void xllmModelProfileInit(xllm_model_profile* pProfile);
/* Look up a built-in profile by id or model name; NULL when unknown. */
XRT_API const xllm_model_profile* xllmModelProfileBuiltin(const char* sIdOrModel);
/* Validate profile consistency (limits and caps); fills pError on failure. */
XRT_API bool xllmModelProfileValidate(const xllm_model_profile* pProfile, xllm_error* pError);
/* Test whether the profile declares every required capability bit. */
XRT_API bool xllmModelProfileSupports(const xllm_model_profile* pProfile, xllm_capability_flags uRequired);
/* Validate a request against the profile (model, tools, modalities, budgets). */
XRT_API bool xllmModelProfileValidateRequest(const xllm_model_profile* pProfile,
    const xllm_request* pRequest, xllm_error* pError);

/* Zero an error struct; reusable across failure paths. */
XRT_API void xllmErrorInit(xllm_error* pError);
/* Stable name for an error code (never NULL). */
XRT_API const char* xllmErrorCodeName(xllm_error_code eCode);
/* Stable name for a finish reason (never NULL). */
XRT_API const char* xllmFinishReasonName(xllm_finish eFinish);
/* Whether a call failed with this error may be retried per transport policy. */
XRT_API bool xllmErrorRetryable(const xllm_error* pError);

/* ------------------------------------------------------------------ */
/* Message and request construction                                    */
/* ------------------------------------------------------------------ */

/* Start a message with the given role; add content via the Set and Add helpers. */
XRT_API void xllmMessageInit(xllm_message* pMessage, xllm_role eRole);
/* Deep-free a message (parts, strings, tool calls). */
XRT_API void xllmMessageUnit(xllm_message* pMessage);
/* Replace the text content; copied, fails only on OOM. */
XRT_API bool xllmMessageSetContent(xllm_message* pMessage, const char* sContent);
/* Set the reasoning text captured from thinking models; copied. */
XRT_API bool xllmMessageSetReasoning(xllm_message* pMessage, const char* sReasoningContent);
/* Set the tool-call id this message answers (tool role). */
XRT_API bool xllmMessageSetToolCallId(xllm_message* pMessage, const char* sToolCallId);
/* Append a tool call (id, name, arguments JSON); all copied. */
XRT_API bool xllmMessageAddToolCall(xllm_message* pMessage, const char* sId, const char* sName, const char* sArgumentsJson);
/* Append a multimodal part; deep-copied. */
XRT_API bool xllmMessageAddPart(xllm_message* pMessage, const xllm_part* pPart);
/* Replace with a provider-native message body (raw JSON, replayed verbatim). */
XRT_API bool xllmMessageSetNative(xllm_message* pMessage, const char* sNativeJson);

/* Convenience: build a history assistant message from a completed response
 * (text, reasoning, tool calls; provider-native replay blobs are preserved). */
XRT_API bool xllmMessageFromResponse(const xllm_response* pResponse, xllm_message* pMessage);

/* JSON response mode (off or object). */
typedef enum xllm_json_mode {
    XLLM_JSON_NONE = 0,
    XLLM_JSON_OBJECT
} xllm_json_mode;

/* Borrowed extra transport header. */
typedef struct xllm_header {
    const char* sName;
    const char* sValue;
} xllm_header;

/* Owned request: messages, tools, sampling, budgets, cancellation. */
typedef struct xllm_request {
    xllm_message* pMessages;
    size_t iMessageCount;
    size_t iMessageCap;
    xllm_tool* pTools;
    size_t iToolCount;
    size_t iToolCap;
    /* Borrowed-view bookkeeping (allocation discipline 改造 A/B): entries
     * flagged true are shallow struct copies whose strings point into the
     * lender's storage — xllmRequestUnit skips their deep teardown. NULL
     * means "everything owned" (the default AddMessage/AddTool path). */
    bool* pbMessageBorrowed;           /* parallel to pMessages; may be NULL */
    bool* pbToolBorrowed;              /* parallel to pTools; may be NULL */
    char* sModel;
    char* sReasoningEffort;
    char* sNamedTool;
    uint32_t uReasoningBudgetTokens;   /* thinking budget for dialects that take one */
    uint32_t uMaxOutputTokens;
    double fTemperature;
    bool bHasTemperature;
    double fTopP;
    bool bHasTopP;
    char* sStop;                       /* stop sequence */
    bool bParallelToolCalls;
    xllm_tool_choice eToolChoice;
    xllm_json_mode eJsonMode;
    bool bStream;                      /* default true */
    const xllm_header* pExtraHeaders;  /* borrowed */
    size_t iExtraHeaderCount;
    char* sExtraBodyJson;              /* owned raw JSON object, shallow-merged */
    /* Borrowed cancellation token; it must outlive this request's model call. */
    xcancel* pCancel;
    /* Relative milliseconds; XRT_WAIT_FOREVER disables the timeout. */
    int64_t iTimeout;
    /* Borrowed per-call lifecycle hooks; replaces the client-level set. */
    const xllm_hooks* pHooks;
    /* Wire-prefix cache stamp (set by borrowed-view renders only): the
     * client's serialization cache reuses bytes for [0..iStableMessages)
     * while (pStablePrefixOwner, uStablePrefixStamp) match. Cleared on
     * request clones because hook mutations invalidate the prefix. */
    void* pStablePrefixOwner;
    uint64_t uStablePrefixStamp;
    size_t iStableMessages;
} xllm_request;

/* Zero a request and apply defaults; Unit frees everything it owns. */
XRT_API void xllmRequestInit(xllm_request* pRequest);
/* Deep-free the request contents (messages, tools, strings). */
XRT_API void xllmRequestUnit(xllm_request* pRequest);
/* Shallow-append a message whose strings the lender owns (ledger entries,
 * cached tool tables). The request must not outlive the lender; the lender
 * must not mutate the message while the request holds it. */
XRT_API bool xllmRequestAddMessageView(xllm_request* pRequest, const xllm_message* pMessage);
/* Attach a whole borrowed tool table (replaces any existing owned tools;
 * frees what it replaces). Same lifetime contract as AddMessageView. */
XRT_API bool xllmRequestSetToolsView(xllm_request* pRequest, const xllm_tool* pTools, size_t iCount);
/* Override the model id; copied. */
XRT_API bool xllmRequestSetModel(xllm_request* pRequest, const char* sModel);
/* Set reasoning effort ("low"/"medium"/"high"). */
XRT_API bool xllmRequestSetReasoningEffort(xllm_request* pRequest, const char* sEffort);
/* Add a stop sequence (within the provider limit); copied. */
XRT_API bool xllmRequestSetStop(xllm_request* pRequest, const char* sStop);
/* Merge a JSON object into the wire body (top-level keys, shallow). */
XRT_API bool xllmRequestSetExtraBody(xllm_request* pRequest, const char* sJsonObject);
/* Bind a cancel token consulted during the call; borrowed. */
XRT_API void xllmRequestSetCancel(xllm_request* pRequest, xcancel* pCancel);
/* Per-call timeout in ms overriding the client default (0 keeps the default). */
XRT_API void xllmRequestSetTimeout(xllm_request* pRequest, int64_t iTimeout);
/* Choose tool selection mode (auto/none/required/named). */
XRT_API bool xllmRequestSetToolChoice(xllm_request* pRequest, xllm_tool_choice eChoice, const char* sNamedTool);
/* Append a full message; deep-copied. */
XRT_API bool xllmRequestAddMessage(xllm_request* pRequest, const xllm_message* pMessage);
/* Convenience: append a text-only message. */
XRT_API bool xllmRequestAddTextMessage(xllm_request* pRequest, xllm_role eRole, const char* sContent);
/* Convenience: append a tool-role result answering a call id. */
XRT_API bool xllmRequestAddToolResult(xllm_request* pRequest, const char* sToolCallId, const char* sContent);
/* Register a tool (name, description, JSON schema); strict enforces the schema. */
XRT_API bool xllmRequestAddTool(xllm_request* pRequest, const char* sName, const char* sDescription, const char* sParametersJson, bool bStrict);

/* Free a response and everything it owns. */
XRT_API void xllmResponseDestroy(xllm_response* pResponse);

/* Deterministic lexical token estimates used by budget governance. */
XRT_API uint64_t xllmEstimateTextTokens(const char* sText);
/* Rough token estimate for budget checks; no model call. */
XRT_API uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage);

/* ------------------------------------------------------------------ */
/* Lifecycle hooks: mutable data seams around one model call          */
/* ------------------------------------------------------------------ */

/* Wire-level body handed to the byte seams. The buffer is NUL-terminated;
 * hooks may edit in place (OUT bounded by iBodyCapacity) or replace the
 * pointer wholesale (the library frees the old buffer; the replacement
 * must be xllmFree-compatible and NUL-terminated with iBodySize equal to
 * strlen). Embedded NUL bytes are rejected. */
typedef struct xllm_wire {
    uint32_t uAttempt;        /* retry ordinal, from 1 */
    uint32_t uHttpStatus;     /* response seam only */
    char* sBody;
    size_t iBodySize;
    size_t iBodyCapacity;     /* request seam only */
    uint32_t uReserved[4];
} xllm_wire;

/* Every seam is optional (NULL). Returning false aborts the call with
 * XLLM_ERROR_HOOK. All pointers are mutable in place; borrowed storage a
 * hook attaches (e.g. extra headers) must outlive the call. */
struct xllm_hooks {
    bool (*pOnRequest)(xllm_client* pClient, xllm_request* pRequest, void* pUserData);
    bool (*pOnRequestBody)(xllm_client* pClient, xllm_wire* pWire, void* pUserData);
    bool (*pOnRetry)(xllm_client* pClient, const xllm_diagnostics* pDiagnostics,
        uint32_t uNextAttempt, void* pUserData);
    bool (*pOnResponseBody)(xllm_client* pClient, xllm_wire* pWire, void* pUserData);
    bool (*pOnToolCall)(xllm_client* pClient, xllm_tool_call* pCall,
        uint32_t uIndex, void* pUserData);
    bool (*pOnResponse)(xllm_client* pClient, xllm_response* pResponse, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
};

/* Client-level default hooks (borrowed struct; NULL removes). A request may
 * carry its own pHooks which replaces the whole set for that call. */
XRT_API void xllmClientSetHooks(xllm_client* pClient, const xllm_hooks* pHooks);

/* ------------------------------------------------------------------ */
/* History: a deep-copying message ledger for hand-written agents      */
/* ------------------------------------------------------------------ */

typedef struct xllm_history xllm_history;

/* Create an empty owned message history. */
XRT_API xllm_history* xllmHistoryCreate(void);
/* Free the history. */
XRT_API void xllmHistoryDestroy(xllm_history* pHistory);
/* Number of stored messages. */
XRT_API size_t xllmHistoryCount(const xllm_history* pHistory);
/* Borrowed view of one stored message; valid until the next mutation. */
XRT_API const xllm_message* xllmHistoryAt(const xllm_history* pHistory, size_t iIndex);

/* Append a deep copy of the message. */
XRT_API bool xllmHistoryAdd(xllm_history* pHistory, const xllm_message* pMessage);
/* Convenience: append a text-only message. */
XRT_API bool xllmHistoryAddText(xllm_history* pHistory, xllm_role eRole, const char* sContent);
/* Text + reasoning + tool calls from a completed response. */
XRT_API bool xllmHistoryAddFromResponse(xllm_history* pHistory, const xllm_response* pResponse);
/* Convenience: append a tool-result message. */
XRT_API bool xllmHistoryAddToolResult(xllm_history* pHistory, const char* sCallId,
    const char* sContent);
/* Remove iCount messages starting at iIndex; the tail shifts down. */
XRT_API bool xllmHistoryRemove(xllm_history* pHistory, size_t iIndex, size_t iCount);
/* Appends every stored message into a request (deep copy again). */
XRT_API bool xllmHistoryAppendInto(const xllm_history* pHistory, xllm_request* pRequest);

/* ------------------------------------------------------------------ */
/* Client lifecycle                                                    */
/* ------------------------------------------------------------------ */

/* Client setup: base URL, key, model, dialect, timeouts, retry policy. */
typedef struct xllm_client_config {
    const char* sBaseUrl;
    const char* sApiKey;
    const char* sModel;
    const char* sReasoningEffort;
    const char* sUserAgent;
    uint32_t uMaxOutputTokens;
    uint32_t uTimeoutMs;
    uint32_t uIdleTimeoutMs;
    uint32_t uMaxAttempts;
    uint32_t uRetryBaseDelayMs;
    uint32_t uRetryMaxDelayMs;
    uint32_t uMaxIdleConnections;   /* 0 = default 4; capped at 8 */
    /* Borrowed shared XRT engine; multiple clients may share one. NULL = the
     * client creates and owns a private engine. Must outlive the client. */
    xnetengine* pNetEngine;
    bool bVerifyPeer;
    /* Private-CA trust: PEM text (may carry a chain) or a borrowed store.
     * Either one implies verification; pX509Store wins over sCaPem. */
    const char* sCaPem;
    xx509store* pX509Store;
    xllm_provider eProvider;
    /* Optional borrowed profile. When present, model/provider/limits are
     * validated and the client retains an owned snapshot. */
    const xllm_model_profile* pModelProfile;
} xllm_client_config;

/* Zero a client config with defaults (30-minute timeout, retry enabled). */
XRT_API void xllmClientConfigInit(xllm_client_config* pConfig);
/* Create a client; fills pError and returns NULL on failure. */
XRT_API xllm_client* xllmClientCreate(const xllm_client_config* pConfig, xllm_error* pError);
/* Install lifecycle hooks (wire/auth/stats seams); NULL restores defaults. */
XRT_API void xllmClientSetHooks(xllm_client* pClient, const xllm_hooks* pHooks);
/* Destroy the client; in-flight calls must be finished first. */
XRT_API void xllmClientDestroy(xllm_client* pClient);
/* Copy out the effective, config-resolved model profile. */
XRT_API bool xllmClientGetModelProfile(const xllm_client* pClient, xllm_model_profile* pProfile);
/* Replace the client's capability profile on an existing client (custom
 * endpoints such as self-hosted models; config.pModelProfile covers the
 * create-time path). The profile is validated first; the wire model stays
 * the client's configured sModel. */
XRT_API bool xllmClientSetModelProfile(xllm_client* pClient, const xllm_model_profile* pProfile, xllm_error* pError);

/* Submit a call asynchronously; returns the call and a borrowed future, or NULL with pError. */
XRT_API xllm_call* xllmClientStart(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_error* pError
);
/* The call's completion future; transport runs on the engine workers.
 * The returned future is borrowed: valid until xllmCallWait/Destroy. */
XRT_API xfuture* xllmCallFuture(xllm_call* pCall);
/* Wait once for completion; yields the owned response via ppResponse. */
XRT_API xllm_result xllmCallWait(xllm_call* pCall, xllm_response** ppResponse, xllm_error* pError);
/* Request cooperative cancellation; safe to call after completion. */
XRT_API bool xllmCallCancel(xllm_call* pCall);
/* Destroy the call handle after waiting or cancelling. */
XRT_API void xllmCallDestroy(xllm_call* pCall);

/* Blocking one-shot: start, wait, destroy; NULL response with pError on failure. */
XRT_API xllm_result xllmClientComplete(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
);

/* Builds the provider JSON body without credentials. Free with xllmFree(). */
/* Full classic serialization for inspection/testing; it never reads or
 * updates the client's wire-prefix cache (stamped view requests through
 * this API also bypass it) and works on any dialect. */
XRT_API char* xllmClientBuildRequestJson(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError);
/* Free memory returned by xllm (portable across allocator boundaries). */
XRT_API void xllmFree(void* pMemory);

#ifdef __cplusplus
}
#endif

#endif /* selected xllm */

#endif
