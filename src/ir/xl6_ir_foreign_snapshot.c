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
const XL6_ForeignDataSnapshotContract* xl6IrForeignResultDataSnapshotGet(
    const XL6_IrModule* m,const XL6_IrCallTarget* target,size_t ordinal)
{
    if(!m || !target || ordinal>=64 || m->foreignDataSnapshotCount>m->foreignDataSnapshotCapacity ||
        (m->foreignDataSnapshotCount && !m->foreignDataSnapshots))return NULL;
    uint32 encoded=XL6_FOREIGN_DATA_SNAPSHOT_RESULT|(uint32)ordinal;
    for(size_t i=0;i<m->foreignDataSnapshotCount;++i)
        if(m->foreignDataSnapshots[i].declaration==target->declaration && m->foreignDataSnapshots[i].parameter==encoded)
            return &m->foreignDataSnapshots[i];
    return NULL;
}
static int xl6IrResultDataSnapshotExpected(const XL6_IrModule* m,
    const XL6_ForeignDataSnapshotContract* row,XL6_IrTypeId* result)
{
    const XL6_IrNode* node=xl6IrNodeGet(m,row->declaration);
    uint32 ordinal=row->parameter&~XL6_FOREIGN_DATA_SNAPSHOT_RESULT;
    if(!node || node->kind!=XL6_IR_NODE_FUNC_DECL || ordinal>=64 ||
        !xl6ForeignResultHasDataSnapshots(node->foreignCall.inputMappingFacts) ||
        (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC &&
         !(node->sourceKind==XL6_AST_GET && (node->flags&XL6_AST_FLAG_EXTERN))) ||
        (node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD)) || node->foreignCall.receiverDereference ||
        node->firstChild>m->childCount || node->childCount>m->childCount-node->firstChild ||
        node->childCount<3 || !m->children)return 0;
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,node->declarationOwnerType);
    const char* header=owner ? owner->foreignHeader:NULL;
    for(size_t h=0;h<m->foreignFunctionHeaderCount;++h)
        if(m->foreignFunctionHeaders[h].declaration==row->declaration)header=m->foreignFunctionHeaders[h].header;
    if(!header || !xl6ForeignHeaderName(header))return 0;
    const XL6_IrNode* returns=xl6IrNodeGet(m,m->children[node->firstChild+2]);
    if(!returns || returns->kind!=XL6_IR_NODE_LIST || returns->firstChild>m->childCount ||
        returns->childCount>m->childCount-returns->firstChild || ordinal>=returns->childCount || returns->childCount>64)return 0;
    const XL6_IrNode* output=xl6IrNodeGet(m,m->children[returns->firstChild+ordinal]);
    const XL6_IrType* type=output ? xl6IrTypeGet(&m->types,output->type):NULL;
    if(!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->paramCount || !type->name || strcmp(type->name,"var") ||
        type->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 0;
    if(node->foreignCall.valueOutput){
        XL6_IrCallTarget target={0};target.declaration=row->declaration;
        const XL6_ForeignOutputViewContract* view=xl6IrForeignOutputViewGet(m,&target,ordinal);
        if(!view || view->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_OWNED_VALUE || view->logicalType!=output->type)return 0;
    }else if(returns->childCount!=1 || node->foreignCall.borrowedViewResult)return 0;
    *result=output->type;return 1;
}
static int xl6IrDataSnapshotExpected(const XL6_IrModule* m,
    const XL6_ForeignDataSnapshotContract* row,XL6_IrTypeId* result)
{
    if(row->parameter&XL6_FOREIGN_DATA_SNAPSHOT_RESULT)
        return xl6IrResultDataSnapshotExpected(m,row,result);
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
        const XL6_IrNode* node=&m->nodes[n];size_t count=0,inputs=0,results=0;
        if(!node->foreignCall.dataSnapshotCount &&
            !xl6ForeignHasDataSnapshots(node->foreignCall.inputMappingFacts))continue;
        for(size_t i=0;i<m->foreignDataSnapshotCount;++i)if(m->foreignDataSnapshots[i].declaration==n+1){
            ++count;
            if(m->foreignDataSnapshots[i].parameter&XL6_FOREIGN_DATA_SNAPSHOT_RESULT)++results;
            else ++inputs;
        }
        if(count!=node->foreignCall.dataSnapshotCount ||
            !!inputs!=xl6ForeignInputHasDataSnapshots(node->foreignCall.inputMappingFacts) ||
            !!results!=xl6ForeignResultHasDataSnapshots(node->foreignCall.inputMappingFacts))goto invalid;
    }
    for(size_t t=0;t<m->callTargetCount;++t){const XL6_IrCallTarget* target=&m->callTargets[t];
        for(size_t ordinal=0;ordinal<64;++ordinal){
            const XL6_ForeignDataSnapshotContract* row=xl6IrForeignResultDataSnapshotGet(m,target,ordinal);
            if(!row)continue;
            if(target->kind!=XL6_IR_CALL_TARGET_EXTERN_C || target->runtimeBridge ||
                !xl6IrForeignOperationInvokesFunction(target->foreignOperationAbi) || target->nativeResultAbi ||
                target->errorAbi!=XL6_IR_ERROR_ABI_MAY_RAISE ||
                target->errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE ||
                target->resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
                target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_BORROWED_VALUE_COPY ||
                (xl6IrNodeGet(m,target->declaration)->foreignCall.valueOutput ?
                    xl6IrForeignOutputType(m,target,ordinal):(ordinal ? 0:target->returnType))!=row->logicalType)goto invalid;
        }
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
invalid:return xl6IrDiag(diags,NULL,"native data snapshot rows require exact var inputs or owned results, counts, headers and raising admission");
}
