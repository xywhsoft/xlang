#ifndef XLANG_IDE_C_LANGUAGE_H
#define XLANG_IDE_C_LANGUAGE_H
#include "services/language_client.h"
#include <stdint.h>
typedef struct xide_c_session xide_c_session;
typedef struct xide_c_document { uint64_t id; const char *path,*text; int version; } xide_c_document;
enum xide_c_state { XIDE_C_STARTING,XIDE_C_READY,XIDE_C_FAILED,XIDE_C_STOPPED };
enum xide_c_query { XIDE_C_COMPLETION,XIDE_C_DEFINITION,XIDE_C_HOVER,XIDE_C_SYMBOLS };
enum xide_c_event_kind { XIDE_C_INITIALIZED,XIDE_C_DIAGNOSTICS,XIDE_C_RESULT,XIDE_C_INPUT_TRACE };
typedef struct xide_c_event {
 enum xide_c_event_kind kind;
 enum xide_c_query query;
 uint64_t documentId,generation;
 int version,requestId;
 xvalue* message;
 /* Consumer-local exact xlang target, separate from the real wire JSON. */
 xvalue* sourceDefinition;
} xide_c_event;
/* An immutable analysis session: copies commands and all opened C/header
   texts. Replace/stop the session when any participating input, configuration
   or disk dependency changes. It does not claim a complete dependency guard.
   All calls are owner-thread calls; wake only signals that owner. */
xide_c_session* xideCSessionStart(const char* clangd,const char* directory,const xvalue* commands,const xide_c_document* documents,size_t count,xide_model_error* error);
/* Dedicated semantic-completion session builds/waits for a real preamble.
   Accepts 64 user documents plus one job-owned bridge indexing header.
   Diagnostics still use Start to avoid forced-header command-macro replay. */
xide_c_session* xideCSessionStartCompletion(const char* clangd,const char* directory,const xvalue* commands,const xide_c_document* documents,size_t count,xide_model_error* error);
uint64_t xideCSessionGeneration(const xide_c_session* session);
void xideCSessionSetWake(xide_c_session* session,void (*wake)(void*),void* user);
enum xide_c_state xideCSessionState(xide_c_session* session,char* error,size_t capacity);
int xideCSessionSupports(xide_c_session* session,enum xide_c_query query);
/* Optional negotiated provider extension; ordinary clangd remains compatible. */
int xideCSessionTracing(const xide_c_session* session);
int xideCSessionRequest(xide_c_session* session,enum xide_c_query query,uint64_t documentId,int byteOffset,xide_model_error* error);
int xideCSessionCancel(xide_c_session* session,int requestId);
/* Returned event owns its wire JSON and optional local sourceDefinition.
   Initialize it to zero and clear both after use.
   Standard response errors are events; malformed/out-of-snapshot diagnostics
   are rejected. Closed-file definition positions require consumer disk checks. */
/* At most 32 ignored messages or 8 ms per call, plus validation of one
   bounded reply. Remaining queued messages re-signal the owner wake. */
int xideCSessionNext(xide_c_session* session,xide_c_event* event);
void xideCEventClear(xide_c_event* event);
void xideCSessionStop(xide_c_session* session);
void xideCSessionDestroy(xide_c_session* session);
#endif
