#include "analysis/signature_internal.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_raw_body.h"
#include "analysis/module_store.h"
#include "analysis/frontend_context.h"
#include "analysis/ast_clone.h"
#include "analysis/ast_materialize.h"
#include "src/sem/xl6_completion.h"
#include <xrt.h>
#include <string.h>
#define ENTRY_BYTES (32u*1024u*1024u)
struct xls_entry_cache_data { xls_context* context;char *path,*text;size_t length,bytes;XL6_AstNode* root;xls_syntax* syntax;struct xls_entry_cache_data *previous,*next; };
struct xls_entry_cache { xls_entry_cache_data *data,*first,*last;size_t limit,moduleBytes;int active;xmap modules; };
static _Thread_local xls_entry_cache_scope* current;
static void releaseData(xls_entry_cache_data* data)
{if(data) {xlsContextRelease(data->context);xrtFree(data->path);if(!data->syntax)xrtFree(data->text);xlsSyntaxRelease(data->syntax);xl6AstFree(data->root);xrtFree(data);}}
static xbytesview key(const char* path) {return (xbytesview){(const unsigned char*)path,strlen(path)};}
static void unlinkModule(xls_entry_cache* cache,xls_entry_cache_data* data)
{
    if(data->previous) data->previous->next=data->next;else cache->first=data->next;
    if(data->next) data->next->previous=data->previous;else cache->last=data->previous;
    data->previous=data->next=NULL;
}
static void linkModule(xls_entry_cache* cache,xls_entry_cache_data* data)
{
    data->next=cache->first;data->previous=NULL;
    if(cache->first) cache->first->previous=data;else cache->last=data;
    cache->first=data;
}
static void dropModule(xbytesview name,void* value,void* user)
{
    (void)name;xls_entry_cache* cache=user;xls_entry_cache_data* data=*(xls_entry_cache_data**)value;
    if(data) {unlinkModule(cache,data);cache->moduleBytes-=data->bytes;releaseData(data);}
}
static void dropPending(xbytesview name,void* value,void* user)
{(void)name;(void)user;releaseData(*(xls_entry_cache_data**)value);}
xls_entry_cache* xlsEntryCacheCreate(size_t bytes)
{
    if(!bytes || bytes>ENTRY_BYTES) return NULL;
    xls_entry_cache* cache=xrtCalloc(1,sizeof(*cache));if(!cache) return NULL;
    cache->limit=bytes;
    if(!xrtMapInit(&cache->modules,sizeof(xls_entry_cache_data*)) || !xrtMapSetDrop(&cache->modules,dropModule,cache)) {xrtMapUnit(&cache->modules);xrtFree(cache);return NULL;}
    return cache;
}
void xlsEntryCacheDestroy(xls_entry_cache* cache)
{if(cache) {xrtMapUnit(&cache->modules);releaseData(cache->data);xrtFree(cache);}}
size_t xlsEntryCacheBytes(const xls_entry_cache* cache) {return cache && cache->data ? cache->data->bytes : 0;}
size_t xlsModuleCacheBytes(const xls_entry_cache* cache) {return cache ? cache->moduleBytes : 0;}
static void trimModules(xls_entry_cache* cache,size_t budget)
{
    while(cache->last && (cache->moduleBytes>budget || xrtMapCount(&cache->modules)>512))
        if(!xrtMapRemove(&cache->modules,key(cache->last->path))) break;
}
static void commitModules(xls_entry_cache_scope* scope)
{
    xls_module_store* shared=xlsModuleScopeCurrent();
    if(shared){
        if(scope->pendingModulesReady){
            xmapiter iterator;if(!xrtMapIterBegin(&scope->pendingModules,&iterator))return;
            xls_entry_cache_data** pending;
            while((pending=xrtMapIterNext(&iterator,NULL))!=NULL)
                if(!xlsModuleStorePublish(shared,(*pending)->path,(*pending)->syntax))++scope->moduleWork.declined;
            xrtMapIterEnd(&iterator);
        }
        return;
    }
    xls_entry_cache* cache=scope->cache;size_t budget=cache->limit-xlsEntryCacheBytes(cache);
    trimModules(cache,budget);
    if(!scope->pendingModulesReady) return;
    xmapiter iterator;if(!xrtMapIterBegin(&scope->pendingModules,&iterator)) return;
    xls_entry_cache_data** pending;
    while((pending=xrtMapIterNext(&iterator,NULL))!=NULL) {
        xls_entry_cache_data* data=*pending;
        if(data->bytes>budget) {++scope->moduleWork.declined;continue;}
        /* Obtain a stable destination before any eviction. A key allocation
         * failure preserves the old cache and never affects compiler output. */
        xls_entry_cache_data** slot=xrtMapGetOrAdd(&cache->modules,key(data->path),NULL);
        if(!slot) {++scope->moduleWork.declined;continue;}
        if(*slot) {dropModule((xbytesview){0},slot,cache);*slot=NULL;}
        trimModules(cache,budget-data->bytes);
        *slot=data;*pending=NULL;linkModule(cache,data);cache->moduleBytes+=data->bytes;
    }
    xrtMapIterEnd(&iterator);
}
void xlsFrontendEntryBegin(xls_entry_cache_scope* scope,xls_entry_cache* cache,const xls_context* context,const char* entry)
{
    *scope=(xls_entry_cache_scope){.previous=current,.context=context,.entry=entry};
    if(cache && !cache->active) {scope->cache=cache;cache->active=1;scope->canonicalEntry=entry ? xrtPathAbs(entry) : NULL;}
    current=scope;
}
xls_completion_entry_work xlsFrontendEntryEnd(xls_entry_cache_scope* scope,int commit)
{
    if(current!=scope) return (xls_completion_entry_work){0};
    if(scope->cache) {
        if(commit && scope->pending) {releaseData(scope->cache->data);scope->cache->data=scope->pending;scope->pending=NULL;}
        if(commit) commitModules(scope);
        scope->cache->active=0;scope->work.bytes=xlsEntryCacheBytes(scope->cache);scope->moduleWork.bytes=xlsModuleCacheBytes(scope->cache);
    }
    if(scope->pendingModulesReady) xrtMapUnit(&scope->pendingModules);
    releaseData(scope->pending);scope->pending=NULL;xrtFree(scope->canonicalEntry);scope->canonicalEntry=NULL;current=scope->previous;return scope->work;
}
static xls_entry_cache_data* capture(xls_entry_cache_scope* scope,const char* text,const char* path,size_t length,const XL6_AstNode* root,size_t limit,size_t extra)
{
    size_t pathBytes=strlen(path)+1,contextBytes=xlsContextBytes(scope->context);
    if(extra>limit || contextBytes>limit-extra) return NULL;
    contextBytes+=extra;
    if(contextBytes>limit || sizeof(xls_entry_cache_data)>limit-contextBytes) return NULL;
    size_t bytes=contextBytes+sizeof(xls_entry_cache_data);
    if(pathBytes>limit-bytes || length>=limit-bytes-pathBytes) return NULL;
    bytes+=pathBytes+length+1;if(bytes==limit) return NULL;
    xls_entry_cache_data* data=xrtCalloc(1,sizeof(*data));if(!data) return NULL;
    data->context=xlsContextRetain((xls_context*)scope->context);data->path=xrtStrDup(path);data->text=xrtStrDupN(text,length);data->length=length;
    xls_ast_clone_options options={.bytes=limit-bytes};size_t treeBytes=0;
    if(data->path && data->text) data->root=xlsAstClone(root,&options,&treeBytes);
    if(!data->root) {releaseData(data);return NULL;}
    data->bytes=bytes+treeBytes;return data;
}
static int matches(const xls_entry_cache_data* old,const xls_context* context,const char* text,const char* path,size_t length)
{return old && xlsFrontendContextsEqual(old->context,context) && !strcmp(old->path,path) && old->length==length && !memcmp(old->text,text,length);}
static int copy(const xls_entry_cache_data* old,size_t limit,XL6_ParseResult* result)
{
    if(old->syntax) {
        if(!xlsRawBodyCopyParse(old->syntax,result)) return 0;
        result->diags.defaultFile=xl6AstSourcePath(result->root); return 1;
    }
    xls_ast_clone_options options={.bytes=limit};XL6_AstNode* tree=xlsAstMaterialize(old->root,&options,NULL);
    if(!tree) return 0;
    result->root=tree;result->diags.defaultFile=xl6AstSourcePath(tree);return 1;
}
static xls_entry_cache_data* captureSyntax(xls_entry_cache_scope* scope,const char* path,
    xls_syntax* syntax,size_t limit)
{
    size_t fixed=sizeof(xls_entry_cache_data),context=xlsContextBytes(scope->context),pathBytes=strlen(path)+1,tree=0;
    if(!syntax || context>limit || fixed>limit-context || pathBytes>(limit-context-fixed)/2) return NULL;
    fixed+=context+2*pathBytes;
    if(!xlsSyntaxCacheBytes(syntax,limit-fixed,&tree)) return NULL;
    xls_entry_cache_data* data=xrtCalloc(1,sizeof(*data));if(!data) return NULL;
    data->context=xlsContextRetain((xls_context*)scope->context);data->path=xrtStrDup(path);
    data->syntax=xlsSyntaxRetain(syntax);data->text=(char*)xlsTextData(xlsSyntaxText(syntax));data->length=xlsTextSize(xlsSyntaxText(syntax));data->bytes=fixed+tree;
    if(!data->path) {releaseData(data);return NULL;}return data;
}
static void stageModule(xls_entry_cache_scope* scope,const char* path,xls_entry_cache_data* data,size_t retained)
{
    if(!data) {++scope->moduleWork.declined;return;}
    if(!scope->pendingModulesReady) {
        if(xrtMapInit(&scope->pendingModules,sizeof(xls_entry_cache_data*)) && xrtMapSetDrop(&scope->pendingModules,dropPending,NULL)) scope->pendingModulesReady=1;
        else xrtMapUnit(&scope->pendingModules);
    }
    if(scope->pendingModulesReady && (xrtMapHas(&scope->pendingModules,key(path)) || xrtMapCount(&scope->pendingModules)<512) && xrtMapSet(&scope->pendingModules,key(path),&data)) scope->pendingModuleBytes=retained+data->bytes;
    else {releaseData(data);++scope->moduleWork.declined;}
}
static int sharedParse(xls_entry_cache_scope* scope,xls_module_store* store,const char* text,const char* path,
    XL6_ParseResult* result,XL6_CompletionQuery* query,int entry)
{
    size_t length=strlen(text);int exact=0;xls_entry_parse_work* measured=entry ? &scope->work : &scope->moduleWork;
    xls_module_snapshot* old=xlsModuleStoreAcquire(store,path,text,length,&exact);
    if(exact){
        if(xlsRawBodyCopyParse(xlsModuleSnapshotSyntax(old),result)){
            result->diags.defaultFile=xl6AstSourcePath(result->root);++measured->reused;xlsModuleSnapshotRelease(old);return 1;
        }
        ++measured->declined;
    }
    xls_entry_cache_data** pending=scope->pendingModulesReady ? xrtMapGet(&scope->pendingModules,key(path)) : NULL;
    size_t budget=scope->cache->limit,retained=scope->pendingModuleBytes-(pending ? (*pending)->bytes : 0);
    xls_text* body=old ? xlsTextCreate(text,length,budget,NULL) : NULL;xls_parse_work work={0};
    xls_syntax* syntax=body ? xlsSyntaxTryFunction(path,body,xlsModuleSnapshotSyntax(old),&work) : NULL;
    if(!entry){scope->moduleSyntaxWork.fragments+=work.fragments;scope->moduleSyntaxWork.bytes+=work.bytes;}
    if(syntax && xlsRawBodyCopyParse(syntax,result)){
        result->diags.defaultFile=xl6AstSourcePath(result->root);++measured->parsed;
        if(!entry)scope->moduleSyntaxWork.reused+=work.reused;
        stageModule(scope,path,retained<budget ? captureSyntax(scope,path,syntax,budget-retained) : NULL,retained);
        xlsSyntaxRelease(syntax);xlsTextRelease(body);xlsModuleSnapshotRelease(old);return 1;
    }
    xlsSyntaxRelease(syntax);xlsTextRelease(body);xlsModuleSnapshotRelease(old);
    ++measured->parsed;if(!entry){++scope->moduleSyntaxWork.full;scope->moduleSyntaxWork.bytes+=length;}
    int okay=xl6ParseSourceWithCompletion(text,path,result,query);
    if(okay && result->root && !result->diags.count){
        /* Same original parse and private compiler tree. The shared ordinary
         * chunks own an independent clone;publication stays at EntryEnd. */
        xls_entry_cache_data* copy=retained<budget ? capture(scope,text,path,length,result->root,budget-retained,strlen(path)+1) : NULL;
        xls_entry_cache_data* data=NULL;
        if(copy){
            body=xlsTextCreate(text,length,budget,NULL);syntax=body ? xlsSyntaxAdoptOrdinary(path,body,&copy->root) : NULL;
            data=syntax ? captureSyntax(scope,path,syntax,budget-retained) : NULL;
            xlsSyntaxRelease(syntax);xlsTextRelease(body);releaseData(copy);
        }
        if(data && data->syntax)xlsRawBodyRegisterSyntax(data->syntax,result->root);
        stageModule(scope,path,data,retained);
    }
    return okay;
}
static int moduleParse(xls_entry_cache_scope* scope,const char* text,const char* path,XL6_ParseResult* result,XL6_CompletionQuery* query)
{
    xls_module_store* shared=xlsModuleScopeCurrent();if(shared)return sharedParse(scope,shared,text,path,result,query,0);
    xls_entry_cache* cache=scope->cache;size_t length=strlen(text);
    xls_entry_cache_data** pending=scope->pendingModulesReady ? xrtMapGet(&scope->pendingModules,key(path)) : NULL;
    xls_entry_cache_data** stored=xrtMapGet(&cache->modules,key(path));
    xls_entry_cache_data* old=pending ? *pending : stored ? *stored : NULL;
    if(matches(old,scope->context,text,path,length)) {
        if(copy(old,cache->limit,result)) {
            if(!pending) {unlinkModule(cache,old);linkModule(cache,old);}
            ++scope->moduleWork.reused;return 1;
        }
        ++scope->moduleWork.declined;
    }
    size_t entry=scope->pending ? scope->pending->bytes : xlsEntryCacheBytes(cache);
    size_t retained=scope->pendingModuleBytes-(pending ? (*pending)->bytes : 0);
    size_t budget=cache->limit-entry;
    if(old && old->syntax && xlsFrontendContextsEqual(old->context,scope->context) && !strcmp(old->path,path)) {
        xls_text* body=xlsTextCreate(text,length,cache->limit,NULL);xls_parse_work work={0};
        xls_syntax* syntax=body ? xlsSyntaxTryFunction(path,body,old->syntax,&work) : NULL;
        scope->moduleSyntaxWork.fragments+=work.fragments;scope->moduleSyntaxWork.bytes+=work.bytes;
        if(syntax && xlsRawBodyCopyParse(syntax,result)) {
            result->diags.defaultFile=xl6AstSourcePath(result->root);
            ++scope->moduleWork.parsed;scope->moduleSyntaxWork.reused+=work.reused;
            stageModule(scope,path,retained<budget ? captureSyntax(scope,path,syntax,budget-retained) : NULL,retained);
            xlsSyntaxRelease(syntax);xlsTextRelease(body);return 1;
        }
        xlsSyntaxRelease(syntax);xlsTextRelease(body);
    }
    ++scope->moduleWork.parsed;
    ++scope->moduleSyntaxWork.full;scope->moduleSyntaxWork.bytes+=length;
    int okay=xl6ParseSourceWithCompletion(text,path,result,query);
    if(okay && result->root && !result->diags.count) {
        xls_entry_cache_data* data=retained<budget ? capture(scope,text,path,length,result->root,budget-retained,strlen(path)+1) : NULL;
        if(data) {
            xls_text* body=xlsTextCreate(text,length,cache->limit,NULL);
            xls_syntax* syntax=body ? xlsSyntaxAdoptOrdinary(path,body,&data->root) : NULL;
            xls_entry_cache_data* chunks=syntax ? captureSyntax(scope,path,syntax,budget-retained) : NULL;
            xlsSyntaxRelease(syntax);xlsTextRelease(body);releaseData(data);data=chunks;
        }
        if(data && data->syntax)xlsRawBodyRegisterSyntax(data->syntax,result->root);
        stageModule(scope,path,data,retained);
    }
    return okay;
}
int xlsFrontendEntryParse(const char* text,const char* path,XL6_ParseResult* result,XL6_CompletionQuery* query)
{
    xls_entry_cache_scope* scope=current;
    if(scope && scope->cache && query && path && xl6CompletionTarget(query,path) && xlsSignatureScopeCurrent(query))
        return xlsSignatureParseCached(text,path,result,query);
    if(scope && scope->cursorSyntax && text && path && result && query && xl6CompletionTarget(query,path)) {
        const xls_syntax* syntax=scope->cursorSyntax;const xls_text* source=xlsSyntaxText(syntax);
        size_t skip=source && xlsTextSize(source)>=3 && !memcmp(xlsTextData(source),"\xef\xbb\xbf",3) ? 3 : 0;
        if(source && !strcmp(path,xlsSyntaxPath(syntax)) && strlen(text)==xlsTextSize(source)-skip &&
           !memcmp(text,xlsTextData(source)+skip,xlsTextSize(source)-skip) && xlsRawBodyCopyCursorParse(syntax,query,result)) {
            scope->cursorApplied=1;result->diags.defaultFile=xl6AstSourcePath(result->root);return xlsSyntaxAccepted(syntax);
        }
        scope->cursorFailed=1;return 0;
    }
    if(!scope || !scope->cache || !text || !path || !result || result->root || result->diags.count ||
       !scope->entry || (query && xl6CompletionTarget(query,path)))
        return xl6ParseSourceWithCompletion(text,path,result,query);
    if(strcmp(path,scope->entry) && (!scope->canonicalEntry || strcmp(path,scope->canonicalEntry)))
        return moduleParse(scope,text,path,result,query);
    xls_module_store* shared=xlsModuleScopeCurrent();if(shared)return sharedParse(scope,shared,text,path,result,query,1);
    size_t length=strlen(text);xls_entry_cache_data* old=scope->cache->data;
    if(matches(old,scope->context,text,path,length)) {
        if(copy(old,scope->cache->limit,result)) {++scope->work.reused;return 1;}
        ++scope->work.declined;
    }
    ++scope->work.parsed;
    int okay=xl6ParseSourceWithCompletion(text,path,result,query);
    if(okay && result->root && !result->diags.count) {
        size_t budget=scope->cache->limit-scope->pendingModuleBytes;
        xls_entry_cache_data* data=capture(scope,text,path,length,result->root,budget,0);
        if(data) {releaseData(scope->pending);scope->pending=data;}
        else ++scope->work.declined;
    }
    return okay;
}
