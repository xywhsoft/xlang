/* Validate equivalent physical inputs without weakening sealed producers.
 * The backend continues to consume each call's existing explicit projection. */
static int xl6IrProjectionResultEqual(const XL6_IrCallTarget* a,const XL6_IrCallTarget* b)
{
#define XL6_SAME(field) if(a->field!=b->field)return 0
    XL6_SAME(foreignOperationAbi);XL6_SAME(returnType);XL6_SAME(resultPhysicalAbi);
    XL6_SAME(resultConversionAbi);XL6_SAME(resultCAbi);XL6_SAME(outputResultAbi);
    XL6_SAME(nativeSuccessCode);XL6_SAME(resultPhysicalFieldDeclaration);XL6_SAME(resultPhysicalDestroySymbol);
    XL6_SAME(errorAbi);XL6_SAME(errorPhysicalAbi);XL6_SAME(producerErrorPhysicalAbi);
    XL6_SAME(preparationFailureAbi);XL6_SAME(executionBoundaryAbi);XL6_SAME(implementationAbi);
    XL6_SAME(lifecycleAbi);XL6_SAME(effectAbi);
#undef XL6_SAME
    return 1;
}
static int xl6IrProjectionArraySpan(const XL6_IrModule* m,const XL6_IrCallTarget* t,
    const XL6_ForeignPhysicalStep* step,const XL6_ForeignParameterContract* span)
{
    const XL6_ForeignArrayGroupContract* group=xl6IrForeignArrayGroupGet(m,t,step->physicalIndex);
    if(!group || group->firstParameter!=step->parameter || group->arrayCount!=1 || group->recordCName[0] ||
        group->countAbi!=span->countAbi || group->sizeParameter>=t->parameterCount || !xl6IrForeignArrayGroupsMatch(m,t))return 0;
    size_t found=0;
    for(size_t i=0;i<m->foreignArrayInputCount;++i){const XL6_ForeignArrayInputContract* row=&m->foreignArrayInputs[i];
        if(row->declaration!=t->declaration || row->groupPhysicalIndex!=step->physicalIndex)continue;
        if(row->column || row->kind!=XL6_FOREIGN_ARRAY_ELEMENT_VALUE || strcmp(row->pointerCName,span->pointerCName))return 0;
        ++found;
    }
    return found==1;
}
static const XL6_ForeignParameterContract* xl6IrProjectionSpan(const XL6_IrModule* m,
    const XL6_IrCallTarget* t,const XL6_ForeignPhysicalStep* step)
{
    if(step->kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT)return NULL;
    const XL6_ForeignParameterContract* span=xl6IrForeignParameterGet(m,t,step->parameter);
    return span && span->kind==XL6_FOREIGN_PARAMETER_SPAN && span->readOnly ? span:NULL;
}
static int xl6IrForeignProjectionCompatible(const XL6_IrModule* m,const XL6_IrCallTarget* a,const XL6_IrCallTarget* b)
{
    const XL6_IrNode* an=xl6IrNodeGet(m,a->declaration);const XL6_IrNode* bn=xl6IrNodeGet(m,b->declaration);
    if(!an || !bn)return 0;
    if(!xl6ForeignInputHasPacks(an->foreignCall.inputMappingFacts) && !xl6ForeignInputHasPacks(bn->foreignCall.inputMappingFacts) &&
        !xl6ForeignInputHasArrayGroups(an->foreignCall.inputMappingFacts) && !xl6ForeignInputHasArrayGroups(bn->foreignCall.inputMappingFacts))return -1;
    if(a->parameterCount==b->parameterCount && xl6IrForeignOverloadMetadataEqual(m,a,b))return -1;
    const uint64 flags=XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_NO_RAISE|
        XL6_AST_FLAG_FOREIGN_NONBLOCKING|XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    const char* ah=xl6IrNullHeader(m,an,a->declaration);const char* bh=xl6IrNullHeader(m,bn,b->declaration);
    if(a->kind!=XL6_IR_CALL_TARGET_EXTERN_C || b->kind!=a->kind || a->runtimeBridge || b->runtimeBridge ||
        a->variadicAbi || b->variadicAbi || a->nativeContextAbi || b->nativeContextAbi ||
        a->nativeServicesRequired || b->nativeServicesRequired || a->nativeResultAbi || b->nativeResultAbi ||
        a->nativeArrayResultAbi || b->nativeArrayResultAbi || an->sourceKind!=bn->sourceKind ||
        an->declarationOwnerType!=bn->declarationOwnerType || ((an->flags^bn->flags)&flags) ||
        !(an->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || !ah || !bh || strcmp(ah,bh) ||
        !xl6IrProjectionResultEqual(a,b) || a->parameterCount>64 || b->parameterCount>64)return 0;
    XL6_ForeignCallFacts af=an->foreignCall,bf=bn->foreignCall;
    if(af.staticCallbackAddresses || bf.staticCallbackAddresses || af.retainedCallbacks || bf.retainedCallbacks ||
        af.controlViews || bf.controlViews || af.resultOwnerIndexPlusOne || bf.resultOwnerIndexPlusOne ||
        xl6ForeignInputCallbackCount(af.inputMappingFacts) || xl6ForeignInputCallbackCount(bf.inputMappingFacts) ||
        xl6ForeignInputHasRegions(af.inputMappingFacts) || xl6ForeignInputHasRegions(bf.inputMappingFacts) ||
        xl6ForeignInputHasFixedInputs(af.inputMappingFacts) || xl6ForeignInputHasFixedInputs(bf.inputMappingFacts))return 0;
    size_t steps=xl6IrForeignPhysicalStepCount(m,a),other=xl6IrForeignPhysicalStepCount(m,b),changed=0;
    if(steps==SIZE_MAX || steps!=other || steps>a->parameterCount+128)return 0;
    size_t receiver=an->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC && !(an->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    uint32 am[64],bm[64];for(size_t p=0;p<64;++p)am[p]=bm[p]=UINT32_MAX;
    for(size_t i=0;i<steps;++i){XL6_ForeignPhysicalStep x,y;
        if(!xl6IrForeignPhysicalStepGet(m,a,i,&x) || !xl6IrForeignPhysicalStepGet(m,b,i,&y) || x.physicalIndex!=y.physicalIndex)return 0;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK && y.kind==x.kind){
            const XL6_ForeignPackContract* ap=xl6IrForeignPackGet(m,a,x.physicalIndex);
            const XL6_ForeignPackContract* bp=xl6IrForeignPackGet(m,b,y.physicalIndex);
            if(!ap || !bp || strcmp(ap->recordCName,bp->recordCName) || strcmp(ap->header,bp->header) ||
                !xl6IrForeignPacksMatch(m,a) || !xl6IrForeignPacksMatch(m,b))return 0;
            ++changed;continue;
        }
        const XL6_ForeignParameterContract* xs=xl6IrProjectionSpan(m,a,&x);
        const XL6_ForeignParameterContract* ys=xl6IrProjectionSpan(m,b,&y);
        if(xs && y.kind==XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP){if(!xl6IrProjectionArraySpan(m,b,&y,xs))return 0;++changed;continue;}
        if(ys && x.kind==XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP){if(!xl6IrProjectionArraySpan(m,a,&x,ys))return 0;++changed;continue;}
        if(x.kind!=y.kind)return 0;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_OUTPUT){if(x.ordinal!=y.ordinal)return 0;continue;}
        if(x.kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT || x.parameter>=a->parameterCount || y.parameter>=b->parameterCount)return 0;
        am[x.parameter]=x.physicalIndex;bm[y.parameter]=y.physicalIndex;
        if(!xl6IrNullInputMeta(m,a,x.parameter,am,b,y.parameter,bm,receiver))return 0;
    }
    if(!changed)return -1;
    af.constAggregateAddresses=bf.constAggregateAddresses=af.textViews=bf.textViews=0;
    af.bytesViews=bf.bytesViews=af.terminatedStrings=bf.terminatedStrings=0;
    af.physicalPairs=bf.physicalPairs=af.inputMappingFacts=bf.inputMappingFacts=0;
    if(!xl6ForeignCallFactsEqual(&af,&bf))return 0;
#define XL6_PROJECTION_OUTPUT_ROWS(field,count) if(!xl6IrForeignRowsEqual(m->field,m->count,sizeof(*m->field),a->declaration,b->declaration))return 0
    XL6_PROJECTION_OUTPUT_ROWS(foreignOutputs,foreignOutputCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignOutputSizes,foreignOutputSizeCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignOutputViews,foreignOutputViewCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignOutputRecords,foreignOutputRecordCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignOutputFields,foreignOutputFieldCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignNativeSequences,foreignNativeSequenceCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignFieldLengths,foreignFieldLengthCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignFieldCalls,foreignFieldCallCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignFunctionHeaders,foreignFunctionHeaderCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignStorage,foreignStorageCount);
    XL6_PROJECTION_OUTPUT_ROWS(foreignRetains,foreignRetainCount);
#undef XL6_PROJECTION_OUTPUT_ROWS
    return 1;
}
