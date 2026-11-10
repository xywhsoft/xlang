#ifndef XIDE_SEMANTIC_TOKEN_JOB_H
#define XIDE_SEMANTIC_TOKEN_JOB_H
#include "protocol/semantic_tokens_wire.h"
typedef struct xide_semantic_token_job xide_semantic_token_job;
enum xide_token_job_state {XIDE_TOKEN_RUNNING,XIDE_TOKEN_READY,XIDE_TOKEN_FAILED,XIDE_TOKEN_STALE,XIDE_TOKEN_CANCELLED};
typedef struct xide_token_request {int mode;xide_token_range range;char resultId[64];} xide_token_request;
enum {XIDE_TOKENS_FULL,XIDE_TOKENS_RANGE,XIDE_TOKENS_DELTA};
/* Captures one native owner and small immutable request metadata. Text lookup,
 * native validation, classification, batch coordinates, relative encoding and
 * full/delta JSON preparation run on an XRT thread. Borrowed callback data must
 * outlive Destroy; no callback reentry/destruction. No GUI or compiler DLL. */
xide_semantic_token_job* xideSemanticTokenJobStart(xls_semantic_check*,const char* path,int prefix,
 const xide_token_legend*,const xide_token_request*,xide_token_data* previous,
 void (*wake)(void*),void* user,char* error,size_t capacity);
void xideSemanticTokenJobCancel(xide_semantic_token_job*);
int xideSemanticTokenJobCancelled(const xide_semantic_token_job*);
int xideSemanticTokenJobFinished(xide_semantic_token_job*);
enum xide_token_job_state xideSemanticTokenJobState(const xide_semantic_token_job*);
/* READY only; independently retained immutable result/storage. */
xvalue* xideSemanticTokenJobResult(const xide_semantic_token_job*);
xide_token_data* xideSemanticTokenJobData(const xide_semantic_token_job*);
xide_token_position_work xideSemanticTokenJobWork(const xide_semantic_token_job*);
const xls_semantic_check* xideSemanticTokenJobOwner(const xide_semantic_token_job*);
/* Cancels then joins. The protocol owner only destroys Finished jobs during
 * normal operation; shutdown may join the remaining bounded jobs. */
void xideSemanticTokenJobDestroy(xide_semantic_token_job*);
#endif
