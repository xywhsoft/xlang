/* Owner-serialized source-reader lease. No actor is destroyed/recreated.
 * Transport input and held notifications share the existing 256/64MiB bound.
 * Only an exact, never-reused positive connection-local token can resume. */
static int analysisLeaseQuiet(server* s)
{
 if(s->check || s->guard || s->completion || s->signature)return 0;
#ifdef XLS_COMPILER_XRT
 collectTokenJobs(s);
 for(size_t i=0;i<4;++i)if(s->tokenJobs[i])return 0;
 if(!xideProjectReferencesIdle(s->references) || !xideWorkspaceSymbolsIdle(s->workspaceSymbols))return 0;
#endif
 return xlsWorkerIdle(s->analysis) && xlsWorkerIdle(s->semanticAnalysis) &&
  xlsWorkerIdle(s->cursorAnalysis) && xlsWorkerIdle(s->signatureAnalysis) && xideDiagnosticStoreIdle(s->diagnostics);
}
static xvalue* analysisLeaseState(server* s)
{
 xvalue* out=xrtValueObject();char serial[32];snprintf(serial,sizeof(serial),"%llu",s->inputSerial);
 if(!out || !put(out,"token",xrtValueInt(s->analysisLeaseToken)) ||
    !put(out,"held",xrtValueBool(s->analysisLeaseHeld!=0)) ||
    !put(out,"invalidated",xrtValueBool(s->analysisLeaseInvalidated!=0)) ||
    !put(out,"deferred",xrtValueInt((int64)s->deferredCount)) ||
    !put(out,"inputSerial",xrtValueString(sv(serial))) || !put(out,"epoch",xrtValueInt(s->configurationEpoch))){xrtValueRelease(out);return NULL;}
 return out;
}
static void analysisLeasePoll(server* s)
{
 if(!s->analysisLeaseToken || s->analysisLeaseHeld || !s->analysisLeaseId || s->shutdown)return;
 if(!analysisLeaseQuiet(s))return;
 s->analysisLeaseHeld=1;xvalue* out=analysisLeaseState(s);
 /* An unsuccessful reply never releases the lease: a disconnected or failed
    holder cannot accidentally authorize other readers during source writes. */
 response(s,s->analysisLeaseId,out,out?0:-32603,out?NULL:"Cannot encode analysis lease acknowledgement.");
 xrtValueRelease(s->analysisLeaseId);s->analysisLeaseId=NULL;
}
static void analysisLeaseInvalidate(server* s)
{
 if(s->analysisLeaseInvalidated)return;
 s->analysisLeaseInvalidated=1;
 xvalue* out=xrtValueObject();xvalue* state=analysisLeaseState(s);
 if(!out || !state || !put(out,"jsonrpc",xrtValueString(sv("2.0"))) ||
    !put(out,"method",xrtValueString(sv("xlang/analysisLeaseInvalidated")))){xrtValueRelease(out);xrtValueRelease(state);s->writeFailed=1;return;}
 if(!put(out,"params",state)){xrtValueRelease(out);s->writeFailed=1;return;}
 sendMessage(s,out);
}
static message* analysisLeaseNext(server* s,int* deferred)
{
 *deferred=0;EnterCriticalSection(&s->lock);message* m=s->head;
 if(m){
  s->head=m->next;if(!s->head)s->tail=NULL;m->next=NULL;
  /* Requests are refused while held. Only input notifications are retained;
     this also avoids inventing cancellation semantics for deferred requests. */
  const char* method=string(get(m->value,"method"));
  if(s->analysisLeaseToken && !get(m->value,"id") && (m->input || (method && !strcmp(method,"initialized")))){
   if(s->deferredTail)s->deferredTail->next=m;else s->deferredHead=m;
   s->deferredTail=m;++s->deferredCount;*deferred=1;m=NULL;
  }else{s->queueBytes-=m->size;--s->queueCount;s->queueInputs-=(size_t)m->input;}
 }
 LeaveCriticalSection(&s->lock);return m;
}
static void analysisLeaseRelease(server* s)
{
 /* Splice before later requests without recapturing/cloning any JSON. */
 EnterCriticalSection(&s->lock);
 if(s->deferredHead){s->deferredTail->next=s->head;s->head=s->deferredHead;if(!s->tail)s->tail=s->deferredTail;}
 s->deferredHead=s->deferredTail=NULL;s->deferredCount=0;
 LeaveCriticalSection(&s->lock);
 s->analysisLeaseToken=0;s->analysisLeaseHeld=s->analysisLeaseInvalidated=0;
 if(s->analysisLeaseId){response(s,s->analysisLeaseId,NULL,-32800,"Analysis suspension abandoned before acknowledgement.");xrtValueRelease(s->analysisLeaseId);s->analysisLeaseId=NULL;}
 /* Keep cached records; demand fresh validation/publication after the host's
    source operation, even when it changed only unopened files. */
 for(int i=0;i<DOC_LIMIT;++i)if(s->docs[i].uri && s->automaticDiagnostics){s->diagnosticDue[i]=GetTickCount64()+1;s->diagnosticFailed[i]=0;}
 s->diagnosticSweepAt=GetTickCount64()+1;
#ifdef XLS_COMPILER_XRT
 s->workspaceRefresh.seen=0;
#endif
}
static int analysisLeaseDispatch(server* s,const char* method,const xvalue* id,const xvalue* params)
{
 int suspend=!strcmp(method,"xlang/suspendAnalysis"),resume=!strcmp(method,"xlang/resumeAnalysis");
 if(suspend || resume){
#ifndef XLS_COMPILER_XRT
  if(id)response(s,id,NULL,-32601,"Analysis leases require the matched native XRT runtime.");
  return 1;
#endif
  int64 token=0;
  if(!id || xrtValueType(params)!=XVALUE_OBJECT || xrtValueCount(params)!=1 || !xrtValueGetInt(get(params,"token"),&token) || token<=0){
   if(id)response(s,id,NULL,-32602,"An analysis lease request requires one positive integer token.");
   return 1;
  }
  if(resume){
   if(!s->analysisLeaseToken || token!=s->analysisLeaseToken)response(s,id,NULL,-32602,"Analysis lease token is not current.");
   else{analysisLeaseRelease(s);response(s,id,xrtValueNull(),0,NULL);}return 1;
  }
  if(s->analysisLeaseToken){response(s,id,NULL,-32001,"An analysis lease is already pending or held.");return 1;}
  if(token<=s->analysisLeaseLast){response(s,id,NULL,-32602,"Analysis lease tokens must increase for this connection.");return 1;}
  xvalue* saved=xrtValueDeepClone(id);if(!saved){response(s,id,NULL,-32603,"Cannot retain analysis lease request.");return 1;}
  s->analysisLeaseToken=s->analysisLeaseLast=token;s->analysisLeaseId=saved;s->analysisLeaseHeld=s->analysisLeaseInvalidated=0;s->pending=-1;
#ifdef XLS_COMPILER_XRT
  cancelReferences(s,NULL,-32800,"Analysis suspended.");cancelWorkspaceSymbols(s,NULL,-32800,"Analysis suspended.");workspaceRefreshCancel(s,0);
  for(size_t i=0;i<4;++i)xideSemanticTokenJobCancel(s->tokenJobs[i]);
#endif
  diagnosticStop(s);cancelNative(s,-1,-32800,"Analysis suspended.");clearPublished(s);cancel(s,-32800,"Analysis suspended.");
  cancelSemantic(s,-1,-32800,"Analysis suspended.");cancelCompletion(s,-32800,"Analysis suspended.",1);cancelSignature(s,-32800,"Analysis suspended.",1);
  return 1;
 }
 if(!strcmp(method,"xlang/analysisLeaseState")){if(id){xvalue* out=analysisLeaseState(s);response(s,id,out,out?0:-32603,out?NULL:"Cannot encode analysis lease state.");}return 1;}
 if(!s->analysisLeaseToken)return 0;
 if(!strcmp(method,"$/cancelRequest") && !id){
  /* Cancellation can abandon only an unacknowledged request. After the reply
     the holder may already be writing: stale cancellation must do nothing. */
  if(!s->analysisLeaseHeld && s->analysisLeaseId && sameId(s->analysisLeaseId,get(params,"id")))analysisLeaseRelease(s);
  return 1;
 }
 if(!strcmp(method,"shutdown")){
  if(id && s->analysisLeaseId){response(s,s->analysisLeaseId,NULL,-32800,"Server shutting down before analysis suspension acknowledgement.");xrtValueRelease(s->analysisLeaseId);s->analysisLeaseId=NULL;}
  return 0;
 }
 /* These only serialize immutable owner state/counters and do not submit
    actor work, validate source proofs or resolve project/configuration files. */
 if(!strcmp(method,"xlang/inputState") || !strcmp(method,"xlang/analysisStats") || !strcmp(method,"xlang/diagnosticStats") ||
    !strcmp(method,"xlang/workspaceSymbolStats") || !strcmp(method,"xlang/referenceStats"))return 0;
 if(id)response(s,id,NULL,-32001,"Analysis is suspended; retry after the lease resumes.");
 return 1;
}
