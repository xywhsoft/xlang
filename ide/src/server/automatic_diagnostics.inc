/* Protocol-owner scheduling; all report merging and input I/O stay in the
   diagnostic actor. One in-flight operation and one captured result bound
   ownership independently of the number of edited documents. */
enum { DIAGNOSTIC_INVALIDATE=1,DIAGNOSTIC_PUT,DIAGNOSTIC_SWEEP,DIAGNOSTIC_SNAPSHOT,DIAGNOSTIC_CLEAR };
static void diagnosticSchedule(server* s,int index,unsigned delay)
{
 if(!s->automaticDiagnostics || s->shutdown || index<0 || index>=DOC_LIMIT || !s->docs[index].uri) return;
 if(!s->diagnosticDue[index]) s->diagnosticOrder[index]=++s->diagnosticNextOrder;
 uint64_t due=GetTickCount64()+delay; s->diagnosticDue[index]=due ? due : 1; s->diagnosticFailed[index]=0;
}
static void diagnosticLog(server* s,const char* reason)
{
 const char* message=reason && *reason && xrtUtf8Valid(sv(reason),NULL) ? reason : "Automatic xlang diagnostics failed.";
 size_t size=strlen(message); if(size>=sizeof(s->diagnosticError)) { size=sizeof(s->diagnosticError)-1; while(size && ((unsigned char)message[size]&0xc0)==0x80) --size; }
 memcpy(s->diagnosticError,message,size); s->diagnosticError[size]=0;
 uint64_t now=GetTickCount64(); if(s->diagnosticLogAt && now-s->diagnosticLogAt<1000) return; s->diagnosticLogAt=now;
 xvalue *out=xrtValueObject(),*params=xrtValueObject();
 put(out,"jsonrpc",xrtValueString(sv("2.0"))); put(out,"method",xrtValueString(sv("window/logMessage")));
 put(params,"type",xrtValueInt(1)); put(params,"message",xrtValueString(sv(s->diagnosticError))); put(out,"params",params); sendMessage(s,out);
 fprintf(stderr,"xlang-ls automatic diagnostics: %s\n",s->diagnosticError);
}
static void diagnosticDropResult(server* s)
{ xrtValueRelease(s->diagnosticResult); s->diagnosticResult=NULL; xideLanguageInputGuardRelease(s->diagnosticProof); s->diagnosticProof=NULL; }
static void diagnosticMap(server* s,const xide_diagnostic_publication* publication,size_t* indices)
{ for(size_t i=0;i<DOC_LIMIT;++i) indices[i]=s->docs[i].uri ? xideDiagnosticPublicationFind(publication,s->docs[i].path) : SIZE_MAX; }
static void diagnosticEmit(server* s,const xide_diagnostic_publication* publication,size_t index,const size_t* indices,int clear)
{
 const xvalue* group=xideDiagnosticPublicationGroup(publication,index); const char* path=string(get(group,"path"));
 const char* uri=NULL; char* ownedUri=NULL; const xvalue* version=clear ? NULL : get(group,"version"); xvalue* ownedVersion=NULL;
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri && indices[i]==index) { uri=s->docs[i].uri; if(clear) { ownedVersion=xrtValueInt(s->docs[i].version); version=ownedVersion; } break; }
 if(!uri) { ownedUri=path ? xideLspPathToUri(path,NULL) : NULL; uri=ownedUri; }
 xvalue *out=xrtValueObject(),*params=xrtValueObject(); int okay=uri && out && params;
 if(okay) okay=put(out,"jsonrpc",xrtValueString(sv("2.0"))) && put(out,"method",xrtValueString(sv("textDocument/publishDiagnostics"))) && put(params,"uri",xrtValueString(sv(uri)));
 if(okay && s->diagnosticVersions && version) okay=put(params,"version",xrtValueDeepClone(version));
 if(okay) okay=clear ? put(params,"diagnostics",xrtValueArray()) : xrtValueObjectSet(params,sv("diagnostics"),get(group,"diagnostics"));
 if(okay) { okay=put(out,"params",params); params=NULL; }
 xrtValueRelease(params); xrtValueRelease(ownedVersion); free(ownedUri);
 if(okay) sendMessage(s,out); else { xrtValueRelease(out); s->writeFailed=1; }
}
static void diagnosticDiscardPublication(server* s,int clear)
{
 if(clear && s->diagnosticPublication) {
  size_t indices[DOC_LIMIT]; diagnosticMap(s,s->diagnosticPublication,indices);
  for(size_t i=0;i<xideDiagnosticPublicationCount(s->diagnosticPublication);++i) diagnosticEmit(s,s->diagnosticPublication,i,indices,1);
 }
 xideDiagnosticPublicationDestroy(s->diagnosticPublication); s->diagnosticPublication=NULL;
}
static void diagnosticResetState(server* s,const char* reason)
{
 for(size_t i=0;i<s->diagnosticPathCount;++i) free(s->diagnosticPaths[i]);
 s->diagnosticPathCount=s->diagnosticPathBytes=0; diagnosticDropResult(s); diagnosticDiscardPublication(s,1);
 s->diagnosticReset=1; s->diagnosticNeedsSnapshot=1; s->diagnosticBindingsChanged=1;
 for(int i=0;i<DOC_LIMIT;++i) diagnosticSchedule(s,i,(unsigned)s->diagnosticDelay);
 if(s->diagnosticQuery) xideDiagnosticQueryCancel(s->diagnosticQuery);
 if(reason) diagnosticLog(s,reason);
}
static void diagnosticCheckFailed(server* s,int index,int code,const char* reason)
{
 if(!s->automaticDiagnostics || s->shutdown || index<0 || index>=DOC_LIMIT || !s->docs[index].uri) return;
 if(code==-32800 || code==-32801) { diagnosticSchedule(s,index,(unsigned)s->diagnosticDelay); return; }
 s->diagnosticFailed[index]=1; s->diagnosticDue[index]=0; diagnosticLog(s,reason);
}
static void diagnosticInput(server* s,int index,const char* path,int bindingsChanged)
{
 if(!s->automaticDiagnostics || s->shutdown) return;
 s->diagnosticBindingsChanged|=bindingsChanged; s->diagnosticNeedsSnapshot=1;
 if(s->diagnosticResult) {
  int root=s->diagnosticResultRoot;
  if(s->docs[root].uri && s->docs[root].identity==s->diagnosticResultIdentity) diagnosticSchedule(s,root,(unsigned)s->diagnosticDelay);
  diagnosticDropResult(s);
 }
 if(s->check && s->checkForDiagnostics) diagnosticSchedule(s,s->checkDocument,(unsigned)s->diagnosticDelay);
 for(int i=0;i<DOC_LIMIT;++i) if(s->diagnosticFailed[i]) diagnosticSchedule(s,i,(unsigned)s->diagnosticDelay);
 diagnosticSchedule(s,index,(unsigned)s->diagnosticDelay);
 if(s->diagnosticReset) return;
 for(size_t i=0;i<s->diagnosticPathCount;++i) if(!strcmp(s->diagnosticPaths[i],path)) return;
 size_t bytes=strlen(path)+1;
 if(s->diagnosticPathCount==INVALIDATION_LIMIT || bytes>1024u*1024u-s->diagnosticPathBytes) { diagnosticResetState(s,"Automatic diagnostic invalidations exceeded their bound; resynchronizing open documents."); return; }
 char* captured=copy(path); if(!captured) { diagnosticResetState(s,"Unable to retain diagnostic input changes."); return; }
 s->diagnosticPaths[s->diagnosticPathCount++]=captured; s->diagnosticPathBytes+=bytes;
}
static int diagnosticCapture(server* s,int root,xide_language_check* check,xvalue** result,int keepOriginal)
{
 xide_model_error error={0}; xide_language_input_guard* proof=xideLanguageCheckCaptureInputGuard(check,&error);
 if(!proof) { diagnosticCheckFailed(s,root,-32603,error.message); return 0; }
 xvalue* owned=keepOriginal ? xrtValueDeepClone(*result) : *result;
 if(!owned) { xideLanguageInputGuardRelease(proof); diagnosticCheckFailed(s,root,-32603,"Unable to retain checked diagnostics."); return 0; }
 if(!keepOriginal) *result=NULL;
 if(s->diagnosticResult) {
  int previous=s->diagnosticResultRoot;
  if(s->docs[previous].uri && s->docs[previous].identity==s->diagnosticResultIdentity) diagnosticSchedule(s,previous,(unsigned)s->diagnosticDelay);
  diagnosticDropResult(s);
 }
 xvalue* analysis=xrtValueDeepClone(get(owned,"analysis"));
 if(analysis) { xrtValueRelease(s->diagnosticAnalysis); s->diagnosticAnalysis=analysis; }
 s->diagnosticResult=owned; s->diagnosticProof=proof; s->diagnosticResultRoot=root;
 s->diagnosticResultIdentity=s->docs[root].identity; s->diagnosticResultGeneration=s->generation;
 s->diagnosticDue[root]=0; s->diagnosticFailed[root]=0; return 1;
}
static int diagnosticPublish(server* s,xide_diagnostic_publication* next)
{
 size_t oldIndices[DOC_LIMIT],newIndices[DOC_LIMIT]; diagnosticMap(s,s->diagnosticPublication,oldIndices); diagnosticMap(s,next,newIndices);
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri && newIndices[i]!=SIZE_MAX) {
  int version=0; const xvalue* group=xideDiagnosticPublicationGroup(next,newIndices[i]);
  if(!integer(get(group,"version"),&version) || version!=s->docs[i].version) return 0;
 }
 size_t oldCount=xideDiagnosticPublicationCount(s->diagnosticPublication),newCount=xideDiagnosticPublicationCount(next),a=0,b=0;
 while(a<oldCount || b<newCount) {
  int order=a==oldCount ? 1 : b==newCount ? -1 : xideDiagnosticPublicationCompare(s->diagnosticPublication,a,next,b);
  if(order<0) diagnosticEmit(s,s->diagnosticPublication,a++,oldIndices,1);
  else if(order>0) diagnosticEmit(s,next,b++,newIndices,0);
  else { if(s->diagnosticBindingsChanged || !xideDiagnosticPublicationEqual(s->diagnosticPublication,a,next,b)) diagnosticEmit(s,next,b,newIndices,0); ++a; ++b; }
 }
 xideDiagnosticPublicationDestroy(s->diagnosticPublication); s->diagnosticPublication=next; s->diagnosticBindingsChanged=0; s->diagnosticNeedsSnapshot=0; return 1;
}
static void diagnosticInvalidatedRoots(server* s,const xvalue* invalidated,int kind,unsigned long long replaced)
{
 if(xrtValueCount(invalidated)) for(int i=0;i<DOC_LIMIT;++i) if(s->diagnosticFailed[i]) diagnosticSchedule(s,i,(unsigned)s->diagnosticDelay);
 for(size_t i=0;i<xrtValueCount(invalidated);++i) {
  const char* key=string(xrtValueArrayGet(invalidated,i)); char* end=NULL; unsigned long long identity=key ? strtoull(key,&end,10) : 0;
  if(!identity || !end || *end || (kind==DIAGNOSTIC_PUT && identity==replaced)) continue;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri && s->docs[j].identity==identity) {
   if(s->diagnosticResult && s->diagnosticResultRoot==j && s->diagnosticResultGeneration==s->generation) break;
   if(s->check && s->checkForDiagnostics && s->checkDocument==j && s->checkBatchGeneration==s->generation) break;
   diagnosticSchedule(s,j,(unsigned)s->diagnosticDelay); break;
  }
 }
}
static void diagnosticPoll(server* s)
{
 if(!s->automaticDiagnostics || s->shutdown) return;
 uint64_t now=GetTickCount64();
 EnterCriticalSection(&s->lock); int incoming=s->queueInputs!=0; LeaveCriticalSection(&s->lock);
 if(s->diagnosticQuery) {
  xide_diagnostic_snapshot result={0}; enum xide_diagnostic_state state=xideDiagnosticQueryResult(s->diagnosticQuery,&result);
  if(state!=XIDE_DIAGNOSTIC_PENDING) {
   int kind=s->diagnosticKind; unsigned long long generation=s->diagnosticJobGeneration,identity=s->diagnosticJobIdentity; int root=s->diagnosticRoot;
   if(kind==DIAGNOSTIC_SWEEP) s->diagnosticSweepAt=now+1000;
   if(state==XIDE_DIAGNOSTIC_READY) {
    xide_diagnostic_publication* publication=NULL; xvalue* invalidated=NULL;
    s->diagnosticRecords=result.records; s->diagnosticWorkerThread=result.workerThreadId; ++s->diagnosticOperations;
    if(xideDiagnosticQueryTake(s->diagnosticQuery,&publication,&invalidated)) {
     diagnosticInvalidatedRoots(s,invalidated,kind,identity);
     if(kind==DIAGNOSTIC_CLEAR) s->diagnosticReset=0;
     if(generation==s->generation && !s->diagnosticPathCount && !s->diagnosticReset && !incoming) {
      if(diagnosticPublish(s,publication)) publication=NULL;
      else { diagnosticResetState(s,"Diagnostic publication no longer matches open document versions."); s->diagnosticRetryAt=now+1000; }
     } else s->diagnosticNeedsSnapshot=1;
     xideDiagnosticPublicationDestroy(publication); xrtValueRelease(invalidated);
    }
   } else if(state==XIDE_DIAGNOSTIC_FAILED) {
    if(result.conflictPath) {
     ++s->generation; if(s->check && s->snapshotMode) cancel(s,-32801,"Observed diagnostic inputs changed.");
     cancelCompletion(s,-32801,"Observed diagnostic inputs changed.",1); clearPublished(s);
     diagnosticInput(s,-1,result.conflictPath,0);
     if(root>=0 && s->docs[root].uri && s->docs[root].identity==identity) diagnosticSchedule(s,root,(unsigned)s->diagnosticDelay);
    } else if(kind==DIAGNOSTIC_PUT && root>=0 && s->docs[root].uri && s->docs[root].identity==identity) { diagnosticCheckFailed(s,root,-32603,result.error); s->diagnosticNeedsSnapshot=1; }
    else { diagnosticResetState(s,result.error); s->diagnosticRetryAt=now+1000; }
   }
   xideDiagnosticQueryDestroy(s->diagnosticQuery); s->diagnosticQuery=NULL;
  }
 }
 if(!s->diagnosticQuery && now>=s->diagnosticRetryAt) {
  xide_diagnostic_query* query=NULL; int kind=0,root=-1; unsigned long long identity=0;
  if(s->diagnosticReset) { kind=DIAGNOSTIC_CLEAR; query=xideDiagnosticStoreClear(s->diagnostics); }
  else if(s->diagnosticPathCount) {
   kind=DIAGNOSTIC_INVALIDATE; char* path=s->diagnosticPaths[0]; query=xideDiagnosticStoreInvalidate(s->diagnostics,path);
   s->diagnosticPathBytes-=strlen(path)+1; free(path); --s->diagnosticPathCount;
   memmove(s->diagnosticPaths,s->diagnosticPaths+1,s->diagnosticPathCount*sizeof(*s->diagnosticPaths));
  } else if(s->diagnosticResult) {
   root=s->diagnosticResultRoot; identity=s->diagnosticResultIdentity;
   if(s->docs[root].uri && s->docs[root].identity==identity && s->diagnosticResultGeneration==s->generation) {
    char key[32]; snprintf(key,sizeof(key),"%llu",identity); kind=DIAGNOSTIC_PUT;
    xvalue* owned=s->diagnosticResult; s->diagnosticResult=NULL; query=xideDiagnosticStorePutTake(s->diagnostics,key,s->diagnosticProof,owned);
   } else if(s->docs[root].uri) diagnosticSchedule(s,root,(unsigned)s->diagnosticDelay);
   diagnosticDropResult(s);
  } else if(s->diagnosticNeedsSnapshot) { kind=DIAGNOSTIC_SNAPSHOT; query=xideDiagnosticStoreSnapshot(s->diagnostics); }
  else if(now>=s->diagnosticSweepAt) { kind=DIAGNOSTIC_SWEEP; query=xideDiagnosticStoreSweep(s->diagnostics); }
  if(kind) {
   if(!query || !xideDiagnosticQuerySetWake(query,nativeWake,s)) {
    xideDiagnosticQueryCancel(query); xideDiagnosticQueryDestroy(query); diagnosticResetState(s,"Unable to submit automatic diagnostic work."); s->diagnosticRetryAt=now+1000;
   } else { s->diagnosticQuery=query; s->diagnosticKind=kind; s->diagnosticRoot=root; s->diagnosticJobIdentity=identity; s->diagnosticJobGeneration=s->generation; }
  }
 }
 if(!incoming && !s->check && !s->diagnosticResult && !s->diagnosticReset && !s->diagnosticPathCount && now>=s->diagnosticRetryAt) {
  int next=-1;
  for(int i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri && s->diagnosticDue[i] && now>=s->diagnosticDue[i] && (next<0 || s->diagnosticOrder[i]<s->diagnosticOrder[next])) next=i;
  if(next>=0) {
   s->diagnosticDue[next]=0; startCheck(s,next,NULL,1);
   if(s->check) s->checkForDiagnostics=1;
   else diagnosticCheckFailed(s,next,-32603,"Unable to start automatic source analysis.");
  }
 }
}
static void diagnosticStop(server* s)
{
 if(s->diagnosticQuery) { xideDiagnosticQueryCancel(s->diagnosticQuery); xideDiagnosticQueryDestroy(s->diagnosticQuery); s->diagnosticQuery=NULL; }
 for(size_t i=0;i<s->diagnosticPathCount;++i) free(s->diagnosticPaths[i]);
 s->diagnosticPathCount=s->diagnosticPathBytes=0; diagnosticDropResult(s); diagnosticDiscardPublication(s,0);
 memset(s->diagnosticDue,0,sizeof(s->diagnosticDue));
}
static void diagnosticWatched(server* s,const xvalue* params)
{
 if(!s->automaticDiagnostics) return;
 const xvalue* changes=get(params,"changes");
 if(!changes || xrtValueType(changes)!=XVALUE_ARRAY) { fputs("xlang-ls: rejected watched file changes.\n",stderr); return; }
 size_t count=xrtValueCount(changes); if(!count) return;
 if(count>INVALIDATION_LIMIT) {
  ++s->generation; if(s->check && s->snapshotMode) cancel(s,-32801,"Watched input set changed.");
  cancelCompletion(s,-32801,"Watched input set changed.",1); clearPublished(s);
  diagnosticResetState(s,"Watched file changes exceeded their bound; resynchronizing open documents."); return;
 }
 char* paths[INVALIDATION_LIMIT]={0}; size_t bytes=0; int okay=1;
 for(size_t i=0;i<count && okay;++i) {
  const xvalue* change=xrtValueArrayGet(changes,i); const char* uri=string(get(change,"uri")); int type=0;
  paths[i]=uri ? xideLspUriToPath(uri,NULL) : NULL;
  okay=paths[i] && integer(get(change,"type"),&type) && type>=1 && type<=3;
  if(okay) { size_t size=strlen(paths[i])+1; okay=size<=1024u*1024u-bytes; if(okay) bytes+=size; }
 }
 if(okay) {
  ++s->generation; if(s->check && s->snapshotMode) cancel(s,-32801,"Watched input set changed.");
  cancelCompletion(s,-32801,"Watched input set changed.",1); clearPublished(s);
  for(size_t i=0;i<count;++i) diagnosticInput(s,-1,paths[i],0);
 } else fputs("xlang-ls: rejected watched file changes.\n",stderr);
 for(size_t i=0;i<count;++i) free(paths[i]);
}
static void diagnosticStats(server* s,const xvalue* id)
{
 xide_diagnostic_load load={0}; xideDiagnosticStoreLoad(s->diagnostics,&load); size_t dirty=0,failed=0;
 for(size_t i=0;i<DOC_LIMIT;++i) { if(s->docs[i].uri && s->diagnosticDue[i]) ++dirty; if(s->docs[i].uri && s->diagnosticFailed[i]) ++failed; }
 xvalue* result=xrtValueObject(); put(result,"enabled",xrtValueBool(s->automaticDiagnostics)); put(result,"pendingInputs",xrtValueInt((int64)s->diagnosticPathCount));
 put(result,"dirtyRoots",xrtValueInt((int64)dirty)); put(result,"failedRoots",xrtValueInt((int64)failed)); put(result,"pendingReport",xrtValueBool(s->diagnosticResult!=NULL));
 put(result,"checking",xrtValueBool(s->check && s->checkForDiagnostics)); put(result,"observing",xrtValueBool(s->diagnosticQuery!=NULL));
 put(result,"submittedChecks",xrtValueInt((int64)s->diagnosticChecks)); put(result,"completedOperations",xrtValueInt((int64)s->diagnosticOperations)); put(result,"retainedRoots",xrtValueInt((int64)s->diagnosticRecords));
 put(result,"publishedFiles",xrtValueInt((int64)xideDiagnosticPublicationCount(s->diagnosticPublication))); put(result,"workerThreadId",xrtValueInt((int64)s->diagnosticWorkerThread)); put(result,"ownerThreadId",xrtValueInt((int64)xrtThreadCurrentId()));
 put(result,"queued",xrtValueInt((int64)load.queued)); put(result,"running",xrtValueInt((int64)load.running)); put(result,"error",xrtValueString(sv(s->diagnosticError)));
 if(s->diagnosticAnalysis) put(result,"lastAnalysis",xrtValueDeepClone(s->diagnosticAnalysis));
 response(s,id,result,0,NULL);
}
