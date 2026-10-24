#include "analysis/ast_clone.h"
#include <xrt.h>
#include <string.h>

typedef struct clone_context {
    xls_ast_clone_options options;
    xmap sources;
    size_t bytes;
    xls_ast_cursor_source_policy cursorPolicy;
    void* policyUser;
    xls_ast_storage* storage;
    int logicalBudget;
    size_t poolStart,poolAllowance;
} clone_context;
static int charge(clone_context* c,size_t bytes)
{
    if(bytes>SIZE_MAX-c->bytes || (c->options.bytes && bytes>c->options.bytes-c->bytes)) return 0;
    c->bytes+=bytes; return 1;
}
static char* duplicate(clone_context* c,const char* text)
{ return text && charge(c,strlen(text)+1) ? xrtStrDup(text) : NULL; }
static int source(clone_context* c,const XL6_AstSource* in,XL6_AstNode* out)
{
    if(!in || c->options.dropSources) return 1;
    xbytesview key={(const unsigned char*)&in,sizeof(in)};
    XL6_AstSource** found=xrtMapGet(&c->sources,key); XL6_AstSource* value=found ? *found : NULL;
    if(value) { out->source=value; ++value->references; return 1; }
    if(c->options.targetSource) {
        XL6_AstSource* target=c->options.targetSource(in,c->options.user);
        if(target) {
            if(target==in || !target->references || !!target->path!=!!in->path ||
               (in->path && strcmp(target->path,in->path)) || target->completionQuery!=in->completionQuery) return 0;
            if((target->completionStart!=in->completionStart || target->completionEnd!=in->completionEnd) &&
               (in->completionQuery!=1 || !c->cursorPolicy || !c->cursorPolicy(in,target,c->policyUser))) return 0;
            if(!xrtMapSet(&c->sources,key,&target)) return 0;
            out->source=target; ++target->references; return 1;
        }
    }
    if(!value) {
        if(!charge(c,sizeof(*value))) return 0;
        value=xrtCalloc(1,sizeof(*value)); if(!value) return 0;
        *value=*in; value->references=0; value->path=duplicate(c,in->path);
        if((in->path && !value->path) || !xrtMapSet(&c->sources,key,&value)) { xrtFree(value->path); xrtFree(value); return 0; }
    }
    out->source=value; ++value->references; return 1;
}
static XL6_AstNode* clone(clone_context* c,const XL6_AstNode* in,size_t depth)
{
    if(!in || depth>512) return NULL;
    XL6_AstNode* out;
    if(c->storage && c->logicalBudget){
        if(!charge(c,sizeof(*in)))return NULL;
        size_t used=xlsAstStorageWork(c->storage).bytes-c->poolStart;
        if(used>c->poolAllowance)return NULL;
        out=xlsAstStorageNode(c->storage,c->poolAllowance-used);if(!out)return NULL;
    }else if(c->storage){
        size_t before=xlsAstStorageWork(c->storage).bytes;
        out=xlsAstStorageNode(c->storage,c->options.bytes ? c->options.bytes-c->bytes : SIZE_MAX);
        if(!out)return NULL;
        if(!charge(c,xlsAstStorageWork(c->storage).bytes-before)){xlsAstStorageFreeNode(out);return NULL;}
    }else{
        if(!charge(c,sizeof(*in)))return NULL;
        out=xrtCalloc(1,sizeof(*out));if(!out)return NULL;
    }
    *out=*in; out->xlsStorage=c->storage;out->text=out->doc=NULL; out->literalData=NULL; out->source=NULL;
    out->children=NULL; out->childCount=out->childCapacity=0; out->inferredType=out->semanticDeclaredType=NULL;
    int transformed=!c->options.transform || c->options.transform(in,out,c->options.user);
    out->xlsStorage=c->storage; /* a transform never transfers allocation ownership */
    if(!transformed) goto failed;
    if(in->text && !(out->text=duplicate(c,in->text))) goto failed;
    if(in->doc && !(out->doc=duplicate(c,in->doc))) goto failed;
    if(!source(c,in->source,out)) goto failed;
    if(in->literalData) {
        if(in->literalSize==SIZE_MAX || !charge(c,in->literalSize+1) || !xl6AstSetLiteralData(out,in->literalData,in->literalSize)) goto failed;
        out->literalDecoded=in->literalDecoded;
    }
    if(in->childCount) {
        if(in->childCount>SIZE_MAX/sizeof(*out->children) || !charge(c,in->childCount*sizeof(*out->children))) goto failed;
        out->children=xrtCalloc(in->childCount,sizeof(*out->children)); if(!out->children) goto failed;
        out->childCapacity=in->childCount;
        for(size_t i=0;i<in->childCount;++i) {
            XL6_AstNode* child=clone(c,in->children[i],depth+1); if(!child) goto failed;
            out->children[out->childCount++]=child;
        }
    }
    if(in->inferredType && !(out->inferredType=clone(c,in->inferredType,depth+1))) goto failed;
    if(in->semanticDeclaredType && !(out->semanticDeclaredType=clone(c,in->semanticDeclaredType,depth+1))) goto failed;
    return out;
failed: xl6AstFree(out); return NULL;
}
XL6_AstNode* xlsAstClonePooled(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,size_t* bytes)
{
    if(bytes) *bytes=0;
    if(work)*work=(xls_ast_pool_work){0};
    clone_context context={.cursorPolicy=policy,.policyUser=user}; if(options) context.options=*options;
    if(!input || !xrtMapInit(&context.sources,sizeof(XL6_AstSource*))) return NULL;
    if(poolBudget){
        context.storage=xlsAstStorageCreate(poolBudget);
        if(!context.storage || !charge(&context,xlsAstStorageWork(context.storage).bytes)){
            xlsAstStorageUnpin(context.storage);xrtMapUnit(&context.sources);return NULL;
        }
    }
    XL6_AstNode* root=clone(&context,input,0); xrtMapUnit(&context.sources);
    if(root && bytes) *bytes=context.bytes;
    if(root && work)*work=xlsAstStorageWork(context.storage);
    xlsAstStorageUnpin(context.storage);
    return root;
}
XL6_AstNode* xlsAstCloneWithCursorSource(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t* bytes)
{return xlsAstClonePooled(input,options,policy,user,0,NULL,bytes);}
XL6_AstNode* xlsAstClone(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes)
{return xlsAstCloneWithCursorSource(input,options,NULL,NULL,bytes);}
XL6_AstNode* xlsAstCloneInStorage(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_storage* storage,size_t extraPoolBytes,size_t* bytes)
{
    if(bytes)*bytes=0;
    if(!input || !storage)return NULL;
    clone_context context={.storage=storage,.logicalBudget=1,.poolStart=xlsAstStorageWork(storage).bytes,.poolAllowance=extraPoolBytes};
    if(options)context.options=*options;
    if(!xrtMapInit(&context.sources,sizeof(XL6_AstSource*)))return NULL;
    XL6_AstNode* root=clone(&context,input,0);xrtMapUnit(&context.sources);
    if(root && bytes)*bytes=context.bytes;
    return root;
}
