/* This helper compares sealed physical steps; it never trusts a C header
 * failure to repair an invalid language/ownership contract after emission. */
static const char* xl6IrNullHeader(const XL6_IrModule* m,const XL6_IrNode* node,uint32 declaration)
{
    const XL6_IrType* type=xl6IrTypeGet(&m->types,node->declarationOwnerType);const char* header=type ? type->foreignHeader:NULL;
    for(size_t i=0;i<m->foreignFunctionHeaderCount;++i)if(m->foreignFunctionHeaders[i].declaration==declaration)header=m->foreignFunctionHeaders[i].header;
    return header && xl6ForeignHeaderName(header) ? header:NULL;
}
static int xl6IrNullOmittedType(const XL6_IrModule* m,const XL6_IrCallTarget* full,
    const XL6_ForeignPhysicalStep* step,const char* pointer)
{
    char expected[128];
    if(step->kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){const XL6_ForeignPackContract* pack=xl6IrForeignPackGet(m,full,step->physicalIndex);
        if(!pack || pack->firstParameter!=step->parameter || !xl6IrForeignPacksMatch(m,full))return 0;
        snprintf(expected,sizeof(expected),"const %s*",pack->recordCName);return !strcmp(pointer,expected);
    }
    if(step->kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT || step->parameter>=full->parameterCount ||
        full->parameterPhysicalAbis[step->parameter]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_C_AGGREGATE_BORROW ||
        full->parameterAbis[step->parameter]!=XL6_IR_PARAMETER_ABI_VALUE || full->parameterPhysicalFieldDeclarations[step->parameter] ||
        full->parameterConstraintAbis[step->parameter])return 0;
    const XL6_IrType* type=xl6IrTypeGet(&m->types,full->parameterTypes[step->parameter]);
    if(!type || type->kind!=XL6_IR_TYPE_C_RECORD || !xl6IrForeignTrivialCValue(m,full->parameterTypes[step->parameter],0))return 0;
    snprintf(expected,sizeof(expected),"const %s*",type->cName);return !strcmp(pointer,expected);
}
static int xl6IrNullInputBit(uint64 bits,size_t p)
{return p<64 && (bits&(UINT64_C(1)<<p))!=0;}
static unsigned xl6IrNullInputFlags(const XL6_IrNode* node,size_t p,size_t receiver)
{
    const XL6_ForeignCallFacts* f=&node->foreignCall;size_t explicitIndex=p>=receiver ? p-receiver:SIZE_MAX;
    return xl6IrNullInputBit(f->constAggregateAddresses,explicitIndex)|
        xl6IrNullInputBit(f->staticCallbackAddresses,explicitIndex)<<1|xl6IrNullInputBit(f->textViews,explicitIndex)<<2|
        xl6IrNullInputBit(f->bytesViews,explicitIndex)<<3|xl6IrNullInputBit(f->terminatedStrings,explicitIndex)<<4|
        xl6IrNullInputBit(f->physicalPairs,explicitIndex)<<5|xl6IrNullInputBit(f->retainedCallbacks,explicitIndex)<<6|
        xl6IrNullInputBit(f->controlViews,p)<<7;
}
static int xl6IrNullInputMeta(const XL6_IrModule* m,const XL6_IrCallTarget* a,size_t p,const uint32* am,
    const XL6_IrCallTarget* b,size_t q,const uint32* bm,size_t receiver)
{
    const XL6_IrNode* an=xl6IrNodeGet(m,a->declaration);const XL6_IrNode* bn=xl6IrNodeGet(m,b->declaration);
    if(p>=a->parameterCount || q>=b->parameterCount || a->parameterTypes[p]!=b->parameterTypes[q] ||
        a->parameterAbis[p]!=b->parameterAbis[q] || a->parameterPhysicalAbis[p]!=b->parameterPhysicalAbis[q] ||
        a->parameterLifetimes[p].rebindable!=b->parameterLifetimes[q].rebindable ||
        a->parameterLifetimes[p].borrowReadOnly!=b->parameterLifetimes[q].borrowReadOnly ||
        a->parameterLifetimes[p].cleanupEffect!=b->parameterLifetimes[q].cleanupEffect ||
        a->parameterPhysicalFieldDeclarations[p]!=b->parameterPhysicalFieldDeclarations[q] || a->parameterConstraintAbis[p]!=b->parameterConstraintAbis[q] ||
        xl6IrNullInputFlags(an,p,receiver)!=xl6IrNullInputFlags(bn,q,receiver))return 0;
    const XL6_ForeignParameterContract* xp=xl6IrForeignParameterGet(m,a,p);const XL6_ForeignParameterContract* yp=xl6IrForeignParameterGet(m,b,q);
    if((xp!=NULL)!=(yp!=NULL))return 0;
    if(xp){XL6_ForeignParameterContract x=*xp,y=*yp;
        x.declaration=y.declaration=0;x.parameter=y.parameter=0;x.callbackTypeCName[0]=y.callbackTypeCName[0]=0;
        memset(x.callbackTypeCName,0,sizeof(x.callbackTypeCName));memset(y.callbackTypeCName,0,sizeof(y.callbackTypeCName));
        if(x.callbackOwnerIndexPlusOne){size_t i=x.callbackOwnerIndexPlusOne-1;if(i>=a->parameterCount || am[i]==UINT32_MAX)return 0;x.callbackOwnerIndexPlusOne=am[i]+1;}
        if(y.callbackOwnerIndexPlusOne){size_t i=y.callbackOwnerIndexPlusOne-1;if(i>=b->parameterCount || bm[i]==UINT32_MAX)return 0;y.callbackOwnerIndexPlusOne=bm[i]+1;}
        if(memcmp(&x,&y,sizeof(x)))return 0;
    }
    size_t lq=0,rq=0,lv=0,rv=0;
    for(size_t i=0;i<m->foreignPointerQualifierCount;++i){const XL6_ForeignPointerQualifierContract* row=&m->foreignPointerQualifiers[i];
        lq+=row->declaration==a->declaration && row->parameter==p;rq+=row->declaration==b->declaration && row->parameter==q;}
    for(size_t i=0;i<m->foreignValueCount;++i){const XL6_ForeignValueContract* row=&m->foreignValues[i];if(row->declaration!=a->declaration || row->parameter!=p)continue;
        ++lv;size_t matches=0;for(size_t j=0;j<m->foreignValueCount;++j){const XL6_ForeignValueContract* other=&m->foreignValues[j];
            matches+=other->declaration==b->declaration && other->parameter==q && !strcmp(row->cName,other->cName);}
        if(matches!=1)return 0;
    }
    for(size_t i=0;i<m->foreignValueCount;++i)rv+=m->foreignValues[i].declaration==b->declaration && m->foreignValues[i].parameter==q;
    if(lq!=rq || lv!=rv)return 0;
    if(p<receiver || q<receiver)return p<receiver && q<receiver;
    size_t lc=0,rc=0;
    for(size_t i=0;i<m->foreignCallbackSpanCount;++i){XL6_ForeignCallbackSpanContract row=m->foreignCallbackSpans[i];
        if(row.declaration!=a->declaration || row.parameter!=p-receiver)continue;
        ++lc;row.declaration=row.parameter=0;size_t matches=0;
        for(size_t j=0;j<m->foreignCallbackSpanCount;++j){XL6_ForeignCallbackSpanContract other=m->foreignCallbackSpans[j];
            if(other.declaration!=b->declaration || other.parameter!=q-receiver)continue;
            other.declaration=other.parameter=0;matches+=!memcmp(&row,&other,sizeof(row));}
        if(matches!=1)return 0;
    }
    for(size_t i=0;i<m->foreignCallbackSpanCount;++i)rc+=m->foreignCallbackSpans[i].declaration==b->declaration && m->foreignCallbackSpans[i].parameter==q-receiver;
    return lc==rc;
}
static int xl6IrNullPackMeta(const XL6_IrModule* m,const XL6_IrCallTarget* a,const XL6_ForeignPackContract* ap,
    const XL6_IrCallTarget* b,const XL6_ForeignPackContract* bp)
{
    if(!ap || !bp || ap->parameterCount!=bp->parameterCount || ap->physicalIndex!=bp->physicalIndex ||
        strcmp(ap->recordCName,bp->recordCName) || strcmp(ap->header,bp->header))return 0;
    size_t left=0,right=0;
    for(size_t i=0;i<m->foreignPackFieldCount;++i){XL6_ForeignPackFieldContract row=m->foreignPackFields[i];
        if(row.declaration!=a->declaration || row.packPhysicalIndex!=ap->physicalIndex)continue;
        ++left;row.declaration=row.recordParentIndexPlusOne=0;if(row.parameter!=UINT32_MAX)row.parameter-=ap->firstParameter;size_t matches=0;
        for(size_t j=0;j<m->foreignPackFieldCount;++j){XL6_ForeignPackFieldContract other=m->foreignPackFields[j];
            if(other.declaration!=b->declaration || other.packPhysicalIndex!=bp->physicalIndex)continue;
            other.declaration=other.recordParentIndexPlusOne=0;if(other.parameter!=UINT32_MAX)other.parameter-=bp->firstParameter;matches+=!memcmp(&row,&other,sizeof(row));}
        if(matches!=1)return 0;
    }
    for(size_t i=0;i<m->foreignPackFieldCount;++i)right+=m->foreignPackFields[i].declaration==b->declaration && m->foreignPackFields[i].packPhysicalIndex==bp->physicalIndex;
    return left==right;
}
static int xl6IrForeignNullPhysicalCompatible(const XL6_IrModule* m,const XL6_IrCallTarget* a,const XL6_IrCallTarget* b)
{
    const XL6_IrNode* an=xl6IrNodeGet(m,a->declaration);const XL6_IrNode* bn=xl6IrNodeGet(m,b->declaration);
    if(!an || !bn)return 0;
    if(!xl6ForeignInputHasFixedInputs(an->foreignCall.inputMappingFacts) && !xl6ForeignInputHasFixedInputs(bn->foreignCall.inputMappingFacts))return -1;
    size_t steps=xl6IrForeignPhysicalStepCount(m,a),other=xl6IrForeignPhysicalStepCount(m,b);int changed=0;
    if(a->parameterCount>64 || b->parameterCount>64 || steps!=other || steps>a->parameterCount+128)return 0;
    for(size_t i=0;i<steps;++i){XL6_ForeignPhysicalStep x,y;
        if(!xl6IrForeignPhysicalStepGet(m,a,i,&x) || !xl6IrForeignPhysicalStepGet(m,b,i,&y) || x.physicalIndex!=y.physicalIndex)return 0;
        if((x.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED)!=(y.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED)){
            const XL6_IrCallTarget* fixed=x.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED ? a:b;
            const XL6_IrCallTarget* full=fixed==a ? b:a;const XL6_ForeignPhysicalStep* omitted=fixed==a ? &y:&x;
            const XL6_ForeignFixedInputContract* row=xl6IrForeignFixedInputGet(m,fixed,x.physicalIndex);
            if(!row || row->kind!=XL6_FOREIGN_FIXED_TYPED_NULL || !xl6IrNullOmittedType(m,full,omitted,row->pointerCName))return 0;
            ++changed;
        }
    }
    if(!changed)return -1;
    const uint64 flags=XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_NO_RAISE|XL6_AST_FLAG_FOREIGN_NONBLOCKING|XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    const char* ah=xl6IrNullHeader(m,an,a->declaration);const char* bh=xl6IrNullHeader(m,bn,b->declaration);
    if(a->kind!=XL6_IR_CALL_TARGET_EXTERN_C || b->kind!=a->kind || a->runtimeBridge || b->runtimeBridge ||
        a->variadicAbi || b->variadicAbi || a->nativeContextAbi || b->nativeContextAbi || a->nativeServicesRequired || b->nativeServicesRequired ||
        a->nativeResultAbi || b->nativeResultAbi || a->nativeArrayResultAbi || b->nativeArrayResultAbi ||
        !a->cName || !b->cName || strcmp(a->cName,b->cName) || an->sourceKind!=bn->sourceKind || an->declarationOwnerType!=bn->declarationOwnerType ||
        ((an->flags^bn->flags)&flags) || !(an->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        !ah || !bh || strcmp(ah,bh) || xl6ForeignInputHasRegions(an->foreignCall.inputMappingFacts) || xl6ForeignInputHasRegions(bn->foreignCall.inputMappingFacts) ||
        xl6ForeignInputHasArrayGroups(an->foreignCall.inputMappingFacts) || xl6ForeignInputHasArrayGroups(bn->foreignCall.inputMappingFacts) ||
        a->foreignOperationAbi!=b->foreignOperationAbi || a->returnType!=b->returnType || a->resultPhysicalAbi!=b->resultPhysicalAbi ||
        a->resultConversionAbi!=b->resultConversionAbi || a->resultCAbi!=b->resultCAbi || a->outputResultAbi!=b->outputResultAbi ||
        a->nativeSuccessCode!=b->nativeSuccessCode || a->resultPhysicalFieldDeclaration!=b->resultPhysicalFieldDeclaration ||
        a->resultPhysicalDestroySymbol!=b->resultPhysicalDestroySymbol || a->errorAbi!=b->errorAbi || a->errorPhysicalAbi!=b->errorPhysicalAbi ||
        a->producerErrorPhysicalAbi!=b->producerErrorPhysicalAbi || a->preparationFailureAbi!=b->preparationFailureAbi ||
        a->executionBoundaryAbi!=b->executionBoundaryAbi || a->implementationAbi!=b->implementationAbi || a->lifecycleAbi!=b->lifecycleAbi ||
        a->effectAbi!=b->effectAbi)return 0;
    uint32 am[64],bm[64];for(size_t i=0;i<64;++i)am[i]=bm[i]=UINT32_MAX;
    size_t receiver=an->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC && !(an->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    for(size_t i=0;i<steps;++i){XL6_ForeignPhysicalStep x,y;if(!xl6IrForeignPhysicalStepGet(m,a,i,&x) || !xl6IrForeignPhysicalStepGet(m,b,i,&y))return 0;
        if(x.kind!=y.kind)continue;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_INPUT){am[x.parameter]=x.physicalIndex*65;bm[y.parameter]=y.physicalIndex*65;}
        else if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){const XL6_ForeignPackContract* ap=xl6IrForeignPackGet(m,a,x.physicalIndex);const XL6_ForeignPackContract* bp=xl6IrForeignPackGet(m,b,y.physicalIndex);
            if(!ap || !bp || ap->parameterCount!=bp->parameterCount)return 0;
            for(size_t n=0;n<ap->parameterCount;++n){am[x.parameter+n]=x.physicalIndex*65+(uint32)n;bm[y.parameter+n]=y.physicalIndex*65+(uint32)n;}}
    }
    for(size_t i=0;i<steps;++i){XL6_ForeignPhysicalStep x,y;if(!xl6IrForeignPhysicalStepGet(m,a,i,&x) || !xl6IrForeignPhysicalStepGet(m,b,i,&y))return 0;
        if(x.kind!=y.kind)continue;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_OUTPUT){if(x.ordinal!=y.ordinal)return 0;continue;}
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED){const XL6_ForeignFixedInputContract* xf=xl6IrForeignFixedInputGet(m,a,x.physicalIndex);const XL6_ForeignFixedInputContract* yf=xl6IrForeignFixedInputGet(m,b,y.physicalIndex);
            if(!xf || !yf || xf->kind!=yf->kind || strcmp(xf->pointerCName,yf->pointerCName))return 0;
            continue;}
        size_t count=1;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){const XL6_ForeignPackContract* ap=xl6IrForeignPackGet(m,a,x.physicalIndex);const XL6_ForeignPackContract* bp=xl6IrForeignPackGet(m,b,y.physicalIndex);
            if(!xl6IrNullPackMeta(m,a,ap,b,bp))return 0;
            count=ap->parameterCount;}
        else if(x.kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT)return 0;
        for(size_t n=0;n<count;++n)if(!xl6IrNullInputMeta(m,a,x.parameter+n,am,b,y.parameter+n,bm,receiver))return 0;
    }
    XL6_ForeignCallFacts af=an->foreignCall,bf=bn->foreignCall;
    if(af.resultOwnerIndexPlusOne || bf.resultOwnerIndexPlusOne){size_t p=af.resultOwnerIndexPlusOne-1,q=bf.resultOwnerIndexPlusOne-1;
        if(!af.resultOwnerIndexPlusOne || !bf.resultOwnerIndexPlusOne || p>=a->parameterCount || q>=b->parameterCount || am[p]==UINT32_MAX || am[p]!=bm[q])return 0;}
    af.constAggregateAddresses=bf.constAggregateAddresses=af.staticCallbackAddresses=bf.staticCallbackAddresses=0;
    af.textViews=bf.textViews=af.bytesViews=bf.bytesViews=af.terminatedStrings=bf.terminatedStrings=0;
    af.physicalPairs=bf.physicalPairs=af.retainedCallbacks=bf.retainedCallbacks=af.controlViews=bf.controlViews=0;
    af.resultOwnerIndexPlusOne=bf.resultOwnerIndexPlusOne=0;af.inputMappingFacts=bf.inputMappingFacts=0;
    if(!xl6ForeignCallFactsEqual(&af,&bf))return 0;
#define XL6_NULL_OUTPUT_ROWS(field,count) if(!xl6IrForeignRowsEqual(m->field,m->count,sizeof(*m->field),a->declaration,b->declaration))return 0
    XL6_NULL_OUTPUT_ROWS(foreignOutputs,foreignOutputCount);
    XL6_NULL_OUTPUT_ROWS(foreignOutputSizes,foreignOutputSizeCount);
    XL6_NULL_OUTPUT_ROWS(foreignOutputViews,foreignOutputViewCount);
    XL6_NULL_OUTPUT_ROWS(foreignOutputRecords,foreignOutputRecordCount);
    XL6_NULL_OUTPUT_ROWS(foreignOutputFields,foreignOutputFieldCount);
    XL6_NULL_OUTPUT_ROWS(foreignNativeSequences,foreignNativeSequenceCount);
    XL6_NULL_OUTPUT_ROWS(foreignFieldLengths,foreignFieldLengthCount);
    XL6_NULL_OUTPUT_ROWS(foreignFieldCalls,foreignFieldCallCount);
    XL6_NULL_OUTPUT_ROWS(foreignFunctionHeaders,foreignFunctionHeaderCount);
    XL6_NULL_OUTPUT_ROWS(foreignStorage,foreignStorageCount);
    XL6_NULL_OUTPUT_ROWS(foreignRetains,foreignRetainCount);
#undef XL6_NULL_OUTPUT_ROWS
    return 1;
}
