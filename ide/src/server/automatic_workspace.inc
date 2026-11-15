/* Root discovery owns all filesystem/manifest work on its existing XRT worker.
 * Owner work is a bounded document roster plus retained immutable metadata.
 * Explicit client epochs are never consumed by automatic catalogs (epoch0). */
#define DISCOVERY_WAIT_BYTES (1024u*1024u)
#define DISCOVERY_DEBOUNCE 75u
#define DISCOVERY_WATCH_ID "xlang:workspace-discovery:watch"
static void discoveryIncrement(uint64_t* value){if(*value<INT64_MAX)++*value;}
static int automaticWorkspaceMode(const server* s)
{return s->discovery.enabled && !s->discovery.explicitConfiguration && !s->cliCompletion && !s->cliDiagnostics;}
static int automaticWorkspaceBlocked(const server* s)
{return automaticWorkspaceMode(s) && s->renameWorkspaceDirty;}
static void automaticWorkspaceCancel(server* s,int reschedule)
{
 if(s->discovery.query){xideWorkspaceDiscoveryCancel(s->discovery.query);xideWorkspaceDiscoveryQueryDestroy(s->discovery.query);s->discovery.query=NULL;discoveryIncrement(&s->discovery.cancelled);}
 s->discovery.pending=reschedule!=0;if(reschedule)s->discovery.due=GetTickCount64()+DISCOVERY_DEBOUNCE;
}
static void discoveryWaitingClear(server* s,size_t i)
{s->discovery.waitingBytes-=s->discovery.waiting[i].bytes;xrtValueRelease(s->discovery.waiting[i].message);memset(s->discovery.waiting+i,0,sizeof(s->discovery.waiting[i]));}
static void automaticWorkspaceCancelRequests(server* s,const xvalue* id,int code,const char* reason)
{
 for(size_t i=0;i<16;++i){xvalue* message=s->discovery.waiting[i].message;if(message && (!id || sameId(id,get(message,"id")))){if(code)response(s,get(message,"id"),NULL,code,reason);discoveryWaitingClear(s,i);}}
}
static void automaticWorkspaceExplicit(server* s)
{
 s->discovery.explicitConfiguration=1;automaticWorkspaceCancel(s,0);
 s->discovery.failedState=0;*s->discovery.error=0;
 xideDiscoverySnapshotRelease(s->discovery.installed);s->discovery.installed=NULL;
 automaticWorkspaceCancelRequests(s,NULL,-32801,"An explicit analysis configuration replaced automatic discovery.");
}
static void automaticWorkspaceChanged(server* s,int inputChanged,const char* reason)
{
 if(!automaticWorkspaceMode(s))return;
 if(s->discovery.revision==INT64_MAX || (!inputChanged && (s->generation==ULLONG_MAX || s->inputSerial==ULLONG_MAX))){s->writeFailed=1;return;}
 ++s->discovery.revision;if(!inputChanged){++s->generation;++s->inputSerial;}s->pending=-1;
 xideDiscoverySnapshotRelease(s->discovery.installed);s->discovery.installed=NULL;
 automaticWorkspaceCancelRequests(s,NULL,-32801,reason);automaticWorkspaceCancel(s,1);s->discovery.failedState=0;*s->discovery.error=0;
 s->renameWorkspaceDirty=1;
 cancel(s,-32801,reason);cancelSemantic(s,-1,-32801,reason);cancelCompletion(s,-32801,reason,1);cancelSignature(s,-32801,reason,1);
 cancelReferences(s,NULL,-32801,reason);cancelRenames(s,NULL,-32801,reason);cancelWorkspaceSymbols(s,NULL,-32801,reason);workspaceRefreshCancel(s,0);clearPublished(s);
 if(s->automaticDiagnostics)diagnosticResetState(s,NULL);
}
static int discoveryContextMethod(const char* method)
{
 static const char* const names[]={"workspace/symbol","xlang/workspaceSymbolNavigation","textDocument/references","textDocument/prepareRename","textDocument/rename",XIDE_NATIVE_RENAME_METHOD,
  "textDocument/definition","textDocument/hover","textDocument/documentSymbol","textDocument/documentHighlight","textDocument/completion","textDocument/signatureHelp",
  "textDocument/semanticTokens/full","textDocument/semanticTokens/range","textDocument/semanticTokens/full/delta","xlang/check","xlang/checkSaved"};
 for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)if(!strcmp(method,names[i]))return 1;
 return 0;
}
static int automaticWorkspaceAwait(server* s,const xvalue* message,const char* method,const xvalue* id)
{
 if(!id || !automaticWorkspaceBlocked(s) || !discoveryContextMethod(method))return 0;
 size_t slot=16;for(size_t i=0;i<16;++i){xvalue* old=s->discovery.waiting[i].message;
  if(old && sameId(id,get(old,"id"))){automaticWorkspaceCancelRequests(s,id,-32600,"Request ID already waits for workspace discovery.");return 1;}if(!old && slot==16)slot=i;
 }
 size_t bytes=s->activeMessageBytes;
 if(slot==16 || !bytes || bytes>128u*1024u || bytes>DISCOVERY_WAIT_BYTES-s->discovery.waitingBytes){response(s,id,NULL,-32001,"Workspace discovery request queue is full.");return 1;}
 /* A retained DOM is immutable; no body/JSON copy or owner serialization. */
 s->discovery.waiting[slot].message=xrtValueRetain(message);s->discovery.waiting[slot].bytes=bytes;s->discovery.waitingBytes+=bytes;
 s->discovery.waiting[slot].serial=s->inputSerial;s->discovery.waiting[slot].folders=s->folderRevision;
 if(s->discovery.failedState){s->discovery.failedState=0;s->discovery.pending=1;s->discovery.due=GetTickCount64();}
 return 1;
}
static void discoveryFailed(server* s,const char* error)
{
 discoveryIncrement(&s->discovery.failed);s->discovery.failedState=1;s->discovery.pending=0;
 if(!error || !*error || !xrtUtf8Valid(sv(error),NULL))error="Automatic workspace discovery failed.";
 size_t n=strlen(error);if(n>=sizeof(s->discovery.error)){n=sizeof(s->discovery.error)-1;while(n && ((unsigned char)error[n]&0xc0u)==0x80u)--n;}
 memcpy(s->discovery.error,error,n);s->discovery.error[n]=0;
 automaticWorkspaceCancelRequests(s,NULL,-32803,s->discovery.error);diagnosticLog(s,s->discovery.error);
}
static void discoveryDrain(server* s,unsigned long long serial)
{
 for(size_t i=0;i<16;++i){xvalue* message=s->discovery.waiting[i].message;if(!message)continue;
  size_t bytes=s->discovery.waiting[i].bytes;int current=s->discovery.waiting[i].serial==serial && s->discovery.waiting[i].folders==s->folderRevision;
  message=xrtValueRetain(message);discoveryWaitingClear(s,i);
  if(current){size_t saved=s->activeMessageBytes;s->activeMessageBytes=bytes;dispatch(s,message);s->activeMessageBytes=saved;}
  else response(s,get(message,"id"),NULL,-32801,"Inputs changed while the request awaited workspace discovery.");
  xrtValueRelease(message);
 }
}
static void pollAutomaticWorkspace(server* s)
{
 if(!automaticWorkspaceMode(s) || !s->discovery.ready || s->shutdown || s->analysisLeaseToken){automaticWorkspaceCancel(s,automaticWorkspaceMode(s) && s->discovery.pending && !s->shutdown);return;}
 if(xideDiscoverySnapshotInvalidated(s->discovery.installed))automaticWorkspaceChanged(s,0,"Workspace discovery proof became stale.");
 EnterCriticalSection(&s->lock);size_t inputs=s->queueInputs;LeaveCriticalSection(&s->lock);if(inputs)return;
 if(s->discovery.query){
  if(s->discovery.jobRevision!=s->discovery.revision || s->discovery.jobFolders!=s->folderRevision){automaticWorkspaceCancel(s,1);return;}
  char error[512]={0};xide_discovery_work work={0};int state=xideWorkspaceDiscoveryResult(s->discovery.query,NULL,&work,error,sizeof(error));if(state==XIDE_DISCOVERY_PENDING)return;s->discovery.work=work;
  if(state!=XIDE_DISCOVERY_READY){xideWorkspaceDiscoveryQueryDestroy(s->discovery.query);s->discovery.query=NULL;discoveryFailed(s,*error?error:"Workspace discovery cancelled or unavailable.");return;}
  xide_discovery_snapshot* snapshot=xideWorkspaceDiscoveryTake(s->discovery.query);xideWorkspaceDiscoveryQueryDestroy(s->discovery.query);s->discovery.query=NULL;
  xide_model_error e={0};unsigned long long serial=s->inputSerial;
  int okay=snapshot && installAnalysis(s,xideDiscoverySnapshotCatalog(snapshot),xideDiscoverySnapshotValue(snapshot),0,&e);
  if(okay){xideDiscoverySnapshotRelease(s->discovery.installed);s->discovery.installed=xideDiscoverySnapshotRetain(snapshot);memcpy(s->discovery.fingerprint,xideDiscoverySnapshotFingerprint(snapshot),32);discoveryIncrement(&s->discovery.completed);*s->discovery.error=0;s->discovery.failedState=0;}
  xideDiscoverySnapshotRelease(snapshot);
  if(okay)discoveryDrain(s,serial);else discoveryFailed(s,*e.message?e.message:"Cannot install automatic workspace bindings.");return;
 }
 if(!s->discovery.pending || GetTickCount64()<s->discovery.due || !xideWorkspaceDiscoveryIdle(s->discovery.service))return;
 xide_model_error error={0};if(!s->discovery.service)s->discovery.service=xideWorkspaceDiscoveryCreate(NULL,&error);
 s->discovery.query=s->discovery.service?xideWorkspaceDiscoverySubmit(s->discovery.service,s->folders,s->discovery.package):NULL;
 if(!s->discovery.query || !xideWorkspaceDiscoverySetWake(s->discovery.query,nativeWake,s)){automaticWorkspaceCancel(s,0);discoveryFailed(s,*error.message?error.message:"Cannot submit bounded workspace discovery.");return;}
 s->discovery.pending=0;s->discovery.jobRevision=s->discovery.revision;s->discovery.jobFolders=s->folderRevision;discoveryIncrement(&s->discovery.started);
}
static void automaticWorkspaceWatched(server* s,const xvalue* params)
{
 if(!automaticWorkspaceMode(s) || !xideWorkspaceFoldersCount(s->folders))return;
 const xvalue* changes=get(params,"changes");if(xrtValueType(changes)!=XVALUE_ARRAY || !xrtValueCount(changes))return;
 /* Large valid input batches already force diagnostic resynchronization. */
 if(xrtValueCount(changes)>INVALIDATION_LIMIT){automaticWorkspaceChanged(s,1,"Workspace watcher batch requires rediscovery.");return;}
 int relevant=0;
 for(size_t i=0;i<xrtValueCount(changes);++i){const xvalue* row=xrtValueArrayGet(changes,i);const char* uri=string(get(row,"uri"));int type=0;char* path=uri?xideLspUriToPath(uri,NULL):NULL;
  if(!path || !integer(get(row,"type"),&type) || type<1 || type>3){free(path);return;}
  if(type!=2 || xidePathHasExtension(path,".xlp") || xidePathHasExtension(path,".xlw")){
   int selected=xideWorkspaceDiscoveryRelevantPath(s->folders,path);
   /* A workspace manifest can explicitly reference a project below an
      otherwise ignored child directory. This is bounded immutable metadata,
      not a directory/manifest probe on the owner. */
   if(!selected && xidePathHasExtension(path,".xlp")){
    selected=s->renameWorkspaceDirty || !s->catalog;
    for(size_t j=0;!selected && j<xideAnalysisCatalogCount(s->catalog);++j){
     const xls_context_spec* spec=xlsContextSpec(xideAnalysisCatalogAt(s->catalog,j));
     if(spec && spec->project && xidePathLexicalEqual(path,spec->project))selected=1;
    }
   }
   if(selected)relevant=1;
  }
  free(path);
 }
 if(relevant)automaticWorkspaceChanged(s,1,"Workspace membership or manifest changed.");
}
static void automaticWorkspaceState(server* s,const xvalue* id)
{
 xvalue* out=xrtValueObject();const char* mode=s->discovery.explicitConfiguration?"explicit":automaticWorkspaceMode(s)?"automatic":"disabled";
 int okay=out && put(out,"mode",xrtValueString(sv(mode))) && put(out,"dirty",xrtValueBool(s->renameWorkspaceDirty!=0)) && put(out,"pending",xrtValueBool(s->discovery.pending!=0)) && put(out,"running",xrtValueBool(s->discovery.query!=NULL)) && put(out,"idle",xrtValueBool(xideWorkspaceDiscoveryIdle(s->discovery.service)!=0)) && put(out,"failed",xrtValueBool(s->discovery.failedState!=0)) && put(out,"watchRegistered",xrtValueBool(s->discovery.watchRegistered!=0));
#define DISCOVERY_STAT(field) okay=okay && put(out,#field,xrtValueInt((int64)s->discovery.field))
 DISCOVERY_STAT(started);DISCOVERY_STAT(completed);DISCOVERY_STAT(cancelled);DISCOVERY_STAT(revision);DISCOVERY_STAT(waitingBytes);
#undef DISCOVERY_STAT
 const xide_discovery_work* work=&s->discovery.work;
#define DISCOVERY_WORK(field) okay=okay && put(out,#field,xrtValueInt((int64)work->field))
 DISCOVERY_WORK(directories);DISCOVERY_WORK(entries);DISCOVERY_WORK(sources);DISCOVERY_WORK(manifests);DISCOVERY_WORK(projects);DISCOVERY_WORK(workerThread);
#undef DISCOVERY_WORK
 okay=okay && put(out,"failures",xrtValueInt((int64)s->discovery.failed)) && put(out,"ownerThread",xrtValueInt((int64)xrtThreadCurrentId())) && put(out,"epoch",xrtValueInt(s->configurationEpoch)) && put(out,"lastError",xrtValueString(sv(s->discovery.error)));
 if(okay)response(s,id,out,0,NULL);else{xrtValueRelease(out);response(s,id,NULL,-32603,"Cannot encode workspace discovery state.");}
}
static void automaticWorkspaceRegister(server* s)
{
 if(!automaticWorkspaceMode(s) || !s->discovery.watchSupported || s->discovery.watchPending || s->discovery.watchRegistered)return;
 static const char request[]={"{\"jsonrpc\":\"2.0\",\"id\":\"" DISCOVERY_WATCH_ID "\",\"method\":\"client/registerCapability\",\"params\":{\"registrations\":[{\"id\":\"xlang-workspace-discovery\",\"method\":\"workspace/didChangeWatchedFiles\",\"registerOptions\":{\"watchers\":[{\"globPattern\":\"**/*.{xl,xlp,xlw,c,h}\",\"kind\":7}]}}]}}"};
 xvalue* value=xrtJsonParse((xstrview){request,sizeof(request)-1});if(!value){diagnosticLog(s,"Cannot construct workspace watcher registration.");return;}
 s->discovery.watchPending=1;sendMessage(s,value);
}
static void automaticWorkspaceResponse(server* s,const xvalue* message,const xvalue* id)
{
 if(s->shutdown)return;
 const char* named=string(id);if(!s->discovery.watchPending || !named || strcmp(named,DISCOVERY_WATCH_ID))return;
 s->discovery.watchPending=0;
 if(get(message,"result") && xrtValueType(get(message,"result"))==XVALUE_NULL && !get(message,"error")){s->discovery.watchRegistered=1;return;}
 diagnosticLog(s,"Client declined or returned an invalid workspace watcher registration; result publication still validates membership proofs.");
}
#undef DISCOVERY_WAIT_BYTES
#undef DISCOVERY_DEBOUNCE
#undef DISCOVERY_WATCH_ID
