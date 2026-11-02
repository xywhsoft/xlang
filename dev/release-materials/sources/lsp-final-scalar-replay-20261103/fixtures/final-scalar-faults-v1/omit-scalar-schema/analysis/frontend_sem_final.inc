/* Final native function phases reuse the existing final-output receipt.
 * Environment and producer-unit equality are separate from source freshness.
 * Original C-owned validation and full IR verification always remain eager. */
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#define XLS_FINAL_CONTEXT_BYTES (8u*1024u*1024u)
static int xlsSemFinalEnvironment(XL6_SemContext* ctx,xstrbuf* key)
{
    if(!ctx || !ctx->root || ctx->root->kind!=XL6_AST_MODULE || ctx->completionQuery ||
       ctx->currentClass || ctx->currentFunction || ctx->typeParams || ctx->captureBoundary || ctx->currentReturns ||
       ctx->types.count>ctx->types.capacity || (ctx->types.count && !ctx->types.items) ||
       !xlsSemGlobalState(ctx,key) || !xlsSemKeyText(key,ctx->moduleName) ||
       !xlsSemKeyNumber(key,ctx->captureIndex) || !xlsSemKeyNode(key,ctx->root,0,1) ||
       !xlsSemKeyNumber(key,ctx->types.count) || !xlsSemKeyNumber(key,ctx->types.nextId))return 0;
    /* Complete ordered native TypeInfo values;the already audited encoder
     * keeps declaration/type forests and source spelling,never pointer IDs. */
    for(size_t i=0;i<ctx->types.count;++i){const XL6_TypeInfo* type=&ctx->types.items[i];
        if(xlsFrontendCancelPoll() || !xlsSemKeyNumber(key,type->id) || !xlsSemKeyNumber(key,type->abiId) ||
           !xlsSemKeyText(key,type->name) || !xlsSemKeyText(key,type->abiName) ||
           !xlsSemKeyNumber(key,type->kind) || !xlsSemKeyNumber(key,type->allowGeneric) ||
           !xlsSemKeyNumber(key,type->genericArity) || !xlsSemKeyNode(key,type->decl,0,0))return 0;
    }
    return 1;
}
static int xlsSemFinalRegionCount(const XL6_AstNode* node,size_t depth,uint32_t* count,xls_sem_final_work* work)
{
    if(!node)return 1;
    if(depth>512 || (node->childCount && !node->children) || xlsFrontendCancelPoll())return 0;
    ++work->predictionNodes;
    if(xl6SemPlanNodeIsCallable(node) || node->kind==XL6_AST_BLOCK || node->kind==XL6_AST_WHILE ||
       node->kind==XL6_AST_DO_WHILE || node->kind==XL6_AST_FOR || node->kind==XL6_AST_TRY){
        if(*count==UINT32_MAX)return 0;
        ++*count;
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsSemFinalRegionCount(node->children[i],depth+1,count,work))return 0;
    return 1;
}
static void xlsSemFinalBegin(XL6_SemContext* ctx,XL6_AstNode* root)
{
    xls_sem_build* build=xlsActiveSemBuild;xls_sem_memo* memo=build ? build->memo : NULL;
    if(memo && memo->nativeFinalPrepared)return;
    size_t limit=xlsActiveFinalControl ? xlsActiveFinalControl->bytes : XLS_FINAL_CONTEXT_BYTES;
    if(limit>XLS_FINAL_CONTEXT_BYTES)limit=XLS_FINAL_CONTEXT_BYTES;
    if(!ctx || !memo || !build->enabled || build->deferredQuery || !memo->trackInputs || ctx->root!=root ||
       !root || root->kind!=XL6_AST_MODULE || root->childCount!=memo->entries.Count ||
       (root->childCount && !root->children) || (ctx->diags && ctx->diags->count) || build->bodyJobs.Count ||
       !xlsSemIrFinalScope || xlsSemIrFinalScope->root!=root || !xlsSemIrFinalScope->bytes ||
       limit<sizeof(xls_sem_final_context) || memo->bytes>XLS_SEM_MEMO_BYTES)return;
    if(limit>XLS_SEM_MEMO_BYTES-memo->bytes)limit=XLS_SEM_MEMO_BYTES-memo->bytes;
    if(limit<sizeof(xls_sem_final_context))return;
    xstrbuf key={0};
    if(!xlsSemFinalEnvironment(ctx,&key) || key.Capacity>limit-sizeof(xls_sem_final_context)){
        xrtStrBufFree(&key);++memo->finalWork.declined;return;
    }
    const xls_sem_memo* old=build->baseline;
    if(old && old->finalContext && old->finalContext->bytes<=limit && xlsSemEqualKey(&key,&old->finalContext->key)){
        memo->finalContext=old->finalContext;xrtAtomic32FetchAdd(&memo->finalContext->references,1,XMEMORY_RELAXED);
        xrtStrBufFree(&key);++memo->finalWork.contextHits;
    }else{
        memo->finalContext=xrtCalloc(1,sizeof(*memo->finalContext));
        if(!memo->finalContext){xrtStrBufFree(&key);++memo->finalWork.declined;return;}
        xrtAtomic32Init(&memo->finalContext->references,1);memo->finalContext->key=key;
        memo->finalContext->bytes=sizeof(*memo->finalContext)+key.Capacity;++memo->finalWork.contexts;
    }
    memo->bytes+=memo->finalContext->bytes;memo->finalWork.contextBytes=memo->finalContext->bytes;
    if(!memo->finalWork.contextHits || !old || old->entries.Count!=memo->entries.Count)return;
    uint32_t next=2;
    for(size_t i=0;i<root->childCount;++i){
        xls_sem_entry* entry=xrtArrayGet(&memo->entries,i);const xls_sem_entry* prior=xrtArrayConstGet(&old->entries,i);
        const XL6_AstNode* child=root->children[i];uint32_t count=0;
        int candidate=xlsSemFunction(child) && entry->unit && entry->unit==prior->unit && prior->finalInput && prior->finalPlanValid;
        if(prior->finalPlanValid && (!xlsSemFunction(child) || candidate))count=prior->finalRegionEnd-prior->finalRegionStart;
        else if(!xlsSemFinalRegionCount(child,0,&count,&memo->finalWork))return;
        if(count>UINT32_MAX-next)return;
        entry->finalPredictedStart=next;entry->finalPredictedEnd=next+count;entry->finalCandidate=candidate;next+=count;
    }
    memo->finalPrepared=1;
}
static int xlsSemNativeFailure(xls_sem_build* build)
{
    if(build)build->bodyFailure=1;
    xls_raw_scope* raw=xlsRawBodyScopeCurrent();if(raw)raw->failed=1;return 0;
}
/* After its original private-header walk;the original C-owned validator runs
 * again over the COMPLETE composed tree on success. Detached headers must stay
 * alive until the original context/registry has retired;global declarations
 * can still point at them. No borrowed AST is ever written. */
static int xlsSemPrepareNative(XL6_SemContext* ctx,XL6_AstNode* root)
{
    xls_raw_scope* raw=xlsRawBodyScopeCurrent();if(!raw || !raw->ordinary || !raw->deferred)return 1;
    xls_sem_build* build=xlsActiveSemBuild;xls_sem_memo* memo=build ? build->memo : NULL;
    if(!memo || !build->baseline || raw->failed || raw->consumed!=raw->deferred)return xlsSemNativeFailure(build);
    xlsSemFinalBegin(ctx,root);memo->nativeFinalPrepared=1;
    if(!memo->finalPrepared || !memo->finalWork.contextHits)return xlsSemNativeFailure(build);
    size_t count=0;
    for(size_t i=0;i<memo->entries.Count;++i){const xls_sem_entry* entry=xrtArrayConstGet(&memo->entries,i);
        if(!entry->native)continue;
        const xls_sem_entry* old=xrtArrayConstGet(&build->baseline->entries,i);
        if(!entry->finalCandidate || !old || entry->native!=old->native || entry->unit!=old->unit ||
           entry->finalPredictedStart!=old->finalRegionStart || entry->finalPredictedEnd!=old->finalRegionEnd ||
           !old->finalInput || !old->finalPlanValid || xlsFrontendCancelPoll())return xlsSemNativeFailure(build);
        ++count;
    }
    if(count!=raw->deferred)return xlsSemNativeFailure(build);
    for(size_t i=0;i<memo->entries.Count;++i){xls_sem_entry* entry=xrtArrayGet(&memo->entries,i);
        if(!entry->native)continue;
        entry->nativeStub=root->children[i];root->children[i]=(XL6_AstNode*)xlsTypedFunctionRoot(entry->native);
        entry->nativePlaced=entry->finalReplayed=1;++memo->nativeWork.reused;
    }
    return 2;
}
typedef struct xls_sem_final_reader {
    const xls_sem_ir_receipt* receipt;
    size_t at,nodes,types;
    uint32_t oldStart,oldEnd,start,end;
    int write;
} xls_sem_final_reader;
#define XLS_FINAL_PATCH_MASK ((UINT32_C(1)<<0)|(UINT32_C(1)<<1)|(UINT32_C(1)<<2)|(UINT32_C(1)<<9)|(UINT32_C(1)<<10)|(UINT32_C(1)<<11)|(UINT32_C(1)<<12)|(UINT32_C(1)<<13)|(UINT32_C(1)<<14))
#define XLS_FINAL_SCALAR_MASK ((UINT32_C(1)<<XLS_SEM_IR_FIELDS)-1)
static int xlsSemFinalFixedMatches(const XL6_AstNode* node,unsigned field,uint64_t value)
{
    /* The original encoder keeps enum and signed package fields as exact
     * uint32 bit patterns. Current symbol restoration can already have
     * produced these final values from an older immutable unit's origin. */
    uint64_t fields[XLS_SEM_IR_FIELDS];xlsSemIrNodeFields(node,fields);
    return fields[field]==value;
}
static void xlsSemFinalSet(XL6_AstNode* node,unsigned field,uint64_t value)
{
    switch(field){
        case 0:node->flags=(unsigned)value;break;
        case 1:node->semanticTypeId=(uint32_t)value;break;
        case 2:node->semanticTypeAbiId=value;break;
        case 9:node->semanticOwnership=(XL6_SemOwnershipAction)value;break;
        case 10:node->semanticExitKind=(XL6_SemExitKind)value;break;
        case 11:node->semanticScopeDepth=(uint32_t)value;break;
        case 12:node->semanticTargetScopeDepth=(uint32_t)value;break;
        case 13:node->semanticRegionId=(uint32_t)value;break;
        case 14:node->semanticTargetRegionId=(uint32_t)value;break;
    }
}
static int xlsSemFinalVisit(xls_sem_final_reader* reader,XL6_AstNode* node,size_t depth,int type)
{
    if(!node)return 1;
    const xls_sem_ir_receipt* receipt=reader->receipt;
    if(depth>512 || (node->childCount && !node->children) || reader->at>receipt->data.Size ||
       sizeof(uint32_t)>receipt->data.Size-reader->at)return 0;
    if(type)++reader->types;else ++reader->nodes;
    if(reader->nodes>receipt->nodes || reader->types>receipt->types ||
       (!reader->write && ((reader->nodes+reader->types)&63u)==0 && xlsFrontendCancelPoll()))return 0;
    uint32_t mask=0;memcpy(&mask,receipt->data.Data+reader->at,sizeof(mask));reader->at+=sizeof(mask);

    for(unsigned field=0;field<XLS_SEM_IR_FIELDS;++field)if(mask&(UINT32_C(1)<<field)){
        size_t bytes=field==2 ? 8 : 4;uint64_t value=0;
        if(reader->at>receipt->data.Size || bytes>receipt->data.Size-reader->at)return 0;
        memcpy(&value,receipt->data.Data+reader->at,bytes);reader->at+=bytes;
        if((field==13 || field==14) && value){
            if(value>=reader->oldStart && value<reader->oldEnd){
                value=(uint64_t)reader->start+value-reader->oldStart;
                if(value>=reader->end || value>UINT32_MAX)return 0;
            }else if(value!=1)return 0; /* original module target remains exact */
        }
        /* Only the original final-pass write set can be replayed. Other
         * encoded scalars must ALREADY equal this complete prior output;
         * changing native IDs still declines and runs the original passes.
         * Validation and commit have no intervening callback or mutation. */
        if(!(XLS_FINAL_PATCH_MASK&(UINT32_C(1)<<field)) && !reader->write &&
           !xlsSemFinalFixedMatches(node,field,value))return 0;
        if(reader->write && (XLS_FINAL_PATCH_MASK&(UINT32_C(1)<<field)))xlsSemFinalSet(node,field,value);
    }
    if(!xlsSemFinalVisit(reader,node->inferredType,depth+1,1) ||
       !xlsSemFinalVisit(reader,node->semanticDeclaredType,depth+1,1))return 0;
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsSemFinalVisit(reader,node->children[i],depth+1,type))return 0;
    return 1;
}
static int xlsSemFinalTypeApply(XL6_SemContext* ctx,size_t slot)
{
    xls_sem_build* build=xlsActiveSemBuild;xls_sem_memo* memo=build ? build->memo : NULL;
    if(!memo || !memo->finalPrepared || !memo->finalContext || !build->baseline || slot>=memo->entries.Count)return 0;
    xls_sem_entry* entry=xrtArrayGet(&memo->entries,slot);const xls_sem_entry* old=xrtArrayConstGet(&build->baseline->entries,slot);
    XL6_AstNode* node=ctx->root->children[slot];
    if(entry->nativePlaced){++memo->finalWork.typeReused;return 1;}
    if(!entry->finalCandidate || !xlsSemFunction(node) || !entry->unit || entry->unit!=old->unit ||
       !old->finalInput || !old->finalPlanValid || old->finalRegionEnd<=old->finalRegionStart ||
       entry->finalPredictedEnd-entry->finalPredictedStart!=old->finalRegionEnd-old->finalRegionStart ||
       old->finalInput->nodes!=entry->unit->facts.Count || xlsFrontendCancelObserved())return 0;
    xls_sem_final_reader reader={.receipt=old->finalInput,.oldStart=old->finalRegionStart,.oldEnd=old->finalRegionEnd,
        .start=entry->finalPredictedStart,.end=entry->finalPredictedEnd};
    if(!xlsSemFinalVisit(&reader,node,0,0) || reader.at!=reader.receipt->data.Size ||
       reader.nodes!=reader.receipt->nodes || reader.types!=reader.receipt->types){++memo->finalWork.declined;return 0;}
    memo->finalWork.validatedNodes+=reader.nodes+reader.types;
    /* Construction-private tree and immutable receipt. No allocation,callback,
     * cancellation or structural write occurs between validation and commit. */
    reader.at=reader.nodes=reader.types=0;reader.write=1;
    if(!xlsSemFinalVisit(&reader,node,0,0))return 0;
    entry->finalReplayed=1;++memo->finalWork.typeReused;memo->finalWork.replayedNodes+=reader.nodes+reader.types;
    return 1;
}
static int xlsSemFinalRootState(const XL6_SemPlanWalkState* state)
{
    return state && state->nextRegion && !state->loop && !state->scopeDepth && !state->functionTargetScopeDepth &&
        !state->exceptionTargetScopeDepth && state->currentRegion==1 && state->functionTargetRegion==1 && state->exceptionTargetRegion==1;
}
static int xlsSemFinalPlanBefore(XL6_SemContext* ctx,size_t slot,const XL6_SemPlanWalkState* state)
{
    (void)ctx;xls_sem_build* build=xlsActiveSemBuild;xls_sem_memo* memo=build ? build->memo : NULL;
    if(!memo || slot>=memo->entries.Count)return 0;
    xls_sem_entry* entry=xrtArrayGet(&memo->entries,slot);
    if(entry->nativePlaced && (!memo->finalPrepared || !xlsSemFinalRootState(state) ||
       entry->finalPredictedStart!=*state->nextRegion || entry->finalPredictedEnd<entry->finalPredictedStart)){
        xlsSemNativeFailure(build);return 1; /* protect immutable input even in a rejected attempt */
    }
    if(!memo->finalContext || !xlsSemFinalRootState(state))return 0;
    entry->finalRegionStart=*state->nextRegion;
    if(!memo->finalPrepared || !entry->finalReplayed || entry->finalPredictedStart!=*state->nextRegion ||
       entry->finalPredictedEnd<entry->finalPredictedStart)return 0;
    *state->nextRegion=entry->finalPredictedEnd;++memo->finalWork.planReused;return 1;
}
static void xlsSemFinalPlanAfter(XL6_SemContext* ctx,size_t slot,const XL6_SemPlanWalkState* state)
{
    (void)ctx;xls_sem_build* build=xlsActiveSemBuild;xls_sem_memo* memo=build ? build->memo : NULL;
    if(!memo || !memo->finalContext || slot>=memo->entries.Count || !xlsSemFinalRootState(state))return;
    xls_sem_entry* entry=xrtArrayGet(&memo->entries,slot);entry->finalRegionEnd=*state->nextRegion;
    entry->finalPlanValid=entry->finalRegionStart>=2 && entry->finalRegionEnd>=entry->finalRegionStart;
}
#endif
