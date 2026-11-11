/* Borrowed physical outputs have explicit logical ownership conversions. */
static int xl6IrForeignOwnershipHelperMatches(const XL6_IrModule*,XL6_SymbolId,XL6_IrTypeId,int);
static int xl6IrOutputViewType(const XL6_IrModule* module,XL6_IrTypeId id,
    XL6_ForeignOutputConversion conversion)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    const XL6_IrType* inner=type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params ?
        xl6IrTypeGet(&module->types,type->params[0]):type;
    if(conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)
        return type && inner && inner->kind==XL6_IR_TYPE_CLASS && inner->foreignHeader && inner->foreignPolicy &&
            inner->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED && !inner->foreignWatch &&
            type->cAbi==inner->cAbi && type->ownershipAbi==inner->ownershipAbi &&
            type->foreignLifetime==inner->foreignLifetime;
    XL6_IrCAbi abi=(conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT || conversion==XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING) ? XL6_IR_C_ABI_MANAGED_STRING:
        conversion==XL6_FOREIGN_OUTPUT_CONVERSION_BYTES ? XL6_IR_C_ABI_MANAGED_BYTES:XL6_IR_C_ABI_UNSUPPORTED;
    return type && inner && abi!=XL6_IR_C_ABI_UNSUPPORTED && inner->kind==XL6_IR_TYPE_BUILTIN &&
        !inner->paramCount && type->cAbi==abi && inner->cAbi==abi &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED && inner->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED;
}
static int xl6IrOutputViewExpected(const XL6_IrModule* module,const XL6_ForeignOutputViewContract* row,
    XL6_ForeignOutputViewContract* expected)
{
    const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
    if(!node || node->kind!=XL6_IR_NODE_FUNC_DECL || !xl6ForeignValueOutputBitsValid(node->foreignCall.valueOutput) ||
        !xl6ForeignValueOutputHasView(node->foreignCall.valueOutput) || row->physicalIndex>=64 ||
        (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC && node->sourceKind!=XL6_AST_GET) ||
        node->firstChild>module->childCount || node->childCount>module->childCount-node->firstChild || node->childCount<3 || !module->children)return 0;
    if(row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE){
        if(!row->retainSymbol || !row->rollbackSymbol ||
            (row->retainSuccess!=XL6_FOREIGN_RETAIN_ZERO && row->retainSuccess!=XL6_FOREIGN_RETAIN_POSITIVE) ||
            !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
            (node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_OWNED_STRING)))return 0;
    }else if(row->retainSymbol || row->rollbackSymbol || row->retainSuccess)return 0;
    const XL6_IrNode* returns=xl6IrNodeGet(module,module->children[node->firstChild+2]);
    if(!returns || returns->kind!=XL6_IR_NODE_LIST || returns->firstChild>module->childCount ||
        returns->childCount>module->childCount-returns->firstChild || returns->childCount>64)return 0;
    size_t ordinal=SIZE_MAX;XL6_ForeignValueOutputKind mode=xl6ForeignValueOutputBase(node->foreignCall.valueOutput);
    if(xl6ForeignValueOutputIsRecord(node->foreignCall.valueOutput)){
        XL6_IrCallTarget target={0};target.declaration=row->declaration;
        const XL6_ForeignOutputRecordContract* record=xl6IrForeignOutputRecordGet(module,&target);
        const XL6_ForeignOutputFieldContract* field=xl6IrForeignOutputFieldGet(module,&target,row->ordinal);
        if(!record || !field || row->physicalIndex!=record->physicalIndex || row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)return 0;
        ordinal=row->ordinal;
    }else if(mode==XL6_FOREIGN_VALUE_OUTPUT_STATUS || mode==XL6_FOREIGN_VALUE_OUTPUT_VOID){
        if(returns->childCount!=1 || row->physicalIndex+1!=node->foreignCall.outputIndexPlusOne)return 0;
        ordinal=0;
    }else if(mode==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || mode==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID){
        if(module->foreignOutputCount>module->foreignOutputCapacity || (module->foreignOutputCount && !module->foreignOutputs))return 0;
        for(size_t i=0;i<module->foreignOutputCount;++i){
            const XL6_ForeignOutputContract* output=&module->foreignOutputs[i];
            if(output->declaration==row->declaration && output->physicalIndex==row->physicalIndex){
                if(ordinal!=SIZE_MAX)return 0;
                ordinal=output->ordinal;
            }
        }
    }else return 0;
    if(ordinal>=returns->childCount)return 0;
    const XL6_IrNode* result=xl6IrNodeGet(module,module->children[returns->firstChild+ordinal]);
    if(!result || !xl6IrOutputViewType(module,result->type,row->conversion))return 0;
    *expected=*row;expected->ordinal=(uint32)ordinal;expected->logicalType=result->type;return 1;
}
static int xl6IrCloseOutputViews(XL6_IrModule* module)
{
    for(size_t i=0;i<module->foreignOutputViewCount;++i){
        XL6_ForeignOutputViewContract expected;
        if(!xl6IrOutputViewExpected(module,&module->foreignOutputViews[i],&expected))return 0;
        module->foreignOutputViews[i]=expected;
    }
    return 1;
}
static int xl6IrVerifyOutputViews(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignOutputViewCount>module->foreignOutputViewCapacity ||
        (module->foreignOutputViewCount && !module->foreignOutputViews) ||
        (module->foreignOutputViewCount && (module->foreignOutputViewCount-1)/64u>=module->nodeCount))goto invalid;
    for(size_t i=0;i<module->foreignOutputViewCount;++i){
        const XL6_ForeignOutputViewContract* row=&module->foreignOutputViews[i];XL6_ForeignOutputViewContract expected;
        if(!xl6IrOutputViewExpected(module,row,&expected) || row->ordinal!=expected.ordinal || row->logicalType!=expected.logicalType)goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignOutputViews[j].declaration==row->declaration &&
            (module->foreignOutputViews[j].ordinal==row->ordinal ||
             (module->foreignOutputViews[j].physicalIndex==row->physicalIndex &&
              !xl6ForeignValueOutputIsRecord(xl6IrNodeGet(module,row->declaration)->foreignCall.valueOutput))))goto invalid;
        for(size_t j=0;j<module->foreignOutputSizeCount;++j)if(module->foreignOutputSizes[j].declaration==row->declaration &&
            module->foreignOutputSizes[j].physicalIndex==row->physicalIndex &&
            !xl6ForeignValueOutputIsRecord(xl6IrNodeGet(module,row->declaration)->foreignCall.valueOutput))goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        const XL6_IrNode* node=&module->nodes[n];if(node->kind!=XL6_IR_NODE_FUNC_DECL)continue;
        size_t count=0;for(size_t i=0;i<module->foreignOutputViewCount;++i)count+=module->foreignOutputViews[i].declaration==n+1;
        if((count!=0)!=xl6ForeignValueOutputHasView(node->foreignCall.valueOutput))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native view outputs require unique physical slots, exact logical owned types and closed conversion presence flags");
}
const XL6_ForeignOutputViewContract* xl6IrForeignOutputViewGet(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    if(!module || !target || module->foreignOutputViewCount>module->foreignOutputViewCapacity ||
        (module->foreignOutputViewCount && !module->foreignOutputViews))return NULL;
    for(size_t i=0;i<module->foreignOutputViewCount;++i)
        if(module->foreignOutputViews[i].declaration==target->declaration && module->foreignOutputViews[i].ordinal==ordinal)
            return &module->foreignOutputViews[i];
    return NULL;
}
int xl6IrForeignOutputViewsMatch(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    if(!node || module->foreignOutputViewCount>module->foreignOutputViewCapacity ||
        (module->foreignOutputViewCount && !module->foreignOutputViews))return 0;
    size_t count=0;
    for(size_t i=0;i<module->foreignOutputViewCount;++i){
        const XL6_ForeignOutputViewContract* row=&module->foreignOutputViews[i];
        if(row->declaration!=target->declaration)continue;
        if(row->logicalType!=xl6IrForeignOutputType(module,target,row->ordinal) ||
            !xl6IrOutputViewType(module,row->logicalType,row->conversion))return 0;
        if(row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE){
            const XL6_IrType* result=xl6IrTypeGet(&module->types,row->logicalType);
            XL6_IrTypeId input=result->kind==XL6_IR_TYPE_NULLABLE ? result->params[0]:row->logicalType;
            if(!xl6IrForeignOwnershipHelperMatches(module,row->retainSymbol,input,1) ||
                !xl6IrForeignOwnershipHelperMatches(module,row->rollbackSymbol,input,0))return 0;
        }
        ++count;
    }
    return (count!=0)==xl6ForeignValueOutputHasView(node->foreignCall.valueOutput);
}
