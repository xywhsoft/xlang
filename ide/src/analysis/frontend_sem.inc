/* Included after the shared XL6_SemContext definition. The build overlay
 * retains the original checker body and every language rule. */
#include "analysis/frontend_sem.h"
#include "analysis/ast_clone.h"
#define XLS_SEM_MEMO_BYTES (64u*1024u*1024u)
#define XLS_SEM_KEY_BYTES (8u*1024u*1024u)
typedef struct xls_sem_fact {
    unsigned flags;
    uint32 type,symbol,declared,target,compound;
    uint64 abi;
    XL6_SemCallKind call;
    XL6_SemLvalueKind lvalue;
    XL6_SemOwnershipAction ownership;
    XL6_SemExitKind exit;
    uint32 scope,targetScope,region,targetRegion;
    XL6_AstNode *inferred,*declaredType;
} xls_sem_fact;
typedef struct xls_sem_raw {
    XL6_AstNode* node;
    XL6_AstKind kind;
    const char* text;
    XL6_AstNode** children;
    size_t count;
} xls_sem_raw;
typedef struct xls_sem_unit {
    xatomic32 references;
    xarray facts;
    size_t bytes;
} xls_sem_unit;
typedef struct xls_sem_position { XL6_AstRange range,token; int line,column; } xls_sem_position;
typedef struct xls_sem_entry {
    xstrbuf key;
    xls_sem_unit* unit;
    xarray positions;
    XL6_SymbolId start,end;
    XL6_TypeId typeStart,typeEnd;
    size_t typeNodes,typeBytes;
} xls_sem_entry;
struct xls_sem_memo {
    char* path;
    xls_text* text;
    xstrbuf context;
    xarray entries;
    size_t bytes;
};
typedef struct xls_sem_move {
    const xls_text *oldText,*newText;
    const char* path;
    size_t start,end,oldSkip,newSkip;
    int64_t delta;
    int changed;
} xls_sem_move;
typedef struct xls_sem_build {
    struct xls_sem_build* previous;
    const xls_sem_memo* baseline;
    xls_sem_memo* memo;
    xmap slots,sources;
    xls_sem_work work;
    xls_sem_move move;
    size_t cursor;
    int begun,enabled,compatible;
} xls_sem_build;
static _Thread_local xls_sem_build* xlsActiveSemBuild;
static int xlsSemCheckFuncOriginal(XL6_SemContext*,XL6_AstNode*);

static int xlsSemKeyBytes(xstrbuf* key,const void* data,size_t size)
{ return size<=XLS_SEM_KEY_BYTES-key->Size && xrtStrBufAppend(key,(xstrview){data,size}); }
static int xlsSemKeyNumber(xstrbuf* key,uint64_t value)
{
    unsigned char bytes[10]; size_t count=0;
    do { unsigned char byte=(unsigned char)(value&127u); value>>=7; bytes[count++]=(unsigned char)(byte|(value ? 128u : 0u)); } while(value);
    return xlsSemKeyBytes(key,bytes,count);
}
static int xlsSemKeyText(xstrbuf* key,const char* value)
{
    size_t size=value ? strlen(value) : 0;
    return xlsSemKeyNumber(key,value!=NULL) && xlsSemKeyNumber(key,size) && (!size || xlsSemKeyBytes(key,value,size));
}
static int xlsSemFunction(const XL6_AstNode* node)
{ return node && (node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC); }
static int xlsSemWorthMemo(const XL6_AstNode* node,size_t* nodes,size_t depth)
{
    if(!node || depth>512) return 0;
    /* Small, straight-line functions are cheaper to check than to restore.
     * Calls, generic declarations and control-flow ownership are less local;
     * retain those, or bodies with at least 64 lexical nodes. This gate is
     * deterministic; every excluded function still runs the real checker. */
    if(++*nodes>=64 || node->kind==XL6_AST_CALL || node->kind==XL6_AST_ANON_FUNC ||
       node->kind==XL6_AST_TRY || node->kind==XL6_AST_YIELD || node->kind==XL6_AST_IF ||
       node->kind==XL6_AST_WHILE || node->kind==XL6_AST_FOR || node->kind==XL6_AST_MATCH ||
       (node->kind==XL6_AST_TYPE_PARAMS && node->childCount)) return 1;
    for(size_t i=0;i<node->childCount;++i) if(xlsSemWorthMemo(node->children[i],nodes,depth+1)) return 1;
    return 0;
}
static int xlsSemCandidate(const XL6_AstNode* node)
{ size_t nodes=0; return xlsSemFunction(node) && xlsSemWorthMemo(node,&nodes,0); }
typedef struct xls_sem_key_paths { xmap paths; size_t next,lastId; const char* last; } xls_sem_key_paths;
static int xlsSemKeySource(xstrbuf* key,const char* path,xls_sem_key_paths* paths)
{
    if(!path) return xlsSemKeyNumber(key,0) && xlsSemKeyNumber(key,0);
    if(path==paths->last) return xlsSemKeyNumber(key,0) && xlsSemKeyNumber(key,paths->lastId);
    xbytesview name={(const unsigned char*)path,strlen(path)}; size_t* old=xrtMapGet(&paths->paths,name);
    if(old) { paths->last=path; paths->lastId=*old; return xlsSemKeyNumber(key,0) && xlsSemKeyNumber(key,*old); }
    size_t id=++paths->next;
    paths->last=path; paths->lastId=id;
    return xrtMapSet(&paths->paths,name,&id) && xlsSemKeyNumber(key,1) && xlsSemKeyNumber(key,id) && xlsSemKeyText(key,path);
}
static int xlsSemKeyNodePaths(xstrbuf* key,const XL6_AstNode* node,size_t depth,int module,xls_sem_key_paths* paths)
{
    if(depth>512 || !xlsSemKeyNumber(key,node!=NULL)) return 0;
    if(!node) return 1;
    uint64_t fields[]={node->kind,node->flags,node->literalSize,node->literalScalar,node->literalDecoded,
        node->semanticTypeId,node->semanticTypeAbiId,node->semanticSymbolId,node->semanticDeclaredSymbolId,
        node->semanticTargetSymbolId,node->semanticCompoundOperatorSymbolId,node->semanticCallKind,node->semanticLvalueKind,
        node->semanticOwnership,node->semanticExitKind,node->semanticScopeDepth,node->semanticTargetScopeDepth,
        node->semanticRegionId,node->semanticTargetRegionId,node->childCount};
    uint64_t present=0;
    for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i) if(fields[i]) present|=(uint64_t)1<<i;
    if(!xlsSemKeyNumber(key,present)) return 0;
    for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i) if(fields[i] && !xlsSemKeyNumber(key,fields[i])) return 0;
    if(!xlsSemKeyText(key,node->text) || !xlsSemKeyText(key,node->doc) ||
       !xlsSemKeySource(key,xl6AstSourcePath(node),paths) || !xlsSemKeyNumber(key,node->literalData!=NULL) ||
       (node->literalData && !xlsSemKeyBytes(key,node->literalData,node->literalSize)) ||
       !xlsSemKeyNodePaths(key,node->inferredType,depth+1,0,paths) || !xlsSemKeyNodePaths(key,node->semanticDeclaredType,depth+1,0,paths)) return 0;
    for(size_t i=0;i<node->childCount;++i) {
        /* Only direct module functions omit their bodies in this environment
         * key. Classes, aliases, globals and default expressions stay exact. */
        if(module && depth==1 && xlsSemFunction(node) && i==3) { if(!xlsSemKeyNumber(key,0)) return 0; }
        else if(!xlsSemKeyNodePaths(key,node->children[i],depth+1,module,paths)) return 0;
    }
    return 1;
}
static int xlsSemKeyNode(xstrbuf* key,const XL6_AstNode* node,size_t depth,int module)
{
    xls_sem_key_paths paths={0}; if(!xrtMapInit(&paths.paths,sizeof(size_t))) return 0;
    int okay=xlsSemKeyNodePaths(key,node,depth,module,&paths); xrtMapUnit(&paths.paths); return okay;
}
static int xlsSemEqualKey(const xstrbuf* a,const xstrbuf* b)
{ return a->Size==b->Size && (!a->Size || !memcmp(a->Data,b->Data,a->Size)); }
static int xlsSemGlobalState(XL6_SemContext* ctx,xstrbuf* key)
{
    if(!ctx->scope || ctx->scope->parent || ctx->flowFactCount) return 0;
    uint64_t state[]={ctx->currentReturnTypesValid,ctx->captureBoundaryAllowsCapture,ctx->inClassCreate,
        ctx->loopDepth,ctx->catchDepth,ctx->generatorDepth,ctx->suppressDefiniteAssignment};
    if(!xlsSemKeyBytes(key,state,sizeof(state)) || !xlsSemKeyText(key,ctx->implicitSelfName) ||
       !xlsSemKeyNumber(key,ctx->types.count) || ctx->directCallCallee) return 0;
    for(size_t i=0;i<ctx->scope->count;++i) {
        const XL6_Symbol* symbol=&ctx->scope->items[i];
        if(symbol->kind==XL6_SYM_FUNC) continue;
        if(!xlsSemKeyText(key,symbol->name) || !xlsSemKeyNumber(key,symbol->id) ||
           !xlsSemKeyNumber(key,symbol->kind) || !xlsSemKeyNumber(key,symbol->definitelyAssigned) ||
           !xlsSemKeyNode(key,symbol->type,0,0)) return 0;
    }
    return 1;
}
static int xlsSemSourcePool(const XL6_AstNode* node,xmap* sources,size_t depth)
{
    if(!node || depth>512) return 0;
    const char* path=xl6AstSourcePath(node);
    if(path && !xrtMapHas(sources,(xbytesview){(const unsigned char*)path,strlen(path)})) {
        XL6_AstNode* pin=xl6AstNew(XL6_AST_MODULE,NULL); if(!pin) return 0;
        xl6AstCopySource(pin,node);
        if(!xrtMapSet(sources,(xbytesview){(const unsigned char*)path,strlen(path)},&pin)) { xl6AstFree(pin); return 0; }
    }
    for(size_t i=0;i<node->childCount;++i) if(!xlsSemSourcePool(node->children[i],sources,depth+1)) return 0;
    return 1;
}
static void xlsSemSourceUnpin(xbytesview key,void* value,void* user)
{ (void)key; (void)user; xl6AstFree(*(XL6_AstNode**)value); }
static void xlsSemMemoBegin(XL6_SemContext* ctx)
{
    xls_sem_build* build=xlsActiveSemBuild;
    if(!build || ctx->completionQuery) return;
    build->begun=1;
    if(!build->memo || (ctx->diags && ctx->diags->count)) return;
    int candidate=0;
    for(size_t i=0;i<ctx->root->childCount && !candidate;++i) candidate=xlsSemCandidate(ctx->root->children[i]);
    if(!candidate) return;
    xls_sem_memo* m=build->memo;
    if(!xrtArrayResize(&m->entries,ctx->root->childCount) || m->entries.Capacity>XLS_SEM_MEMO_BYTES/sizeof(xls_sem_entry)) return;
    m->bytes=m->entries.Capacity*sizeof(xls_sem_entry);
    if(!xlsSemKeyText(&m->context,ctx->moduleName) || !xlsSemKeyNumber(&m->context,ctx->captureIndex) ||
       !xlsSemKeyNumber(&m->context,ctx->nextSymbolId) || !xlsSemKeyNumber(&m->context,ctx->types.nextId) || !xlsSemKeyNode(&m->context,ctx->root,0,1) ||
       !xlsSemGlobalState(ctx,&m->context) || m->context.Capacity>XLS_SEM_MEMO_BYTES-m->bytes) return;
    m->bytes+=m->context.Capacity;
    if(!xlsSemSourcePool(ctx->root,&build->sources,0)) return;
    for(size_t i=0;i<ctx->root->childCount;++i) {
        const XL6_AstNode* node=ctx->root->children[i]; if(!xlsSemFunction(node)) continue;
        if(!xrtMapSet(&build->slots,(xbytesview){(const unsigned char*)&node,sizeof(node)},&i)) return;
    }
    build->enabled=1;
    build->compatible=build->baseline && !strcmp(m->path,build->baseline->path) &&
        xlsSemEqualKey(&m->context,&build->baseline->context) && m->entries.Count==build->baseline->entries.Count;
    if(build->compatible) ++build->work.contextHits;
}
static size_t xlsSemBom(const xls_text* text)
{ return xlsTextSize(text)>=3 && !memcmp(xlsTextData(text),"\xef\xbb\xbf",3) ? 3 : 0; }
static int xlsSemLocation(const xls_text* text,size_t raw,int* line,int* column)
{
    xls_position pos; size_t at=0,scalars=0;
    if(!xlsTextPosition(text,raw,&pos) || pos.line>=INT_MAX || !xlsTextOffset(text,(xls_position){pos.line,0},&at)) return 0;
    if(!pos.line) at=xlsSemBom(text);
    while(at<raw) { uint32 scalar; size_t read; if(xrtUtf8Decode((xstrview){xlsTextData(text)+at,raw-at},&scalar,&read)!=XUTF_OK) return 0; at+=read; ++scalars; }
    if(scalars>=INT_MAX) return 0;
    *line=(int)pos.line+1; *column=(int)scalars+1; return 1;
}
static int xlsSemMovePoint(const xls_sem_move* move,size_t input,int ending,size_t* output,int* line,int* column)
{
    if(input>xlsTextSize(move->oldText)-move->oldSkip) return 0;
    size_t raw=input+move->oldSkip;
    if(move->changed) {
        if(raw>move->start && raw<move->end) return 0;
        if(raw>=move->end && !(ending && raw==move->start && move->start==move->end)) raw=(size_t)((int64_t)raw+move->delta);
    }
    if(raw<move->newSkip || !xlsSemLocation(move->newText,raw,line,column)) return 0;
    *output=raw-move->newSkip; return 1;
}
static int xlsSemMoveRange(const xls_sem_move* move,XL6_AstRange input,XL6_AstRange* output)
{
    *output=input;
    if(!input.known || !move->changed || (move->oldSkip==move->newSkip && input.endOffset+move->oldSkip<move->start)) return 1;
    if(move->changed && input.startOffset==input.endOffset && input.startOffset+move->oldSkip==move->start) return 0;
    return xlsSemMovePoint(move,input.startOffset,0,&output->startOffset,&output->startLine,&output->startColumn) &&
        xlsSemMovePoint(move,input.endOffset,1,&output->endOffset,&output->endLine,&output->endColumn);
}
static int xlsSemMoveNode(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{
    const xls_sem_move* move=user; const char* path=xl6AstSourcePath(in);
    if(!path || strcmp(path,move->path)) return 1;
    if(!xlsSemMoveRange(move,in->range,&out->range) || !xlsSemMoveRange(move,in->tokenRange,&out->tokenRange)) return 0;
    if(in->line) {
        if(!in->tokenRange.known || in->line!=in->tokenRange.startLine || in->column!=in->tokenRange.startColumn) return 0;
        out->line=out->tokenRange.startLine; out->column=out->tokenRange.startColumn;
    }
    return 1;
}
static int xlsSemRawNodes(XL6_AstNode* node,xarray* nodes,size_t depth)
{
    if(!node || depth>512) return 0;
    xls_sem_raw raw={node,node->kind,node->text,node->children,node->childCount};
    if(!xrtArrayPush(nodes,&raw)) return 0;
    for(size_t i=0;i<node->childCount;++i) if(!xlsSemRawNodes(node->children[i],nodes,depth+1)) return 0;
    return 1;
}
static int xlsSemShapeNode(const XL6_AstNode* node,const xarray* nodes,size_t* at,size_t depth)
{
    if(!node || depth>512) return 0;
    const xls_sem_raw* raw=xrtArrayConstGet(nodes,(*at)++);
    /* The real checker can replace/free a lexical child (implicit stdlib
     * methods, typed literals). Walk only the current tree. Never inspect
     * the old pointers before proving they still name the current node. */
    if(!raw || node!=raw->node || node->kind!=raw->kind || node->text!=raw->text || node->children!=raw->children || node->childCount!=raw->count) return 0;
    for(size_t i=0;i<node->childCount;++i) if(!xlsSemShapeNode(node->children[i],nodes,at,depth+1)) return 0;
    return 1;
}
static int xlsSemSameShape(const XL6_AstNode* node,const xarray* nodes)
{ size_t at=0; return xlsSemShapeNode(node,nodes,&at,0) && at==nodes->Count; }
static void xlsSemFactsFree(xarray* facts)
{
    for(size_t i=0;i<facts->Count;++i) { xls_sem_fact* fact=xrtArrayGet(facts,i); xl6AstFree(fact->inferred); xl6AstFree(fact->declaredType); }
    xrtArrayUnit(facts);
}
static xls_sem_unit* xlsSemUnitRetain(xls_sem_unit* unit)
{ if(unit) xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED); return unit; }
static void xlsSemUnitRelease(xls_sem_unit* unit)
{ if(unit && xrtAtomic32FetchSub(&unit->references,1,XMEMORY_ACQ_REL)==1) { xlsSemFactsFree(&unit->facts); xrtFree(unit); } }
typedef struct xls_sem_type_copy {
    const XL6_AstNode* root;
    xls_ast_clone_options options;
} xls_sem_type_copy;
static int xlsSemTypeTransform(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{
    xls_sem_type_copy* copy=user;
    return in==copy->root || !copy->options.transform || copy->options.transform(in,out,copy->options.user);
}
static XL6_AstSource* xlsSemTypeSource(const XL6_AstSource* in,void* user)
{ xls_sem_type_copy* copy=user; return copy->options.targetSource(in,copy->options.user); }
static int xlsSemCopyFactTypes(xarray* facts,const xls_ast_clone_options* options,size_t* bytes)
{
    /* Borrow input type pointers only while gathering. Clear them before any
     * failure can make the caller destroy this output array. One clone owns
     * every copied type, sharing its source map across the whole function. */
    xarray types={0},slots={0};
    int okay=xrtArrayInit(&types,sizeof(XL6_AstNode*)) && xrtArrayInit(&slots,sizeof(XL6_AstNode**));
    for(size_t i=0;i<facts->Count;++i) {
        xls_sem_fact* fact=xrtArrayGet(facts,i);
        XL6_AstNode** fields[]={&fact->inferred,&fact->declaredType};
        for(size_t j=0;okay && j<2;++j) if(*fields[j]) okay=xrtArrayPush(&types,fields[j]) && xrtArrayPush(&slots,&fields[j]);
    }
    for(size_t i=0;i<facts->Count;++i) { xls_sem_fact* f=xrtArrayGet(facts,i); f->inferred=f->declaredType=NULL; }
    *bytes=0;
    if(!okay) { xrtArrayUnit(&types); xrtArrayUnit(&slots); return 0; }
    if(!types.Count) { xrtArrayUnit(&types); xrtArrayUnit(&slots); return 1; }
    XL6_AstNode root={.kind=XL6_AST_MODULE,.children=(XL6_AstNode**)types.Data,.childCount=types.Count};
    xls_sem_type_copy copy={&root,*options};
    XL6_AstNode* owned=options->bytes ? xlsAstClone(&root,&(xls_ast_clone_options){xlsSemTypeTransform,&copy,options->bytes,0,
        options->targetSource ? xlsSemTypeSource : NULL},bytes) : NULL;
    int copied=owned!=NULL;
    if(owned) {
        for(size_t i=0;i<slots.Count;++i) {
            XL6_AstNode** field=*(XL6_AstNode***)xrtArrayGet(&slots,i); *field=owned->children[i];
        }
        owned->childCount=0; xl6AstFree(owned);
    }
    xrtArrayUnit(&types); xrtArrayUnit(&slots); return copied;
}
static int xlsSemCaptureFacts(const xarray* nodes,xarray* facts,size_t limit,size_t* bytes)
{
    *bytes=0;
    if(!xrtArrayInit(facts,sizeof(xls_sem_fact)) || nodes->Count>limit/sizeof(xls_sem_fact) ||
       !xrtArrayResize(facts,nodes->Count) || facts->Capacity>limit/sizeof(xls_sem_fact)) return 0;
    *bytes=facts->Capacity*sizeof(xls_sem_fact);
    for(size_t i=0;i<nodes->Count;++i) {
        const XL6_AstNode* n=((const xls_sem_raw*)xrtArrayConstGet(nodes,i))->node;
        xls_sem_fact* f=xrtArrayGet(facts,i);
        *f=(xls_sem_fact){n->flags,n->semanticTypeId,n->semanticSymbolId,n->semanticDeclaredSymbolId,n->semanticTargetSymbolId,
            n->semanticCompoundOperatorSymbolId,n->semanticTypeAbiId,n->semanticCallKind,n->semanticLvalueKind,n->semanticOwnership,
            n->semanticExitKind,n->semanticScopeDepth,n->semanticTargetScopeDepth,n->semanticRegionId,n->semanticTargetRegionId,n->inferredType,n->semanticDeclaredType};
    }
    size_t owned=0; int okay=xlsSemCopyFactTypes(facts,&(xls_ast_clone_options){.bytes=limit-*bytes},&owned);
    *bytes+=owned; return okay;
}

typedef struct xls_sem_apply {
    const xls_sem_move* move;
    const xarray* previous;
    xarray* positions;
    const xmap* sources;
    size_t visited;
    int different;
} xls_sem_apply;
static int xlsSemSameRange(XL6_AstRange a,XL6_AstRange b)
{ return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.endLine==b.endLine && a.startColumn==b.startColumn && a.endColumn==b.endColumn; }
static int xlsSemUnchangedUnlocated(const XL6_AstNode* node,const xls_sem_move* move)
{
    /* Synthetic types sometimes have only a call-site line/column. Without
     * provenance, leave them intact only if every coordinate precedes the
     * edit. Moving such a coordinate would require guessing its source. */
    if(!move->changed) return 1;
    if((node->range.known && node->range.endOffset+move->oldSkip>=move->start) ||
       (node->tokenRange.known && node->tokenRange.endOffset+move->oldSkip>=move->start)) return 0;
    if(node->line || node->column) {
        size_t at=0;
        if(node->line<1 || node->column<1 || !xlsTextOffset(move->oldText,(xls_position){(size_t)node->line-1,0},&at)) return 0;
        if(node->line==1) at=xlsSemBom(move->oldText);
        for(int column=1;column<node->column;++column) {
            uint32 scalar; size_t read;
            if(at>=xlsTextSize(move->oldText) || xrtUtf8Decode((xstrview){xlsTextData(move->oldText)+at,xlsTextSize(move->oldText)-at},&scalar,&read)!=XUTF_OK || scalar=='\r' || scalar=='\n') return 0;
            at+=read;
        }
        if(at>=move->start) return 0;
    }
    return 1;
}
static int xlsSemApplyPosition(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{
    xls_sem_apply* apply=user; XL6_AstNode positioned=*in;
    /* Most inferred primitive types have no coordinates. They cannot move,
     * so keep them out of the per-version coordinate table entirely. */
    if(!in->line && !in->column && !in->range.known && !in->tokenRange.known) return 1;
    if(apply->previous->Count) {
        const xls_sem_position* old=xrtArrayConstGet(apply->previous,apply->visited); if(!old) return 0;
        positioned.range=old->range; positioned.tokenRange=old->token; positioned.line=old->line; positioned.column=old->column;
    }
    out->range=positioned.range; out->tokenRange=positioned.tokenRange; out->line=positioned.line; out->column=positioned.column;
    if(!xl6AstSourcePath(&positioned) && !xlsSemUnchangedUnlocated(&positioned,apply->move)) return 0;
    if(!xlsSemMoveNode(&positioned,out,(void*)apply->move)) return 0;
    xls_sem_position next={out->range,out->tokenRange,out->line,out->column};
    if(!xlsSemSameRange(in->range,next.range) || !xlsSemSameRange(in->tokenRange,next.token) || in->line!=next.line || in->column!=next.column) apply->different=1;
    ++apply->visited;
    return apply->positions->Count<XLS_SEM_MEMO_BYTES/sizeof(next) && xrtArrayPush(apply->positions,&next);
}
static XL6_AstSource* xlsSemApplySource(const XL6_AstSource* source,void* user)
{
    xls_sem_apply* apply=user; if(!source->path) return NULL;
    XL6_AstNode* const* found=xrtMapConstGet(apply->sources,(xbytesview){(const unsigned char*)source->path,strlen(source->path)});
    return found ? (*found)->source : NULL;
}
static int xlsSemApplyFacts(const xls_sem_entry* entry,const xarray* nodes,const xls_sem_move* move,const xmap* sources,xarray* positions)
{
    if(!entry->unit || entry->unit->facts.Count!=nodes->Count) return 0;
    xarray staged={0}; size_t owned=0;
    xls_sem_apply apply={move,&entry->positions,positions,sources,0,0};
    if(!xrtArrayInit(&staged,sizeof(xls_sem_fact)) || !xrtArrayResize(&staged,nodes->Count) || !xrtArrayInit(positions,sizeof(xls_sem_position))) goto failed;
    /* Stage every owned type before touching the fresh tree. On resource or
     * coordinate failure the original checker sees its untouched input. */
    for(size_t i=0;i<staged.Count;++i) *(xls_sem_fact*)xrtArrayGet(&staged,i)=*(const xls_sem_fact*)xrtArrayConstGet(&entry->unit->facts,i);
    if(!xlsSemCopyFactTypes(&staged,&(xls_ast_clone_options){xlsSemApplyPosition,&apply,XLS_SEM_MEMO_BYTES,0,xlsSemApplySource},&owned)) goto failed;
    if(entry->positions.Count && entry->positions.Count!=apply.visited) goto failed;
    if(!apply.different) xrtArrayUnit(positions);
    for(size_t i=0;i<nodes->Count;++i) {
        XL6_AstNode* n=((const xls_sem_raw*)xrtArrayConstGet(nodes,i))->node; xls_sem_fact* f=xrtArrayGet(&staged,i);
        n->flags=f->flags; n->semanticTypeId=f->type; n->semanticTypeAbiId=f->abi; n->semanticSymbolId=f->symbol;
        n->semanticDeclaredSymbolId=f->declared; n->semanticTargetSymbolId=f->target; n->semanticCompoundOperatorSymbolId=f->compound;
        n->semanticCallKind=f->call; n->semanticLvalueKind=f->lvalue; n->semanticOwnership=f->ownership; n->semanticExitKind=f->exit;
        n->semanticScopeDepth=f->scope; n->semanticTargetScopeDepth=f->targetScope; n->semanticRegionId=f->region; n->semanticTargetRegionId=f->targetRegion;
        xl6AstFree(n->inferredType); xl6AstFree(n->semanticDeclaredType);
        n->inferredType=f->inferred; n->semanticDeclaredType=f->declaredType; f->inferred=f->declaredType=NULL;
    }
    xlsSemFactsFree(&staged); return 1;
failed: xlsSemFactsFree(&staged); xrtArrayUnit(positions); return 0;
}
static int xl6SemCheckFunc(XL6_SemContext* ctx,XL6_AstNode* node)
{
    xls_sem_build* build=xlsActiveSemBuild;
    size_t* slot=build && build->enabled ? xrtMapGet(&build->slots,(xbytesview){(const unsigned char*)&node,sizeof(node)}) : NULL;
    if(!slot || ctx->currentClass || ctx->currentFunction || ctx->typeParams || ctx->captureBoundary || ctx->currentReturns || ctx->completionQuery) {
        if(build && build->begun && !build->enabled && !ctx->currentClass && !ctx->currentFunction &&
           !ctx->completionQuery && ctx->scope && !ctx->scope->parent) {
            for(size_t i=build->cursor;i<ctx->root->childCount;++i) if(ctx->root->children[i]==node) {
                ++build->work.checked; build->cursor=i+1; break;
            }
        }
        return xlsSemCheckFuncOriginal(ctx,node);
    }
    xls_sem_memo* m=build->memo;
    xls_sem_entry* entry=xrtArrayGet(&m->entries,*slot);
    const xls_sem_entry* old=build->compatible ? xrtArrayConstGet(&build->baseline->entries,*slot) : NULL;
    entry->start=ctx->nextSymbolId; entry->typeStart=ctx->types.nextId;
    if(!xlsSemCandidate(node)) {
        if(old && (entry->start!=old->start || entry->typeStart!=old->typeStart)) build->compatible=0;
        ++build->work.checked; int okay=xlsSemCheckFuncOriginal(ctx,node); entry->end=ctx->nextSymbolId;
        entry->typeEnd=ctx->types.nextId;
        if(old && (entry->end!=old->end || entry->typeEnd!=old->typeEnd)) build->compatible=0;
        return okay;
    }
    size_t nodes=ctx->queryTypeNodes,bytes=ctx->queryTypeBytes; xstrbuf state={0},after={0}; xarray raw={0};
    int usable=(!ctx->diags || !ctx->diags->count) && xlsSemGlobalState(ctx,&state) && xlsSemKeyNode(&entry->key,node,0,0) &&
        xlsSemKeyBytes(&entry->key,state.Data,state.Size) && xrtArrayInit(&raw,sizeof(xls_sem_raw)) && xlsSemRawNodes(node,&raw,0);
    int okay=0,reused=0;
    /* A change to earlier local allocation can renumber class fields and
     * later contexts as well. Stop reuse for the remaining module, rather
     * than treating a later coincidental counter match as proof of identity. */
    if(old && (entry->start!=old->start || entry->typeStart!=old->typeStart)) build->compatible=0;
    if(build->compatible && old && old->unit && old->unit->facts.Count && usable && ctx->queryTypeNodes<=65536u && ctx->queryTypeBytes<=4u*1024u*1024u &&
       xlsSemEqualKey(&entry->key,&old->key) &&
       old->typeNodes<=65536u-ctx->queryTypeNodes && old->typeBytes<=4u*1024u*1024u-ctx->queryTypeBytes) {
        if(xlsSemApplyFacts(old,&raw,&build->move,&build->sources,&entry->positions)) {
            ctx->nextSymbolId=old->end; ctx->types.nextId=old->typeEnd;
            ctx->queryTypeNodes+=old->typeNodes; ctx->queryTypeBytes+=old->typeBytes;
            okay=reused=1; ++build->work.reused;
        }
    }
    if(!reused) { ++build->work.checked; okay=xlsSemCheckFuncOriginal(ctx,node); }
    entry->end=ctx->nextSymbolId; entry->typeNodes=ctx->queryTypeNodes-nodes; entry->typeBytes=ctx->queryTypeBytes-bytes;
    entry->typeEnd=ctx->types.nextId;
    if(old && (entry->end!=old->end || entry->typeEnd!=old->typeEnd)) build->compatible=0;
    if(okay && usable && (!ctx->diags || !ctx->diags->count) && xlsSemSameShape(node,&raw) && xlsSemGlobalState(ctx,&after) && xlsSemEqualKey(&state,&after) &&
       entry->key.Capacity<=XLS_SEM_MEMO_BYTES-m->bytes) {
        m->bytes+=entry->key.Capacity; size_t owned=0;
        size_t positions=entry->positions.Capacity*sizeof(xls_sem_position);
        if(reused && positions<=XLS_SEM_MEMO_BYTES-m->bytes && old->unit->bytes<=XLS_SEM_MEMO_BYTES-m->bytes-positions) {
            /* Immutable values share across versions; a compact position
             * table carries current coordinates. Neither keeps old text or
             * an old memo/check alive. Published AST types remain private. */
            entry->unit=xlsSemUnitRetain(old->unit); m->bytes+=old->unit->bytes+positions;
        } else if(sizeof(xls_sem_unit)<XLS_SEM_MEMO_BYTES-m->bytes) {
            xrtArrayUnit(&entry->positions);
            xls_sem_unit* unit=xrtCalloc(1,sizeof(*unit));
            if(unit) {
                xrtAtomic32Init(&unit->references,1);
                if(xlsSemCaptureFacts(&raw,&unit->facts,XLS_SEM_MEMO_BYTES-m->bytes-sizeof(*unit),&owned)) {
                    unit->bytes=owned+sizeof(*unit);
                    entry->unit=unit; m->bytes+=unit->bytes;
                } else xlsSemUnitRelease(unit);
            }
        }
        if(!entry->unit) { m->bytes-=entry->key.Capacity; xrtStrBufFree(&entry->key); xrtArrayUnit(&entry->positions); }
    } else { xrtStrBufFree(&entry->key); xrtArrayUnit(&entry->positions); }
    xrtArrayUnit(&raw); xrtStrBufFree(&state); xrtStrBufFree(&after); return okay;
}
void xlsFrontendSemMemoDestroy(xls_sem_memo* memo)
{
    if(!memo) return;
    for(size_t i=0;i<memo->entries.Count;++i) { xls_sem_entry* e=xrtArrayGet(&memo->entries,i); xrtStrBufFree(&e->key); xrtArrayUnit(&e->positions); xlsSemUnitRelease(e->unit); }
    xrtArrayUnit(&memo->entries); xrtStrBufFree(&memo->context); xlsTextRelease(memo->text); xrtFree(memo->path); xrtFree(memo);
}
int xlsFrontendSemCheckMemo(XL6_AstNode* root,XL6_DiagList* diags,const char* module,
    int captureIndex,const char* path,xls_text* text,const xls_sem_memo* previous,xls_sem_memo** result,xls_sem_work* work)
{
    if(result) *result=NULL;
    if(work) memset(work,0,sizeof(*work));
    xls_sem_build build={.previous=xlsActiveSemBuild,.baseline=previous};
    xls_sem_memo* memo=path && text ? xrtCalloc(1,sizeof(*memo)) : NULL;
    if(memo) {
        memo->path=xrtStrDup(path); memo->text=xlsTextRetain(text);
        if(!memo->path || !text || !xrtArrayInit(&memo->entries,sizeof(xls_sem_entry)) || !xrtMapInit(&build.slots,sizeof(size_t)) ||
           !xrtMapInit(&build.sources,sizeof(XL6_AstNode*)) || !xrtMapSetDrop(&build.sources,xlsSemSourceUnpin,NULL)) {
            xlsFrontendSemMemoDestroy(memo); memo=NULL;
        }
    }
    build.memo=memo; xlsActiveSemBuild=&build;
    if(memo && previous) {
        build.move=(xls_sem_move){.oldText=previous->text,.newText=text,.path=path,.oldSkip=xlsSemBom(previous->text),.newSkip=xlsSemBom(text)};
        xls_text_edit edit; int changed=xlsTextDiff(previous->text,text,&edit);
        if(changed>0) {
            build.move.changed=1; build.move.start=(size_t)(edit.text-xlsTextData(text));
            build.move.end=xlsTextSize(previous->text)-(xlsTextSize(text)-build.move.start-edit.length);
            build.move.delta=(int64_t)xlsTextSize(text)-(int64_t)xlsTextSize(previous->text);
        } else if(changed<0) build.baseline=NULL;
    }
    int okay=xl6SemCheckModuleWithQuery(root,diags,module,captureIndex,NULL);
    xlsActiveSemBuild=build.previous; xrtMapUnit(&build.slots); xrtMapUnit(&build.sources);
    build.work.bytes=memo && build.enabled ? memo->bytes : 0;
    if(work) *work=build.work;
    if(result && build.enabled) *result=memo; else xlsFrontendSemMemoDestroy(memo);
    return okay;
}
