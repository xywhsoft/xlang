/* Existing syntax-unit ownership,placement,clone and parser rules.
 * Only the native consumer can admit headers;public AST getters stay full. */
#include "analysis/raw_body_projection.h"

typedef struct raw_body_job {
    syntax_unit* unit;
    XL6_AstNode *function,*placeholder;
    size_t offset;
    int line,column;
} raw_body_job;
struct xls_raw_body_projection {
    xatomic32 references;
    xmutex lock;
    int lockReady;
    XL6_ParseResult parse;
    xarray jobs;
    char* path;
    size_t next,rawBytes;
    xls_raw_body_work work;
};
#ifdef XLS_RAW_BODY_VERIFY_OWNERS
static xatomic64 rawBodyLeases;
uint64_t xlsRawBodyProjectionOwnedLeases(void)
{return xrtAtomic64Load(&rawBodyLeases,XMEMORY_RELAXED);}
#define RAW_LEASE_ADD() xrtAtomic64FetchAdd(&rawBodyLeases,1,XMEMORY_RELAXED)
#define RAW_LEASE_DROP() xrtAtomic64FetchSub(&rawBodyLeases,1,XMEMORY_RELAXED)
#else
#define RAW_LEASE_ADD() ((void)0)
#define RAW_LEASE_DROP() ((void)0)
#endif
xls_raw_unit* xlsRawUnitRetain(xls_raw_unit* unit){return retainUnit(unit);}
void xlsRawUnitRelease(xls_raw_unit* unit){releaseUnit(unit);}
const XL6_AstNode* xlsRawUnitNode(const xls_raw_unit* unit){return unit ? unit->node : NULL;}
int xlsRawUnitBytes(const xls_raw_unit* unit,size_t limit,size_t* bytes)
{
    if(bytes)*bytes=0;
    if(!unit || !bytes || sizeof(*unit)>limit)return 0;
    size_t tree=unit->storageBytes;
    if(!unit->storageKnown && !cacheTreeBytes(unit->node,limit-sizeof(*unit),&tree))return 0;
    if(tree>limit-sizeof(*unit))return 0;
    *bytes=tree+sizeof(*unit);return 1;
}
xls_raw_body_projection* xlsRawBodyProjectionRetain(xls_raw_body_projection* p)
{if(p)xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
void xlsRawBodyProjectionRelease(xls_raw_body_projection* p)
{
    if(!p || xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)!=1)return;
    xl6ParseResultFree(&p->parse);
    for(size_t i=0;i<p->jobs.Count;++i){releaseUnit(((raw_body_job*)xrtArrayGet(&p->jobs,i))->unit);RAW_LEASE_DROP();}
    xrtArrayUnit(&p->jobs);xrtFree(p->path);
    if(p->lockReady)xrtMutexUnit(&p->lock);
    xrtFree(p);
}
static int rawBodyReserve(xls_raw_body_projection* p,const syntax_unit* unit,const char* path,size_t cached,size_t* owned)
{
    size_t base=sizeof(*p),label=strlen(path)+1,limit=p->work.limit;
    if(base>limit || label>limit-base || p->rawBytes>limit-base-label)return 0;
    if(cached){if(cached<sizeof(*unit))return 0;*owned=cached;}
    else{
    size_t tree=unit->storageBytes;
    if(!unit->storageKnown){
        ++p->work.sizingPasses;
        if(!cacheTreeBytes(unit->node,p->work.limit,&tree))return 0;
    }
    if(tree>SIZE_MAX-sizeof(*unit) || p->jobs.Count>=UNIT_LIMIT)return 0;
    *owned=tree+sizeof(*unit);
    }
    if(p->jobs.Count>=UNIT_LIMIT)return 0;
    if(base>limit || label>limit-base || p->rawBytes>limit-base-label || *owned>limit-base-label-p->rawBytes)return 0;
    size_t payload=base+label+p->rawBytes+*owned;
    if(p->jobs.Count==p->jobs.Capacity){
        xarray next={0};
        if(!xrtArrayInit(&next,sizeof(raw_body_job)) || !xrtArrayReserve(&next,p->jobs.Count+1) ||
           next.Capacity>(limit-payload)/sizeof(raw_body_job) || !xrtArrayResize(&next,p->jobs.Count)){
            xrtArrayUnit(&next);return 0;
        }
        if(p->jobs.Count)memcpy(next.Data,p->jobs.Data,p->jobs.Count*sizeof(raw_body_job));
        xrtArrayUnit(&p->jobs);p->jobs=next;
    }else if(p->jobs.Capacity>(limit-payload)/sizeof(raw_body_job))return 0;
    if(!p->path && !(p->path=xrtStrDup(path)))return 0;
    return 1;
}
static int rawBodyEligible(const xls_syntax* s,const placement* placed,XL6_AstRange* range)
{
    const XL6_AstNode* node=placed->unit->node;size_t header,end;
    if(!node || !xlsSyntaxFunctionHeader(node,&header,&end) || node->childCount<4 ||
       !node->children[3] || node->children[3]->kind!=XL6_AST_BLOCK || !node->children[3]->childCount)return 0;
    XL6_AstNode current=*node;
    if(!placeNode(node,&current,(void*)placed) || !current.range.known || current.range.endOffset<=current.range.startOffset)return 0;
    /* Keep the active query and boundaries fully materialized. The native
     * caller will additionally prove query neutrality/source metadata. */
    if(s->cursor && current.range.startOffset<=s->cursorOffset && s->cursorOffset<=current.range.endOffset)return 0;
    *range=current.range;
    return 1;
}
static XL6_AstNode* rawBodyHeader(const placement* placed)
{
    const XL6_AstNode* in=placed->unit->node;
    if(in->childCount<4 || in->childCount>UNIT_LIMIT || in->childCount>SIZE_MAX/sizeof(*in->children))return NULL;
    XL6_AstNode filtered=*in,body=*in->children[3];
    XL6_AstNode** children=xrtMalloc(in->childCount*sizeof(*children));if(!children)return NULL;
    memcpy(children,in->children,in->childCount*sizeof(*children));
    body.children=NULL;body.childCount=body.childCapacity=0;body.inferredType=body.semanticDeclaredType=NULL;
    children[3]=&body;filtered.children=children;
    xls_ast_clone_options options={placeNode,(void*)placed,0,1,NULL};
    XL6_AstNode* out=xlsAstMaterialize(&filtered,&options,NULL);xrtFree(children);return out;
}
static xls_raw_body_projection* rawBodyCreate(const xls_syntax* current,const xls_syntax* previous,size_t bytes,
    xls_raw_body_admit admit,xls_raw_body_record record,void* user)
{
    xls_syntax* lease=xlsSyntaxRetain((xls_syntax*)current);
    xls_syntax* old=xlsSyntaxRetain((xls_syntax*)previous);
    xls_raw_body_projection* p=NULL;xmap shared={0};int indexed=0;
    if(!lease || !lease->accepted)goto done;
    p=xrtCalloc(1,sizeof(*p));if(!p)goto done;xrtAtomic32Init(&p->references,1);p->work.limit=bytes;xl6ParseResultInit(&p->parse);
    if(!xrtMutexInit(&p->lock))goto failed;
    p->lockReady=1;
    if(bytes>sizeof(*p) && admit)indexed=1;
    else if(bytes>sizeof(*p) && old && old->accepted && old->cursor==lease->cursor && !strcmp(old->path,lease->path) &&
       xrtMapInit(&shared,sizeof(unsigned char))){
        indexed=1;unsigned char yes=1;
        for(size_t i=0;i<old->units.Count;++i){syntax_unit* unit=((const placement*)xrtArrayConstGet(&old->units,i))->unit;
            if(!xrtMapSet(&shared,(xbytesview){(const unsigned char*)&unit,sizeof(unit)},&yes)){indexed=0;break;}}
    }
    xls_ast_clone_options rootOptions={.dropSources=1};p->parse.root=xlsAstMaterialize(lease->parse.root,&rootOptions,NULL);
    if(!p->parse.root)goto failed;
    if(lease->units.Count){p->parse.root->children=xrtCalloc(lease->units.Count,sizeof(*p->parse.root->children));if(!p->parse.root->children)goto failed;p->parse.root->childCapacity=lease->units.Count;}
    for(size_t i=0;i<lease->units.Count;++i){
        const placement* placed=xrtArrayConstGet(&lease->units,i);XL6_AstNode* node=NULL;size_t owned=0,cached=0;XL6_AstRange range={0};
        int eligible=indexed && rawBodyEligible(lease,placed,&range) && (admit ? admit(placed->unit,lease->path,&range,&cached,user) :
            xrtMapHas(&shared,(xbytesview){(const unsigned char*)&placed->unit,sizeof(placed->unit)}));
        if(eligible && rawBodyReserve(p,placed->unit,lease->path,cached,&owned)){
            node=rawBodyHeader(placed);
            if(node){raw_body_job job={retainUnit(placed->unit),node,node->children[3],placed->offset,placed->line,placed->column};
                ((raw_body_job*)p->jobs.Data)[p->jobs.Count++]=job;p->rawBytes+=owned;++p->work.deferred;RAW_LEASE_ADD();}
        }
        if(!node){xls_ast_clone_options options={placeNode,(void*)placed,0,1,NULL};node=xlsAstMaterialize(placed->unit->node,&options,NULL);++p->work.declined;}
        if(!node)goto failed;
        p->parse.root->children[p->parse.root->childCount++]=node;
    }
    if(!attachSource(lease,p->parse.root))goto failed;
    for(size_t i=0;i<lease->parse.diags.count;++i){const XL6_Diag* d=&lease->parse.diags.items[i];
        xl6DiagAddFile(&p->parse.diags,d->file ? d->file : lease->path,d->line,d->column,d->message);
        if(p->parse.diags.count!=i+1)goto failed;}
    if(!p->jobs.Count){xrtArrayUnit(&p->jobs);xrtFree(p->path);p->path=NULL;}
    else p->work.retainedBytes=sizeof(*p)+strlen(p->path)+1+p->rawBytes+p->jobs.Capacity*sizeof(raw_body_job);
    p->work.complete=!p->jobs.Count;
    /* No fallible construction remains after publishing copy provenance to
     * the internal native scope. A failed earlier clone exposes no records. */
    if(record){size_t job=0;
        for(size_t i=0;i<lease->units.Count;++i){
            const placement* placed=xrtArrayConstGet(&lease->units,i);XL6_AstNode* node=p->parse.root->children[i];
            int deferred=job<p->jobs.Count && ((const raw_body_job*)xrtArrayConstGet(&p->jobs,job))->function==node;
            if(deferred)++job;
            record(placed->unit,lease->path,node,deferred,user);
        }
    }
    goto done;
failed:xlsRawBodyProjectionRelease(p);p=NULL;
done:xrtMapUnit(&shared);xlsSyntaxRelease(old);xlsSyntaxRelease(lease);return p;
}
xls_raw_body_projection* xlsRawBodyProjectionCreate(const xls_syntax* current,const xls_syntax* previous,size_t bytes)
{return rawBodyCreate(current,previous,bytes,NULL,NULL,NULL);}
xls_raw_body_projection* xlsRawBodyProjectionCreatePolicy(const xls_syntax* current,size_t bytes,
    xls_raw_body_admit admit,xls_raw_body_record record,void* user)
{return rawBodyCreate(current,NULL,bytes,admit,record,user);}
int xlsRawBodyProjectionTakeHeaderParse(xls_raw_body_projection* p,XL6_ParseResult* out)
{
    if(!p || !out || out->root || out->diags.count || !xrtMutexLock(&p->lock))return 0;
    int okay=!p->work.detached && p->parse.root;
    if(okay){xl6ParseResultFree(out);*out=p->parse;xl6ParseResultInit(&p->parse);p->work.detached=1;}
    xrtMutexUnlock(&p->lock);return okay;
}
const XL6_AstNode* xlsRawBodyProjectionHeaders(const xls_raw_body_projection* p)
{return p ? p->parse.root : NULL;}
const XL6_AstNode* xlsRawBodyProjectionAst(xls_raw_body_projection* p)
{
    if(!p || !xrtMutexLock(&p->lock))return NULL;
    if(p->work.detached){xrtMutexUnlock(&p->lock);return NULL;}
    int okay=1;
    while(p->next<p->jobs.Count){
        raw_body_job* job=xrtArrayGet(&p->jobs,p->next);
        if(!job->function || job->function->childCount<4 || job->function->children[3]!=job->placeholder){okay=0;break;}
        placement placed={job->unit,job->offset,job->line,job->column};
        xls_ast_clone_options options={placeNode,&placed,0,1,NULL};
        XL6_AstNode* body=xlsAstMaterialize(job->unit->node->children[3],&options,NULL);
        if(!body || !xl6AstSetSourceTree(body,p->path)){xl6AstFree(body);okay=0;break;}
        if(body->source && p->parse.root->source){body->source->completionQuery=p->parse.root->source->completionQuery;
            body->source->completionStart=p->parse.root->source->completionStart;body->source->completionEnd=p->parse.root->source->completionEnd;}
        job->function->children[3]=body;xl6AstFree(job->placeholder);job->placeholder=NULL;
        ++p->next;++p->work.materialized;
    }
    if(okay)p->work.complete=1;
    const XL6_AstNode* out=okay ? p->parse.root : NULL;xrtMutexUnlock(&p->lock);return out;
}
xls_raw_body_work xlsRawBodyProjectionWork(xls_raw_body_projection* p)
{
    if(!p || !xrtMutexLock(&p->lock))return (xls_raw_body_work){0};
    xls_raw_body_work work=p->work;xrtMutexUnlock(&p->lock);return work;
}

#include "analysis/frontend_raw_body.h"
void xlsRawBodySyntaxRegister(const xls_syntax* s,XL6_AstNode* root,xls_raw_scope* scope)
{
    if(!s || !root || !s->accepted || root->childCount!=s->units.Count)return;
    for(size_t i=0;i<s->units.Count;++i){const placement* p=xrtArrayConstGet(&s->units,i);XL6_AstNode* copy=root->children[i];
        XL6_AstNode placed=*p->unit->node;
        if(!copy || !placeNode(p->unit->node,&placed,(void*)p) || placed.kind!=copy->kind ||
           placed.childCount!=copy->childCount || placed.range.known!=copy->range.known ||
           (placed.range.known && (placed.range.startOffset!=copy->range.startOffset || placed.range.endOffset!=copy->range.endOffset)) ||
           !xl6AstSourcePath(copy) || strcmp(xl6AstSourcePath(copy),s->path)){scope->ready=0;if(scope->deferred)scope->failed=1;return;}
        xlsRawBodyRecord(p->unit,s->path,copy,0,scope);
    }
}
int xlsRawBodySyntaxCopy(const xls_syntax* s,XL6_CompletionQuery* query,XL6_ParseResult* result,xls_raw_scope* scope)
{
    if(!s || !result || result->root || result->diags.count)return 0;
    /* Without an earlier raw memo no body can be omitted. Keep the original
     * complete copier and register its exact completed provenance;avoid an
     * unused projection/lock and speculative admission on the cold path. */
    if(!scope->baseline){
        int okay=query ? xlsSyntaxCopyCursorParse(s,query,result) : xlsSyntaxCopyParse(s,result);
        if(okay)xlsRawBodySyntaxRegister(s,result->root,scope);
        return okay;
    }
    char* prefix=NULL;
    if(query){
        if(s->cursor!=1 || s->cursorParsed!=1 || !emptyQuery(query) || query->offset!=s->cursorOffset || !xl6CompletionTarget(query,s->path))return 0;
        if(s->cursorPrefix){size_t size=xlsTextSize(s->text)-bom(s->text),start=s->cursorRange.startOffset;
            if(!s->cursorRange.known || start>query->offset || query->offset>s->cursorRange.endOffset || s->cursorRange.endOffset>size)return 0;
            prefix=xrtStrDupN(xlsTextData(s->text)+bom(s->text)+start,query->offset-start);if(!prefix)return 0;}
    }
    xls_raw_body_projection* p=xlsRawBodyProjectionCreatePolicy(s,scope->limit,xlsRawBodyAdmit,xlsRawBodyRecord,scope);
    if(!p){xrtFree(prefix);int okay=query ? xlsSyntaxCopyCursorParse(s,query,result) : xlsSyntaxCopyParse(s,result);if(okay)xlsRawBodySyntaxRegister(s,result->root,scope);return okay;}
    int okay=xlsRawBodyProjectionTakeHeaderParse(p,result);xlsRawBodyProjectionRelease(p);
    if(!okay){xrtFree(prefix);return 0;}
    if(query){query->prefix=prefix;query->error=s->cursorError;query->parsed=s->cursorParsed;query->replacement=s->cursorRange;}
    return 1;
}
