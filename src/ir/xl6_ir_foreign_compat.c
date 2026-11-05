/* Same C target overloads remain distinct logical signatures. The sole
 * source-kind substitution is bytes <-> Buffer for an identical readonly
 * Region; every producer, effect and owning edge stays invariant. */
static int xl6IrForeignRowsEqual(const void* data,size_t count,size_t stride,uint32 a,uint32 b)
{
    const unsigned char* bytes=data;size_t left=0,right=0;
    for(size_t i=0;i<count;++i){
        const unsigned char* row=bytes+i*stride;uint32 declaration;memcpy(&declaration,row,sizeof(declaration));
        if(declaration==b)++right;
        if(declaration!=a)continue;
        ++left;size_t matches=0;
        for(size_t j=0;j<count;++j){
            const unsigned char* other=bytes+j*stride;uint32 owner;memcpy(&owner,other,sizeof(owner));
            matches+=owner==b && !memcmp(row+sizeof(uint32),other+sizeof(uint32),stride-sizeof(uint32));
        }
        if(matches!=1)return 0;
    }
    return left==right;
}
#include "src/ir/xl6_ir_foreign_span_compat.c"
static int xl6IrForeignReadonlySpanInputCompatible(const XL6_IrModule* m,const XL6_IrCallTarget* a,const XL6_IrCallTarget* b,size_t p)
{return xl6IrForeignReadonlySpanByteInputCompatible(m,a,b,p);}
static int xl6IrForeignPackFieldsEqual(const XL6_IrModule* m,const XL6_IrCallTarget* a,const XL6_IrCallTarget* b)
{
    size_t left=0,right=0;
    for(size_t i=0;i<m->foreignPackFieldCount;++i){const XL6_ForeignPackFieldContract* row=&m->foreignPackFields[i];
        right+=row->declaration==b->declaration;if(row->declaration!=a->declaration)continue;
        ++left;size_t matches=0;
        for(size_t j=0;j<m->foreignPackFieldCount;++j){const XL6_ForeignPackFieldContract* other=&m->foreignPackFields[j];if(other->declaration!=b->declaration)continue;
            XL6_ForeignPackFieldContract x=*row,y=*other;x.declaration=y.declaration=0;
            x.recordParentIndexPlusOne=y.recordParentIndexPlusOne=0;
            if(x.logicalType!=y.logicalType){
                if(x.parameter!=y.parameter || x.kind!=y.kind ||
                    (x.kind!=XL6_FOREIGN_PACK_FIELD_SPAN_DATA && x.kind!=XL6_FOREIGN_PACK_FIELD_SPAN_COUNT) ||
                    !xl6IrForeignReadonlySpanInputCompatible(m,a,b,x.parameter))continue;
                x.logicalType=y.logicalType=0;
            }
            matches+=!memcmp(&x,&y,sizeof(x));
        }
        if(matches!=1)return 0;
    }
    return left==right;
}
static int xl6IrForeignOverloadMetadataEqual(const XL6_IrModule* m,const XL6_IrCallTarget* left,const XL6_IrCallTarget* right)
{
    uint32 a=left->declaration,b=right->declaration;
#define XL6_EQUAL_ROWS(field,count) if(!xl6IrForeignRowsEqual(m->field,m->count,sizeof(*m->field),a,b))return 0
    XL6_EQUAL_ROWS(foreignOutputs,foreignOutputCount);
    XL6_EQUAL_ROWS(foreignOutputSizes,foreignOutputSizeCount);
    XL6_EQUAL_ROWS(foreignOutputViews,foreignOutputViewCount);
    XL6_EQUAL_ROWS(foreignOutputRecords,foreignOutputRecordCount);
    XL6_EQUAL_ROWS(foreignOutputFields,foreignOutputFieldCount);
    XL6_EQUAL_ROWS(foreignPacks,foreignPackCount);
    XL6_EQUAL_ROWS(foreignFixedInputs,foreignFixedInputCount);
    if(!xl6IrForeignPackFieldsEqual(m,left,right))return 0;
    XL6_EQUAL_ROWS(foreignPointerQualifiers,foreignPointerQualifierCount);
    XL6_EQUAL_ROWS(foreignValues,foreignValueCount);
    XL6_EQUAL_ROWS(foreignCallbackSpans,foreignCallbackSpanCount);
    XL6_EQUAL_ROWS(foreignFieldLengths,foreignFieldLengthCount);
    XL6_EQUAL_ROWS(foreignFieldCalls,foreignFieldCallCount);
    XL6_EQUAL_ROWS(foreignFunctionHeaders,foreignFunctionHeaderCount);
    XL6_EQUAL_ROWS(foreignStorage,foreignStorageCount);
    XL6_EQUAL_ROWS(foreignRetains,foreignRetainCount);
#undef XL6_EQUAL_ROWS
    return 1;
}
static int xl6IrForeignRegionEqual(const XL6_ForeignRegionContract* a,const XL6_ForeignRegionContract* b)
{
    return a && b && a->physicalIndex==b->physicalIndex && a->parameter==b->parameter && a->unitBytes==b->unitBytes &&
        a->access==b->access && a->nullable==b->nullable && !memcmp(&a->width,&b->width,sizeof(a->width)) &&
        !memcmp(&a->height,&b->height,sizeof(a->height)) && !memcmp(&a->stride,&b->stride,sizeof(a->stride));
}
static int xl6IrForeignManagedInputCompatible(const XL6_IrModule* module,const XL6_IrCallTarget* a,const XL6_IrCallTarget* b,size_t p)
{
    const XL6_IrType* left=xl6IrTypeGet(&module->types,a->parameterTypes[p]);const XL6_IrType* right=xl6IrTypeGet(&module->types,b->parameterTypes[p]);
    return left && right && left->kind==XL6_IR_TYPE_CLASS && right->kind==XL6_IR_TYPE_CLASS &&
        a->parameterAbis[p]==XL6_IR_PARAMETER_ABI_BORROW && b->parameterAbis[p]==XL6_IR_PARAMETER_ABI_BORROW &&
        a->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW &&
        b->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW &&
        xl6IrForeignManagedViewParameter(module,a,p) && xl6IrForeignManagedViewParameter(module,b,p) &&
        ((left->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED && right->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) ||
         (right->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED && left->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK)) &&
        left->foreignHeader && right->foreignHeader && !strcmp(left->cName,right->cName) && !strcmp(left->foreignHeader,right->foreignHeader);
}
#include "src/ir/xl6_ir_foreign_null_compat.c"
static int xl6IrForeignOverloadsCompatible(const XL6_IrModule* module,const XL6_IrCallTarget* a,const XL6_IrCallTarget* b)
{
    int nullCompatibility=xl6IrForeignNullPhysicalCompatible(module,a,b);
    if(nullCompatibility>=0)return nullCompatibility;
    const XL6_IrNode* left=xl6IrNodeGet(module,a->declaration);const XL6_IrNode* right=xl6IrNodeGet(module,b->declaration);
    if(!left || !right || !xl6ForeignCallFactsEqual(&left->foreignCall,&right->foreignCall) ||
        ((left->flags^right->flags)&(XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_OWNED_STRING|XL6_AST_FLAG_FOREIGN_PURE|
            XL6_AST_FLAG_FOREIGN_NO_RAISE|XL6_AST_FLAG_FOREIGN_NONBLOCKING)) ||
        a->foreignOperationAbi!=b->foreignOperationAbi || a->returnType!=b->returnType || a->resultPhysicalAbi!=b->resultPhysicalAbi ||
        a->resultConversionAbi!=b->resultConversionAbi || a->resultCAbi!=b->resultCAbi || a->outputResultAbi!=b->outputResultAbi ||
        a->nativeSuccessCode!=b->nativeSuccessCode || a->resultPhysicalFieldDeclaration!=b->resultPhysicalFieldDeclaration ||
        a->resultPhysicalDestroySymbol!=b->resultPhysicalDestroySymbol || a->errorAbi!=b->errorAbi || a->effectAbi!=b->effectAbi ||
        a->parameterCount!=b->parameterCount || a->variadicAbi!=b->variadicAbi || a->nativeContextAbi!=b->nativeContextAbi ||
        a->nativeServicesRequired!=b->nativeServicesRequired || a->nativeResultAbi!=b->nativeResultAbi ||
        !xl6IrForeignOverloadMetadataEqual(module,a,b))return 0;
    for(size_t p=0;p<a->parameterCount;++p){
        const XL6_ForeignRegionContract* x=xl6IrForeignRegionGet(module,a,p);const XL6_ForeignRegionContract* y=xl6IrForeignRegionGet(module,b,p);
        if((x!=NULL)!=(y!=NULL) || (x && !xl6IrForeignRegionEqual(x,y)) ||
            (a->parameterAbis[p]!=b->parameterAbis[p] && !xl6IrForeignReadonlySpanInputCompatible(module,a,b,p)) || a->parameterPhysicalAbis[p]!=b->parameterPhysicalAbis[p] ||
            a->parameterConstraintAbis[p]!=b->parameterConstraintAbis[p])return 0;
        if(a->parameterTypes[p]!=b->parameterTypes[p]){
            const XL6_IrType* lt=xl6IrTypeGet(&module->types,a->parameterTypes[p]);const XL6_IrType* rt=xl6IrTypeGet(&module->types,b->parameterTypes[p]);
            if(!xl6IrForeignManagedInputCompatible(module,a,b,p) && !xl6IrForeignReadonlySpanInputCompatible(module,a,b,p) && (!x || x->access!=XL6_FOREIGN_REGION_ACCESS_READ || !lt || !rt ||
                !((lt->cAbi==XL6_IR_C_ABI_MANAGED_BYTES && xl6IrForeignRegionBufferField(module,b,p)) ||
                  (rt->cAbi==XL6_IR_C_ABI_MANAGED_BYTES && xl6IrForeignRegionBufferField(module,a,p)))))return 0;
        }else if(a->parameterPhysicalFieldDeclarations[p]!=b->parameterPhysicalFieldDeclarations[p])return 0;
        const XL6_ForeignRegionObserverContract* xo=xl6IrForeignRegionObserverGet(module,a,p);
        const XL6_ForeignRegionObserverContract* yo=xl6IrForeignRegionObserverGet(module,b,p);
        if((xo!=NULL)!=(yo!=NULL) || (xo && (xo->querySymbol!=yo->querySymbol || xo->recordType!=yo->recordType ||
            xo->inputCount!=yo->inputCount || memcmp(xo->parameters,yo->parameters,sizeof(xo->parameters)) ||
            memcmp(xo->physicalInputs,yo->physicalInputs,sizeof(xo->physicalInputs)))))return 0;
        const XL6_ForeignParameterContract* xp=xl6IrForeignParameterGet(module,a,p);const XL6_ForeignParameterContract* yp=xl6IrForeignParameterGet(module,b,p);
        if((xp!=NULL)!=(yp!=NULL) || (xp && (xp->kind!=yp->kind || strcmp(xp->pointerCName,yp->pointerCName) ||
            strcmp(xp->elementCName,yp->elementCName) || strcmp(xp->header,yp->header) || xp->countAbi!=yp->countAbi || xp->readOnly!=yp->readOnly ||
            strcmp(xp->callbackOwnerCName,yp->callbackOwnerCName) || xp->callbackOwnerIndexPlusOne!=yp->callbackOwnerIndexPlusOne ||
            xp->callbackFailureCode!=yp->callbackFailureCode || xp->callbackUserFirst!=yp->callbackUserFirst || xp->callbackResultKind!=yp->callbackResultKind)))return 0;
    }
    return 1;
}
static int xl6IrVerifyForeignOverloads(const XL6_IrModule* module,XL6_DiagList* diags)
{
    for(size_t i=0;i<module->callTargetCount;++i){
        const XL6_IrCallTarget* a=&module->callTargets[i];const XL6_IrNode* left=xl6IrNodeGet(module,a->declaration);
        if(a->kind!=XL6_IR_CALL_TARGET_EXTERN_C || !left || !xl6IrForeignOperationInvokesFunction(a->foreignOperationAbi))continue;
        for(size_t j=0;j<i;++j){
            const XL6_IrCallTarget* b=&module->callTargets[j];const XL6_IrNode* right=xl6IrNodeGet(module,b->declaration);
            if(b->kind!=XL6_IR_CALL_TARGET_EXTERN_C || !right || a->declaration==b->declaration || strcmp(a->cName,b->cName) ||
                left->sourceKind!=right->sourceKind || left->declarationOwnerType!=right->declarationOwnerType ||
                a->foreignOperationAbi!=b->foreignOperationAbi)continue;
            int lifetimeVariants=0;
            if(b->kind==XL6_IR_CALL_TARGET_EXTERN_C && a->parameterCount==b->parameterCount)
                for(size_t p=0;p<a->parameterCount;++p){
                    const XL6_IrType* lt=xl6IrTypeGet(&module->types,a->parameterTypes[p]);const XL6_IrType* rt=xl6IrTypeGet(&module->types,b->parameterTypes[p]);
                    if(lt && rt && lt->foreignHeader && rt->foreignHeader && lt->foreignLifetime!=rt->foreignLifetime)lifetimeVariants=1;
                }
            int retainedOutputs=0;
            for(size_t o=0;o<module->foreignOutputViewCount;++o)
                if((module->foreignOutputViews[o].declaration==a->declaration || module->foreignOutputViews[o].declaration==b->declaration) &&
                    module->foreignOutputViews[o].conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)retainedOutputs=1;
            int spans=0;
            for(size_t p=0;p<module->foreignParameterCount;++p)
                if((module->foreignParameters[p].declaration==a->declaration || module->foreignParameters[p].declaration==b->declaration) &&
                    module->foreignParameters[p].kind==XL6_FOREIGN_PARAMETER_SPAN)spans=1;
            if(left->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC && !spans && !xl6ForeignInputHasRegions(left->foreignCall.inputMappingFacts) &&
                !xl6ForeignInputHasRegions(right->foreignCall.inputMappingFacts) && !xl6ForeignInputHasPacks(left->foreignCall.inputMappingFacts) &&
                !xl6ForeignInputHasPacks(right->foreignCall.inputMappingFacts) &&
                !xl6ForeignInputHasFixedInputs(left->foreignCall.inputMappingFacts) && !xl6ForeignInputHasFixedInputs(right->foreignCall.inputMappingFacts) &&
                !xl6ForeignValueOutputIsRecord(left->foreignCall.valueOutput) &&
                !xl6ForeignValueOutputIsRecord(right->foreignCall.valueOutput) && !lifetimeVariants && !retainedOutputs)continue;
            if(!xl6IrForeignOverloadsCompatible(module,a,b))return xl6IrDiag(diags,NULL,"native target overloads disagree on their sealed physical, effect or ownership contract");
        }
    }
    return 1;
}
