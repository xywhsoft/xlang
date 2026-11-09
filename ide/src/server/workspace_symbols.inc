/* Background collection/JSON generation never runs in the stdio owner. */
static void workspaceSymbolCapabilities(server* s,const xvalue* params)
{
 const xvalue* set=get(get(get(get(params,"capabilities"),"workspace"),"symbol"),"symbolKind");set=get(set,"valueSet");s->workspaceSymbolKinds=0;
 if(xrtValueType(set)!=XVALUE_ARRAY)return;
 for(size_t i=0;i<xrtValueCount(set);++i){int kind=0;if(!integer(xrtValueArrayGet(set,i),&kind) || kind<1 || kind>26){s->workspaceSymbolKinds=0;return;}s->workspaceSymbolKinds|=UINT32_C(1)<<(kind-1);}
}
static void cancelWorkspaceSymbols(server* s,const xvalue* id,int code,const char* reason)
{
 xvalue* retained=xrtValueRetain(id);id=retained;
 for(size_t i=0;i<8;++i)if(s->workspaceSymbolRequests[i].query && (!id || sameId(id,s->workspaceSymbolRequests[i].id))){xideWorkspaceSymbolsCancel(s->workspaceSymbolRequests[i].query);if(code)response(s,s->workspaceSymbolRequests[i].id,NULL,code,reason);xideWorkspaceSymbolsQueryDestroy(s->workspaceSymbolRequests[i].query);xrtValueRelease(s->workspaceSymbolRequests[i].id);memset(s->workspaceSymbolRequests+i,0,sizeof(s->workspaceSymbolRequests[i]));}
 xrtValueRelease(retained);
}
static int appendWorkspaceScope(xls_context** scopes,size_t* count,xls_context* context)
{
 if(!context)return 0;
 for(size_t i=0;i<*count;++i)if(xlsContextEqual(scopes[i],context))return 1;
 if(*count==64)return 0;
 scopes[(*count)++]=xlsContextRetain(context);return 1;
}
static void requestWorkspaceSymbols(server* s,const xvalue* id,const xvalue* params,int navigation)
{
 if(s->cliCompletion || s->cliDiagnostics){response(s,id,NULL,-32601,"Workspace symbols require native analysis.");return;}
 const char *query=string(get(params,"query")),*namedId=string(id);int limit=200;const xvalue *supplied=get(params,"xlangLimit"),*selection=navigation?get(params,"symbol"):NULL;
 if(!query || !xideWorkspaceSymbolQueryValid(query) || (navigation && !xideWorkspaceSymbolSelectionValid(selection)) || (namedId && strlen(namedId)>65535) || (supplied && (!integer(supplied,&limit) || limit<1 || limit>4096))){response(s,id,NULL,-32602,"Workspace query/selection invalid;query within512 bytes/128 scalars/16 terms;limit must be1..4096.");return;}
 size_t slot=8;for(size_t i=0;i<8;++i){if(s->workspaceSymbolRequests[i].query && sameId(id,s->workspaceSymbolRequests[i].id)){cancelWorkspaceSymbols(s,id,-32600,"Workspace symbol request ID is already active.");return;}if(!s->workspaceSymbolRequests[i].query && slot==8)slot=i;}
 if(slot==8){response(s,id,NULL,-32603,"Workspace symbol request queue is full.");return;}
 xls_context* contexts[64]={0};size_t contextCount=0,documentCount=0;xls_analysis_document documents[DOC_LIMIT];int okay=1;char* package=NULL;xide_model_error error={0};
 for(size_t i=0;i<xideAnalysisCatalogCount(s->catalog) && okay;++i)okay=appendWorkspaceScope(contexts,&contextCount,(xls_context*)xideAnalysisCatalogAt(s->catalog,i));
 if(!s->catalog)okay=okay && xideLanguageCheckPackagePath(&package,&error);
 for(size_t i=0;i<DOC_LIMIT && okay;++i){document* d=s->docs+i;if(!d->uri)continue;documents[documentCount++]=(xls_analysis_document){d->uri,d->path,d->content,d->identity,(uint64_t)((int64_t)d->version-INT_MIN)};
  if(s->catalog && d->binding!=XIDE_ANALYSIS_UNOWNED)continue;
  xls_context* context=xlsContextRetain(d->context);
  if(!context){char* directory=xidePathDirectory(d->path);xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=d->path,.directory=directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=package};context=directory?xlsContextCreate(&spec,16u*1024u*1024u,NULL):NULL;free(directory);}
  okay=appendWorkspaceScope(contexts,&contextCount,context);xlsContextRelease(context);
 }
 xrtFree(package);xvalue* captured=okay?xrtValueDeepClone(id):NULL;
 if(okay && !s->workspaceSymbols)s->workspaceSymbols=xideWorkspaceSymbolsCreate(s->cache,s->compiler,s->modules);
 xide_workspace_symbol_query* work=NULL;if(okay && captured)work=navigation?xideWorkspaceSymbolsSubmitNavigation(s->workspaceSymbols,contexts,contextCount,documents,documentCount,query,(size_t)limit,s->workspaceSymbolKinds,selection):xideWorkspaceSymbolsSubmitLsp(s->workspaceSymbols,contexts,contextCount,documents,documentCount,query,(size_t)limit,s->workspaceSymbolKinds);
 for(size_t i=0;i<contextCount;++i)xlsContextRelease(contexts[i]);
 if(!work || !xideWorkspaceSymbolsSetWake(work,nativeWake,s)){xideWorkspaceSymbolsCancel(work);xideWorkspaceSymbolsQueryDestroy(work);xrtValueRelease(captured);response(s,id,NULL,okay?-32603:-32602,okay?"Cannot start background workspace symbol query.":"Workspace analysis scope/input limits exceeded.");return;}
 s->workspaceSymbolRequests[slot].query=work;s->workspaceSymbolRequests[slot].id=captured;s->workspaceSymbolRequests[slot].serial=s->inputSerial;s->workspaceSymbolRequests[slot].epoch=s->configurationEpoch;
}
static void pollWorkspaceSymbols(server* s)
{
 for(size_t i=0;i<8;++i){xide_workspace_symbol_query* query=s->workspaceSymbolRequests[i].query;if(!query)continue;
  if(s->inputSerial!=s->workspaceSymbolRequests[i].serial || s->configurationEpoch!=s->workspaceSymbolRequests[i].epoch){cancelWorkspaceSymbols(s,s->workspaceSymbolRequests[i].id,-32801,"Workspace input/configuration snapshot changed.");continue;}
  EnterCriticalSection(&s->lock);size_t waiting=s->queueInputs;LeaveCriticalSection(&s->lock);if(waiting)continue;
  char error[512]={0};xide_workspace_symbol_work work={0};enum xide_workspace_symbol_state state=xideWorkspaceSymbolsResult(query,NULL,&work,error,sizeof(error));if(state==XIDE_WORKSPACE_SYMBOL_PENDING)continue;s->workspaceSymbolWork=work;
  xvalue* result=state==XIDE_WORKSPACE_SYMBOL_READY?xideWorkspaceSymbolsTakeLsp(query):NULL;
  if(result)response(s,s->workspaceSymbolRequests[i].id,result,0,NULL);else response(s,s->workspaceSymbolRequests[i].id,NULL,state==XIDE_WORKSPACE_SYMBOL_CANCELLED?-32800:-32803,*error?error:"Workspace symbol query cancelled or unavailable.");
  xideWorkspaceSymbolsQueryDestroy(query);xrtValueRelease(s->workspaceSymbolRequests[i].id);memset(s->workspaceSymbolRequests+i,0,sizeof(s->workspaceSymbolRequests[i]));
 }
}
static void workspaceSymbolStats(server* s,const xvalue* id)
{
 const xide_workspace_symbol_work* work=&s->workspaceSymbolWork;xvalue* out=xrtValueObject();int okay=out!=NULL;
#define WS_STAT(field) okay=okay && put(out,#field,xrtValueInt((int64)work->field))
 WS_STAT(roots);WS_STAT(captured);WS_STAT(reused);WS_STAT(covered);WS_STAT(sourceRejections);WS_STAT(guardBytes);WS_STAT(overlayHashBytes);WS_STAT(coordinatorThread);WS_STAT(analysisThread);WS_STAT(analysisBuilds);WS_STAT(analysisHits);
 WS_STAT(index.generation);WS_STAT(index.rows);WS_STAT(index.parts);WS_STAT(index.scopes);WS_STAT(index.bytes);
#undef WS_STAT
 if(okay)response(s,id,out,0,NULL);else{xrtValueRelease(out);response(s,id,NULL,-32603,"Cannot encode workspace symbol counters.");}
}
