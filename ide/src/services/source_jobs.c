#define _WIN32_WINNT 0x0601
#include "services/source_jobs.h"
#include "model/workspace.h"
#include <windows.h>
#include <limits.h>
#include <stdio.h>
static int sourceFail(xide_model_error* error,const char* text)
{if(error){error->code=XIDE_MODEL_CONFLICT;snprintf(error->message,sizeof(error->message),"%s",text);}return 0;}
int xideSourceJobsHeld(const xide_source_jobs* jobs)
{return jobs && jobs->token!=0;}
int xideSourceJobsBegin(xide_source_jobs* jobs,xide_model_error* error)
{
 if(!jobs || jobs->token || jobs->admissions==UINT_MAX)return sourceFail(error,"Background source jobs are suspended or unavailable.");
 ++jobs->admissions;return 1;
}
void xideSourceJobsEnd(xide_source_jobs* jobs)
{if(jobs && jobs->admissions)--jobs->admissions;}
int xideSourceJobLink(xide_source_jobs* jobs,xide_source_job* job)
{
 if(!jobs || !jobs->admissions || jobs->token || !job || job->owner || jobs->count==SIZE_MAX)return 0;
 job->owner=jobs;job->previous=NULL;job->next=jobs->first;job->thread=NULL;
 if(job->next)job->next->previous=job;
 jobs->first=job;++jobs->count;return 1;
}
int xideSourceJobBind(xide_source_job* job,void* thread)
{
 if(!job || !job->owner || !job->owner->admissions || job->thread || !thread || thread==INVALID_HANDLE_VALUE)return 0;
 job->thread=thread;return 1;
}
int xideSourceJobStopped(const xide_source_job* job)
{return job && job->owner && job->thread && WaitForSingleObject(job->thread,0)==WAIT_OBJECT_0;}
void xideSourceJobUnlink(xide_source_job* job)
{
 if(!job || !job->owner)return;
 xide_source_jobs* jobs=job->owner;
 if(jobs->token)jobs->invalidated=1;
 if(job->previous)job->previous->next=job->next;else jobs->first=job->next;
 if(job->next)job->next->previous=job->previous;
 --jobs->count;*job=(xide_source_job){0};
}
uint64_t xideSourceJobsSuspend(xide_source_jobs* jobs,xide_model_error* error)
{
 if(!jobs || jobs->token || jobs->admissions || jobs->serial==UINT64_MAX){sourceFail(error,"Background source job admission is active, held or exhausted.");return 0;}
 jobs->token=++jobs->serial;jobs->invalidated=0;return jobs->token;
}
int xideSourceJobsState(const xide_source_jobs* jobs,uint64_t token,xide_source_jobs_status* out)
{
 if(out)*out=(xide_source_jobs_status){0};
 if(!jobs || !out || !token || jobs->token!=token)return 0;
 int quiet=!jobs->admissions;
 for(const xide_source_job* job=jobs->first;job && quiet;job=job->next)quiet=xideSourceJobStopped(job);
 *out=(xide_source_jobs_status){quiet,jobs->invalidated,jobs->count};return 1;
}
int xideSourceJobsResume(xide_source_jobs* jobs,uint64_t token,xide_model_error* error)
{
 if(!jobs || !token || jobs->token!=token)return sourceFail(error,"Background source job lease is stale or belongs to another owner.");
 jobs->token=0;jobs->invalidated=0;return 1;
}
