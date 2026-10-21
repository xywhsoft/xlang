#ifndef XLANG_IDE_LANGUAGE_CLIENT_H
#define XLANG_IDE_LANGUAGE_CLIENT_H
#include "protocol/lsp_wire.h"
typedef struct xide_language_client_spec {
 const char *program,*directory;
 const char* const* arguments;
 size_t argumentCount;
} xide_language_client_spec;
typedef struct xide_language_client xide_language_client;
enum xide_language_client_state { XIDE_LS_STARTING,XIDE_LS_RUNNING,XIDE_LS_STOPPED,XIDE_LS_FAILED };
xide_language_client* xideLanguageClientStart(const char* program,const char* compiler,const char* directory,xide_model_error* error);
/* Deep-copies up to 64 literal arguments before starting the stdio worker. */
xide_language_client* xideLanguageClientStartProcess(const xide_language_client_spec* spec,xide_model_error* error);
/* Worker callback only wakes the owner; it must not mutate UI. User data must
   remain valid until Stop joins the worker, including during callback removal. */
void xideLanguageClientSetWake(xide_language_client* client,void (*wake)(void*),void* user);
int xideLanguageClientSend(xide_language_client* client,const xvalue* message,xide_model_error* error);
xvalue* xideLanguageClientNext(xide_language_client* client);
int xideLanguageClientHasPending(xide_language_client* client);
enum xide_language_client_state xideLanguageClientState(xide_language_client* client,char* error,size_t capacity);
void xideLanguageClientStop(xide_language_client* client);
#endif
