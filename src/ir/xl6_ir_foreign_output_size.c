/* Sparse, closed sizeof initializers for trivial native record outputs. */
static int xl6IrOutputSizeUnsigned(const XL6_IrType* type)
{
    return type && type->kind==XL6_IR_TYPE_BUILTIN && type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        (type->cAbi==XL6_IR_C_ABI_C_UCHAR || type->cAbi==XL6_IR_C_ABI_C_USHORT || type->cAbi==XL6_IR_C_ABI_C_UINT ||
         type->cAbi==XL6_IR_C_ABI_C_ULONG || type->cAbi==XL6_IR_C_ABI_C_ULONGLONG ||
         type->cAbi==XL6_IR_C_ABI_C_SIZE || type->cAbi==XL6_IR_C_ABI_C_UINTPTR);
}
static int xl6IrOutputSizeExpected(const XL6_IrModule* module,const XL6_ForeignOutputSizeContract* row,
    XL6_ForeignOutputSizeContract* expected)
{
    const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
    if(!node || node->kind!=XL6_IR_NODE_FUNC_DECL || !xl6ForeignValueOutputBitsValid(node->foreignCall.valueOutput) ||
        !xl6ForeignValueOutputHasSize(node->foreignCall.valueOutput) || row->physicalIndex>=64 ||
        !memchr(row->field,0,sizeof(row->field)) || !xl6ForeignIdentifier(row->field) ||
        (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC && node->sourceKind!=XL6_AST_GET) ||
        node->firstChild>module->childCount || node->childCount>module->childCount-node->firstChild || node->childCount<3 ||
        !module->children)return 0;
    const XL6_IrNode* returns=xl6IrNodeGet(module,module->children[node->firstChild+2]);
    if(!returns || returns->kind!=XL6_IR_NODE_LIST || returns->firstChild>module->childCount ||
        returns->childCount>module->childCount-returns->firstChild || returns->childCount>64)return 0;
    size_t ordinal=SIZE_MAX;uint8 mode=(uint8)xl6ForeignValueOutputBase(node->foreignCall.valueOutput);
    if(mode==XL6_FOREIGN_VALUE_OUTPUT_STATUS || mode==XL6_FOREIGN_VALUE_OUTPUT_VOID){
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
    const XL6_IrType* record=result ? xl6IrTypeGet(&module->types,result->type):NULL;
    if(!record || record->kind!=XL6_IR_TYPE_C_RECORD || record->cAbi!=XL6_IR_C_ABI_C_RECORD ||
        record->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL || !record->foreignHeader)return 0;
    const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(module,result->type);
    if(!layout || layout->firstField>module->aggregateFieldCount ||
        layout->fieldCount>module->aggregateFieldCount-layout->firstField || !module->aggregateFields)return 0;
    const XL6_IrAggregateField* field=NULL;
    for(size_t f=0;f<layout->fieldCount;++f){
        const XL6_IrAggregateField* item=&module->aggregateFields[layout->firstField+f];
        if(memchr(item->cName,0,sizeof(item->cName)) && !strcmp(item->cName,row->field)){
            if(field)return 0;
            field=item;
        }
    }
    if(!field || field->ownerType!=result->type || !xl6IrOutputSizeUnsigned(xl6IrTypeGet(&module->types,field->type)))return 0;
    *expected=*row;expected->ordinal=(uint32)ordinal;expected->recordType=result->type;
    expected->fieldDeclaration=field->declaration;expected->fieldType=field->type;return 1;
}
static int xl6IrCloseOutputSizes(XL6_IrModule* module)
{
    for(size_t i=0;i<module->foreignOutputSizeCount;++i){
        XL6_ForeignOutputSizeContract expected;
        if(!xl6IrOutputSizeExpected(module,&module->foreignOutputSizes[i],&expected))return 0;
        module->foreignOutputSizes[i]=expected;
    }
    return 1;
}
static int xl6IrVerifyOutputSizes(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignOutputSizeCount>module->foreignOutputSizeCapacity ||
        (module->foreignOutputSizeCount && !module->foreignOutputSizes) ||
        (module->foreignOutputSizeCount && (module->foreignOutputSizeCount-1)/64u>=module->nodeCount))goto invalid;
    for(size_t i=0;i<module->foreignOutputSizeCount;++i){
        const XL6_ForeignOutputSizeContract* row=&module->foreignOutputSizes[i];XL6_ForeignOutputSizeContract expected;
        if(!xl6IrOutputSizeExpected(module,row,&expected) || row->ordinal!=expected.ordinal || row->recordType!=expected.recordType ||
            row->fieldDeclaration!=expected.fieldDeclaration || row->fieldType!=expected.fieldType)goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignOutputSizes[j].declaration==row->declaration &&
            module->foreignOutputSizes[j].physicalIndex==row->physicalIndex)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        const XL6_IrNode* node=&module->nodes[n];
        if(!xl6ForeignValueOutputBitsValid(node->foreignCall.valueOutput))goto invalid;
        if(node->kind!=XL6_IR_NODE_FUNC_DECL)continue;
        size_t count=0;for(size_t i=0;i<module->foreignOutputSizeCount;++i)count+=module->foreignOutputSizes[i].declaration==n+1;
        if((count!=0)!=xl6ForeignValueOutputHasSize(node->foreignCall.valueOutput))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native sizeof output initializers require unique exact unsigned record members and closed output presence flags");
}
const XL6_ForeignOutputSizeContract* xl6IrForeignOutputSizeGet(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    if(!module || !target || module->foreignOutputSizeCount>module->foreignOutputSizeCapacity ||
        (module->foreignOutputSizeCount && !module->foreignOutputSizes))return NULL;
    for(size_t i=0;i<module->foreignOutputSizeCount;++i)
        if(module->foreignOutputSizes[i].declaration==target->declaration && module->foreignOutputSizes[i].ordinal==ordinal)
            return &module->foreignOutputSizes[i];
    return NULL;
}
int xl6IrForeignOutputSizesMatch(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    if(!node || !xl6ForeignValueOutputBitsValid(node->foreignCall.valueOutput))return 0;
    size_t count=0;
    if(module->foreignOutputSizeCount>module->foreignOutputSizeCapacity ||
        (module->foreignOutputSizeCount && !module->foreignOutputSizes))return 0;
    for(size_t i=0;i<module->foreignOutputSizeCount;++i){
        const XL6_ForeignOutputSizeContract* row=&module->foreignOutputSizes[i];
        if(row->declaration!=target->declaration)continue;
        if(row->recordType!=xl6IrForeignOutputType(module,target,row->ordinal))return 0;
        ++count;
    }
    return (count!=0)==xl6ForeignValueOutputHasSize(node->foreignCall.valueOutput);
}
