/* Only fixed roster capture, ownership gates and preframed output on owner.
 * Metadata, source reads, native analysis and large JSON encoding are workers. */
static void cancelRenames(server* s,const xvalue* id,int code,const char* reason)
{
 xvalue* retained=xrtValueRetain(id);id=retained;
 for(size_t i=0;i<4;++i)if(s->renameRequests[i].query && (!id || sameId(id,s->renameRequests[i].id))){
  xideRenameCandidateCancel(s->renameRequests[i].query);
  if(code)response(s,s->renameRequests[i].id,NULL,code,reason);
  xideRenameCandidateDestroy(s->renameRequests[i].query);xrtValueRelease(s->renameRequests[i].id);memset(s->renameRequests+i,0,sizeof(s->renameRequests[i]));
 }
 xrtValueRelease(retained);
}
static void requestRename(server* s,const xvalue* id,const xvalue* params,int standard)
{
 int number=0,i=find(s,string(get(get(params,"textDocument"),"uri")));
 if(s->cliCompletion || s->cliDiagnostics){response(s,id,NULL,-32601,"Rename candidates require native analysis.");return;}
 if((!standard && (xrtValueType(id)!=XVALUE_INT || !integer(id,&number) || number<=0 || !s->catalog || !s->catalogValue || (i>=0 && s->docs[i].binding<0))) || (standard && xrtValueType(id)!=XVALUE_INT && (!string(id) || strlen(string(id))>65535)) || i<0){response(s,id,NULL,-32602,"Rename needs a valid request ID and current open document; native candidate requests also require an acknowledged unambiguous configuration.");return;}
 if(standard && (s->renameWorkspaceDirty || (s->renameWorkspaceScope && !s->catalog))){response(s,id,NULL,-32803,"Workspace rename requires a current complete analysisConfiguration; open documents alone do not establish workspace membership.");return;}
 size_t slot=4;for(size_t j=0;j<4;++j){if(s->renameRequests[j].query && sameId(id,s->renameRequests[j].id)){cancelRenames(s,id,-32600,"Rename request ID is already active.");return;}if(!s->renameRequests[j].query && slot==4)slot=j;}
 if(slot==4){response(s,id,NULL,-32603,"Rename request queue is full.");return;}
 if(!s->references)s->references=xideProjectReferencesCreate(s->cache,s->compiler,s->modules);
 if(!s->renames)s->renames=xideRenameCandidatesCreate(s->references);
 xls_analysis_document docs[DOC_LIMIT];size_t count=0;
 for(size_t j=0;j<DOC_LIMIT;++j)if(s->docs[j].uri){document* d=s->docs+j;docs[count++]=(xls_analysis_document){d->uri,d->path,d->content,d->identity,(uint64_t)((int64_t)d->version-INT_MIN)};}
 xvalue* saved=xrtValueRetain(id);xide_rename_candidate_query* query=saved ? (standard==2 ? xideRenamePrepareSubmit(s->renames,id,params,s->catalog,docs,count) : standard ? xideRenameStandardSubmit(s->renames,id,s->renameDocumentChanges,params,s->catalog,docs,count) : xideRenameCandidateSubmit(s->renames,number,params,s->catalogValue,s->catalog,docs,count)) : NULL;
 if(!query || !xideRenameCandidateSetWake(query,nativeWake,s)){xideRenameCandidateCancel(query);xideRenameCandidateDestroy(query);xrtValueRelease(saved);response(s,id,NULL,-32603,"Cannot start native rename orchestration.");return;}
 s->renameRequests[slot].query=query;s->renameRequests[slot].id=saved;s->renameRequests[slot].document=i;s->renameRequests[slot].version=s->docs[i].version;s->renameRequests[slot].identity=s->docs[i].identity;s->renameRequests[slot].serial=s->inputSerial;s->renameRequests[slot].epoch=s->configurationEpoch;
}
static void pollRenames(server* s)
{
 for(size_t i=0;i<4;++i){
  xide_rename_candidate_query* q=s->renameRequests[i].query;if(!q)continue;document* d=s->docs+s->renameRequests[i].document;
  if(!d->uri || d->identity!=s->renameRequests[i].identity || d->version!=s->renameRequests[i].version || s->inputSerial!=s->renameRequests[i].serial || s->configurationEpoch!=s->renameRequests[i].epoch){cancelRenames(s,s->renameRequests[i].id,-32801,"Rename input/configuration snapshot changed.");continue;}
  EnterCriticalSection(&s->lock);size_t waiting=s->queueInputs;LeaveCriticalSection(&s->lock);if(waiting)continue;
  int code=0;char error[512]={0};int state=xideRenameCandidateResult(q,&code,error,sizeof(error));if(!state)continue;
  if(state<0)response(s,s->renameRequests[i].id,NULL,code?code:-32603,*error?error:"Rename candidate failed.");
  else{
   size_t bytes=0;char* frame=xideRenameCandidateTakeFrame(q,&bytes);
   if(!frame || !bytes)s->writeFailed=1;
   else for(size_t at=0;at<bytes;){DWORD written=0;size_t n=bytes-at;if(n>16384)n=16384;if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),frame+at,(DWORD)n,&written,NULL) || !written){s->writeFailed=1;break;}at+=written;}
   free(frame);
  }
  xideRenameCandidateDestroy(q);xrtValueRelease(s->renameRequests[i].id);memset(s->renameRequests+i,0,sizeof(s->renameRequests[i]));
 }
}
