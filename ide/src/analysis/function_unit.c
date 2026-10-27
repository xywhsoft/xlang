#include "analysis/function_unit_internal.h"
#include <xrt.h>
#include <string.h>
#include <stdio.h>
#define FUNCTION_BYTES (64u*1024u*1024u)
#define ORIGIN_BYTES (16u*1024u*1024u)
#define FUNCTION_NODES 65536u
typedef struct function_row {const XL6_AstNode* node;uint32_t parent,inferred,declared;size_t first;} function_row;
typedef struct function_stop {int (*cancelled)(void*);void* user;} function_stop;
typedef struct origin_source {xls_function_source view;xfileinfo info;} origin_source;
struct xls_function_origin {
    xatomic32 references;xls_context* context;xls_module_graph* modules;xls_entity_view* entities;
    xarray sources,probes;const char* packagePath;int packagePathObserved;size_t bytes;
};
struct xls_function_unit {
    xatomic32 references;XL6_AstNode* root;xls_function_origin* origin;
    xarray nodes,children,facts;size_t limit,treeBytes,stringBytes,bytes;
};
static int stopped(const function_stop* stop){return stop->cancelled && stop->cancelled(stop->user);}
static int copyNode(const XL6_AstNode* input,XL6_AstNode* output,void* user)
{(void)input;(void)output;return !stopped(user);}
static int charge(size_t* total,size_t limit,size_t extra)
{if(*total>limit || extra>limit-*total)return 0;*total+=extra;return 1;}
static int duplicate(const char** out,const char* text,size_t* bytes,size_t limit)
{
    *out=NULL;if(!text)return 1;
    size_t length=strlen(text);if(length==SIZE_MAX || !charge(bytes,limit,length+1))return 0;
    *out=xrtStrDup(text);return *out!=NULL;
}
static xls_function_origin* originRetain(xls_function_origin* origin)
{if(origin)xrtAtomic32FetchAdd(&origin->references,1,XMEMORY_RELAXED);return origin;}
void xlsFunctionOriginRelease(xls_function_origin* o)
{
    if(!o || xrtAtomic32FetchSub(&o->references,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t i=0;i<o->sources.Count;++i){origin_source* s=xrtArrayGet(&o->sources,i);xrtFree((char*)s->view.path);xrtFree((char*)s->view.origin);}
    for(size_t i=0;i<o->probes.Count;++i)xrtFree((char*)((xls_input_probe*)xrtArrayGet(&o->probes,i))->path);
    xrtArrayUnit(&o->sources);xrtArrayUnit(&o->probes);xrtFree((char*)o->packagePath);
    xlsEntityViewRelease(o->entities);xlsModuleGraphRelease(o->modules);xlsContextRelease(o->context);xrtFree(o);
}
xls_function_origin* xlsFunctionOriginCapture(const XL6_SourceReport* report,xls_context* context,
    xls_module_graph* modules,const xls_entity_view* entities,int (*cancelled)(void*),void* user)
{
    if(!report || !report->complete || report->errorPath || report->error[0] || report->count>XL6_SOURCE_REPORT_FILES ||
       report->probeCount>65536 || (report->count && !report->items) || (report->probeCount && !report->probes))return NULL;
    function_stop stop={cancelled,user};if(stopped(&stop))return NULL;
    xls_function_origin* o=xrtCalloc(1,sizeof(*o));if(!o)return NULL;xrtAtomic32Init(&o->references,1);o->bytes=sizeof(*o);
    if(!xrtArrayInit(&o->sources,sizeof(origin_source)) || !xrtArrayInit(&o->probes,sizeof(xls_input_probe)) ||
       !xrtArrayResize(&o->sources,report->count) || !xrtArrayResize(&o->probes,report->probeCount) ||
       !charge(&o->bytes,ORIGIN_BYTES,o->sources.Capacity*sizeof(origin_source)) ||
       !charge(&o->bytes,ORIGIN_BYTES,o->probes.Capacity*sizeof(xls_input_probe)))goto failed;
    for(size_t i=0;i<report->count;++i){
        if(stopped(&stop))goto failed;
        const XL6_SourceReportItem* s=&report->items[i];origin_source* d=xrtArrayGet(&o->sources,i);
        if(!s->path || !duplicate(&d->view.path,s->path,&o->bytes,ORIGIN_BYTES) ||
           !duplicate(&d->view.origin,s->origin,&o->bytes,ORIGIN_BYTES))goto failed;
        d->view.size=s->size;d->view.rawSize=s->rawSize;d->view.kind=s->kind;d->info=s->info;
        memcpy(d->view.textSha256,s->textSha256,32);memcpy(d->view.rawSha256,s->rawSha256,32);
    }
    for(size_t i=0;i<report->probeCount;++i){
        if(stopped(&stop))goto failed;
        xls_input_probe* d=xrtArrayGet(&o->probes,i);if(!report->probes[i].path || !duplicate(&d->path,report->probes[i].path,&o->bytes,ORIGIN_BYTES))goto failed;d->exists=report->probes[i].exists;
    }
    if(!duplicate(&o->packagePath,report->packagePath,&o->bytes,ORIGIN_BYTES) || stopped(&stop))goto failed;
    o->packagePathObserved=report->packagePathObserved;o->context=xlsContextRetain(context);o->modules=xlsModuleGraphRetain(modules);o->entities=xlsEntityViewRetain((xls_entity_view*)entities);return o;
failed:xlsFunctionOriginRelease(o);return NULL;
}
static int unitBytes(xls_function_unit* u)
{
    size_t total=sizeof(*u);
    if(!charge(&total,u->limit,u->treeBytes) || !charge(&total,u->limit,u->stringBytes) ||
       !charge(&total,u->limit,u->nodes.Capacity*sizeof(function_row)) ||
       !charge(&total,u->limit,u->children.Capacity*sizeof(uint32_t)) ||
       !charge(&total,u->limit,u->facts.Capacity*sizeof(xls_index_fact)))return 0;
    u->bytes=total;return 1;
}
static int rows(xls_function_unit* u,const XL6_AstNode* node,uint32_t parent,size_t depth,uint32_t* id,function_stop* stop)
{
    *id=0;if(!node)return 1;
    if(stopped(stop) || depth>512 || u->nodes.Count>=FUNCTION_NODES || node->childCount>FUNCTION_NODES ||
       node->childCount>FUNCTION_NODES-u->children.Count || (node->childCount && !node->children))return 0;
    function_row row={.node=node,.parent=parent,.first=u->children.Count};
    if(!xrtArrayPush(&u->nodes,&row) || !xrtArrayResize(&u->children,u->children.Count+node->childCount) || !unitBytes(u))return 0;
    *id=(uint32_t)u->nodes.Count;
    for(size_t i=0;i<node->childCount;++i){uint32_t child;if(!rows(u,node->children[i],*id,depth+1,&child,stop))return 0;*(uint32_t*)xrtArrayGet(&u->children,row.first+i)=child;}
    uint32_t inferred,declared;
    if(!rows(u,node->inferredType,*id,depth+1,&inferred,stop) || !rows(u,node->semanticDeclaredType,*id,depth+1,&declared,stop))return 0;
    function_row* current=xrtArrayGet(&u->nodes,*id-1);current->inferred=inferred;current->declared=declared;return 1;
}
xls_function_unit* xlsFunctionUnitRetain(xls_function_unit* unit)
{if(unit)xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED);return unit;}
void xlsFunctionUnitRelease(xls_function_unit* u)
{
    if(!u || xrtAtomic32FetchSub(&u->references,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t i=0;i<u->facts.Count;++i)xrtFree((char*)((xls_index_fact*)xrtArrayGet(&u->facts,i))->canonical);
    xrtArrayUnit(&u->nodes);xrtArrayUnit(&u->children);xrtArrayUnit(&u->facts);xl6AstFree(u->root);xlsFunctionOriginRelease(u->origin);xrtFree(u);
}
xls_function_unit* xlsFrontendFunctionUnitCapture(const XL6_AstNode* root,const xls_semantic_index* index,
    xls_function_origin* origin,size_t bytes,int (*cancelled)(void*),void* user)
{
    if(!xlsFrontendFunctionDefinition(root) || !origin)return NULL;
    if(bytes>FUNCTION_BYTES)bytes=FUNCTION_BYTES;
    function_stop stop={cancelled,user};if(bytes<=sizeof(xls_function_unit) || stopped(&stop))return NULL;
    xls_function_unit* u=xrtCalloc(1,sizeof(*u));if(!u)return NULL;xrtAtomic32Init(&u->references,1);u->limit=bytes;
    u->root=xlsAstClone(root,&(xls_ast_clone_options){copyNode,&stop,bytes-sizeof(*u),0,NULL},&u->treeBytes);
    if(!u->root || !xrtArrayInit(&u->nodes,sizeof(function_row)) || !xrtArrayInit(&u->children,sizeof(uint32_t)) || !xrtArrayInit(&u->facts,sizeof(xls_index_fact)))goto failed;
    uint32_t id;if(!rows(u,u->root,0,0,&id,&stop) || id!=1)goto failed;
    size_t first=0,count=0;
    if(index && (!xlsFrontendIndexFunctionSpan(index,root,&first,&count) ||
       !xrtArrayResize(&u->facts,count) || !unitBytes(u)))goto failed;
    for(size_t i=0;i<count;++i){
        if(stopped(&stop))goto failed;
        xls_index_fact fact;if(!xlsSemanticIndexFact(index,first+i,&fact))goto failed;
        xls_index_fact* out=xrtArrayGet(&u->facts,i);*out=fact;out->canonical=NULL;
        if(!duplicate(&out->canonical,fact.canonical,&u->stringBytes,bytes) || !unitBytes(u))goto failed;
    }
    if(stopped(&stop))goto failed;
    u->origin=originRetain(origin);return u;
failed:xlsFunctionUnitRelease(u);return NULL;
}
static xls_index_range range(XL6_AstRange r)
{return (xls_index_range){r.startOffset,r.endOffset,r.startLine,r.startColumn,r.endLine,r.endColumn,r.known};}
int xlsFunctionUnitNode(const xls_function_unit* u,uint32_t id,xls_function_node* out)
{
    if(out)*out=(xls_function_node){0};
    if(!u || !out || !id || id>u->nodes.Count)return 0;
    const function_row* row=xrtArrayConstGet(&u->nodes,id-1);const XL6_AstNode* n=row->node;
    *out=(xls_function_node){.kindName=xl6AstKindName(n->kind),.text=n->text,.doc=n->doc,.sourcePath=xl6AstSourcePath(n),.literalData=n->literalData,
        .literalSize=n->literalSize,.children=n->childCount,.id=id,.parent=row->parent,.inferredType=row->inferred,.declaredType=row->declared,
        .kind=(uint32_t)n->kind,.flags=n->flags,.literalScalar=n->literalScalar,.type=n->semanticTypeId,.symbol=n->semanticSymbolId,
        .declaredSymbol=n->semanticDeclaredSymbolId,.targetSymbol=n->semanticTargetSymbolId,.compoundOperatorSymbol=n->semanticCompoundOperatorSymbolId,
        .typeAbi=n->semanticTypeAbiId,.scope=n->semanticScopeDepth,.targetScope=n->semanticTargetScopeDepth,.region=n->semanticRegionId,.targetRegion=n->semanticTargetRegionId,
        .literalDecoded=n->literalDecoded,.call=n->semanticCallKind,.lvalue=n->semanticLvalueKind,.ownership=n->semanticOwnership,.exit=n->semanticExitKind,
        .line=n->line,.column=n->column,.hasSource=n->source!=NULL,.range=range(n->range),.tokenRange=range(n->tokenRange)};
    if(n->source){out->completionQuery=n->source->completionQuery;out->completionStart=n->source->completionStart;out->completionEnd=n->source->completionEnd;}return 1;
}
int xlsFunctionUnitChild(const xls_function_unit* u,uint32_t id,size_t child,uint32_t* out)
{
    if(out)*out=0;
    if(!u || !out || !id || id>u->nodes.Count)return 0;
    const function_row* row=xrtArrayConstGet(&u->nodes,id-1);if(child>=row->node->childCount)return 0;
    *out=*(const uint32_t*)xrtArrayConstGet(&u->children,row->first+child);return 1;
}
xls_function_info xlsFunctionUnitInfo(const xls_function_unit* u)
{
    if(!u)return(xls_function_info){0};
    xls_entity_view_info entities=xlsEntityViewInfo(u->origin->entities);
    const char* name=u->root->childCount && u->root->children[0] ? u->root->children[0]->text : NULL;
    return(xls_function_info){name,xl6AstSourcePath(u->root),u->nodes.Count,u->facts.Count,u->treeBytes,u->bytes,u->origin->bytes,entities.domain,u->root->semanticSymbolId};
}
int xlsFunctionUnitFact(const xls_function_unit* u,size_t at,xls_index_fact* out)
{if(out)*out=(xls_index_fact){0};if(!u || !out || at>=u->facts.Count)return 0;*out=*(const xls_index_fact*)xrtArrayConstGet(&u->facts,at);return 1;}
int xlsFunctionUnitInputs(const xls_function_unit* u,xls_inputs* out)
{if(out)*out=(xls_inputs){0};if(!u || !out)return 0;xls_function_origin* o=u->origin;*out=(xls_inputs){o->sources.Count,o->probes.Count,o->packagePath,o->packagePathObserved};return 1;}
int xlsFunctionUnitSource(const xls_function_unit* u,size_t at,xls_function_source* out)
{if(out)*out=(xls_function_source){0};if(!u || !out || at>=u->origin->sources.Count)return 0;*out=((const origin_source*)xrtArrayConstGet(&u->origin->sources,at))->view;return 1;}
int xlsFunctionUnitProbe(const xls_function_unit* u,size_t at,xls_input_probe* out)
{if(out)*out=(xls_input_probe){0};if(!u || !out || at>=u->origin->probes.Count)return 0;*out=*(const xls_input_probe*)xrtArrayConstGet(&u->origin->probes,at);return 1;}
const xls_context* xlsFunctionUnitContext(const xls_function_unit* u){return u?u->origin->context:NULL;}
const xls_entity_view* xlsFunctionUnitEntities(const xls_function_unit* u){return u?u->origin->entities:NULL;}
const xls_module_graph* xlsFunctionUnitModules(const xls_function_unit* u){return u?u->origin->modules:NULL;}
const XL6_AstNode* xlsFrontendFunctionUnitRoot(const xls_function_unit* u){return u?u->root:NULL;}
XL6_AstNode* xlsFrontendFunctionUnitMaterialize(const xls_function_unit* u,size_t bytes,int (*cancelled)(void*),void* user)
{
    function_stop stop={cancelled,user};if(!u || !bytes || stopped(&stop))return NULL;
    return xlsAstClone(u->root,&(xls_ast_clone_options){copyNode,&stop,bytes,0,NULL},NULL);
}
#define FUNCTION_SET_BYTES (256u*1024u*1024u)
struct xls_function_set {
    xatomic32 references;xarray units;xls_function_origin* origin;
    size_t built,unitBytes,bytes,limit;
};
static int setBytes(xls_function_set* set)
{
    size_t total=sizeof(*set);
    if(!charge(&total,set->limit,set->origin->bytes) || !charge(&total,set->limit,set->unitBytes) ||
       !charge(&total,set->limit,set->units.Capacity*sizeof(void*)))return 0;
    set->bytes=total;return 1;
}
static int collectFunctions(xls_function_set* set,const XL6_AstNode* node,size_t depth,function_stop* stop)
{
    if(!node)return 1;
    if(depth>512 || stopped(stop) || (node->childCount && !node->children))return 0;
    if(xlsFrontendFunctionDefinition(node)){
        const void* borrowed=node;
        if(set->units.Count>=FUNCTION_NODES || !xrtArrayPush(&set->units,&borrowed) || !setBytes(set))return 0;
    }
    for(size_t i=0;i<node->childCount;++i)if(!collectFunctions(set,node->children[i],depth+1,stop))return 0;
    return 1;
}
xls_function_set* xlsFunctionSetRetain(xls_function_set* set)
{if(set)xrtAtomic32FetchAdd(&set->references,1,XMEMORY_RELAXED);return set;}
void xlsFunctionSetRelease(xls_function_set* set)
{
    if(!set || xrtAtomic32FetchSub(&set->references,1,XMEMORY_ACQ_REL)!=1)return;
    /* Unfinished slots still borrow producer ASTs only during construction.
     * Only the committed prefix owns units;no partial set ever escapes. */
    for(size_t i=0;i<set->built;++i)xlsFunctionUnitRelease((xls_function_unit*)*(const void* const*)xrtArrayConstGet(&set->units,i));
    xrtArrayUnit(&set->units);xlsFunctionOriginRelease(set->origin);xrtFree(set);
}
xls_function_set* xlsFrontendFunctionSetCapture(const XL6_AstNode* root,const xls_semantic_index* index,
    xls_function_origin* origin,size_t bytes,int (*cancelled)(void*),void* user)
{
    if(bytes>FUNCTION_SET_BYTES)bytes=FUNCTION_SET_BYTES;
    if(!root || !origin || bytes<=sizeof(xls_function_set) || origin->bytes>bytes-sizeof(xls_function_set))return NULL;
    function_stop stop={cancelled,user};if(stopped(&stop))return NULL;
    xls_function_set* set=xrtCalloc(1,sizeof(*set));if(!set)return NULL;
    xrtAtomic32Init(&set->references,1);set->origin=originRetain(origin);set->limit=bytes;
    if(!xrtArrayInit(&set->units,sizeof(void*)) || !setBytes(set) || !collectFunctions(set,root,0,&stop))goto failed;
    for(size_t i=0;i<set->units.Count;++i){
        const XL6_AstNode* node=*(const void* const*)xrtArrayConstGet(&set->units,i);
        xls_function_unit* unit=xlsFrontendFunctionUnitCapture(node,index,origin,bytes-set->bytes,cancelled,user);
        if(!unit)goto failed;
        *(const void**)xrtArrayGet(&set->units,i)=unit;++set->built;set->unitBytes+=unit->bytes;
        if(!setBytes(set))goto failed;
    }
    if(stopped(&stop))goto failed;
    return set;
failed:xlsFunctionSetRelease(set);return NULL;
}
xls_function_set_info xlsFunctionSetInfo(const xls_function_set* set)
{
    if(!set)return(xls_function_set_info){0};
    return(xls_function_set_info){set->units.Count,set->bytes,set->origin->bytes,xlsEntityViewInfo(set->origin->entities).domain};
}
xls_function_unit* xlsFunctionSetUnit(const xls_function_set* set,size_t ordinal)
{
    return set && ordinal<set->built ? xlsFunctionUnitRetain((xls_function_unit*)*(const void* const*)xrtArrayConstGet(&set->units,ordinal)) : NULL;
}
static int scanFunctions(const XL6_AstNode* node,size_t ordinal,size_t* count,const XL6_AstNode** selected,size_t depth,function_stop* stop)
{
    if(!node)return 1;
    if(depth>512 || stopped(stop) || (node->childCount && !node->children))return 0;
    if(xlsFrontendFunctionDefinition(node)){
        if(selected && *count==ordinal){*selected=node;return 1;}
        if(*count==SIZE_MAX)return 0;
        ++*count;
    }
    for(size_t i=0;i<node->childCount;++i){
        if(!scanFunctions(node->children[i],ordinal,count,selected,depth+1,stop))return 0;
        if(selected && *selected)break;
    }
    return 1;
}
int xlsSemanticCheckFunctionCount(const xls_semantic_check* check,size_t* count)
{
    if(count)*count=0;
    if(!check || !count || !xlsSemanticCheckInfo(check).accepted)return 0;
    function_stop stop={0};size_t total=0;
    const XL6_AstNode* root=xlsSemanticCheckAst(check);
    if(!root || !scanFunctions(root,0,&total,NULL,0,&stop) || !xlsSemanticCheckInfo(check).accepted)return 0;
    *count=total;return 1;
}
static xls_function_origin* checkOrigin(xls_semantic_check* check,function_stop* stop)
{
    XL6_SourceReport captured;
    if(!xlsFrontendCheckFunctionInputs(check,&captured))return NULL;
    return xlsFunctionOriginCapture(&captured,(xls_context*)xlsSemanticCheckContext(check),
        (xls_module_graph*)xlsSemanticCheckModules(check),xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(check)),stop->cancelled,stop->user);
}
static void captureError(char* error,size_t capacity)
{if(error && capacity)snprintf(error,capacity,"checked function capture unavailable, cancelled or resource limit reached");}
xls_function_unit* xlsSemanticCheckFunctionCapture(xls_semantic_check* check,size_t ordinal,size_t bytes,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(error && capacity)*error=0;
    function_stop stop={cancelled,user};
    if(!check || bytes<=sizeof(xls_function_unit)+sizeof(XL6_AstNode)){captureError(error,capacity);return NULL;}
    xlsSemanticCheckRetain(check);xls_function_unit* unit=NULL;xls_function_origin* origin=NULL;const XL6_AstNode* selected=NULL;size_t count=0;
    if(stopped(&stop) || !xlsSemanticCheckValidate(check) || !xlsSemanticCheckInfo(check).accepted ||
       !scanFunctions(xlsSemanticCheckAst(check),ordinal,&count,&selected,0,&stop) || !selected)goto done;
    origin=checkOrigin(check,&stop);
    if(origin)unit=xlsFrontendFunctionUnitCapture(selected,xlsSemanticCheckTypedIndex(check),origin,bytes,cancelled,user);
    if(unit && (stopped(&stop) || !xlsSemanticCheckValidate(check))){xlsFunctionUnitRelease(unit);unit=NULL;}
done:
    xlsFunctionOriginRelease(origin);xlsSemanticCheckDestroy(check);
    if(!unit)captureError(error,capacity);
    return unit;
}
xls_function_set* xlsSemanticCheckFunctionsCapture(xls_semantic_check* check,size_t bytes,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(error && capacity)*error=0;
    function_stop stop={cancelled,user};
    if(!check || bytes<=sizeof(xls_function_set)){captureError(error,capacity);return NULL;}
    xlsSemanticCheckRetain(check);xls_function_set* set=NULL;xls_function_origin* origin=NULL;
    if(stopped(&stop) || !xlsSemanticCheckValidate(check) || !xlsSemanticCheckInfo(check).accepted)goto done;
    origin=checkOrigin(check,&stop);
    if(origin)set=xlsFrontendFunctionSetCapture(xlsSemanticCheckAst(check),xlsSemanticCheckTypedIndex(check),origin,bytes,cancelled,user);
    if(set && (stopped(&stop) || !xlsSemanticCheckValidate(check))){xlsFunctionSetRelease(set);set=NULL;}
done:
    xlsFunctionOriginRelease(origin);xlsSemanticCheckDestroy(check);
    if(!set)captureError(error,capacity);
    return set;
}
