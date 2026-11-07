/* One bounded worker per help view. It never reads or mutates GUI state.
 * Jobs retain the immutable catalog that owns their document pointers. */
typedef struct help_job {struct help_job* next;LONG generation;xide_catalog_lease* lease;const xide_help_document** docs;size_t count;xide_help_view* model;} help_job;
typedef struct help_preparer {
 CRITICAL_SECTION lock;HANDLE wake,thread;volatile LONG generation;int stop,garbageCount;
 help_job *pending,*done,*garbage;
 xide_help_view* models[8];size_t modelBytes[8],bytes;unsigned next;
 volatile LONG builds,hits;
} help_preparer;
static void modelRelease(xide_help_view* model) {
 if(model && !InterlockedDecrement(&model->references)){clear(model,0);xideCatalogRelease(model->lease);free(model);}
}
static void jobFree(help_job* job) {if(job){modelRelease(job->model);xideCatalogRelease(job->lease);free(job->docs);free(job);}}
static xide_help_view* prepareModel(help_job* job) {
 xide_help_view* v=calloc(1,sizeof(*v));if(!v)return NULL;v->modelOnly=1;v->references=1;int result=XUI_OK;
 v->documents=calloc(job->count ? job->count:1,sizeof(*v->documents));if(!v->documents){free(v);return NULL;}
 memcpy(v->documents,job->docs,job->count*sizeof(*v->documents));v->documentCount=job->count;
 size_t sourceBytes=0;
 for(size_t i=0;i<job->count && result==XUI_OK;++i) {
  size_t bytes=strlen(job->docs[i]->markdown);
  if(bytes>16u*1024u*1024u-sourceBytes){result=XUI_DOC_ERROR_LIMIT;break;}sourceBytes+=bytes;
  const xide_help_document* source=job->docs[i];size_t first=v->blockCount;char title[1024];snprintf(title,sizeof(title),"章节导航 · %s",source->title);result=chapter(v,title,first);
  if(result==XUI_OK)result=addBlock(v,source->title,1,NULL);
  if(result==XUI_OK){char origin[1024];snprintf(origin,sizeof(origin),"文档来源：%s:%d",source->source,source->sourceLine);result=addBlock(v,origin,0,NULL);}
  xui_document document=NULL;xui_document_snapshot snapshot=NULL;xui_doc_desc_t desc={0};desc.iSize=sizeof(desc);desc.iProfile=XUI_DOCUMENT_MARKDOWN;desc.bDisableHistory=1;desc.iMaxTextBytes=4u*1024u*1024u;desc.iMaxNodes=100000;
  if(result==XUI_OK)result=xuiDocumentCreate(&desc,&document);
  if(result==XUI_OK)result=xuiDocumentLoadMarkdown(document,source->markdown,strlen(source->markdown));
  if(result==XUI_OK)result=xuiDocumentAcquireSnapshot(document,&snapshot);
  if(result==XUI_OK)result=renderNode(v,snapshot,XUI_DOCUMENT_ROOT,"",0);
  xuiDocumentSnapshotRelease(snapshot);xuiDocumentRelease(document);
  for(size_t b=first;b<v->blockCount;++b)v->blocks[b]->source=source;
 }
 if(result!=XUI_OK){const xide_help_document** docs=v->documents;size_t count=v->documentCount;v->documents=NULL;clear(v,0);v->documents=docs;v->documentCount=count;if(addBlock(v,"文档超过显示限额或格式无效；请使用声明/文档来源查看原文。",0,NULL)!=XUI_OK){clear(v,0);free(v);return NULL;}if(count)v->blocks[0]->source=docs[0];}
 v->lease=job->lease;job->lease=NULL;return v;
}
static int sameModel(const xide_help_view* v,const help_job* j) {
 return v && v->lease==j->lease && v->documentCount==j->count && (!j->count || !memcmp(v->documents,j->docs,j->count*sizeof(*j->docs)));
}
static DWORD WINAPI prepareWorker(void* user) {
 help_preparer* p=user;
 for(;;) {
  WaitForSingleObject(p->wake,INFINITE);EnterCriticalSection(&p->lock);
  help_job* garbage=p->garbage;p->garbage=NULL;p->garbageCount=0;help_job* job=p->pending;p->pending=NULL;int stop=p->stop;LeaveCriticalSection(&p->lock);
  while(garbage){help_job* next=garbage->next;jobFree(garbage);garbage=next;}
  if(stop){jobFree(job);break;}
  if(!job)continue;
  if(job->generation!=InterlockedCompareExchange(&p->generation,0,0)){jobFree(job);continue;}
  xide_help_view* model=NULL;for(int i=0;i<8;++i)if(sameModel(p->models[i],job)){model=p->models[i];InterlockedIncrement(&model->references);InterlockedIncrement(&p->hits);break;}
  if(!model) {
   /* Reloads must not retain eight complete old catalog snapshots. */
   for(int i=0;i<8;++i)if(p->models[i] && p->models[i]->lease!=job->lease){modelRelease(p->models[i]);p->models[i]=NULL;p->bytes-=p->modelBytes[i];p->modelBytes[i]=0;}
   model=prepareModel(job);InterlockedIncrement(&p->builds);
   if(model) {
    size_t bytes=sizeof(*model)+model->blockCount*sizeof(help_block);
    for(size_t i=0;i<model->blockCount;++i)bytes+=strlen(model->blocks[i]->text)+1;
    if(bytes<=16u*1024u*1024u) {
     unsigned slot=p->next++%8;modelRelease(p->models[slot]);p->bytes-=p->modelBytes[slot];p->models[slot]=NULL;p->modelBytes[slot]=0;
     for(int i=0;i<8 && p->bytes+bytes>16u*1024u*1024u;++i)if(p->models[i]){p->bytes-=p->modelBytes[i];p->modelBytes[i]=0;modelRelease(p->models[i]);p->models[i]=NULL;}
     p->models[slot]=model;p->modelBytes[slot]=bytes;p->bytes+=bytes;InterlockedIncrement(&model->references);
    }
   }
  }
  job->model=model;
  EnterCriticalSection(&p->lock);
  if(p->stop || job->generation!=p->generation){LeaveCriticalSection(&p->lock);jobFree(job);continue;}
  help_job* previous=p->done;p->done=job;LeaveCriticalSection(&p->lock);jobFree(previous);xgeRenderRequest();
 }
 for(int i=0;i<8;++i)modelRelease(p->models[i]);
 return 0;
}
static help_preparer* preparer(xide_help_view* v) {
 if(v->preparer)return v->preparer;
 help_preparer* p=calloc(1,sizeof(*p));if(!p)return NULL;InitializeCriticalSection(&p->lock);p->wake=CreateEventW(NULL,FALSE,FALSE,NULL);
 if(p->wake)p->thread=CreateThread(NULL,0,prepareWorker,p,0,NULL);
 if(!p->thread){if(p->wake)CloseHandle(p->wake);DeleteCriticalSection(&p->lock);free(p);return NULL;}
 v->preparer=p;return p;
}
static void cancelPrepared(xide_help_view* v) {if(v->preparer){InterlockedIncrement(&v->preparer->generation);SetEvent(v->preparer->wake);}}
static int retirePage(xide_help_view* v) {
 if(!v->blockCount && !v->activeModel)return 1;
 int slot=!v->retired[0] ? 0:!v->retired[1] ? 1:-1;if(slot<0)return 0;
 xide_help_view* old=calloc(1,sizeof(*old));if(!old)return 0;
 old->blocks=v->blocks;old->blockCount=v->blockCount;old->chapters=v->chapters;old->chapterCount=v->chapterCount;old->documents=v->documents;old->documentCount=v->documentCount;old->pageTitles=v->pageTitles;old->pageCount=v->pageCount;old->pageRoot=v->pageRoot;old->storage=v->storage;old->borrowed=v->borrowed;old->activeModel=v->activeModel;
 if(old->pageRoot)xuiWidgetSetVisible(old->pageRoot,0);
 else for(size_t i=0;i<old->blockCount;++i)if(old->blocks[i]->widget)xuiWidgetSetVisible(old->blocks[i]->widget,0);
 v->blocks=NULL;v->blockCount=0;v->chapters=NULL;v->chapterCount=0;v->documents=NULL;v->documentCount=0;v->pageTitles=NULL;v->pageCount=0;v->pageRoot=NULL;v->storage=NULL;v->activeModel=NULL;v->borrowed=0;v->retired[slot]=old;v->retiredAt[slot]=0;return 1;
}
static void collectPages(xide_help_view* v) {
 for(int slot=0;slot<2;++slot)if(v->retired[slot]) {
  xide_help_view* old=v->retired[slot];int budget=2;
  while(v->retiredAt[slot]<old->blockCount && budget>0){help_block* b=old->blocks[v->retiredAt[slot]++];if(b->widget){xuiWidgetDestroy(b->widget);b->widget=NULL;--budget;}}
  if(v->retiredAt[slot]<old->blockCount)continue;
  help_job* release=NULL;if(old->activeModel){release=calloc(1,sizeof(*release));if(!release)continue;release->model=old->activeModel;}
  if(release) {
   help_preparer* p=v->preparer;
   if(!p || !TryEnterCriticalSection(&p->lock)){free(release);continue;}
   if(p->garbageCount>=8){LeaveCriticalSection(&p->lock);free(release);continue;}
   release->next=p->garbage;p->garbage=release;++p->garbageCount;LeaveCriticalSection(&p->lock);SetEvent(p->wake);
  }
  clear(old,1);free(old);v->retired[slot]=NULL;
 }
}
int xideHelpSetDeferred(xide_help_view* v,const xide_help_document* const* docs,size_t count,const xide_command_catalog* catalog) {
 if(!v || !catalog || !catalog->lease || (count && !docs) || count>4096)return XUI_ERROR_INVALID_ARGUMENT;
 for(size_t i=0;i<count;++i)if(!docs[i])return XUI_ERROR_INVALID_ARGUMENT;
 if(v->requestedGeneration!=v->adoptedGeneration && v->targetLease==catalog->lease && count==v->targetCount && (!count || !memcmp(v->targetDocuments,docs,count*sizeof(*docs))))return XUI_OK;
 if(v->requestedGeneration==v->adoptedGeneration && count==v->documentCount && (!count || !memcmp(v->documents,docs,count*sizeof(*docs))))return XUI_OK;
 if(count>v->targetCapacity){const xide_help_document** target=realloc(v->targetDocuments,count*sizeof(*target));if(!target)return XUI_ERROR_OUT_OF_MEMORY;v->targetDocuments=target;v->targetCapacity=count;}
 help_preparer* p=preparer(v);if(!p)return XUI_ERROR_OUT_OF_MEMORY;
 help_job* j=calloc(1,sizeof(*j));if(!j)return XUI_ERROR_OUT_OF_MEMORY;
 j->docs=malloc((count ? count:1)*sizeof(*docs));j->lease=xideCatalogRetain(catalog);j->count=count;
 if(!j->docs || !j->lease){jobFree(j);return XUI_ERROR_OUT_OF_MEMORY;}if(count)memcpy(j->docs,docs,count*sizeof(*docs));
 if(!TryEnterCriticalSection(&p->lock)){jobFree(j);return XUI_ERROR_INVALID_STATE;}
 if(p->pending && p->garbageCount>=8){LeaveCriticalSection(&p->lock);jobFree(j);return XUI_ERROR_INVALID_STATE;}
 j->generation=InterlockedIncrement(&p->generation);v->requestedGeneration=j->generation;
 v->targetCount=count;v->targetLease=catalog->lease;if(count)memcpy(v->targetDocuments,docs,count*sizeof(*docs));
 if(p->pending){p->pending->next=p->garbage;p->garbage=p->pending;++p->garbageCount;}p->pending=j;
 LeaveCriticalSection(&p->lock);SetEvent(p->wake);
 xuiWidgetSetVisible(v->content,0);xuiWidgetSetVisible(v->navigator,0);xuiWidgetSetVisible(v->pages,0);return XUI_OK;
}
static int adoptModel(xide_help_view* v,xide_help_view* model) {
 if(v->retired[0] && v->retired[1])return 0;
 xide_help_view fresh={0};fresh.app=v->app;fresh.borrowed=1;fresh.virtualized=1;
 fresh.blocks=calloc(model->blockCount ? model->blockCount:1,sizeof(*fresh.blocks));fresh.storage=calloc(model->blockCount ? model->blockCount:1,sizeof(*fresh.storage));fresh.documents=calloc(model->documentCount ? model->documentCount:1,sizeof(*fresh.documents));
 if(!fresh.blocks || !fresh.storage || !fresh.documents){clear(&fresh,1);return 0;}
 fresh.blockCount=model->blockCount;fresh.documentCount=model->documentCount;
 memcpy(fresh.documents,model->documents,model->documentCount*sizeof(*fresh.documents));
 fresh.chapters=model->chapters;fresh.chapterCount=model->chapterCount;
 for(size_t i=0;i<fresh.blockCount;++i){fresh.storage[i]=*model->blocks[i];fresh.blocks[i]=fresh.storage+i;fresh.blocks[i]->view=v;}
 fresh.pageCount=(fresh.chapterCount+XUI_COMBOBOX_ITEM_CAPACITY-1)/XUI_COMBOBOX_ITEM_CAPACITY;
 fresh.pageTitles=calloc(fresh.pageCount ? fresh.pageCount:1,sizeof(*fresh.pageTitles));
 if(!fresh.pageTitles){clear(&fresh,1);return 0;}
 for(size_t i=0;i<fresh.pageCount;++i){char title[80];snprintf(title,sizeof(title),"章节分组 %zu",i+1);fresh.pageTitles[i]=malloc(strlen(title)+1);if(!fresh.pageTitles[i]){clear(&fresh,1);return 0;}strcpy(fresh.pageTitles[i],title);}
 if(xuiWidgetCreate(v->app->context,&fresh.pageRoot)!=XUI_OK || xuiWidgetSetLayoutType(fresh.pageRoot,XUI_LAYOUT_MANUAL)!=XUI_OK || xuiWidgetSetSizeMode(fresh.pageRoot,XUI_SIZE_FILL,XUI_SIZE_FIXED)!=XUI_OK || xuiWidgetAddChild(v->content,fresh.pageRoot)!=XUI_OK){clear(&fresh,1);return 0;}
 if(!retirePage(v)){clear(&fresh,1);return 0;}
 v->blocks=fresh.blocks;v->storage=fresh.storage;v->blockCount=fresh.blockCount;v->documents=fresh.documents;v->documentCount=fresh.documentCount;
 v->chapters=fresh.chapters;v->chapterCount=fresh.chapterCount;v->pageTitles=fresh.pageTitles;v->pageCount=fresh.pageCount;v->pageRoot=fresh.pageRoot;v->activeModel=model;
 v->borrowed=1;v->virtualized=1;v->width=-1;v->height=0;v->naturalWidth=0;v->updating=1;
 xuiComboBoxSetItems(v->navigator,NULL,0);xuiComboBoxSetItems(v->pages,NULL,0);
 if(v->pageCount){xuiComboBoxSetItems(v->pages,(const char**)v->pageTitles,(int)v->pageCount);pageItems(v,0);}
 v->updating=0;v->pendingChapter=SIZE_MAX;
 xuiWidgetSetVisible(v->content,v->blockCount>0);xuiWidgetSetVisible(v->navigator,v->chapterCount>0);xuiWidgetSetVisible(v->pages,v->pageCount>1);return 1;
}
int xideHelpPoll(xide_help_view* v) {
 if(!v || !v->preparer)return XUI_OK;
 collectPages(v);help_preparer* p=v->preparer;
 if(!TryEnterCriticalSection(&p->lock))return XUI_OK;
 help_job* j=p->done;
 if(!j){LeaveCriticalSection(&p->lock);return XUI_OK;}
 if(j->generation!=p->generation){p->done=NULL;j->next=p->garbage;p->garbage=j;++p->garbageCount;LeaveCriticalSection(&p->lock);SetEvent(p->wake);return XUI_OK;}
 p->done=NULL;LeaveCriticalSection(&p->lock);
 if(j->model && adoptModel(v,j->model)){j->model=NULL;v->adoptedGeneration=j->generation;xgeRenderRequest();}
 else if(j->model){EnterCriticalSection(&p->lock);if(!p->done)p->done=j;else {j->next=p->garbage;p->garbage=j;++p->garbageCount;}LeaveCriticalSection(&p->lock);SetEvent(p->wake);return XUI_OK;}
 if(!j->model)v->adoptedGeneration=j->generation;
 EnterCriticalSection(&p->lock);j->next=p->garbage;p->garbage=j;++p->garbageCount;LeaveCriticalSection(&p->lock);SetEvent(p->wake);return XUI_OK;
}
int xideHelpPending(xide_help_view* v) {return v && v->requestedGeneration!=v->adoptedGeneration;}
void xideHelpPreparationCounts(xide_help_view* v,unsigned* builds,unsigned* hits) {if(builds)*builds=v && v->preparer ? (unsigned)InterlockedCompareExchange(&v->preparer->builds,0,0):0;if(hits)*hits=v && v->preparer ? (unsigned)InterlockedCompareExchange(&v->preparer->hits,0,0):0;}
static void destroyPreparer(xide_help_view* v) {
 help_preparer* p=v->preparer;if(!p)return;EnterCriticalSection(&p->lock);p->stop=1;InterlockedIncrement(&p->generation);LeaveCriticalSection(&p->lock);SetEvent(p->wake);WaitForSingleObject(p->thread,INFINITE);
 jobFree(p->pending);jobFree(p->done);while(p->garbage){help_job* j=p->garbage;p->garbage=j->next;jobFree(j);}CloseHandle(p->thread);CloseHandle(p->wake);DeleteCriticalSection(&p->lock);free(p);v->preparer=NULL;
}
