/* Idle maintenance shares the guarded index actor. The owner only captures
 * bounded metadata/leases; no declaration traversal, disk reads or body hash. */
#define WORKSPACE_REFRESH_DELAY 500u
static void workspaceRefreshIncrement(uint64_t* value){if(*value<INT64_MAX)++*value;}
static void workspaceRefreshCancel(server* s,int reschedule)
{
 if(s->workspaceRefresh.query){xideWorkspaceSymbolsCancel(s->workspaceRefresh.query);xideWorkspaceSymbolsQueryDestroy(s->workspaceRefresh.query);s->workspaceRefresh.query=NULL;workspaceRefreshIncrement(&s->workspaceRefresh.cancelled);if(reschedule)s->workspaceRefresh.pending=1;}
 if(reschedule)s->workspaceRefresh.due=GetTickCount64()+WORKSPACE_REFRESH_DELAY;
 else{s->workspaceRefresh.pending=0;s->workspaceRefresh.due=0;}
}
static void workspaceRefreshSatisfied(server* s)
{
 s->workspaceRefresh.seen=1;s->workspaceRefresh.seenSerial=s->inputSerial;s->workspaceRefresh.seenEpoch=s->configurationEpoch;s->workspaceRefresh.pending=0;s->workspaceRefresh.due=0;
}
static void workspaceRefreshActivity(server* s,const char* method,const xvalue* id)
{
 if(!id || !strcmp(method,"initialize") || !strcmp(method,"xlang/workspaceSymbolStats") || !strcmp(method,"xlang/referenceStats") || !strcmp(method,"xlang/analysisStats") || !strcmp(method,"xlang/diagnosticStats") || !strcmp(method,"xlang/inputState"))return;
 workspaceRefreshCancel(s,1);
}
static int workspaceRefreshBusy(server* s)
{
 EnterCriticalSection(&s->lock);size_t waiting=s->queueCount;LeaveCriticalSection(&s->lock);
 if(waiting || s->pending>=0 || s->completionPendingId || s->signaturePendingId || s->diagnosticQuery || s->diagnosticReset || s->diagnosticPathCount)return 1;
 xide_language_check* checks[]={s->check,s->guard,s->completion,s->signature};
 for(size_t i=0;i<sizeof(checks)/sizeof(*checks);++i)if(checks[i] && xideLanguageCheckState(checks[i],NULL,0)==XIDE_CHECK_RUNNING)return 1;
 for(size_t i=0;i<DOC_LIMIT;++i)if(s->native[i].query || s->semantic[i].id || s->diagnosticDue[i])return 1;
 for(size_t i=0;i<8;++i)if(s->referenceRequests[i].query || s->workspaceSymbolRequests[i].query)return 1;
 return 0;
}
static void workspaceRefreshFailed(server* s,const char* error)
{
 workspaceRefreshIncrement(&s->workspaceRefresh.failed);if(!error || !*error || !xrtUtf8Valid(sv(error),NULL))error="Workspace background refresh failed.";
 size_t n=strlen(error);if(n>=sizeof(s->workspaceRefresh.error)){n=sizeof(s->workspaceRefresh.error)-1;while(n && ((unsigned char)error[n]&0xc0u)==0x80u)--n;}memcpy(s->workspaceRefresh.error,error,n);s->workspaceRefresh.error[n]=0;
}
static void pollWorkspaceRefresh(server* s)
{
 if(!s->workspaceRefresh.enabled || !s->workspaceRefresh.ready || s->shutdown){workspaceRefreshCancel(s,0);return;}
 if(!s->workspaceRefresh.seen || s->workspaceRefresh.seenSerial!=s->inputSerial || s->workspaceRefresh.seenEpoch!=s->configurationEpoch){
  workspaceRefreshCancel(s,1);s->workspaceRefresh.seen=1;s->workspaceRefresh.seenSerial=s->inputSerial;s->workspaceRefresh.seenEpoch=s->configurationEpoch;s->workspaceRefresh.pending=1;workspaceRefreshIncrement(&s->workspaceRefresh.scheduled);
 }
 if(s->workspaceRefresh.query){
  if(workspaceRefreshBusy(s)){workspaceRefreshCancel(s,1);return;}
  char error[512]={0};xide_workspace_symbol_work work={0};enum xide_workspace_symbol_state state=xideWorkspaceSymbolsResult(s->workspaceRefresh.query,NULL,&work,error,sizeof(error));if(state==XIDE_WORKSPACE_SYMBOL_PENDING)return;
  /* Recheck the captured epochs even though scheduling observes every input.
     No successful old result may be attributed to the current cohort. */
  if(s->workspaceRefresh.serial!=s->inputSerial || s->workspaceRefresh.epoch!=s->configurationEpoch){workspaceRefreshCancel(s,1);return;}
  if(state==XIDE_WORKSPACE_SYMBOL_READY){s->workspaceRefresh.work=work;workspaceRefreshIncrement(&s->workspaceRefresh.completed);*s->workspaceRefresh.error=0;}
  else workspaceRefreshFailed(s,*error?error:"Workspace background refresh cancelled or unavailable.");
  xideWorkspaceSymbolsQueryDestroy(s->workspaceRefresh.query);s->workspaceRefresh.query=NULL;return;
 }
 if(!s->workspaceRefresh.pending || GetTickCount64()<s->workspaceRefresh.due || workspaceRefreshBusy(s))return;
 s->workspaceRefresh.pending=0;s->workspaceRefresh.due=0;
 xls_context* contexts[64]={0};size_t contextCount=0,documentCount=0;xls_analysis_document documents[DOC_LIMIT];int okay=captureWorkspaceInputs(s,contexts,&contextCount,documents,&documentCount);
 if(okay && (contextCount || documentCount || s->workspaceSymbols)){
  if(!s->workspaceSymbols)s->workspaceSymbols=xideWorkspaceSymbolsCreate(s->cache,s->compiler,s->modules);
  s->workspaceRefresh.query=xideWorkspaceSymbolsRefresh(s->workspaceSymbols,contexts,contextCount,documents,documentCount);
  if(s->workspaceRefresh.query && !xideWorkspaceSymbolsSetWake(s->workspaceRefresh.query,nativeWake,s)){workspaceRefreshCancel(s,0);okay=0;}
  else if(!s->workspaceRefresh.query)okay=0;
  if(okay){s->workspaceRefresh.serial=s->inputSerial;s->workspaceRefresh.epoch=s->configurationEpoch;workspaceRefreshIncrement(&s->workspaceRefresh.started);}
 }
 for(size_t i=0;i<contextCount;++i)xlsContextRelease(contexts[i]);
 /* Source/infrastructure failure gets one attempt per accepted input epoch.
    No timer retry loop;manual searches still use their normal fresh guards. */
 if(!okay)workspaceRefreshFailed(s,"Cannot prepare background workspace scope or submit its bounded task.");
}
static int workspaceRefreshStats(server* s,xvalue* out)
{
 xvalue* value=xrtValueObject();int okay=value && put(value,"enabled",xrtValueBool(s->workspaceRefresh.enabled!=0)) && put(value,"pending",xrtValueBool(s->workspaceRefresh.pending!=0)) && put(value,"running",xrtValueBool(s->workspaceRefresh.query!=NULL));
#define REFRESH_STAT(field) okay=okay && put(value,#field,xrtValueInt((int64)s->workspaceRefresh.field))
 REFRESH_STAT(scheduled);REFRESH_STAT(started);REFRESH_STAT(completed);REFRESH_STAT(cancelled);REFRESH_STAT(failed);REFRESH_STAT(epoch);
#undef REFRESH_STAT
 const xide_workspace_symbol_work* work=&s->workspaceRefresh.work;
#define REFRESH_WORK(field) okay=okay && put(value,#field,xrtValueInt((int64)work->field))
 REFRESH_WORK(roots);REFRESH_WORK(captured);REFRESH_WORK(reused);REFRESH_WORK(covered);REFRESH_WORK(analysisBuilds);REFRESH_WORK(analysisHits);REFRESH_WORK(index.rows);REFRESH_WORK(index.generation);REFRESH_WORK(coordinatorThread);
#undef REFRESH_WORK
 char serial[32];snprintf(serial,sizeof(serial),"%llu",s->workspaceRefresh.serial);okay=okay && put(value,"serial",xrtValueString(sv(serial))) && put(value,"lastError",xrtValueString(sv(s->workspaceRefresh.error)));
 if(okay){okay=put(out,"background",value);value=NULL;}xrtValueRelease(value);return okay;
}
#undef WORKSPACE_REFRESH_DELAY
