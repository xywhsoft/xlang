/* Optional independent raw-function provenance. Native original facts,
 * context,read/source/type/symbol proof remain authoritative. A missing
 * admission for an omitted body rejects the whole private query attempt. */
#define XLS_RAW_MEMO_BYTES (32u*1024u*1024u)
static int xl6SemNodeHasEntityIdentity(const XL6_AstNode*);
typedef struct xls_raw_fact {
    xatomic32 references;
    xls_raw_unit* unit;
    xstrbuf header;
    size_t functionBytes,count,body,end,prefixPositions,bodyPositions,totalPositions,bytes,rawBytes;
} xls_raw_fact;
typedef struct xls_raw_memo {
    xarray entries;
    xmap owners;
    xstrbuf preprocess;
    size_t bytes;
} xls_raw_memo;
static xls_raw_fact* xlsRawFactRetain(xls_raw_fact* fact)
{if(fact)xrtAtomic32FetchAdd(&fact->references,1,XMEMORY_RELAXED);return fact;}
static void xlsRawFactRelease(xls_raw_fact* fact)
{if(fact && xrtAtomic32FetchSub(&fact->references,1,XMEMORY_ACQ_REL)==1){xlsRawUnitRelease(fact->unit);xrtStrBufFree(&fact->header);xrtFree(fact);}}
static void xlsRawMemoDestroy(xls_raw_memo* memo)
{
    if(!memo)return;
    for(size_t i=0;i<memo->entries.Count;++i)xlsRawFactRelease(*(xls_raw_fact**)xrtArrayGet(&memo->entries,i));
    xrtArrayUnit(&memo->entries);xrtMapUnit(&memo->owners);xrtStrBufFree(&memo->preprocess);xrtFree(memo);
}
int xlsRawBodyEnvironmentKey(const XL6_AstNode* root,xstrbuf* out)
{return root && root->kind==XL6_AST_MODULE && out && !out->Size && xlsSemKeyNode(out,root,0,1);}
/* The complete already-audited input encoder is included after the raw
 * module environment registry. Source freshness remains a separate original
 * report validation before publication and every later public read. */
#include "analysis/frontend_raw_preprocess.inc"
int xlsRawBodyMemoPreprocessEqual(const void* opaque,const xstrbuf* key)
{
    const xls_sem_memo* memo=opaque;return memo && memo->raw && key && xlsSemEqualKey(&memo->raw->preprocess,key);
}
static const xls_raw_fact* xlsRawMemoFind(const xls_raw_memo* memo,xls_raw_unit* unit,size_t* slot)
{
    if(!memo || !unit)return NULL;
    const size_t* found=xrtMapConstGet(&memo->owners,(xbytesview){(const unsigned char*)&unit,sizeof(unit)});
    if(!found || *found==SIZE_MAX)return NULL;
    if(slot)*slot=*found;
    return *(xls_raw_fact* const*)xrtArrayConstGet(&memo->entries,*found);
}
int xlsRawBodyMemoAdmit(const void* opaque,xls_raw_unit* unit,const char* path,const XL6_AstRange* range,size_t* bytes)
{
    const xls_sem_memo* memo=opaque;size_t slot=0;
    const xls_raw_fact* fact=memo ? xlsRawMemoFind(memo->raw,unit,&slot) : NULL;
    const XL6_AstNode* raw=xlsRawUnitNode(unit);
    const xls_sem_entry* old=fact ? xrtArrayConstGet(&memo->entries,slot) : NULL;
    if(!fact || !old || !old->unit || !old->unit->proof || !path || !range || !range->known ||
       !raw || !xl6AstSourcePath(raw) || strcmp(path,xl6AstSourcePath(raw)) || !xlsSemTailAllowsBodies() ||
       xlsSemBodyBudget()<sizeof(xls_sem_view) || old->unit->bytes>xlsSemBodyBudget()-sizeof(xls_sem_view))return 0;
    *bytes=fact->rawBytes;return 1;
}
static xls_raw_memo* xlsRawMemoCreate(size_t count)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!s || !s->ready || !s->preprocessValid || !s->preprocess.Size)return NULL;
    xls_raw_memo* memo=xrtCalloc(1,sizeof(*memo));if(!memo)return NULL;
    if(!xrtArrayInit(&memo->entries,sizeof(xls_raw_fact*)) || !xrtArrayResize(&memo->entries,count) ||
       !xrtMapInit(&memo->owners,sizeof(size_t)) || !xlsSemKeyBytes(&memo->preprocess,s->preprocess.Data,s->preprocess.Size) ||
       memo->entries.Capacity>(XLS_RAW_MEMO_BYTES-sizeof(*memo))/sizeof(xls_raw_fact*))goto failed;
    memo->bytes=sizeof(*memo)+memo->entries.Capacity*sizeof(xls_raw_fact*);
    if(memo->preprocess.Capacity>XLS_RAW_MEMO_BYTES-memo->bytes)goto failed;
    memo->bytes+=memo->preprocess.Capacity;return memo;
failed:xlsRawMemoDestroy(memo);return NULL;
}
static int xlsRawMemoAdd(xls_sem_memo* owner,size_t slot,xls_raw_fact* fact)
{
    xls_raw_memo* memo=owner->raw;if(!memo || slot>=memo->entries.Count || !fact || fact->bytes>XLS_RAW_MEMO_BYTES-memo->bytes ||
       128>XLS_RAW_MEMO_BYTES-memo->bytes-fact->bytes || *(xls_raw_fact**)xrtArrayGet(&memo->entries,slot))return 0;
    xbytesview key={(const unsigned char*)&fact->unit,sizeof(fact->unit)};
    size_t* existing=xrtMapGet(&memo->owners,key);
    if(existing)*existing=SIZE_MAX; /* Ambiguity cannot depend on allocation. */
    else if(!xrtMapSet(&memo->owners,key,&slot))return 0;
    *(xls_raw_fact**)xrtArrayGet(&memo->entries,slot)=xlsRawFactRetain(fact);memo->bytes+=fact->bytes+128;return 1;
}
typedef struct xls_raw_capture {
    xls_raw_binding* binding;
    xstrbuf header;
    size_t functionBytes,body,end;
    int inert;
} xls_raw_capture;
static int xlsRawFunctionKey(xls_sem_entry* entry,XL6_AstNode* node,xls_raw_binding* b,xls_raw_capture* capture)
{
    int okay=xlsSemKeyNode(&entry->key,node,0,0);capture->functionBytes=okay ? entry->key.Size : 0;
    if(b && !b->deferred && b->stdlibVisited && !b->stdlibMarks && node->kind==XL6_AST_FUNC &&
       node->childCount>=4 && node->children[2] && node->children[2]->childCount &&
       (node->childCount<=4 || !node->children[4] || !node->children[4]->childCount) &&
       xlsSemKeyNode(&capture->header,node,0,2))capture->binding=b;
    return okay;
}
static void xlsRawCapturePrepare(xls_raw_capture* capture,const xarray* raw,const XL6_AstNode* function)
{
    if(!capture->binding || function->childCount<4)return;
    size_t body=0,end=raw->Count;
    while(body<raw->Count && ((const xls_sem_raw*)xrtArrayConstGet(raw,body))->node!=function->children[3])++body;
    if(!body || body==raw->Count)return;
    if(function->childCount>4){end=body+1;while(end<raw->Count && ((const xls_sem_raw*)xrtArrayConstGet(raw,end))->node!=function->children[4])++end;if(end==raw->Count)return;}
    for(size_t i=body;i<end;++i){const XL6_AstNode* n=((const xls_sem_raw*)xrtArrayConstGet(raw,i))->node;
        if(xl6SemNodeHasEntityIdentity(n) || n->kind==XL6_AST_YIELD || xlsRawBodyFeatureSpelling(n->text))return;
    }
    capture->body=body;capture->end=end;capture->inert=1;
}
static void xlsRawCaptureCommit(xls_sem_memo* memo,size_t slot,const xls_sem_entry* entry,const xarray* raw,xls_raw_capture* capture)
{
    if(!memo->raw || !capture->binding || !capture->inert || !entry->unit || !entry->unit->proof ||
       entry->unit->facts.Count!=raw->Count || !capture->functionBytes || capture->functionBytes>entry->key.Size)return;
    /* The ordinary source report and relocation map use their exact XRT
     * identities. A closed entry can retain the configured spelling in its
     * original parser AST. Keep that function eager when its raw source has
     * no exact report identity: the original coordinate proof would reject
     * it after any source edit. Do not omit it and force a whole-query retry,
     * or relax the original source/coordinate predicate. */
    const char* path=xl6AstSourcePath(xlsRawUnitNode(capture->binding->unit));
    if(!path || !xlsRawBodyEnvironmentHasPath(path))return;
    xls_raw_fact* fact=xrtCalloc(1,sizeof(*fact));if(!fact)return;
    xrtAtomic32Init(&fact->references,1);fact->unit=xlsRawUnitRetain(capture->binding->unit);
    fact->rawBytes=capture->binding->bytes;fact->header=capture->header;capture->header=(xstrbuf){0};
    fact->functionBytes=capture->functionBytes;fact->count=raw->Count;fact->body=capture->body;fact->end=capture->end;
    /* The original immutable proof already enumerated every located type
     * edge in exact clone order. Count the sparse header with the unchanged
     * predicate,then derive its omitted body's complement from that complete
     * proof. Do not walk the entire certified body a second time. */
    int okay=entry->unit->proof->facts==entry->unit->facts.Count;
    fact->totalPositions=entry->unit->proof->located.Count;size_t headerPositions=0;
    for(size_t i=0;okay && i<entry->unit->facts.Count;){
        if(i==fact->body){i=fact->end;continue;}
        const xls_sem_fact* f=xrtArrayConstGet(&entry->unit->facts,i);size_t count=0;
        if(!xlsSemBodyPositionCount(f->inferred,0,&count) || !xlsSemBodyPositionCount(f->declaredType,0,&count) ||
           headerPositions>fact->totalPositions || count>fact->totalPositions-headerPositions){okay=0;break;}
        headerPositions+=count;if(i<fact->body)fact->prefixPositions+=count;++i;
    }
    if(okay)fact->bodyPositions=fact->totalPositions-headerPositions;
    if(fact->rawBytes>XLS_RAW_MEMO_BYTES-sizeof(*fact) || fact->header.Capacity>XLS_RAW_MEMO_BYTES-sizeof(*fact)-fact->rawBytes)okay=0;
    if(okay){fact->bytes=sizeof(*fact)+fact->rawBytes+fact->header.Capacity;xlsRawMemoAdd(memo,slot,fact);}
    xlsRawFactRelease(fact);
}
static int xlsRawSparseHeader(const xls_raw_fact* raw,const xls_sem_view* view,const xarray* nodes,const xmap* sources,
    const XL6_AstNode* function,const xls_sem_cursor* cursor,XL6_SymbolId start,int shifted)
{
    xarray selected={0},facts={0},positions={0},copied={0};xls_sem_moves unchanged={0};int okay=0;
    if(!raw || raw->end<=raw->body || raw->end>raw->count || nodes->Count!=raw->count-(raw->end-raw->body)+1 ||
       view->unit->facts.Count!=raw->count || (view->positions.Count && view->positions.Count!=raw->totalPositions) ||
       !xrtArrayInit(&selected,sizeof(xls_sem_raw)) || !xrtArrayInit(&facts,sizeof(xls_sem_fact)) ||
       !xrtArrayInit(&positions,sizeof(xls_sem_position)) || !xrtMapInit(&unchanged.paths,sizeof(xls_sem_move)))goto done;
    size_t located=0;
    for(size_t i=0;i<raw->count;){
        if(i==raw->body){i=raw->end;located=raw->prefixPositions+raw->bodyPositions;continue;}
        size_t at=i<raw->body ? i : i-(raw->end-raw->body)+1;
        const xls_sem_raw* node=xrtArrayConstGet(nodes,at);const xls_sem_fact* fact=xrtArrayConstGet(&view->unit->facts,i);size_t count=0;
        if(!node || !fact || !xrtArrayPush(&selected,node) || !xrtArrayPush(&facts,fact))goto done;
        if(view->positions.Count){
            if(!xlsSemBodyPositionCount(fact->inferred,0,&count) || !xlsSemBodyPositionCount(fact->declaredType,0,&count))goto done;
            for(size_t j=0;j<count;++j){const xls_sem_position* position=xrtArrayConstGet(&view->positions,located+j);if(!position || !xrtArrayPush(&positions,position))goto done;}
        }
        located+=count;++i;
    }
    if(view->positions.Count && located!=view->positions.Count)goto done;
    xls_sem_unit unit={.symbols=view->unit->symbols,.facts=facts,.cursor=view->unit->cursor};
    xls_sem_entry header={.unit=&unit,.positions=positions,.lexicalRange=function->range};
    okay=xlsSemApplyFacts(&header,&selected,&unchanged,sources,&copied,function,cursor,start,shifted);
done:xrtArrayUnit(&selected);xrtArrayUnit(&facts);xrtArrayUnit(&positions);xrtArrayUnit(&copied);xrtMapUnit(&unchanged.paths);return okay;
}
static int xlsSemRawCheckFunc(XL6_SemContext* ctx,XL6_AstNode* node,xls_raw_binding* binding)
{
    xls_sem_build* build=xlsActiveSemBuild;xls_raw_scope* scope=xlsRawBodyScopeCurrent();int okay=0;
    xstrbuf state={0},header={0};xarray nodes={0},positions={0};xls_sem_view* view=NULL;
    size_t* slot=build && build->enabled ? xrtMapGet(&build->slots,(xbytesview){(const unsigned char*)&node,sizeof(node)}) : NULL;
    const xls_sem_entry* old=slot && build->baseline && xlsSemReadCompatible(build) ? xrtArrayConstGet(&build->baseline->entries,*slot) : NULL;
    size_t oldSlot=SIZE_MAX;const xls_raw_fact* raw=old ? xlsRawMemoFind(build->baseline->raw,binding->unit,&oldSlot) : NULL;
    xls_sem_memo* m=build ? build->memo : NULL;xls_sem_entry* entry=slot ? xrtArrayGet(&m->entries,*slot) : NULL;
    if(!scope || !scope->preprocessValid || scope->failed || !raw || !build->compatible || oldSlot!=*slot || !old->unit || !old->unit->proof ||
       binding->consumed || binding->stdlibMarks || !binding->stdlibVisited || ctx->currentClass || ctx->currentFunction ||
       ctx->typeParams || ctx->captureBoundary || ctx->currentReturns || !build->deferred || !build->deferredQuery ||
       !xlsSemTailAllowsBodies() || (ctx->diags && ctx->diags->count) || !m->raw || node->childCount<4 ||
       !node->children[3] || node->children[3]->kind!=XL6_AST_BLOCK || node->children[3]->childCount ||
       !xlsSemQueryNeutral(node,0,&build->queryCursor) || !xlsSemKeyNode(&header,node,0,2) || !xlsSemEqualKey(&header,&raw->header))goto done;
    binding->consumed=1;
    entry->start=ctx->nextSymbolId;entry->typeStart=ctx->types.nextId;entry->lexicalRange=node->range;
    if(entry->start!=old->start)build->symbolsShifted=1;
    if(xlsSemReadCompatible(build) && build->symbolsShifted){
        if(!build->symbolsSeen)xlsSemReadDisable(build);
        for(size_t i=build->symbolCursor;xlsSemReadCompatible(build) && i<*slot;++i)
            if(!xlsSemFunction(ctx->root->children[i]) && ctx->root->children[i]->kind!=XL6_AST_IMPORT)xlsSemReadDisable(build);
    }
    build->symbolsSeen=1;build->symbolCursor=*slot+1;
    if(entry->typeStart!=old->typeStart)xlsSemReadDisable(build);
    XL6_SymbolId symbolEnd=0;int shifted=build->symbolsShifted || entry->start!=old->unit->symbols.start;
    if(!xlsSemReadCompatible(build) || ctx->queryTypeNodes>65536u || ctx->queryTypeBytes>4u*1024u*1024u ||
       !xlsSemGlobalState(ctx,&state) || !raw->functionBytes || raw->functionBytes>old->key.Size ||
       !xlsSemKeyBytes(&entry->key,old->key.Data,raw->functionBytes) || !xlsSemKeyBytes(&entry->key,state.Data,state.Size) ||
       !xlsSemEqualKey(&entry->key,&old->key) || (!build->compatible && !xlsSemReadMatch(build,ctx,old->unit->reads,*slot)) ||
       old->unit->symbols.floor!=m->symbolFloor || !xlsSemSymbolsLimit(&old->unit->symbols,entry->start,&symbolEnd) ||
       old->end<old->start || old->end-old->start!=old->unit->symbols.end-old->unit->symbols.start ||
       (shifted && !old->unit->symbols.local) || old->typeNodes>65536u-ctx->queryTypeNodes || old->typeBytes>4u*1024u*1024u-ctx->queryTypeBytes ||
       !xrtArrayInit(&nodes,sizeof(xls_sem_raw)) || !xlsSemRawNodes(node,&nodes,0))goto done;
    uint64_t certified=0;
    if(!xlsSemBodyPrepareCount(old,raw->count,&build->moves,&build->sources,&positions,node,&build->queryCursor,entry->start,shifted,&certified))goto done;
    size_t limit=xlsSemBodyBudget();
    view=xlsSemViewCreate(old->unit,&positions,limit);
    if(!view || entry->key.Capacity>XLS_SEM_MEMO_BYTES-m->bytes || view->bytes>XLS_SEM_MEMO_BYTES-m->bytes-entry->key.Capacity ||
       !xlsRawMemoAdd(m,*slot,(xls_raw_fact*)raw) ||
       !xlsRawSparseHeader(raw,view,&nodes,&build->sources,node,&build->queryCursor,entry->start,shifted))goto done;
    entry->view=view;entry->unit=view->unit;entry->positions=view->positions;view=NULL;
    m->bytes+=entry->key.Capacity+entry->view->bytes;
    ctx->nextSymbolId=symbolEnd;ctx->types.nextId=old->typeEnd;ctx->queryTypeNodes+=old->typeNodes;ctx->queryTypeBytes+=old->typeBytes;
    entry->end=symbolEnd;entry->typeEnd=old->typeEnd;entry->typeNodes=old->typeNodes;entry->typeBytes=old->typeBytes;
    if(entry->end!=old->end)build->symbolsShifted=1;
    if(entry->typeEnd!=old->typeEnd)xlsSemReadDisable(build);
    ++build->work.reused;if(!build->compatible)++m->readWork.reused;xlsSemReadOwnChanged(build,node);
    if(certified){++build->work.proofHits;build->work.proofNodes+=certified;}
    ++scope->consumed;okay=1;
done:
    xrtStrBufFree(&state);xrtStrBufFree(&header);xrtArrayUnit(&nodes);xrtArrayUnit(&positions);xlsSemViewRelease(view);
    if(!okay){if(build)build->bodyFailure=1;if(scope)scope->failed=1;}
    return okay;
}

size_t xlsFrontendSemRawMemoBytes(const xls_sem_memo* memo)
{return memo && memo->raw ? memo->raw->bytes : 0;}
