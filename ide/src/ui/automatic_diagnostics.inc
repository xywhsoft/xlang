/* Primary-connection controller. Bodies, JSON decoding, proof reads and range
 * mapping belong to diagnostic_stream. This owner handles small model metadata
 * and installs accepted file groups without clearing unrelated contributions. */
#define AUTO_GROUPS 4096u
#define AUTO_CACHE_BYTES (64u*1024u*1024u)
#define AUTO_ROUTE_BASE ((size_t)0x10000000u)
typedef struct automatic_model {
 xide_document* document;uint64_t id;uint32_t version;int open,dirty,externalState;
 char *path,*language;wchar_t* key;
} automatic_model;
typedef struct automatic_group {
 xide_diagnostic_notice* notice;uint64_t token,checkedEpoch;int valid,applied;
 xide_document* annotated;uint64_t annotatedId;
} automatic_group;
typedef struct automatic_fence {char* uri;wchar_t* key;uint64_t serial;int watch,close,type;} automatic_fence;
typedef struct automatic_cleanup {
 xide_diagnostic_notice* notices;xide_diagnostic_batch* batch;xvalue* root;xide_diagnostic_result* result;
} automatic_cleanup;
struct xide_automatic_diagnostics {
 xide_diagnostic_stream* stream;uint64_t session,epoch,sentSerial,receiptSerial,nextToken,validityEpoch;
 automatic_group* groups;size_t groupCount,cacheBytes;
 automatic_model* models;size_t modelCount;int modelsReady;
 automatic_fence fences[128];size_t fenceCount;
 xide_diagnostic_batch* batch;xvalue* held;size_t heldBytes;
 automatic_cleanup garbage;automatic_cleanup* retirement;
 xide_diagnostic_result* incoming;size_t incomingAt;
 size_t routes[4096],routeCount;int problemsDirty,activate,paused,syncAdopted;
 char *compiler,*package;int configured,packageValid;uint64_t installed,received,resyncs;
 double maxOwnerMs;
};
static const char* automaticText(const xvalue* value)
{xstrview s={0};return xrtValueGetString(value,&s) ? s.Data : NULL;}
static void automaticRelease(void* value)
{
 automatic_cleanup* c=value;
 while(c->notices) {xide_diagnostic_notice* n=c->notices;c->notices=n->retiredNext;xideDiagnosticNoticeRelease(n);}
 xideDiagnosticBatchRelease(c->batch);xrtValueRelease(c->root);xideDiagnosticResultRelease(c->result);xrtFree(c);
}
static void automaticDiscard(xide_automatic_diagnostics* a,xide_diagnostic_notice* n)
{if(n) {n->retiredNext=a->garbage.notices;a->garbage.notices=n;}}
static int automaticRetire(xide_automatic_diagnostics* a)
{
 if(!a->retirement && (a->garbage.notices || a->garbage.batch || a->garbage.root || a->garbage.result)) {
  a->retirement=xrtMalloc(sizeof(*a->retirement));if(!a->retirement) return 0;
  *a->retirement=a->garbage;a->garbage=(automatic_cleanup){0};
 }
 if(a->retirement && xideDiagnosticStreamRetire(a->stream,a->retirement,automaticRelease)) a->retirement=NULL;
 return !a->retirement;
}
static void automaticClearAnnotation(automatic_group* g)
{
 xide_document* d=g->annotated;
 if(d && d->editor) {
  xui_code_annotation_store store=xuiCodeEditGetAnnotations(d->editor);xui_code_diagnostic_t first={.iSize=sizeof(first)};
  if(xuiCodeAnnotationGetDiagnosticCount(store)>0 && xuiCodeAnnotationGetDiagnostic(store,0,&first)==XUI_OK && first.iUserData==AUTO_ROUTE_BASE+(size_t)g->token*4096u)
   xuiCodeAnnotationClearDiagnostics(store);
 }
 g->annotated=NULL;g->annotatedId=0;g->applied=0;
}
static void automaticInvalidate(xide_automatic_diagnostics* a,const wchar_t* key)
{
 if(!key) return;
 for(size_t i=0;i<a->groupCount;++i) {
  automatic_group* g=a->groups+i;if(!xideDiagnosticDependencyContainsKey(g->notice->dependency,key)) continue;
  automaticClearAnnotation(g);g->valid=0;g->checkedEpoch=0;a->problemsDirty=1;
 }
}
static void automaticFenceClear(automatic_fence* f) {free(f->uri);free(f->key);*f=(automatic_fence){0};}
static void automaticReset(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;if(!a) return;
 for(size_t i=0;i<a->groupCount;++i) {automaticClearAnnotation(a->groups+i);automaticDiscard(a,a->groups[i].notice);}
 a->groupCount=a->cacheBytes=a->routeCount=0;a->problemsDirty=1;a->activate=a->syncAdopted=0;
 for(size_t i=0;i<a->fenceCount;++i) automaticFenceClear(a->fences+i);
 a->fenceCount=0;
 if(a->batch) {a->garbage.batch=a->batch;a->batch=NULL;}
 if(a->held) {a->garbage.root=a->held;a->held=NULL;a->heldBytes=0;}
 if(a->incoming) {a->garbage.result=a->incoming;a->incoming=NULL;a->incomingAt=0;}
 a->sentSerial=a->receiptSerial=0;++a->epoch;++a->session;xideDiagnosticStreamReset(a->stream,a->session);
 xideTaskViewClearLanguageProblemsOwned(v->app,a);automaticRetire(a);
}
static void automaticPause(xide_language_view* v) {if(v->autoView) v->autoView->paused=1;}
static void automaticResume(xide_language_view* v) {if(v->autoView) v->autoView->paused=0;}
static void automaticFault(xide_language_view* v,const char* reason)
{
 char message[600];snprintf(message,sizeof(message),"%.560s",reason);automaticPause(v);reset(v->app);
 snprintf(v->message,sizeof(v->message),"xlang 自动检查已暂停：%s；修改输入或再次检查可恢复。",message);
}
static int automaticCreate(xide_language_view* v)
{
 if(v->app->disableAutomaticDiagnostics) return 1;
 xide_automatic_diagnostics* a=calloc(1,sizeof(*a));if(!a) return 0;v->autoView=a;
 a->groups=calloc(AUTO_GROUPS,sizeof(*a->groups));a->session=1;a->epoch=1;
 a->stream=xideDiagnosticStreamCreate(languageWake,v->app);
 return a->groups && a->stream && xideDiagnosticStreamReset(a->stream,a->session);
}
static void automaticModelClear(automatic_model* m)
{free(m->path);free(m->language);free(m->key);*m=(automatic_model){0};}
static void automaticDestroy(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;if(!a) return;
 xideDiagnosticStreamDestroy(a->stream);
 for(size_t i=0;i<a->groupCount;++i) xideDiagnosticNoticeRelease(a->groups[i].notice);
 if(a->retirement) automaticRelease(a->retirement);
 while(a->garbage.notices) {xide_diagnostic_notice* n=a->garbage.notices;a->garbage.notices=n->retiredNext;xideDiagnosticNoticeRelease(n);}
 xideDiagnosticBatchRelease(a->garbage.batch);xrtValueRelease(a->garbage.root);xideDiagnosticResultRelease(a->garbage.result);
 xideDiagnosticBatchRelease(a->batch);xrtValueRelease(a->held);
 xideDiagnosticResultRelease(a->incoming);
 for(size_t i=0;i<a->modelCount;++i) automaticModelClear(a->models+i);
 for(size_t i=0;i<a->fenceCount;++i) automaticFenceClear(a->fences+i);
 free(a->models);free(a->groups);free(a->compiler);free(a->package);free(a);v->autoView=NULL;
}
static int automaticFence(xide_language_view* v,const char* path,int close,int watch,int type)
{
 xide_automatic_diagnostics* a=v->autoView;wchar_t* key=path ? xidePathNativeWide(path) : NULL;if(!key) return 0;
 size_t at=0;while(at<a->fenceCount && xidePathNativeKeyCompare(a->fences[at].key,key)) ++at;
 if(at==a->fenceCount) {
  if(at==128) {free(key);reset(v->app);++a->resyncs;snprintf(v->message,sizeof(v->message),"诊断输入历史已达到上限，正在重新同步打开文档。");return 0;}
  char* uri=xideLspPathToUri(path,NULL);if(!uri) {free(key);return 0;}
  a->fences[a->fenceCount++]=(automatic_fence){uri,key,0,watch,close,type};
 } else {
  free(key);automatic_fence* f=a->fences+at;f->serial=0;f->close|=close;f->watch|=watch;f->type=type;
 }
 automaticInvalidate(a,a->fences[at].key);return 1;
}
static int automaticModels(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;xide_app* app=v->app;int changed=0;
 if(app->documents.count>4096) {automaticFault(v,"打开文档的元数据数量超出诊断控制器上限");return 0;}
 if(a->modelCount<app->documents.count) {
  automatic_model* models=realloc(a->models,app->documents.count*sizeof(*models));if(!models) {automaticFault(v,"无法捕获文档元数据");return 0;}
  memset(models+a->modelCount,0,(app->documents.count-a->modelCount)*sizeof(*models));a->models=models;a->modelCount=app->documents.count;
 }
 for(size_t i=0;i<a->modelCount;++i) {
  automatic_model* m=a->models+i;xide_document* d=i<app->documents.count ? app->documents.items[i] : NULL;
  int open=d && d->open;uint32_t version=open ? xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) : 0;
  if(m->document==d && m->open==open && (!open || (m->id==d->id && m->version==version && m->dirty==d->dirty && m->externalState==d->externalState &&
      (m->path && d->path ? !strcmp(m->path,d->path) : m->path==d->path) && m->language && !strcmp(m->language,d->language)))) continue;
  int oldOnline=m->open && m->path && xidePathHasExtension(m->path,".xl") && (!m->externalState || m->externalState==XIDE_WATCH_MISSING);
  int newOnline=open && namedXlang(d) && !snapshotConflict(d);
  int samePath=m->path && open && d->path && !strcmp(m->path,d->path);
  automaticInvalidate(a,m->key);
  if(v->autoEnabled && a->modelsReady && m->open && m->path && (!open || !samePath || m->id!=d->id || (oldOnline && !newOnline))) {
   if(!automaticFence(v,m->path,oldOnline,!oldOnline,2)) return 0;
  }
  if(v->autoEnabled && a->modelsReady && m->open && m->path && open && samePath && !namedXlang(d) && !d->dirty &&
     (m->version!=version || m->dirty || m->externalState!=d->externalState)) {
   if(!automaticFence(v,d->path,0,1,2)) return 0;
  }
  automaticModelClear(m);m->document=d;m->open=open;
  if(open) {
   m->id=d->id;m->version=version;m->dirty=d->dirty;m->externalState=d->externalState;m->path=d->path ? copy(d->path) : NULL;m->language=copy(d->language);m->key=d->path ? xidePathNativeWide(d->path) : NULL;
   if(!m->language || (d->path && (!m->path || !m->key))) {automaticFault(v,"无法捕获文档路径");return 0;}
   automaticInvalidate(a,m->key);
  }
  changed=1;
 }
 if(changed) {if(v->autoEnabled) clearSemantic(app);a->paused=0;}
 a->modelsReady=1;return 1;
}
static int automaticInputsCurrent(xide_language_view* v)
{
 if(!v->autoView || !v->autoEnabled) return 0;
 size_t count=0;
 for(size_t i=0;i<v->app->documents.count;++i) {xide_document* d=v->app->documents.items[i];if(namedXlang(d) && !snapshotConflict(d)) ++count;}
 if(count!=v->inputCount) return 0;
 for(size_t i=0;i<v->inputCount;++i) {document_binding* b=v->inputs+i;xide_document* d=b->document;
  if(!namedXlang(d) || snapshotConflict(d) || b->id!=d->id || b->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) || b->externalState!=d->externalState || strcmp(b->path,d->path) || strcmp(b->language,d->language)) return 0;
 }
 return 1;
}
static xide_document* automaticRoot(xide_language_view* v)
{
 xide_document* d=v->document;
 if(d && d->open && d->id==v->documentId && namedXlang(d) && !strcmp(d->language,"xlang") && !snapshotConflict(d)) return d;
 for(size_t i=0;i<v->app->documents.count;++i) {d=v->app->documents.items[i];if(namedXlang(d) && !strcmp(d->language,"xlang") && !snapshotConflict(d)) return d;}
 return NULL;
}
static int automaticPrepare(xide_app* app,int manual)
{
 xide_language_view* v=app->languageView;xide_automatic_diagnostics* a=v->autoView;if(!a || !v->autoEnabled || !v->initialized) return 0;
 if(!automaticModels(v)) return 0;
 xide_document* root=manual ? app->activeDocument : automaticRoot(v);
 if(manual && !(manual==1 ? canSnapshot(app,root) : eligible(root))) return 0;
 a->paused=0;v->snapshotMode=v->onDemandMode=1;v->automatic=0;v->invalidated=0;v->recheckAt=v->semanticRefreshAt=0;
 if(root) {if(!v->path || strcmp(v->path,root->path)) {free(v->path);v->path=copy(root->path);if(!v->path) return 0;}v->document=root;v->documentId=root->id;v->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(root->editor));}
 else {v->document=NULL;v->documentId=0;free(v->path);v->path=NULL;}
 if(manual) {v->autoManual=manual;v->busy=1;}
 if(automaticInputsCurrent(v) && v->syncMode==4) return 1;
 if(!automaticInputsCurrent(v) || !a->syncAdopted || manual) {
  a->syncAdopted=0;
  if(!captureInputs(app)) {automaticFault(v,"命名 xlang 文档超过 64 份、8 MiB/份或 32 MiB 总正文预算");return 0;}
  cancelQuery(v,v->receiptRequest);v->receiptRequest=v->receiptReady=0;
  return beginSyncPlan(app,4);
 }
 if(root && !v->receiptReady && !v->receiptRequest && !v->syncMode) return finishOnDemandSync(app);
 return 1;
}
static void automaticSynced(xide_language_view* v,const xide_sync_result* result)
{
 xide_automatic_diagnostics* a=v->autoView;if(!a || !v->autoEnabled) return;
 if(result->operationCount>UINT64_MAX-a->sentSerial) {automaticFault(v,"输入序号已耗尽");return;}
 for(size_t i=0;i<result->operationCount;++i) {
  ++a->sentSerial;if(!result->operations[i].close) continue;const char* uri=automaticText(result->operations[i].uri);
  for(size_t j=0;j<a->fenceCount;++j) if(a->fences[j].close && !strcmp(a->fences[j].uri,uri)) {a->fences[j].serial=a->sentSerial;a->fences[j].close=0;}
 }
 for(size_t i=0;i<a->fenceCount;++i) if(a->fences[i].close) {a->fences[i].close=0;a->fences[i].watch=1;}
 ++a->epoch;++a->validityEpoch;a->syncAdopted=1;
}
static void automaticReceipt(xide_language_view* v,uint64_t* serial)
{if(v->autoView && v->autoEnabled) {v->autoView->receiptSerial=v->autoView->sentSerial;if(serial) *serial=v->autoView->receiptSerial;}}
static int automaticReceiptMatches(xide_language_view* v,const xvalue* result)
{
 if(!v->autoEnabled || !v->autoView) return 1;
 if(!receiptGeneration(get(result,"serial"))) return 0;
 return strtoull(string(get(result,"serial")),NULL,10)==v->autoView->receiptSerial;
}
static void automaticSaved(xide_language_view* v,xide_document* document)
{
 if(!document || !document->path) return;
 automaticFence(v,document->path,0,1,2);xgeRenderRequest();
}
static void automaticWatches(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;if(!v->autoEnabled || !a->syncAdopted || v->syncMode || !automaticInputsCurrent(v)) return;
 xvalue* changes=NULL;size_t count=0;
 for(size_t i=0;i<a->fenceCount;++i) if(a->fences[i].watch) {
  if(!changes) changes=xrtValueArray();
  xvalue* item=xrtValueObject();
  if(!changes || !item || !put(item,"uri",xrtValueString(sv(a->fences[i].uri))) || !put(item,"type",xrtValueInt(a->fences[i].type)) || !xrtValueArrayAppend(changes,item)) {xrtValueRelease(item);xrtValueRelease(changes);automaticFault(v,"无法准备文件变更通知");return;}
  xrtValueRelease(item);++count;
 }
 if(!count) return;
 xvalue* params=xrtValueObject();if(!params || !put(params,"changes",changes)) {xrtValueRelease(params);automaticFault(v,"无法准备文件变更通知");return;}
 if(a->sentSerial==UINT64_MAX || !sendMessage(v,"workspace/didChangeWatchedFiles",params,0)) {automaticFault(v,"无法发送文件变更通知");return;}
 ++a->sentSerial;++a->validityEpoch;for(size_t i=0;i<a->fenceCount;++i) if(a->fences[i].watch) {a->fences[i].watch=0;a->fences[i].serial=a->sentSerial;}
}
static int automaticCurrent(xide_language_view* v,const xide_diagnostic_notice* n)
{
 xide_automatic_diagnostics* a=v->autoView;
 if(!v->autoEnabled || !a || !n || n->cleared || !n->hasSerial || !n->hasOverlays || !n->overlaysMatched || n->inputSerial>a->sentSerial || (n->overlay ? !n->mapped : !n->diskValidated)) return 0;
 for(size_t i=0;i<a->fenceCount;++i) if(xideDiagnosticDependencyContainsKey(n->dependency,a->fences[i].key) && (!a->fences[i].serial || n->inputSerial<a->fences[i].serial)) return 0;
 for(size_t i=0;i<n->bindingCount;++i) {
  const xide_diagnostic_binding* binding=n->bindings+i;document_binding* b=NULL;
  for(size_t j=0;j<v->inputCount;++j) if(!strcmp(v->inputs[j].uri,binding->uri)) {b=v->inputs+j;break;}
  if(!b || b->wireVersion!=binding->version || xlsSyncVersion(v->sync,b->uri)!=binding->version) return 0;
  xide_document* d=b->document;
  if(!namedXlang(d) || snapshotConflict(d) || d->id!=b->id || b->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) || b->externalState!=d->externalState || strcmp(b->path,d->path) || strcmp(b->language,d->language)) return 0;
 }
 for(size_t i=0;i<a->modelCount;++i) {
  const automatic_model* m=a->models+i;if(!m->open || !m->key || !xideDiagnosticDependencyContainsKey(n->dependency,m->key)) continue;
  xide_document* d=m->document;
  if(namedXlang(d) && !snapshotConflict(d)) {
   int found=0;for(size_t j=0;j<n->bindingCount;++j) if(!xidePathNativeKeyCompare(n->bindings[j].key,m->key)) {found=1;break;}if(!found) return 0;
  } else if(d->dirty || d->externalState) return 0;
 }
 return 1;
}
static size_t automaticFind(xide_automatic_diagnostics* a,const wchar_t* key)
{
 size_t begin=0,end=a->groupCount;while(begin<end) {size_t middle=begin+(end-begin)/2;int order=xidePathNativeKeyCompare(a->groups[middle].notice->key,key);if(order<0) begin=middle+1;else end=middle;}return begin;
}
static void automaticWithdrawNavigation(xide_language_view* v,const xide_diagnostic_notice* old)
{
 /* A background check can replace the server's navigation guard. Its file
  * withdrawal must also revoke semantic UI derived from the same dependency
  * closure. Local edits already invalidate these groups before wire delivery;
  * do not let their delayed clears cancel a newer navigation input receipt. */
 xide_document* targets[]={v->outlineDocument,v->hoverDocument,v->definitionDocument,v->highlight.document};
 int affected=0;
 for(size_t i=0;i<sizeof(targets)/sizeof(*targets) && !affected;++i) {
  xide_document* d=targets[i];if(!d) continue;
  for(size_t j=0;j<v->inputCount;++j) {document_binding* b=v->inputs+j;
   if(b->document==d && d->open && b->id==d->id && xideDiagnosticDependencyContainsKey(old->dependency,b->key)) {affected=1;break;}
  }
 }
 if(!affected) return;
 clearSemantic(v->app);cancelQuery(v,v->definitionRequest);v->definitionRequest=v->definitionPending=0;
 cancelQuery(v,v->receiptRequest);v->receiptRequest=v->receiptReady=0;xgeRenderRequest();
}
static int automaticAccept(xide_language_view* v,xide_diagnostic_notice* n)
{
 xide_automatic_diagnostics* a=v->autoView;size_t at=automaticFind(a,n->key);int existing=at<a->groupCount && !xidePathNativeKeyCompare(a->groups[at].notice->key,n->key);
 if(!n->hasSerial || n->inputSerial>a->sentSerial) {automaticDiscard(a,n);return 0;}
 if(existing) {
  automatic_group* old=a->groups+at;
  if(n->cleared && old->valid) automaticWithdrawNavigation(v,old->notice);
  automaticClearAnnotation(old);a->cacheBytes-=old->notice->wireBytes;automaticDiscard(a,old->notice);
  memmove(a->groups+at,a->groups+at+1,(a->groupCount-at-1)*sizeof(*a->groups));--a->groupCount;
 }
 a->problemsDirty=1;
 if(n->cleared) {automaticDiscard(a,n);return 1;}
 if(!n->hasOverlays || !n->hasSerial || n->inputSerial>a->sentSerial || a->groupCount==AUTO_GROUPS || n->wireBytes>AUTO_CACHE_BYTES-a->cacheBytes || a->nextToken>(SIZE_MAX-AUTO_ROUTE_BASE)/4096u-2) {automaticDiscard(a,n);return 0;}
 memmove(a->groups+at+1,a->groups+at,(a->groupCount-at)*sizeof(*a->groups));a->groups[at]=(automatic_group){.notice=n,.token=++a->nextToken,.valid=automaticCurrent(v,n)};++a->groupCount;a->cacheBytes+=n->wireBytes;++a->received;return 1;
}
static int automaticSubmit(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;if(!a->batch || !xideDiagnosticBatchCount(a->batch)) return 1;
 int accepted=xideDiagnosticStreamSubmitCausal(a->stream,a->session,a->epoch,a->sentSerial,a->batch,v->sync,NULL);
 if(accepted>0) {a->batch=NULL;return 1;}
 if(accepted<0) automaticFault(v,"后台诊断提交失败");
 return 0;
}
static int automaticBlocked(xide_language_view* v)
{return v->autoView && v->autoEnabled && (v->autoView->retirement || v->autoView->held);}
static int automaticReceive(xide_language_view* v,xvalue* message,size_t bytes)
{
 xide_automatic_diagnostics* a=v->autoView;
 if(!a->batch) a->batch=xideDiagnosticBatchCreate();
 if(a->batch && xideDiagnosticBatchAppend(a->batch,message,bytes)) return 1;
 if(a->batch && xideDiagnosticBatchCount(a->batch) && automaticSubmit(v)) {
  a->batch=xideDiagnosticBatchCreate();if(a->batch && xideDiagnosticBatchAppend(a->batch,message,bytes)) return 1;
 }
 a->held=message;a->heldBytes=bytes;
 if(!bytes || bytes>8u*1024u*1024u || !a->batch) automaticFault(v,"诊断接收预算或分配失败");
 return 1;
}
static void automaticDrain(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;if(!automaticRetire(a)) return;
 LARGE_INTEGER began,now;QueryPerformanceCounter(&began);
 for(unsigned batch=0;batch<2;++batch) {
  if(!a->incoming) {a->incoming=xideDiagnosticStreamTake(a->stream);a->incomingAt=0;}
  xide_diagnostic_result* r=a->incoming;if(!r) break;
  if(r->session!=a->session || !r->valid) {automaticFault(v,*r->error ? r->error : "后台诊断批次无效");return;}
  while(a->incomingAt<r->count) {
   xide_diagnostic_notice* n=xideDiagnosticResultTakeNotice(r,a->incomingAt++);
   if(n && !automaticAccept(v,n)) {automaticFault(v,"诊断证明、输入序号或缓存预算无效");return;}
   QueryPerformanceCounter(&now);
   if((now.QuadPart-began.QuadPart)*2000>=v->highlightClock.QuadPart && a->incomingAt<r->count) {automaticRetire(a);xgeRenderRequest();return;}
  }
  a->incoming=NULL;a->incomingAt=0;a->garbage.result=r;if(!automaticRetire(a)) return;
 }
 if(a->batch && !automaticSubmit(v)) return;
 if(a->held) {
  xvalue* m=a->held;size_t bytes=a->heldBytes;a->held=NULL;a->heldBytes=0;automaticReceive(v,m,bytes);
 }
}
static size_t automaticCount(xide_language_view* v)
{size_t count=0;xide_automatic_diagnostics* a=v->autoView;if(a) for(size_t i=0;i<a->groupCount;++i) if(a->groups[i].valid) count+=a->groups[i].notice->count;return count;}
static void automaticInstall(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;LARGE_INTEGER began,now;QueryPerformanceCounter(&began);
 for(size_t i=0;i<a->groupCount;++i) {
  automatic_group* g=a->groups+i;
  if(!g->valid && g->checkedEpoch!=a->validityEpoch) {
   g->checkedEpoch=a->validityEpoch;if(automaticCurrent(v,g->notice)) {g->valid=1;g->applied=0;a->problemsDirty=1;}
   QueryPerformanceCounter(&now);if((now.QuadPart-began.QuadPart)*1000>=v->highlightClock.QuadPart) {xgeRenderRequest();break;}
  }
  if(!g->valid || g->applied) continue;
  xide_diagnostic_notice* n=g->notice;xide_document* d=n->overlay ? openPath(v->app,n->path) : NULL;
  if(d && !strcmp(d->language,"xlang")) {
   xui_code_diagnostic_t* items=calloc(n->count+1,sizeof(*items));if(!items) {automaticFault(v,"无法准备诊断显示");return;}
   for(size_t j=0;j<n->count;++j) {const xide_diagnostic_item* item=n->items+j;items[j]=(xui_code_diagnostic_t){.iSize=sizeof(*items),.tRange={item->start,item->end},.iSeverity=item->severity,.sMessage=item->message,.sSource="xlang",.iUserData=AUTO_ROUTE_BASE+(size_t)g->token*4096u+j};}
   int okay=xuiCodeAnnotationSetDiagnostics(xuiCodeEditGetAnnotations(d->editor),items,(int)n->count)==XUI_OK;free(items);
   if(!okay) {automaticFault(v,"诊断显示失败");return;}
   g->annotated=d;g->annotatedId=d->id;++a->installed;
  }
  g->applied=1;QueryPerformanceCounter(&now);if((now.QuadPart-began.QuadPart)*1000>=v->highlightClock.QuadPart) {xgeRenderRequest();break;}
 }
}
static int automaticProblemJump(xide_app* app,size_t row,void* owner)
{xide_automatic_diagnostics* a=owner;return app->languageView && app->languageView->autoView==a && row<a->routeCount && automaticJump(app,a->routes[row])>0;}
static void automaticProblems(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;if(!a->problemsDirty && !a->activate) return;
 void* owner=xideTaskViewLanguageProblemsOwner(v->app);
 if(!a->activate && ((owner && owner!=a) || (!owner && xideTaskViewSelected(v->app)) || !v->app->activeDocument || strcmp(v->app->activeDocument->language,"xlang"))) return;
 const char** titles=calloc(4096,sizeof(*titles));if(!titles) return;size_t count=0,total=automaticCount(v);
 for(size_t i=0;i<a->groupCount && count<4096;++i) if(a->groups[i].valid) {
  automatic_group* g=a->groups+i;for(size_t j=0;j<g->notice->count && count<4096;++j) {a->routes[count]=AUTO_ROUTE_BASE+(size_t)g->token*4096u+j;titles[count++]=g->notice->items[j].title;}
 }
 char scope[160];if(total>count) snprintf(scope,sizeof(scope),"xlang 自动检查 · %zu 个问题（显示前 %zu 项）",total,count);else snprintf(scope,sizeof(scope),"xlang 自动检查 · %zu 个问题",total);
 if(xideTaskViewShowLanguageProblemsRouted(v->app,scope,titles,count,a->activate,a,automaticProblemJump)==XUI_OK) {a->routeCount=count;a->problemsDirty=0;a->activate=0;}
 free(titles);
}
static void automaticManualFinished(xide_language_view* v)
{if(v->autoView) {v->autoView->activate=v->autoView->problemsDirty=1;}v->autoManual=0;}
static void automaticGuard(xide_language_view* v,const xvalue* params)
{
 /* A local sync already revoked the old navigation state. Do not let an old
  * guard notification cancel the receipt queued after that same update. */
 if(!v->receiptReady) return;
 const char* uri=string(get(params,"uri"));int version=0;if(!uri || !number(get(params,"version"),&version)) return;
 for(size_t i=0;i<v->inputCount;++i) if(!strcmp(v->inputs[i].uri,uri) && v->inputs[i].wireVersion==version) {clearDiagnostics(v->app);return;}
}
static int automaticJump(xide_app* app,size_t token)
{
 if(token<AUTO_ROUTE_BASE) return -1;
 xide_language_view* v=app ? app->languageView : NULL;xide_automatic_diagnostics* a=v ? v->autoView : NULL;if(!a || !v->autoEnabled) return 0;
 if(!automaticModels(v)) return 0;
 size_t encoded=token-AUTO_ROUTE_BASE,id=encoded/4096u,item=encoded%4096u;automatic_group* g=NULL;
 for(size_t i=0;i<a->groupCount;++i) if(a->groups[i].token==id) {g=a->groups+i;break;}
 if(!g || !g->valid || !automaticCurrent(v,g->notice) || item>=g->notice->count) return 0;
 xide_diagnostic_notice* n=g->notice;xide_document* target=openPath(app,n->path);const xvalue* proof=get(get(n->params,"xlang"),"document");
 if(!n->overlay) {
  if(!diskIdentity(n->path,proof)) {automaticFence(v,n->path,0,1,2);return 0;}
  xide_model_error error={0};if(!target && !xideEditorOpen(app,n->path,&error)) return 0;if(!target) target=app->activeDocument;
  if(!target || snapshotConflict(target) || !textMatches(proof,target) || !matchesHash(target->baseline.raw,target->baseline.rawLength,get(n->input,"rawSha256")) || !diskIdentity(n->path,proof)) {automaticFence(v,n->path,0,1,2);return 0;}
 }
 return target && selectRange(app,target,get(xrtValueArrayGet(get(n->params,"diagnostics"),item),"range"));
}
static int automaticConfiguration(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;const char* compiler=xideTaskViewCompiler(v->app);char* package=NULL;xide_model_error error={0};int valid=xideLanguageCheckPackagePath(&package,&error);
 int same=a->configured && a->packageValid==valid && (a->compiler && compiler ? !strcmp(a->compiler,compiler) : a->compiler==compiler) && (a->package && package ? !strcmp(a->package,package) : a->package==package);
 if(!same) {
  char* copied=compiler ? copy(compiler) : NULL;if(compiler && !copied) {free(package);return 0;}
  int resetConnection=a->configured && v->client;free(a->compiler);free(a->package);a->compiler=copied;a->package=package;package=NULL;a->packageValid=valid;a->configured=1;a->paused=0;
  if(resetConnection) reset(v->app);
 }
 free(package);return valid && compiler;
}
static void automaticPoll(xide_app* app)
{
 xide_language_view* v=app->languageView;xide_automatic_diagnostics* a=v->autoView;if(!a) return;
 LARGE_INTEGER began,now;QueryPerformanceCounter(&began);
 if(!automaticModels(v) || !automaticConfiguration(v)) return;
 if(!automaticRetire(a)) {xgeRenderRequest();return;}
 if(a->paused) return;
 if(!v->client && !v->autoProbed) {
  xide_document* root=automaticRoot(v);if(!root) return;
  if(!connectClient(app,root)) {automaticPause(v);return;}
  v->snapshotMode=1;v->document=root;v->documentId=root->id;v->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(root->editor));free(v->path);v->path=copy(root->path);v->invalidated=1;
 }
 if(!v->autoEnabled) return;
 if(!automaticInputsCurrent(v) || !a->syncAdopted) {
  if(!automaticPrepare(app,0)) return;
 }
 automaticDrain(v);if(!v->autoEnabled) return;
 automaticWatches(v);if(!v->autoEnabled) return;
 if(v->document && !v->busy && !v->syncMode && !v->receiptReady && !v->receiptRequest && current(v)) finishOnDemandSync(app);
 automaticInstall(v);if(!v->autoEnabled) return;
 automaticProblems(v);QueryPerformanceCounter(&now);double elapsed=(double)(now.QuadPart-began.QuadPart)*1000.0/(double)v->highlightClock.QuadPart;if(elapsed>a->maxOwnerMs) a->maxOwnerMs=elapsed;
}
