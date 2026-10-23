#ifndef XLANG_IDE_LANGUAGE_CLIENT_H
#define XLANG_IDE_LANGUAGE_CLIENT_H
#include "protocol/lsp_wire.h"
typedef struct xide_language_client_spec {
 const char *program,*directory;
 const char* const* arguments;
 size_t argumentCount;
} xide_language_client_spec;
typedef struct xide_language_client xide_language_client;
/* An owned notification/response may be handed off on the transport worker.
 * Install once per client. The callback/user outlive ClientStop, never touch
 * GUI state or call Stop, and must return promptly. Positive success TAKES the
 * parsed root; zero leaves it for the normal bounded queue. Wire byte counts
 * come from the existing parser/frame bound, without JSON re-encoding. */
typedef int (*xide_language_receive_proc)(xvalue* message,size_t bytes,void* user);
int xideLanguageClientSetReceiver(xide_language_client* client,xide_language_receive_proc receive,void* user);
typedef struct xide_language_batch xide_language_batch;
/* Thread-exclusive prepared notification batch. Encoding/allocation happens
 * in Append; Send performs a bounded all-or-none queue splice with no encoding.
 * Successful Send TAKES the batch; false leaves it owned by the caller. */
xide_language_batch* xideLanguageBatchCreate(void);
int xideLanguageBatchAppend(xide_language_batch* batch,const char* method,const xvalue* params);
void xideLanguageBatchDestroy(xide_language_batch* batch);
int xideLanguageBatchSend(xide_language_client* client,xide_language_batch* batch);
enum xide_language_client_state { XIDE_LS_STARTING,XIDE_LS_RUNNING,XIDE_LS_STOPPED,XIDE_LS_FAILED };
xide_language_client* xideLanguageClientStart(const char* program,const char* compiler,const char* directory,xide_model_error* error);
/* Deep-copies up to 64 literal arguments before starting the stdio worker. */
xide_language_client* xideLanguageClientStartProcess(const xide_language_client_spec* spec,xide_model_error* error);
/* Worker callback only wakes the owner; it must not mutate UI. User data must
   remain valid until Stop joins the worker, including during callback removal. */
void xideLanguageClientSetWake(xide_language_client* client,void (*wake)(void*),void* user);
int xideLanguageClientSend(xide_language_client* client,const xvalue* message,xide_model_error* error);
xvalue* xideLanguageClientNext(xide_language_client* client);
/* Moves the same parsed root plus its original UTF-8 Content-Length, excluding
 * framing headers. No JSON encoding/clone; bytes is zero when no root exists.
 * Allows bounded background batches to account for actual received bytes. */
xvalue* xideLanguageClientNextSized(xide_language_client* client,size_t* bytes);
int xideLanguageClientHasPending(xide_language_client* client);
enum xide_language_client_state xideLanguageClientState(xide_language_client* client,char* error,size_t capacity);
void xideLanguageClientStop(xide_language_client* client);
#endif
