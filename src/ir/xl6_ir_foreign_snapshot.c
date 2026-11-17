/* Sparse, closed data admissions. These rows do not change pointer layout. */
const XL6_ForeignDataSnapshotContract* xl6IrForeignDataSnapshotGet(
    const XL6_IrModule* m,const XL6_IrCallTarget* target,size_t p)
{
    if(!m || !target || p>=target->parameterCount ||
        m->foreignDataSnapshotCount>m->foreignDataSnapshotCapacity ||
        (m->foreignDataSnapshotCount && !m->foreignDataSnapshots))return NULL;
    for(size_t i=0;i<m->foreignDataSnapshotCount;++i)
        if(m->foreignDataSnapshots[i].declaration==target->declaration && m->foreignDataSnapshots[i].parameter==p)
            return &m->foreignDataSnapshots[i];
    return NULL;
}
static int xl6IrDataSnapshotExpected(const XL6_IrModule* m,
    const XL6_ForeignDataSnapshotContract* row,XL6_IrTypeId* result)
{
    const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(m,row->declaration,&params,&receiver,slots,&outputs);
    if(!node || (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC) ||
        !xl6ForeignInputHasDataSnapshots(node->foreignCall.inputMappingFacts) ||
        (node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD)) || node->foreignCall.receiverDereference ||
        row->parameter<receiver || row->parameter>=64 || row->parameter-receiver>=params->childCount)return 0;
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,node->declarationOwnerType);
    const char* header=owner ? owner->foreignHeader:NULL;
    for(size_t h=0;h<m->foreignFunctionHeaderCount;++h)
        if(m->foreignFunctionHeaders[h].declaration==row->declaration)header=m->foreignFunctionHeaders[h].header;
    if(!header || !xl6ForeignHeaderName(header))return 0;
    const XL6_IrNode* parameter=xl6IrNodeGet(m,m->children[params->firstChild+row->parameter-receiver]);
    const XL6_IrType* type=parameter ? xl6IrTypeGet(&m->types,parameter->storageType):NULL;
    uint64 bit=UINT64_C(1)<<(row->parameter-receiver);
    if(!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        !type->name || strcmp(type->name,"var") || type->paramCount ||
        type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
        (bit&(node->foreignCall.constAggregateAddresses|node->foreignCall.staticCallbackAddresses|
            node->foreignCall.textViews|node->foreignCall.bytesViews|node->foreignCall.terminatedStrings|
            node->foreignCall.physicalPairs|node->foreignCall.retainedCallbacks)) ||
        (node->foreignCall.controlViews&(UINT64_C(1)<<row->parameter)))return 0;
    *result=parameter->storageType;return 1;
}
static int xl6IrCloseDataSnapshots(XL6_IrModule* m)
{
    for(size_t i=0;i<m->foreignDataSnapshotCount;++i)
        if(!xl6IrDataSnapshotExpected(m,&m->foreignDataSnapshots[i],&m->foreignDataSnapshots[i].logicalType))return 0;
    return 1;
}
static int xl6IrVerifyDataSnapshots(const XL6_IrModule* m,XL6_DiagList* diags)
{
    if(m->foreignDataSnapshotCount>m->foreignDataSnapshotCapacity ||
        (m->foreignDataSnapshotCount && !m->foreignDataSnapshots))goto invalid;
    for(size_t i=0;i<m->foreignDataSnapshotCount;++i){
        const XL6_ForeignDataSnapshotContract* row=&m->foreignDataSnapshots[i];XL6_IrTypeId expected=0;
        if(!xl6IrDataSnapshotExpected(m,row,&expected) || row->logicalType!=expected)goto invalid;
        for(size_t j=0;j<i;++j)if(m->foreignDataSnapshots[j].declaration==row->declaration &&
            m->foreignDataSnapshots[j].parameter==row->parameter)goto invalid;
    }
    for(size_t n=0;n<m->nodeCount;++n){
        const XL6_IrNode* node=&m->nodes[n];size_t count=0;
        if(!node->foreignCall.dataSnapshotCount &&
            !xl6ForeignInputHasDataSnapshots(node->foreignCall.inputMappingFacts))continue;
        for(size_t i=0;i<m->foreignDataSnapshotCount;++i)count+=m->foreignDataSnapshots[i].declaration==n+1;
        if(count!=node->foreignCall.dataSnapshotCount ||
            !!count!=xl6ForeignInputHasDataSnapshots(node->foreignCall.inputMappingFacts))goto invalid;
    }
    for(size_t t=0;t<m->callTargetCount;++t){const XL6_IrCallTarget* target=&m->callTargets[t];
        for(size_t p=0;p<target->parameterCount;++p){
            const XL6_ForeignDataSnapshotContract* row=xl6IrForeignDataSnapshotGet(m,target,p);
            if(!row)continue;
            if(target->kind!=XL6_IR_CALL_TARGET_EXTERN_C || target->runtimeBridge ||
                !xl6IrForeignOperationInvokesFunction(target->foreignOperationAbi) ||
                !target->parameterTypes || !target->parameterAbis || !target->parameterPhysicalAbis ||
                !target->parameterPhysicalFieldDeclarations || !target->parameterConstraintAbis ||
                target->parameterTypes[p]!=row->logicalType || target->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalFieldDeclarations[p] || target->parameterConstraintAbis[p] ||
                (target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT &&
                 target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_XVALUE_BORROW))goto invalid;
        }
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native data snapshot rows require exact var borrows, counts, headers and raising admission");
}
