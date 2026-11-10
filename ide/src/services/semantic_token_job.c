#include "services/semantic_token_job.h"
#include "platform/path.h"
#include <stdio.h>
#include <string.h>
struct xide_semantic_token_job {
 xthread* thread;xatomic32 cancel,state;xls_semantic_check* check;char* path;int prefix;
 xide_token_legend legend;xide_token_request request;xide_token_data *previous,*data;xvalue* result;
 xide_token_position_work work;void (*wake)(void*);void* user;
};
static int cancelled(void* user){xide_semantic_token_job* job=user;return xrtAtomic32Load(&job->cancel,XMEMORY_ACQUIRE)!=0;}
static int32 run(void* user)
{
 xide_semantic_token_job* job=user;enum xide_token_job_state state=XIDE_TOKEN_FAILED;
 xide_token_positions* positions=NULL;xide_token_data* data=NULL;xvalue* result=NULL;
 if(cancelled(job)){state=XIDE_TOKEN_CANCELLED;goto done;}
 if(!xlsSemanticCheckValidate(job->check)){state=XIDE_TOKEN_STALE;goto done;}
 xls_inputs inputs;if(!xlsSemanticCheckInputs(job->check,&inputs)){state=XIDE_TOKEN_STALE;goto done;}
 size_t source=SIZE_MAX;
 for(size_t i=0;i<inputs.sources;++i){xls_input_source row;if(!xlsSemanticCheckSource(job->check,i,&row)){state=XIDE_TOKEN_STALE;goto done;}if((row.kind&1) && xidePathLexicalEqual(row.path,job->path)){if(source!=SIZE_MAX)goto done;source=i;}}
 if(source==SIZE_MAX)goto done;
 enum xls_semantic_tokens_error error;
 positions=xideTokenPositionsCreate(job->check,source,job->prefix,0,cancelled,job,&error);
 if(!positions)goto done;
 job->work=xideTokenPositionsWork(positions);
 data=xideTokenDataEncode(positions,&job->legend,job->request.mode==XIDE_TOKENS_RANGE?&job->request.range:NULL,
  job->request.mode==XIDE_TOKENS_RANGE?NULL:job->request.resultId,cancelled,job);
 if(!data)goto done;
 result=xideTokenWireResult(data,job->previous,job->request.mode==XIDE_TOKENS_DELTA,cancelled,job);if(!result)goto done;
 if(!xlsSemanticCheckValidate(job->check)){state=XIDE_TOKEN_STALE;goto done;}
 state=XIDE_TOKEN_READY;
done:
 if(cancelled(job))state=XIDE_TOKEN_CANCELLED;
 else if(state!=XIDE_TOKEN_READY && !xlsSemanticCheckTypedIndex(job->check))state=XIDE_TOKEN_STALE;
 if(state==XIDE_TOKEN_READY){job->data=data;data=NULL;job->result=result;result=NULL;}
 xideTokenPositionsRelease(positions);xideTokenDataRelease(data);xrtValueRelease(result);
 xrtAtomic32Store(&job->state,state,XMEMORY_RELEASE);
 if(job->wake)job->wake(job->user);
 return 0;
}
xide_semantic_token_job* xideSemanticTokenJobStart(xls_semantic_check* check,const char* path,int prefix,
 const xide_token_legend* legend,const xide_token_request* request,xide_token_data* previous,
 void (*wake)(void*),void* user,char* error,size_t capacity)
{
 if(error && capacity)*error=0;
 if(!check || !path || !*path || strlen(path)>65535 || prefix<0 || prefix>1 || !legend || !legend->enabled || !request || request->mode<XIDE_TOKENS_FULL || request->mode>XIDE_TOKENS_DELTA || !memchr(request->resultId,0,sizeof(request->resultId))){if(error && capacity)snprintf(error,capacity,"Invalid semantic token request.");return NULL;}
 xide_semantic_token_job* job=xrtCalloc(1,sizeof(*job));if(!job)goto failed;
 xrtAtomic32Init(&job->cancel,0);xrtAtomic32Init(&job->state,XIDE_TOKEN_RUNNING);
 job->path=xrtStrDup(path);job->check=xlsSemanticCheckRetain(check);job->prefix=prefix;job->legend=*legend;job->request=*request;
 job->previous=xideTokenDataRetain(previous);job->wake=wake;job->user=user;
 if(!job->path || !(job->thread=xrtThreadCreate(run,job,0))){xideSemanticTokenJobDestroy(job);goto failed;}
 return job;
failed:
 if(error && capacity)snprintf(error,capacity,"Unable to start background semantic tokens.");
 return NULL;
}
void xideSemanticTokenJobCancel(xide_semantic_token_job* j){if(j)xrtAtomic32Store(&j->cancel,1,XMEMORY_RELEASE);}
int xideSemanticTokenJobCancelled(const xide_semantic_token_job* j){return j && xrtAtomic32Load(&j->cancel,XMEMORY_ACQUIRE);}
int xideSemanticTokenJobFinished(xide_semantic_token_job* j){return !j || !j->thread || xrtThreadWaitFor(j->thread,0)==XWAIT_OK;}
enum xide_token_job_state xideSemanticTokenJobState(const xide_semantic_token_job* j){return j?(enum xide_token_job_state)xrtAtomic32Load(&j->state,XMEMORY_ACQUIRE):XIDE_TOKEN_FAILED;}
xvalue* xideSemanticTokenJobResult(const xide_semantic_token_job* j){return xideSemanticTokenJobState(j)==XIDE_TOKEN_READY?xrtValueRetain(j->result):NULL;}
xide_token_data* xideSemanticTokenJobData(const xide_semantic_token_job* j){return xideSemanticTokenJobState(j)==XIDE_TOKEN_READY?xideTokenDataRetain(j->data):NULL;}
xide_token_position_work xideSemanticTokenJobWork(const xide_semantic_token_job* j){return xideSemanticTokenJobState(j)==XIDE_TOKEN_READY?j->work:(xide_token_position_work){0};}
const xls_semantic_check* xideSemanticTokenJobOwner(const xide_semantic_token_job* j){return j?j->check:NULL;}
void xideSemanticTokenJobDestroy(xide_semantic_token_job* j)
{
 if(!j)return;
 xideSemanticTokenJobCancel(j);if(j->thread){xrtThreadWait(j->thread);xrtThreadDestroy(j->thread);}
 xlsSemanticCheckDestroy(j->check);xrtFree(j->path);xideTokenDataRelease(j->previous);xideTokenDataRelease(j->data);xrtValueRelease(j->result);xrtFree(j);
}
