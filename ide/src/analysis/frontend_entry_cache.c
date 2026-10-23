#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_context.h"
#include "analysis/ast_clone.h"
#include "src/sem/xl6_completion.h"
#include <xrt.h>
#include <string.h>
#define ENTRY_BYTES (32u*1024u*1024u)
struct xls_entry_cache_data { xls_context* context;char *path,*text;size_t length,bytes;XL6_AstNode* root; };
struct xls_entry_cache { xls_entry_cache_data* data;size_t limit;int active; };
static _Thread_local xls_entry_cache_scope* current;
static void releaseData(xls_entry_cache_data* data)
{if(data) {xlsContextRelease(data->context);xrtFree(data->path);xrtFree(data->text);xl6AstFree(data->root);xrtFree(data);}}
xls_entry_cache* xlsEntryCacheCreate(size_t bytes)
{
    if(!bytes || bytes>ENTRY_BYTES) return NULL;
    xls_entry_cache* cache=xrtCalloc(1,sizeof(*cache));if(cache) cache->limit=bytes;return cache;
}
void xlsEntryCacheDestroy(xls_entry_cache* cache)
{if(cache) {releaseData(cache->data);xrtFree(cache);}}
size_t xlsEntryCacheBytes(const xls_entry_cache* cache) {return cache && cache->data ? cache->data->bytes : 0;}
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
        scope->cache->active=0;scope->work.bytes=xlsEntryCacheBytes(scope->cache);
    }
    releaseData(scope->pending);scope->pending=NULL;xrtFree(scope->canonicalEntry);scope->canonicalEntry=NULL;current=scope->previous;return scope->work;
}
static xls_entry_cache_data* capture(xls_entry_cache_scope* scope,const char* text,const char* path,size_t length,const XL6_AstNode* root)
{
    size_t pathBytes=strlen(path)+1,contextBytes=xlsContextBytes(scope->context),limit=scope->cache->limit;
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
int xlsFrontendEntryParse(const char* text,const char* path,XL6_ParseResult* result,XL6_CompletionQuery* query)
{
    xls_entry_cache_scope* scope=current;
    if(!scope || !scope->cache || !text || !path || !result || result->root || result->diags.count ||
       !scope->entry || (strcmp(path,scope->entry) && (!scope->canonicalEntry || strcmp(path,scope->canonicalEntry))) ||
       (query && xl6CompletionTarget(query,path)))
        return xl6ParseSourceWithCompletion(text,path,result,query);
    size_t length=strlen(text);xls_entry_cache_data* old=scope->cache->data;
    if(old && xlsFrontendContextsEqual(old->context,scope->context) && !strcmp(old->path,path) && old->length==length && !memcmp(old->text,text,length)) {
        xls_ast_clone_options options={.bytes=scope->cache->limit};XL6_AstNode* tree=xlsAstClone(old->root,&options,NULL);
        if(tree) {result->root=tree;result->diags.defaultFile=xl6AstSourcePath(tree);++scope->work.reused;return 1;}
        ++scope->work.declined;
    }
    ++scope->work.parsed;
    int okay=xl6ParseSourceWithCompletion(text,path,result,query);
    if(okay && result->root && !result->diags.count) {
        xls_entry_cache_data* data=capture(scope,text,path,length,result->root);
        if(data) {releaseData(scope->pending);scope->pending=data;}
        else ++scope->work.declined;
    }
    return okay;
}
