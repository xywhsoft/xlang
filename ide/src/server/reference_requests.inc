static void requestReferences(server* s,const xvalue* id,const xvalue* params)
{
 if(s->renameWorkspaceDirty){response(s,id,NULL,-32803,"Workspace folders changed; refresh analysisConfiguration before references.");return;}
 if(s->cliCompletion||s->cliDiagnostics){response(s,id,NULL,-32601,"References require the native analysis backend.");return;}
 const char* uri=string(get(get(params,"textDocument"),"uri"));int entry=find(s,uri),line=0,character=0,version=entry>=0?s->docs[entry].version:0;bool include=false;const xvalue* supplied=get(params,"version");
 char* path=uri?xideLspUriToPath(uri,NULL):NULL;
 int okay=path&&xidePathHasExtension(path,".xl")&&xideLspRequestIdValid(id)&&integer(get(get(params,"position"),"line"),&line)&&integer(get(get(params,"position"),"character"),&character)&&line>=0&&character>=0&&xrtValueGetBool(get(get(params,"context"),"includeDeclaration"),&include);
 if(supplied)okay=okay&&entry>=0&&integer(supplied,&version)&&version==s->docs[entry].version;
 if(entry>=0)okay=okay&&validPosition(s->docs+entry,line,character);
 if(!okay){free(path);response(s,id,NULL,-32602,"References require a valid xlang URI,UTF16 position and boolean includeDeclaration;closed sources have no overlay version.");return;}
 size_t slot=8;for(size_t i=0;i<8;++i){if(s->referenceRequests[i].query&&sameId(id,s->referenceRequests[i].id)){free(path);cancelReferences(s,id,-32600,"Reference request ID is already active.");return;}if(!s->referenceRequests[i].query&&slot==8)slot=i;}
 if(slot==8){free(path);response(s,id,NULL,-32603,"Reference request queue is full.");return;}
 int binding=XIDE_ANALYSIS_UNOWNED;xls_context* context=entry>=0?xlsContextRetain(s->docs[entry].context):bindAnalysis(s->catalog,path,&binding);
 if(entry>=0)binding=s->docs[entry].binding;
 if(s->catalog&&binding<0){xlsContextRelease(context);free(path);response(s,id,NULL,-32602,"References need an unambiguous configured source owner.");return;}
 if(!context){char* package=xrtEnvGet("XLANG_PACKAGE_PATH");char* directory=entry>=0?NULL:xidePathDirectory(path);xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=path,.directory=directory?directory:s->directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=package};context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);free(directory);xrtFree(package);}
 xls_analysis_document docs[DOC_LIMIT];size_t count=0;for(size_t i=0;i<DOC_LIMIT;++i)if(s->docs[i].uri){document* d=s->docs+i;docs[count++]=(xls_analysis_document){d->uri,d->path,d->content,d->identity,(uint64_t)((int64_t)d->version-INT_MIN)};}
 if(!s->references)s->references=xideProjectReferencesCreate(s->cache,s->compiler,s->modules);
 xvalue* captured=xrtValueDeepClone(id);xide_reference_query* query=context&&captured?xideProjectReferencesSubmitFramed(s->references,s->catalog,context,docs,count,entry>=0?s->docs[entry].uri:NULL,path,line,character,include,s->referenceInputProof,captured):NULL;xlsContextRelease(context);free(path);
 if(!query||!xideProjectReferencesSetWake(query,nativeWake,s)){xideProjectReferencesCancel(query);xideProjectReferencesQueryDestroy(query);xrtValueRelease(captured);response(s,id,NULL,-32603,"Cannot start complete background references.");return;}
 s->referenceRequests[slot].query=query;s->referenceRequests[slot].id=captured;s->referenceRequests[slot].document=entry;s->referenceRequests[slot].version=version;s->referenceRequests[slot].identity=entry>=0?s->docs[entry].identity:0;s->referenceRequests[slot].serial=s->inputSerial;s->referenceRequests[slot].epoch=s->configurationEpoch;
}
static void pollReferences(server* s)
{
 for(size_t i=0;i<8;++i){xide_reference_query* query=s->referenceRequests[i].query;if(!query)continue;int entry=s->referenceRequests[i].document;document* d=entry>=0?s->docs+entry:NULL;
  if((d&&(!d->uri||d->identity!=s->referenceRequests[i].identity||d->version!=s->referenceRequests[i].version))||s->inputSerial!=s->referenceRequests[i].serial||s->configurationEpoch!=s->referenceRequests[i].epoch){cancelReferences(s,s->referenceRequests[i].id,-32801,"Reference input/configuration snapshot changed.");continue;}
  EnterCriticalSection(&s->lock);size_t waiting=s->queueInputs;LeaveCriticalSection(&s->lock);if(waiting)continue;
  char error[512]={0};xide_project_reference_stats stats={0};int state=xideProjectReferencesResult(query,NULL,&stats,error,sizeof(error));if(state==XIDE_REFERENCES_PENDING)continue;s->referenceStats=stats;
  if(state==XIDE_REFERENCES_READY){size_t bytes=0;char* frame=xideProjectReferencesTakeFrame(query,&bytes);if(frame&&bytes)sendFrame(s,frame,bytes);else response(s,s->referenceRequests[i].id,NULL,-32603,"Reference response frame is unavailable.");free(frame);}
  else response(s,s->referenceRequests[i].id,NULL,xideProjectReferencesErrorCode(query),*error?error:"Reference request cancelled or unavailable.");
  xideProjectReferencesQueryDestroy(query);xrtValueRelease(s->referenceRequests[i].id);memset(s->referenceRequests+i,0,sizeof(s->referenceRequests[i]));
 }
}
