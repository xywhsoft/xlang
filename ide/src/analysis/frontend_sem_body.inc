/* Included after the ordinary atomic fact restorer. A query-neutral function
 * keeps all original semantic/environment/allocation admission. This layer
 * validates EVERY type/source/coordinate first, restores every header fact,
 * and retains independent immutable body facts for demand materialization. */
static int xlsSemTailAllowsBodies(void);
static int xlsSemBodyCharge(size_t* bytes,size_t amount)
{
    if(amount>XLS_SEM_MEMO_BYTES-*bytes)return 0;
    *bytes+=amount;return 1;
}
static int xlsSemBodyProofType(const XL6_AstNode* in,xls_sem_apply* apply,size_t depth,size_t* bytes,
    const XL6_AstSource** lastSource,size_t* sourceBytes)
{
    if(!in)return 1;
    if(depth>512 || !xlsSemBodyCharge(bytes,sizeof(*in)) ||
       in->childCount>XLS_SEM_MEMO_BYTES/sizeof(*in->children) ||
       (in->childCount && !in->children) ||
       !xlsSemBodyCharge(bytes,in->childCount*sizeof(*in->children)))return 0;
    if((in->text && !xlsSemBodyCharge(bytes,strlen(in->text)+1)) ||
       (in->doc && !xlsSemBodyCharge(bytes,strlen(in->doc)+1)) ||
       (in->literalData && (in->literalSize==SIZE_MAX || !xlsSemBodyCharge(bytes,in->literalSize+1))))return 0;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    if(in->compiledPackageLink && !xlsSemBodyCharge(bytes,strlen(in->compiledPackageLink)+1))return 0;
#endif
    XL6_AstNode out=*in;
    if(!xlsSemApplyPosition(in,&out,apply))return 0;
    if(in->source){
        const XL6_AstSource* from=in->source;
        /* Exact clone-source contract, including the explicit neutral cursor
         * rebinding policy. Checking only header sources is insufficient. */
        if(from!=*lastSource){
            /* This synchronous walk cannot mutate either source. As in the
             * eager clone's source map, repeated identical input objects need
             * only one target admission; no identity survives this call. */
            XL6_AstSource* target=xlsSemApplySource(from,apply);
            if(target && !xlsAstCloneSourceTargetValid(from,target,xlsSemApplyCursorSource,apply))return 0;
            size_t length=from->path ? strlen(from->path)+1 : 0;
            if(length>SIZE_MAX-sizeof(*from))return 0;
            *sourceBytes=sizeof(*from)+length;*lastSource=from;
        }
        /* A conservative clone charge: duplicated source records/labels are
         * counted per edge even though the real clone deduplicates them. */
        if(!xlsSemBodyCharge(bytes,*sourceBytes))return 0;
    }
    /* Preserve the ordinary clone's preorder, including nested type facts. */
    for(size_t i=0;i<in->childCount;++i)
        if(!xlsSemBodyProofType(in->children[i],apply,depth+1,bytes,lastSource,sourceBytes))return 0;
    return xlsSemBodyProofType(in->inferredType,apply,depth+1,bytes,lastSource,sourceBytes) &&
        xlsSemBodyProofType(in->semanticDeclaredType,apply,depth+1,bytes,lastSource,sourceBytes);
}
static int xlsSemBodyPrepareCount(const xls_sem_entry* old,size_t nodeCount,const xls_sem_moves* moves,
    const xmap* sources,xarray* positions,const XL6_AstNode* function,const xls_sem_cursor* cursor,
    XL6_SymbolId start,int shifted,uint64_t* certifiedNodes)
{
    *certifiedNodes=0;
    if(!old->unit || old->unit->facts.Count!=nodeCount ||
       !xrtArrayInit(positions,sizeof(xls_sem_position)))return 0;
    const xls_sem_proof* proof=old->unit->proof;
    if(proof && proof->bytes<=xlsSemProofBudget() &&
       proof->facts==nodeCount && proof->cloneBytes<=XLS_SEM_MEMO_BYTES && (!shifted || proof->shiftSafe)){
        /* All node/scalar symbol fields were classified against the immutable
         * origin. The caller has proved the current local interval fits.
         * All located edges retain the exact original preorder and run the
         * unchanged coordinate callback. Unlocated types need no relocation. */
        xls_sem_apply apply=xlsSemApplyBegin(old,moves,sources,positions,function,cursor,start,shifted);
        int okay=1;
        for(size_t i=0;okay && i<proof->sources.Count;++i){
            const XL6_AstSource* source=*(const XL6_AstSource* const*)xrtArrayConstGet(&proof->sources,i);
            XL6_AstSource* target=xlsSemApplySource(source,&apply);
            if(target && !xlsAstCloneSourceTargetValid(source,target,xlsSemApplyCursorSource,&apply))okay=0;
        }
        if(!okay)return 0;
        for(size_t i=0;i<proof->located.Count;++i){
            const XL6_AstNode* node=*(const XL6_AstNode* const*)xrtArrayConstGet(&proof->located,i);
            XL6_AstNode positioned=*node;
            if(!xlsSemApplyPosition(node,&positioned,&apply))return 0;
        }
        if(old->positions.Count && old->positions.Count!=apply.visited)return 0;
        if(!apply.different)xrtArrayUnit(positions);
        *certifiedNodes=proof->nodes;return 1;
    }
    size_t bytes=sizeof(XL6_AstNode);
    if(old->unit->facts.Count>XLS_SEM_MEMO_BYTES/(2*sizeof(XL6_AstNode*)) ||
       !xlsSemBodyCharge(&bytes,2*old->unit->facts.Count*sizeof(XL6_AstNode*)))return 0;
    xls_sem_apply apply=xlsSemApplyBegin(old,moves,sources,positions,function,cursor,start,shifted);
    const XL6_AstSource* lastSource=NULL;size_t sourceBytes=0;
    for(size_t i=0;i<old->unit->facts.Count;++i){
        const xls_sem_fact* fact=xrtArrayConstGet(&old->unit->facts,i);
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
        if(fact->packageLink && !xlsSemBodyCharge(&bytes,strlen(fact->packageLink)+1))return 0;
        if(shifted){XL6_SymbolId receiver=fact->receiver;if(!xlsSemSymbolsBind(&old->unit->symbols,start,1,&receiver))return 0;}
#endif
        if(!xlsSemBodyProofType(fact->inferred,&apply,1,&bytes,&lastSource,&sourceBytes) ||
           !xlsSemBodyProofType(fact->declaredType,&apply,1,&bytes,&lastSource,&sourceBytes))return 0;
        if(shifted){
            XL6_SymbolId symbols[]={fact->symbol,fact->declared,fact->target,fact->compound};
            for(size_t j=0;j<4;++j)if(!xlsSemSymbolsBind(&old->unit->symbols,start,1,&symbols[j]))return 0;
        }
    }
    if(old->positions.Count && old->positions.Count!=apply.visited)return 0;
    if(!apply.different)xrtArrayUnit(positions);
    return 1;
}
static int xlsSemBodyPrepare(const xls_sem_entry* old,const xarray* nodes,const xls_sem_moves* moves,
    const xmap* sources,xarray* positions,const XL6_AstNode* function,const xls_sem_cursor* cursor,
    XL6_SymbolId start,int shifted,uint64_t* certifiedNodes)
{return xlsSemBodyPrepareCount(old,nodes->Count,moves,sources,positions,function,cursor,start,shifted,certifiedNodes);}
static int xlsSemBodyPositionCount(const XL6_AstNode* node,size_t depth,size_t* count)
{
    if(!node)return 1;
    if(depth>512)return 0;
    if(node->line || node->column || node->range.known || node->tokenRange.known){
        if(*count==SIZE_MAX)return 0;
        ++*count;
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsSemBodyPositionCount(node->children[i],depth+1,count))return 0;
    return xlsSemBodyPositionCount(node->inferredType,depth+1,count) &&
        xlsSemBodyPositionCount(node->semanticDeclaredType,depth+1,count);
}
static int xlsSemBodyHeader(const xls_sem_view* view,const xarray* nodes,const xmap* sources,
    const XL6_AstNode* function,const xls_sem_cursor* cursor,XL6_SymbolId start,int shifted)
{
    xarray selected={0},facts={0},positions={0},copied={0};xls_sem_moves unchanged={0};int okay=0;
    size_t body=0,end=nodes->Count,located=0;
    if(function->childCount<4)goto done;
    while(body<nodes->Count && ((const xls_sem_raw*)xrtArrayConstGet(nodes,body))->node!=function->children[3])++body;
    if(!body || body==nodes->Count)goto done;
    if(function->childCount>4){
        end=body+1;
        while(end<nodes->Count && ((const xls_sem_raw*)xrtArrayConstGet(nodes,end))->node!=function->children[4])++end;
        if(end==nodes->Count)goto done;
    }
    if(!xrtArrayInit(&selected,sizeof(xls_sem_raw)) || !xrtArrayInit(&facts,sizeof(xls_sem_fact)) ||
       !xrtArrayInit(&positions,sizeof(xls_sem_position)) || !xrtMapInit(&unchanged.paths,sizeof(xls_sem_move)))goto done;
    for(size_t i=0;i<nodes->Count;++i){
        const xls_sem_fact* fact=xrtArrayConstGet(&view->unit->facts,i);size_t count=0;
        if(view->positions.Count && (!xlsSemBodyPositionCount(fact->inferred,0,&count) ||
           !xlsSemBodyPositionCount(fact->declaredType,0,&count) || count>SIZE_MAX-located))goto done;
        if(i<body || i>=end){
            if(!xrtArrayPush(&selected,xrtArrayConstGet(nodes,i)) || !xrtArrayPush(&facts,fact))goto done;
            if(view->positions.Count)for(size_t j=0;j<count;++j){
                const xls_sem_position* position=xrtArrayConstGet(&view->positions,located+j);
                if(!position || !xrtArrayPush(&positions,position))goto done;
            }
        }
        located+=count;
    }
    if(view->positions.Count && located!=view->positions.Count)goto done;
    /* The full proof already converted positions to this result. Header
     * cloning therefore uses those coordinates with an unchanged source map.
     * The suffix includes generic parameters/attributes after the body slot. */
    /* Never copy a shared unit's mutable atomic reference counter as part of
     * a struct assignment. Only these immutable restoration fields are read. */
    xls_sem_unit unit={.symbols=view->unit->symbols,.facts=facts,.cursor=view->unit->cursor};
    xls_sem_entry header={.unit=&unit,.positions=positions,.lexicalRange=function->range};
    okay=xlsSemApplyFacts(&header,&selected,&unchanged,sources,&copied,function,cursor,start,shifted);
done:
    xrtArrayUnit(&selected);xrtArrayUnit(&facts);xrtArrayUnit(&positions);xrtArrayUnit(&copied);
    xrtMapUnit(&unchanged.paths);return okay;
}
static int xlsSemApplyFactsDemand(const xls_sem_entry* old,const xarray* nodes,const xls_sem_moves* moves,
    const xmap* sources,xarray* positions,const XL6_AstNode* function,const xls_sem_cursor* cursor,
    XL6_SymbolId start,int shifted)
{
    xls_sem_build* build=xlsActiveSemBuild;xls_sem_view* view=NULL;xarray current={0};size_t limit=xlsSemBodyBudget();uint64_t certifiedNodes=0;
    if(!build || !build->deferred || !build->deferredQuery || build->deferredQuery->error ||
       !limit || !xlsSemTailAllowsBodies() || function->childCount<4)goto eager;
    const size_t* slot=xrtMapConstGet(&build->slots,(xbytesview){(const unsigned char*)&function,sizeof(function)});
    xls_sem_entry* entry=slot ? xrtArrayGet(&build->memo->entries,*slot) : NULL;
    if(!entry || entry->unit || entry->view || entry->positions.Count ||
       !xlsSemBodyPrepare(old,nodes,moves,sources,&current,function,cursor,start,shifted,&certifiedNodes))goto eager;
    if(!xlsSemBodyJobsReserve(build,limit))goto eager;
    size_t jobs=build->bodyJobs.Capacity*sizeof(xls_sem_body_job);
    if(build->bodyBytes>limit-jobs)goto eager;
    view=xlsSemViewCreate(old->unit,&current,limit-jobs-build->bodyBytes);
    if(!view || entry->key.Capacity>XLS_SEM_MEMO_BYTES-build->memo->bytes ||
       view->bytes>XLS_SEM_MEMO_BYTES-build->memo->bytes-entry->key.Capacity)goto eager;
    /* Header publication is the last fallible operation. Reserved job slots
     * below cannot grow, and all independent ownership is already established. */
    if(!xlsSemBodyHeader(view,nodes,sources,function,cursor,start,shifted))goto eager;
    xls_sem_body_job job={xlsSemViewRetain(view),(XL6_AstNode*)function,function->range,start,shifted};
    size_t at=build->bodyJobs.Count++;
    *(xls_sem_body_job*)xrtArrayGet(&build->bodyJobs,at)=job;
    build->bodyBytes+=view->bytes;entry->view=view;entry->unit=view->unit;*positions=view->positions;
    if(certifiedNodes){++build->work.proofHits;build->work.proofNodes+=certifiedNodes;}
    return 1;
eager:
    xlsSemViewRelease(view);xrtArrayUnit(&current);
    return xlsSemApplyFacts(old,nodes,moves,sources,positions,function,cursor,start,shifted);
}
/* Reconstruct only inside the owning result's AST-exposure lock. A successful
 * job is atomic; a later failed job leaves the complete tree unpublished and
 * may be retried. Neither immutable memo entries nor earlier results change. */
static int xlsSemBodyMaterialize(XL6_AstNode* root,xarray* jobs,size_t* next,const xls_sem_cursor* cursor)
{
    if(*next>=jobs->Count)return 1;
    xmap sources={0};xls_sem_moves unchanged={0};int okay=0;
    if(!xrtMapInit(&sources,sizeof(XL6_AstNode*)) || !xrtMapSetDrop(&sources,xlsSemSourceUnpin,NULL) ||
       !xrtMapInit(&unchanged.paths,sizeof(xls_sem_move)) || !xlsSemSourcePool(root,&sources,0))goto done;
    while(*next<jobs->Count){
        xls_sem_body_job* job=xrtArrayGet(jobs,*next);xarray raw={0},copied={0};
        xls_sem_entry entry={.unit=job->view->unit,.positions=job->view->positions,.lexicalRange=job->lexicalRange};
        int ready=xrtArrayInit(&raw,sizeof(xls_sem_raw)) && xlsSemRawNodes(job->function,&raw,0) &&
            xlsSemApplyFacts(&entry,&raw,&unchanged,&sources,&copied,job->function,cursor,job->start,job->shifted);
        xrtArrayUnit(&raw);xrtArrayUnit(&copied);
        if(!ready)goto done;
        ++*next;
    }
    okay=1;
done:
    xrtMapUnit(&sources);xrtMapUnit(&unchanged.paths);return okay;
}
