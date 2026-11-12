#ifndef XIDE_WORKSPACE_SEARCH_RESULT_H
#define XIDE_WORKSPACE_SEARCH_RESULT_H
#include "protocol/lsp_wire.h"
#include "platform/text_file.h"
#include "services/language_client.h"
#include <xrt.h>
typedef struct xide_workspace_search_item {
 char *path,*label;wchar_t* key;const xvalue *symbol,*document,*range;
 xide_lsp_position start,end;
} xide_workspace_search_item;
typedef struct xide_workspace_search_result {
 uint64_t token,workerThread;int valid,navigation;size_t count;
 xide_workspace_search_item* items;xvalue* message;xide_text_file snapshot;
 char error[512];
} xide_workspace_search_result;
typedef struct xide_workspace_search_decoder xide_workspace_search_decoder;
/* Worker/test API,takes message. Selection/text are borrowed immutable leases;
 * a navigation proof must match selection and exactly the supplied open text
 * version,or a freshly verified disk snapshot when text is NULL. List<=200;
 * typed path/key/label storage<=32MiB;DOM<=8MiB at transport;source<=8MiB.
 * Resolves both UTF16 offsets on the worker. No UI access. */
xide_workspace_search_result* xideWorkspaceSearchDecode(xvalue* message,const xvalue* selection,
 const xvalue* text,int version,int (*cancelled)(void*),void* user);
void xideWorkspaceSearchResultRelease(xide_workspace_search_result* result);
/* Existing bounded XRT worker pattern:one reserved/pending job,one completed
 * and one retired result. All DOM/body disposal happens on worker or shutdown. */
xide_workspace_search_decoder* xideWorkspaceSearchDecoderCreate(void (*wake)(void*),void* user);
int xideWorkspaceSearchDecoderArm(xide_workspace_search_decoder*,uint64_t token,int request,
 const char* query,const xvalue* selection,const xvalue* text,int version);
xide_language_batch* xideWorkspaceSearchDecoderTakeBatch(xide_workspace_search_decoder*,uint64_t token);
int xideWorkspaceSearchDecoderRetireBatch(xide_workspace_search_decoder*,xide_language_batch*);
int xideWorkspaceSearchDecoderReceive(xvalue* message,size_t bytes,void* decoder);
int xideWorkspaceSearchDecoderSubmit(xide_workspace_search_decoder*,uint64_t token,xvalue* message,size_t bytes);
void xideWorkspaceSearchDecoderCancel(xide_workspace_search_decoder*);
/* Nonblocking worker quiescence, including preparation and disposal tails.
 * Owner cancels and excludes Prepare/Submit/Retire until its barrier ends. */
int xideWorkspaceSearchDecoderIdle(xide_workspace_search_decoder*);
xide_workspace_search_result* xideWorkspaceSearchDecoderTake(xide_workspace_search_decoder*);
int xideWorkspaceSearchDecoderRetire(xide_workspace_search_decoder*,xide_workspace_search_result*);
void xideWorkspaceSearchDecoderDestroy(xide_workspace_search_decoder*);
#endif
