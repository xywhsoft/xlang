/* Current original checker observations. Only immutable bytes survive this
 * synchronous call;never retain a context,AST,report or earlier memo owner. */
static int xl6SemIsFuncDecl(const XL6_AstNode*);
static const char* xl6SemFuncName(const XL6_AstNode*);
static int xlsSemReadHeaderKey(XL6_SemContext*,const char*,xstrbuf*,size_t*);
/* Separate namespaces: a global value/function query and a registry type
 * query with the same spelling are different original compiler reads. */
#include "analysis/frontend_sem_read_values.h"
typedef struct xls_sem_read {char* name;xstrbuf key;size_t count;int type;} xls_sem_read;
struct xls_sem_reads {xarray items;size_t bytes;int complete;};
struct xls_sem_read_context {xatomic32 references;xstrbuf key;size_t bytes;};
typedef struct xls_sem_read_capture {
 struct xls_sem_read_capture* previous;XL6_SemContext* context;XL6_Scope* global;xls_sem_build* build;xls_sem_reads* reads;
 xstrbuf scratch;uint64_t observations,typedComparisons,scratchReuses,scratchBytes;
 xarray values;size_t valueStrings,valueBytes;int valuesReady;
} xls_sem_read_capture;
typedef struct xls_sem_read_cached {xstrbuf key;size_t count;uint64_t epoch;} xls_sem_read_cached;
static _Thread_local xls_sem_read_capture* xlsActiveSemReads;
static _Thread_local xls_sem_read_control* xlsActiveReadControl;
static _Thread_local xls_sem_scratch_control* xlsActiveScratchControl;
static _Thread_local xls_sem_value_control* xlsActiveValueControl;
static int xlsSemValuesEnabled(void){return !xlsActiveValueControl || xlsActiveValueControl->enabled;}
void xlsFrontendSemValuesBegin(xls_sem_value_control* c,int enabled)
{if(c){*c=(xls_sem_value_control){xlsActiveValueControl,enabled && xlsSemValuesEnabled()};xlsActiveValueControl=c;}}
void xlsFrontendSemValuesEnd(xls_sem_value_control* c)
{if(c && xlsActiveValueControl==c)xlsActiveValueControl=c->previous;}
static size_t xlsSemScratchBudget(void){return xlsActiveScratchControl ? xlsActiveScratchControl->bytes : XLS_SEM_KEY_BYTES;}
void xlsFrontendSemScratchBegin(xls_sem_scratch_control* c,size_t bytes)
{if(c){size_t limit=xlsSemScratchBudget();*c=(xls_sem_scratch_control){xlsActiveScratchControl,bytes<limit ? bytes : limit};xlsActiveScratchControl=c;}}
void xlsFrontendSemScratchEnd(xls_sem_scratch_control* c)
{if(c && xlsActiveScratchControl==c)xlsActiveScratchControl=c->previous;}
static size_t xlsSemReadBudget(void){return xlsActiveReadControl ? xlsActiveReadControl->bytes : 8u*1024u*1024u;}
void xlsFrontendSemReadsBegin(xls_sem_read_control* c,size_t bytes)
{if(c){size_t limit=xlsSemReadBudget();*c=(xls_sem_read_control){xlsActiveReadControl,bytes<limit ? bytes : limit};xlsActiveReadControl=c;}}
void xlsFrontendSemReadsEnd(xls_sem_read_control* c)
{if(c && xlsActiveReadControl==c)xlsActiveReadControl=c->previous;}
xls_sem_read_work xlsFrontendSemMemoReads(const xls_sem_memo* m)
{return m ? m->readWork : (xls_sem_read_work){0};}
static void xlsSemReadsDestroy(xls_sem_reads* reads)
{if(reads){for(size_t i=0;i<reads->items.Count;++i){xls_sem_read* r=xrtArrayGet(&reads->items,i);xrtFree(r->name);xrtStrBufFree(&r->key);}xrtArrayUnit(&reads->items);xrtFree(reads);}}
static void xlsSemReadContextRelease(xls_sem_read_context* c)
{if(c && xrtAtomic32FetchSub(&c->references,1,XMEMORY_ACQ_REL)==1){xrtStrBufFree(&c->key);xrtFree(c);}}
static void xlsSemReadCacheDrop(xbytesview key,void* value,void* user)
{(void)key;(void)user;xrtStrBufFree(&((xls_sem_read_cached*)value)->key);}
static int xlsSemReadContextKey(XL6_SemContext* ctx,xstrbuf* out)
{
 if(!ctx || !ctx->root || ctx->completionQuery || !ctx->scope || ctx->scope->parent)return 0;
 XL6_AstNode root=*ctx->root;root.children=NULL;root.childCount=0;
 if(!xlsSemKeyText(out,ctx->moduleName) || !xlsSemKeyNumber(out,ctx->captureIndex) || !xlsSemKeyNumber(out,ctx->nextSymbolId) ||
    !xlsSemKeyNumber(out,ctx->types.nextId) || !xlsSemGlobalState(ctx,out) || !xlsSemKeyNode(out,&root,0,0) || !xlsSemKeyNumber(out,ctx->root->childCount))return 0;
 for(size_t i=0;i<ctx->root->childCount;++i){const XL6_AstNode* n=ctx->root->children[i];
  if(!xl6SemIsFuncDecl(n)){if(!xlsSemKeyNumber(out,0) || !xlsSemKeyNode(out,n,1,1))return 0;}
  else{const XL6_AstNode* name=n->childCount ? n->children[0] : NULL;
   uint64_t ids[]={1,n->semanticSymbolId,n->semanticDeclaredSymbolId,name ? name->semanticSymbolId : 0,name ? name->semanticDeclaredSymbolId : 0};
   if(!xlsSemKeyBytes(out,ids,sizeof(ids)))return 0;}
 }
 /* Scope FUNC values preserve allocation and any cached type state. Names
  * and headers are proved by real lookup snapshots,not by this skeleton. */
 if(!xlsSemKeyNumber(out,ctx->scope->count))return 0;
 for(size_t i=0;i<ctx->scope->count;++i){const XL6_Symbol* s=&ctx->scope->items[i];if(s->kind!=XL6_SYM_FUNC)continue;
  if(!xlsSemKeyNumber(out,s->id) || !xlsSemKeyNumber(out,s->kind) || !xlsSemKeyNumber(out,s->definitelyAssigned) || !xlsSemKeyNode(out,s->type,0,0))return 0;}
 return 1;
}
static void xlsSemReadInvalidate(xls_sem_build* b)
{
 if(!b)return;
 if(b->readEpoch==UINT64_MAX){xrtMapUnit(&b->readCache);b->readCacheReady=0;b->readCacheBytes=0;b->readEpoch=1;}
 else ++b->readEpoch;
}
static void xlsSemReadOwnChanged(xls_sem_build* b,const XL6_AstNode* node)
{
 const char* name=xl6SemFuncName(node);if(!b || !b->readCacheReady || !name)return;
 size_t length=strlen(name);if(length>256)return;unsigned char bytes[257]={0};memcpy(bytes+1,name,length);
 xls_sem_read_cached* r=xrtMapGet(&b->readCache,(xbytesview){bytes,length+1});if(r)r->epoch=0;
}
static void xlsSemReadDisable(xls_sem_build* b)
{b->compatible=b->readCompatible=0;xlsSemReadInvalidate(b);}
static int xlsSemReadCompatible(const xls_sem_build* b){return b->compatible || b->readCompatible;}
static void xlsSemReadContextBegin(xls_sem_build* b,XL6_SemContext* ctx)
{
 xls_sem_memo* m=b->memo;size_t budget=xlsSemReadBudget();if(budget<=sizeof(xls_sem_read_context) || ctx->completionQuery)return;
 if(b->compatible && b->baseline->readContext){m->readContext=b->baseline->readContext;xrtAtomic32FetchAdd(&m->readContext->references,1,XMEMORY_RELAXED);}
 else{
  xls_sem_read_context* proof=xrtCalloc(1,sizeof(*proof));if(!proof)return;xrtAtomic32Init(&proof->references,1);
  if(!xlsSemReadContextKey(ctx,&proof->key) || proof->key.Capacity>budget-sizeof(*proof)){xlsSemReadContextRelease(proof);return;}
  proof->bytes=sizeof(*proof)+proof->key.Capacity;m->readContext=proof;++m->readWork.contexts;
 }
 if(m->readContext->bytes>budget || m->readContext->bytes>XLS_SEM_MEMO_BYTES-m->bytes){xlsSemReadContextRelease(m->readContext);m->readContext=NULL;return;}
 m->bytes+=m->readContext->bytes;b->readEpoch=1;
 b->readCacheReady=xrtMapInit(&b->readCache,sizeof(xls_sem_read_cached)) && xrtMapSetDrop(&b->readCache,xlsSemReadCacheDrop,NULL);
 b->readCompatible=b->baseline && b->baseline->readContext && !strcmp(m->path,b->baseline->path) && m->entries.Count==b->baseline->entries.Count &&
  xlsSemEqualKey(&m->readContext->key,&b->baseline->readContext->key);
 if(b->readCompatible)++m->readWork.contextHits;
}
static int xlsSemReadSame(const xls_sem_read* r,const xstrbuf* key,size_t count)
{return r->count==count && xlsSemEqualKey(&r->key,key);}
static int xlsSemReadTypeKey(XL6_SemContext* ctx,const char* name,xstrbuf* key,size_t* count)
{
 const XL6_TypeInfo* info=xl6TypeRegistryFind(&ctx->types,name);*count=info!=NULL;
 if(!xlsSemKeyNumber(key,*count))return 0;
 if(!info)return 1;
 return xlsSemKeyNumber(key,info->id) && xlsSemKeyNumber(key,info->abiId) &&
  xlsSemKeyText(key,info->name) && xlsSemKeyText(key,info->abiName) &&
  xlsSemKeyNumber(key,info->kind) && xlsSemKeyNumber(key,info->allowGeneric) &&
  xlsSemKeyNumber(key,info->genericArity) && xlsSemKeyNode(key,info->decl,0,0);
}
static int xlsSemReadKey(XL6_SemContext* ctx,const char* name,int type,xstrbuf* key,size_t* count)
{return type ? xlsSemReadTypeKey(ctx,name,key,count) : xlsSemReadHeaderKey(ctx,name,key,count);}
static void xlsSemReadsUnknown(XL6_SemContext* ctx)
{(void)ctx;if(xlsActiveSemReads && xlsActiveSemReads->reads)xlsActiveSemReads->reads->complete=0;}
static void xlsSemReadValuesDrop(xls_sem_read_capture* c)
{
 for(size_t i=0;i<c->values.Count;++i)xlsSemReadValueUnit(xrtArrayGet(&c->values,i));
 xrtArrayUnit(&c->values);c->valuesReady=0;c->valueStrings=0;
}
static void xlsSemReadValueRemember(xls_sem_read_capture* c,size_t slot,const XL6_TypeInfo* info)
{
 if(!c->valuesReady || (info && info->decl))return;
 size_t limit=1024u*1024u,strings=info ? (info->name ? strlen(info->name)+1 : 0)+(info->abiName ? strlen(info->abiName)+1 : 0) : 0;
 if(c->valueStrings>limit || strings>limit-c->valueStrings ||
    !xrtArrayResize(&c->values,slot+1) || c->values.Capacity>limit/sizeof(xls_sem_read_value) ||
    c->values.Capacity*sizeof(xls_sem_read_value)>limit-c->valueStrings-strings){xlsSemReadValuesDrop(c);return;}
 xls_sem_read_value* value=xrtArrayGet(&c->values,slot);
 if(!xlsSemReadValueInit(value,info)){xlsSemReadValuesDrop(c);return;}
 c->valueStrings+=value->stringsBytes;size_t bytes=c->values.Capacity*sizeof(xls_sem_read_value)+c->valueStrings;
 if(bytes>c->valueBytes)c->valueBytes=bytes;
}
static xstrbuf xlsSemReadScratchTake(xls_sem_read_capture* c)
{
 xstrbuf key=c->scratch;c->scratch=(xstrbuf){0};
 if(key.Capacity)++c->scratchReuses;
 key.Size=0;if(key.Data)key.Data[0]=0;return key;
}
static void xlsSemReadScratchReturn(xls_sem_read_capture* c,xstrbuf* key)
{
 if(key->Capacity && key->Capacity<=xlsSemScratchBudget()){
  xrtStrBufFree(&c->scratch);c->scratch=*key;*key=(xstrbuf){0};
  if(c->scratch.Capacity>c->scratchBytes)c->scratchBytes=c->scratch.Capacity;
 }
 xrtStrBufFree(key);
}
static void xlsSemReadsObserve(XL6_SemContext* ctx,const char* name,int type,const XL6_TypeInfo* actual)
{
 xls_sem_read_capture* c=xlsActiveSemReads;if(!c || !c->reads || !c->reads->complete)return;
 ++c->observations;
 xls_sem_reads* reads=c->reads;size_t n=name ? strlen(name) : 0;
 if(ctx!=c->context || !n || n>256 || reads->items.Count>=256){reads->complete=0;return;}
 for(size_t i=0;i<reads->items.Count;++i){const xls_sem_read* r=xrtArrayConstGet(&reads->items,i);if(r->type==type && !strcmp(r->name,name)){
   const xls_sem_read_value* value=type && i<c->values.Count ? xrtArrayConstGet(&c->values,i) : NULL;
   if(value && value->ready){++c->typedComparisons;if(!xlsSemReadValueEqual(value,actual))reads->complete=0;return;}
   xstrbuf key=xlsSemReadScratchTake(c);size_t count=0;
   if(!xlsSemReadKey(ctx,name,type,&key,&count) || !xlsSemReadSame(r,&key,count))reads->complete=0;
   xlsSemReadScratchReturn(c,&key);return;}}
 xstrbuf key={0};size_t count=0;
 if(!xlsSemReadKey(ctx,name,type,&key,&count)){reads->complete=0;xrtStrBufFree(&key);return;}
 size_t limit=xlsSemReadBudget();if(limit>1024u*1024u)limit=1024u*1024u;
 xls_sem_read next={.key=key,.count=count,.type=type};next.name=xrtStrDup(name);
 size_t added=sizeof(next)+n+1+key.Capacity;
 if(!next.name || reads->bytes>limit || added>limit-reads->bytes || !xrtArrayPush(&reads->items,&next)){
  xrtFree(next.name);xrtStrBufFree(&key);reads->complete=0;return;}
 if(type)xlsSemReadValueRemember(c,reads->items.Count-1,actual);
 reads->bytes+=added;++c->build->memo->readWork.recorded;if(!count)++c->build->memo->readWork.negative;
}
static void xlsSemReadsName(XL6_SemContext* ctx,const char* name)
{xlsSemReadsObserve(ctx,name,0,NULL);}
/* The generated semantic TU routes every registry Find call here. The
 * original registry still resolves it; observe its complete current record
 * and declaration,including annotations changed while checking classes.
 * Registry enumeration is confined to completion,where this path is off. */
static const XL6_TypeInfo* xlsSemReadTypeFind(const XL6_TypeRegistry* registry,const char* name)
{
 const XL6_TypeInfo* info=xl6TypeRegistryFind(registry,name);xls_sem_read_capture* c=xlsActiveSemReads;
 if(c){if(registry!=&c->context->types)xlsSemReadsUnknown(c->context);else xlsSemReadsObserve(c->context,name,1,info);}
 return info;
}
static XL6_Symbol* xlsSemScopeFindLocalOriginal(XL6_Scope*,const char*);
static XL6_Symbol* xl6ScopeFindLocal(XL6_Scope* scope,const char* name)
{
 XL6_Symbol* symbol=xlsSemScopeFindLocalOriginal(scope,name);xls_sem_read_capture* c=xlsActiveSemReads;
 if(c && scope && !scope->parent){if(scope!=c->global)xlsSemReadsUnknown(c->context);else xlsSemReadsName(c->context,name);}
 return symbol;
}
static void xlsSemReadsStart(xls_sem_read_capture* c,xls_sem_build* build,XL6_SemContext* ctx)
{
 *c=(xls_sem_read_capture){.previous=xlsActiveSemReads,.context=ctx,.global=ctx->scope,.build=build};
 if(build->memo->readContext && !ctx->completionQuery){c->reads=xrtCalloc(1,sizeof(*c->reads));if(c->reads){c->reads->bytes=sizeof(*c->reads);c->reads->complete=xrtArrayInit(&c->reads->items,sizeof(xls_sem_read));}}
 if(c->reads && c->reads->complete && xlsSemValuesEnabled())c->valuesReady=xrtArrayInit(&c->values,sizeof(xls_sem_read_value));
 xlsActiveSemReads=c;
}
static xls_sem_reads* xlsSemReadsFinish(xls_sem_read_capture* c)
{
 xlsActiveSemReads=c->previous;xls_sem_reads* reads=c->reads;
 if(reads && reads->complete)for(size_t i=0;i<reads->items.Count;++i){const xls_sem_read* r=xrtArrayConstGet(&reads->items,i);xstrbuf current=xlsSemReadScratchTake(c);size_t count=0;
  int okay=xlsSemReadKey(c->context,r->name,r->type,&current,&count) && xlsSemReadSame(r,&current,count);xlsSemReadScratchReturn(c,&current);if(!okay){reads->complete=0;break;}}
 if(reads){reads->bytes-=reads->items.Count*sizeof(xls_sem_read);reads->bytes+=reads->items.Capacity*sizeof(xls_sem_read);
  size_t limit=xlsSemReadBudget();if(limit>1024u*1024u)limit=1024u*1024u;if(reads->bytes>limit)reads->complete=0;}
 xrtStrBufFree(&c->scratch);xlsSemReadValuesDrop(c);xls_sem_read_work* work=&c->build->memo->readWork;
 work->observations+=c->observations;work->typedComparisons+=c->typedComparisons;work->scratchReuses+=c->scratchReuses;
 if(c->scratchBytes>work->scratchBytes)work->scratchBytes=c->scratchBytes;
 if(c->valueBytes>work->valueBytes)work->valueBytes=c->valueBytes;
 xlsSemReadInvalidate(c->build);return reads;
}
static int xlsSemReadMatch(xls_sem_build* b,XL6_SemContext* ctx,const xls_sem_reads* reads,size_t slot)
{
 if(!b->readCompatible || !reads || !reads->complete)return 0;
 for(size_t i=b->readCursor;i<slot;++i)if(!xlsSemFunction(ctx->root->children[i]) && ctx->root->children[i]->kind!=XL6_AST_IMPORT){xlsSemReadInvalidate(b);break;}
 b->readCursor=slot+1;
 for(size_t i=0;i<reads->items.Count;++i){const xls_sem_read* r=xrtArrayConstGet(&reads->items,i);unsigned char bytes[257];size_t length=strlen(r->name);
  bytes[0]=(unsigned char)r->type;memcpy(bytes+1,r->name,length);xbytesview name={bytes,length+1};
  xls_sem_read_cached* cached=b->readCacheReady ? xrtMapGet(&b->readCache,name) : NULL;
  if(cached && cached->epoch==b->readEpoch){if(!xlsSemReadSame(r,&cached->key,cached->count))goto mismatch;}
  else{
   xstrbuf key={0};size_t count=0;int same=xlsSemReadKey(ctx,r->name,r->type,&key,&count) && xlsSemReadSame(r,&key,count);
   if(same && b->readCacheReady){size_t old=cached ? cached->key.Capacity : 0,budget=xlsSemReadBudget();
    if(b->readCacheBytes>=old && b->readCacheBytes-old<=budget && key.Capacity<=budget-(b->readCacheBytes-old)){
     xls_sem_read_cached next={key,count,b->readEpoch};
     if(cached){xrtStrBufFree(&cached->key);*cached=next;b->readCacheBytes=b->readCacheBytes-old+key.Capacity;key=(xstrbuf){0};}
     else if(xrtMapSet(&b->readCache,name,&next)){b->readCacheBytes+=key.Capacity;key=(xstrbuf){0};}
    }}
   xrtStrBufFree(&key);if(!same)goto mismatch;
  }
  ++b->memo->readWork.matched;
 }
 return 1;
mismatch:++b->memo->readWork.rejected;return 0;
}
