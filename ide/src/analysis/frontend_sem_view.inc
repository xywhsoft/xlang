/* Private immutable function facts/coordinates. XRT owns the containers and
 * allocation; this record supplies the independent lifetime needed by a
 * demand view, without retaining a report, source text, or an earlier result. */
#define XLS_SEM_BODY_BYTES (64u*1024u*1024u)
static _Thread_local xls_sem_body_control* xlsActiveBodyControl;
static size_t xlsSemBodyBudget(void)
{
    size_t bytes=xlsActiveBodyControl ? xlsActiveBodyControl->bytes : XLS_SEM_BODY_BYTES;
    return bytes<XLS_SEM_BODY_BYTES ? bytes : XLS_SEM_BODY_BYTES;
}
void xlsFrontendSemBodyBegin(xls_sem_body_control* control,size_t bytes)
{
    if(!control)return;
    size_t available=xlsSemBodyBudget();if(bytes>available)bytes=available;
    *control=(xls_sem_body_control){xlsActiveBodyControl,bytes};xlsActiveBodyControl=control;
}
void xlsFrontendSemBodyEnd(xls_sem_body_control* control)
{if(control && xlsActiveBodyControl==control)xlsActiveBodyControl=control->previous;}
static xls_sem_view* xlsSemViewRetain(xls_sem_view* view)
{if(view)xrtAtomic32FetchAdd(&view->references,1,XMEMORY_RELAXED);return view;}
static void xlsSemViewRelease(xls_sem_view* view)
{
    if(view && xrtAtomic32FetchSub(&view->references,1,XMEMORY_ACQ_REL)==1){
        xlsSemUnitRelease(view->unit);xrtArrayUnit(&view->positions);xrtFree(view);
    }
}
static xls_sem_view* xlsSemViewCreate(xls_sem_unit* unit,xarray* positions,size_t limit)
{
    if(!unit || !positions || sizeof(xls_sem_view)>limit || unit->bytes>limit-sizeof(xls_sem_view) ||
       positions->Capacity>(limit-sizeof(xls_sem_view)-unit->bytes)/sizeof(xls_sem_position))return NULL;
    xls_sem_view* view=xrtCalloc(1,sizeof(*view));if(!view)return NULL;
    xrtAtomic32Init(&view->references,1);view->unit=xlsSemUnitRetain(unit);
    view->positions=*positions;*positions=(xarray){0};
    view->bytes=sizeof(*view)+unit->bytes+view->positions.Capacity*sizeof(xls_sem_position);
    return view;
}
static void xlsSemEntryFactsClear(xls_sem_entry* entry)
{
    if(entry->view)xlsSemViewRelease(entry->view);
    else{xlsSemUnitRelease(entry->unit);xrtArrayUnit(&entry->positions);}
    entry->view=NULL;entry->unit=NULL;entry->positions=(xarray){0};
}
static void xlsSemBodyJobsFree(xarray* jobs)
{
    for(size_t i=0;i<jobs->Count;++i)xlsSemViewRelease(((xls_sem_body_job*)xrtArrayGet(jobs,i))->view);
    xrtArrayUnit(jobs);
}
static int xlsSemBodyJobsReserve(xls_sem_build* build,size_t limit)
{
    xarray* jobs=&build->bodyJobs;
    if(build->bodyBytes>limit || jobs->Count==SIZE_MAX)return 0;
    if(jobs->Count<jobs->Capacity)return jobs->Capacity<=(limit-build->bodyBytes)/sizeof(xls_sem_body_job);
    /* Keep the old admitted jobs untouched if geometric XRT growth would
     * exceed their quota. A failed optional admission must not retain an
     * oversized replacement capacity. Element ownership moves only on commit. */
    xarray next={0};
    if(!xrtArrayInit(&next,sizeof(xls_sem_body_job)) || !xrtArrayReserve(&next,jobs->Count+1) ||
       next.Capacity>(limit-build->bodyBytes)/sizeof(xls_sem_body_job) || !xrtArrayResize(&next,jobs->Count)){
        xrtArrayUnit(&next);return 0;
    }
    if(jobs->Count)memcpy(next.Data,jobs->Data,jobs->Count*sizeof(xls_sem_body_job));
    xrtArrayUnit(jobs);*jobs=next;return 1;
}
