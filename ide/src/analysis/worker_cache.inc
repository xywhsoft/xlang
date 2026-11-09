/* One semantic owner thread;the accepted batch supplies the routing key and
 * current revision history independently from each root's semantic baseline.
 * No extra key strings or transport/path normalization policy is introduced. */
static xls_session_state semanticState(const semantic_session* s)
{
    if(s->kind==QUERY_CHECK)return xlsCheckSessionState(s->value.check);
    if(s->kind==QUERY_COMPLETION)return xlsCompletionSessionState(s->value.completion);
    if(s->kind==QUERY_DEPENDENCY)return xlsDependencySessionState(s->value.dependency);
    return xlsSignatureSessionState(s->value.signature);
}
static void destroySemantic(semantic_session* s)
{
    if(s->kind==QUERY_CHECK)xlsCheckSessionDestroy(s->value.check);
    else if(s->kind==QUERY_COMPLETION)xlsCompletionSessionDestroy(s->value.completion);
    else if(s->kind==QUERY_SIGNATURE)xlsSignatureSessionDestroy(s->value.signature);
    else if(s->kind==QUERY_DEPENDENCY)xlsDependencySessionDestroy(s->value.dependency);
    *s=(semantic_session){0};
}
static xls_semantic_checkpoint checkpointSemantic(const semantic_session* s)
{
    if(s->kind==QUERY_CHECK)return xlsCheckSessionCheckpoint(s->value.check);
    if(s->kind==QUERY_COMPLETION)return xlsCompletionSessionCheckpoint(s->value.completion);
    if(s->kind==QUERY_DEPENDENCY)return xlsDependencySessionCheckpoint(s->value.dependency);
    return xlsSignatureSessionCheckpoint(s->value.signature);
}
static void restoreSemantic(semantic_session* s,xls_semantic_checkpoint* old)
{
    if(s->kind==QUERY_CHECK)xlsCheckSessionRestore(s->value.check,old);
    else if(s->kind==QUERY_COMPLETION)xlsCompletionSessionRestore(s->value.completion,old);
    else if(s->kind==QUERY_DEPENDENCY)xlsDependencySessionRestore(s->value.dependency,old);
    else xlsSignatureSessionRestore(s->value.signature,old);
}
static void dropCheckpoint(xls_semantic_checkpoint* old)
{
    xlsInputBatchDestroy(old->inputs);xlsSemanticCheckDestroy(old->check);xlsSemanticCompletionDestroy(old->completion);
    xlsSignatureRelease(old->signature);xlsDependencyCaptureRelease(old->dependency);xlsSyntaxRelease(old->syntax);*old=(xls_semantic_checkpoint){0};
}
static int createSemantic(xls_worker* w,enum query_kind kind,semantic_session* s)
{
    xls_session_limits limits=w->limits;
    if(limits.documents>64)limits.documents=64;
    if(limits.documentBytes>8u*1024u*1024u)limits.documentBytes=8u*1024u*1024u;
    if(limits.totalBytes>32u*1024u*1024u)limits.totalBytes=32u*1024u*1024u;
    s->kind=kind;
    if(kind==QUERY_CHECK){s->value.check=xlsCheckSessionCreate(&limits);return s->value.check!=NULL;}
    if(kind==QUERY_COMPLETION){s->value.completion=xlsCompletionSessionCreate(&limits);return s->value.completion!=NULL;}
    if(kind==QUERY_DEPENDENCY){s->value.dependency=xlsDependencySessionCreate(&limits);return s->value.dependency!=NULL;}
    s->value.signature=xlsSignatureSessionCreate(&limits);return s->value.signature!=NULL;
}
static int sameRequired(const xls_analysis_document* old,const xls_analysis_document* now)
{return old && now && old->identity==now->identity && !strcmp(old->key,now->key) && !strcmp(old->path,now->path);}
static const xls_analysis_document* capturedFind(const check_inputs* inputs,const char* key)
{
    if(key)for(size_t i=0;i<inputs->count;++i)if(!strcmp(inputs->documents[i].key,key))return &inputs->documents[i];
    return NULL;
}
static int semanticMatches(const semantic_session* s,const query_input* input,const check_inputs* inputs)
{
    if(s->kind!=input->kind)return 0;
    xls_session_state state=semanticState(s);if(!state.inputs)return 0;
    const xls_analysis_document* old=xlsInputBatchTarget(state.inputs);
    const xls_analysis_document* now=capturedFind(inputs,input->target);
    if((old!=NULL)!=(now!=NULL)){
        /* Explicit check/dependency contexts identify the actual compilation
         * entry even when its editor key appears/disappears. Reuse the same
         * session across that representation change;the session still validates
         * the full new input batch and reruns semantics when it differs.
         * Named-to-named identity changes keep their original strict routing. */
        const char* entry=xlsInputBatchEntry(state.inputs);
        const char* next=now ? now->path : input->context ? xlsContextSpec(input->context)->entry : NULL;
        if((input->kind!=QUERY_CHECK && input->kind!=QUERY_DEPENDENCY) || !input->context ||
           !xlsInputBatchContext(state.inputs) || !entry || !next || strcmp(entry,next))return 0;
    }else if(old && !sameRequired(old,now))return 0;
    if((state.queryKey!=NULL)!=(input->queryKey!=NULL))return 0;
    if(state.queryKey && (strcmp(state.queryKey,input->queryKey) ||
       !sameRequired(xlsInputBatchFind(state.inputs,state.queryKey),capturedFind(inputs,input->queryKey))))return 0;
    return xlsFrontendContextsEqual(xlsInputBatchContext(state.inputs),input->context);
}
static int stillRequired(const semantic_session* s,const xls_input_batch* current)
{
    xls_session_state state=semanticState(s);const xls_analysis_document* entry=xlsInputBatchTarget(state.inputs);
    if(entry && !sameRequired(entry,xlsInputBatchFind(current,entry->key)))return 0;
    /* A configured disk entry has no target key. Its old disk-only session
     * becomes obsolete when that same path is overlaid (or the overlay's
     * identity changes), just as a closed named target is already obsolete.
     * Unrelated closed project roots remain live. No filesystem/body work. */
    if(!entry){
        const char* path=xlsInputBatchEntry(state.inputs);
        const xls_analysis_document *before=NULL,*after=NULL,*document;
        if(path){
            for(size_t i=0;(document=xlsInputBatchAt(state.inputs,i));++i)if(!strcmp(document->path,path)){before=document;break;}
            for(size_t i=0;(document=xlsInputBatchAt(current,i));++i)if(!strcmp(document->path,path)){after=document;break;}
            if((before!=NULL)!=(after!=NULL) || (before && !sameRequired(before,after)))return 0;
        }
    }
    return !state.queryKey || sameRequired(xlsInputBatchFind(state.inputs,state.queryKey),xlsInputBatchFind(current,state.queryKey));
}
static uint64_t addWork(uint64_t total,uint64_t before,uint64_t after)
{uint64_t change=after-before;return change>UINT64_MAX-total ? UINT64_MAX : total+change;}
static void cacheCount(uint64_t* value){if(*value!=UINT64_MAX)++*value;}
static void removeCached(xls_worker* w,size_t at,int destroy,int pruned)
{
    if(destroy){destroySemantic(&w->cached[at]);cacheCount(&w->cacheStats.evictions);if(pruned)cacheCount(&w->cacheStats.pruned);}
    memmove(&w->cached[at],&w->cached[at+1],(w->cachedCount-at-1)*sizeof(*w->cached));--w->cachedCount;
    w->cached[w->cachedCount]=(semantic_session){0};
}
static size_t cacheInputBytes(const xls_worker* w,size_t extra)
{
    for(size_t i=0;i<w->cachedCount;++i){size_t n=xlsInputBatchStorageBytes(semanticState(&w->cached[i]).inputs);if(n>SIZE_MAX-extra)return SIZE_MAX;extra+=n;}
    return extra;
}
static void commitSemantic(xls_worker* w,semantic_session* selected,size_t slot,int replaced)
{
    semantic_session accepted=*selected;xls_session_state state=semanticState(&accepted);
    xls_input_batch* current=xlsInputBatchRetain(state.inputs);size_t kind=(size_t)accepted.kind-QUERY_CHECK;
    xlsInputBatchDestroy(w->history[kind]);w->history[kind]=current;
    if(slot<w->cachedCount){if(replaced)cacheCount(&w->cacheStats.evictions);removeCached(w,slot,0,0);}
    for(size_t i=0;i<w->cachedCount;)if(!stillRequired(&w->cached[i],current))removeCached(w,i,1,1);else ++i;
    size_t active=xlsInputBatchStorageBytes(current);
    while(w->cachedCount && (w->cachedCount>=w->cacheLimits.sessions || cacheInputBytes(w,active)>w->cacheLimits.inputStorageBytes))removeCached(w,w->cachedCount-1,1,0);
    memmove(&w->cached[1],&w->cached[0],w->cachedCount*sizeof(*w->cached));w->cached[0]=accepted;++w->cachedCount;
    w->cacheStats.sessions=w->cachedCount;w->cacheStats.inputStorageBytes=cacheInputBytes(w,0);
    w->cacheStats.inputBudgetExceeded=w->cacheStats.inputStorageBytes>w->cacheLimits.inputStorageBytes;
}
static xtaskoutcome cachedSemantic(xcancel* cancel,query_input* input,const check_inputs* inputs,xtaskvalue* output)
{
    xls_worker* w=input->worker;query_result* result=xrtCalloc(1,sizeof(*result));if(!result)return XTASK_FAILED;
    for(size_t i=0;i<w->cachedCount;++i){
        semantic_session* s=&w->cached[i];if(!semanticMatches(s,input,inputs))continue;
        const xls_input_batch* batch=semanticState(s).inputs;size_t count=0;const xls_analysis_document* old;
        const xls_analysis_document* target=capturedFind(inputs,input->target);
        const char* entry=target ? target->path : input->context ? xlsContextSpec(input->context)->entry : NULL;
        if(!entry || !xlsInputBatchEntry(batch) || strcmp(entry,xlsInputBatchEntry(batch)))continue;
        for(;(old=xlsInputBatchAt(batch,count));++count){
            const xls_analysis_document* now=capturedFind(inputs,old->key);
            if(!sameRequired(old,now) || old->revision!=now->revision || old->text!=now->text)break;
        }
        if(old || count!=inputs->count)continue;
        xls_semantic_checkpoint saved=checkpointSemantic(s);
        if(saved.captureIndex){result->check=saved.check;saved.check=NULL;}
        dropCheckpoint(&saved);if(result->check)break;
    }
    /* Observation only: no source I/O, checker invocation, cache promotion,
     * history advancement or projection retirement. The caller must validate
     * the immutable candidate against its current source/configuration proof. */
    result->checkStats=w->checkTotals;result->cacheStats=w->cacheStats;result->threadId=xrtThreadCurrentId();
    if(!result->check)snprintf(result->error,sizeof(result->error),"Checked snapshot unavailable.");
    if(xrtCancelRequested(cancel)){freeResult(result,NULL);return XTASK_CANCELLED;}
    *output=(xtaskvalue){result,freeResult,NULL};return XTASK_SUCCESS;
}
static xtaskoutcome executeSemantic(xcancel* cancel,query_input* input,xtaskvalue* output)
{
    xls_worker* w=input->worker;check_inputs inputs={0};xrtMapVisit(&input->documents,captureCheck,&inputs);
    if(input->cachedCheck)return cachedSemantic(cancel,input,&inputs,output);
    query_result* result=xrtCalloc(1,sizeof(*result));if(!result)return XTASK_FAILED;
    size_t slot=w->cachedCount;for(size_t i=0;i<w->cachedCount;++i)if(semanticMatches(&w->cached[i],input,&inputs)){slot=i;break;}
    semantic_session staged={0};semantic_session* selected=NULL;int replaced=0;
    if(slot<w->cachedCount){cacheCount(&w->cacheStats.hits);selected=&w->cached[slot];}
    else{
        cacheCount(&w->cacheStats.misses);
        if(w->cacheLimits.sessions==1 && w->cachedCount==1 && w->cached[0].kind==input->kind){slot=0;selected=&w->cached[0];replaced=1;}
        else if(createSemantic(w,input->kind,&staged))selected=&staged;
        else{
            destroySemantic(&staged);
            for(size_t i=w->cachedCount;i>0;--i)if(w->cached[i-1].kind==input->kind){slot=i-1;selected=&w->cached[slot];replaced=1;cacheCount(&w->cacheStats.allocationFallbacks);break;}
            if(!selected){freeResult(result,NULL);return XTASK_FAILED;}
        }
    }
    int checkpointed=selected!=&staged;
    xls_semantic_checkpoint checkpoint=checkpointed ? checkpointSemantic(selected) : (xls_semantic_checkpoint){0};
    const xls_input_batch* history=w->history[(size_t)input->kind-QUERY_CHECK];
    if(input->kind==QUERY_DEPENDENCY){
        xls_dependency_session_stats before,after;xlsDependencySessionStats(selected->value.dependency,&before);
        result->dependency=xlsDependencySessionCaptureWithHistory(selected->value.dependency,input->context,history,inputs.documents,inputs.count,input->target,cancelled,cancel,result->error,sizeof(result->error));
        xlsDependencySessionStats(selected->value.dependency,&after);
#define ADD(field) w->dependencyTotals.field=addWork(w->dependencyTotals.field,before.field,after.field)
        ADD(builds);ADD(hits);ADD(invalidations);ADD(entryFullParses);ADD(entryFragmentParses);ADD(entryReusedDeclarations);ADD(entryParsedBytes);ADD(entrySyntaxHits);ADD(inputScopeHits);
#undef ADD
        w->dependencyTotals.documents=after.documents;w->dependencyTotals.textBytes=after.textBytes;result->dependencyStats=w->dependencyTotals;
    }else if(input->kind==QUERY_SIGNATURE){
        xls_signature_stats before,after;xlsSignatureSessionStats(selected->value.signature,&before);
        result->signature=xlsSignatureSessionQueryWithHistory(selected->value.signature,input->context,history,inputs.documents,inputs.count,input->target,input->queryKey,input->offset,cancelled,cancel,result->error,sizeof(result->error));
        xlsSignatureSessionStats(selected->value.signature,&after);
#define ADD(field) w->signatureTotals.field=addWork(w->signatureTotals.field,before.field,after.field)
        ADD(builds);ADD(hits);ADD(invalidations);ADD(indexBuilds);ADD(indexHits);ADD(caretHits);ADD(inputScopeHits);
#undef ADD
        w->signatureTotals.documents=after.documents;w->signatureTotals.textBytes=after.textBytes;result->signatureStats=w->signatureTotals;
    }else if(input->kind==QUERY_COMPLETION){
        xls_completion_stats before,after;xlsCompletionSessionStats(selected->value.completion,&before);
        result->completion=xlsCompletionSessionQueryWithHistory(selected->value.completion,input->context,history,inputs.documents,inputs.count,input->target,input->queryKey,input->offset,cancelled,cancel,result->error,sizeof(result->error));
        xlsCompletionSessionStats(selected->value.completion,&after);
#define ADD(field) w->completionTotals.field=addWork(w->completionTotals.field,before.field,after.field)
        ADD(builds);ADD(hits);ADD(invalidations);ADD(cursorFullParses);ADD(cursorFragmentParses);ADD(cursorReusedDeclarations);ADD(cursorParsedBytes);ADD(cursorSyntaxHits);ADD(cursorFreshBuilds);ADD(inputScopeHits);ADD(wordViews);ADD(wordProofNodes);
#undef ADD
        w->completionTotals.documents=after.documents;w->completionTotals.textBytes=after.textBytes;result->completionStats=w->completionTotals;
    }else{
        xls_check_stats before,after;xlsCheckSessionStats(selected->value.check,&before);
        result->check=xlsCheckSessionCheckWithHistory(selected->value.check,input->context,history,inputs.documents,inputs.count,input->target,1,cancelled,cancel,result->error,sizeof(result->error));
        xlsCheckSessionStats(selected->value.check,&after);
#define ADD(field) w->checkTotals.field=addWork(w->checkTotals.field,before.field,after.field)
        ADD(builds);ADD(hits);ADD(invalidations);ADD(entryFullParses);ADD(entryFragmentParses);ADD(entryReusedDeclarations);ADD(entryParsedBytes);ADD(entrySyntaxHits);ADD(semanticFunctionsChecked);ADD(semanticFunctionsReused);ADD(semanticContextHits);ADD(inputScopeHits);
#undef ADD
        w->checkTotals.documents=after.documents;w->checkTotals.textBytes=after.textBytes;w->checkTotals.semanticMemoBytes=after.semanticMemoBytes;result->checkStats=w->checkTotals;
    }
    result->threadId=xrtThreadCurrentId();
    if(xrtCancelRequested(cancel)){if(checkpointed)restoreSemantic(selected,&checkpoint);dropCheckpoint(&checkpoint);destroySemantic(&staged);freeResult(result,NULL);return XTASK_CANCELLED;}
    if(result->check || result->completion || result->signature || result->dependency){commitSemantic(w,selected,slot,replaced);staged=(semantic_session){0};}
    else if(checkpointed)restoreSemantic(selected,&checkpoint);
    dropCheckpoint(&checkpoint);
    destroySemantic(&staged);result->cacheStats=w->cacheStats;
    /* Infrastructure errors carry the existing bounded error;source errors
     * are ordinary successful semantic results and remain cacheable. */
    *output=(xtaskvalue){result,freeResult,NULL};return XTASK_SUCCESS;
}
