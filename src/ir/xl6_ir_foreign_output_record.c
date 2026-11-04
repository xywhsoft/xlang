/* A borrowed native output record is a physical group, independently closed
 * from its logical field values. No language record contains its raw owners. */
static const char* xl6IrPackScalarCName(const XL6_IrType*);
static int xl6IrOutputRecordArraysValid(const XL6_IrModule* m)
{
    return m && m->foreignOutputRecordCount<=m->foreignOutputRecordCapacity &&
        (!m->foreignOutputRecordCount || m->foreignOutputRecords) &&
        m->foreignOutputFieldCount<=m->foreignOutputFieldCapacity &&
        (!m->foreignOutputFieldCount || m->foreignOutputFields) &&
        (!m->foreignOutputRecordCount || m->foreignOutputRecordCount<=m->nodeCount) &&
        (!m->foreignOutputFieldCount || (m->foreignOutputFieldCount-1)/64<m->nodeCount);
}
const XL6_ForeignOutputRecordContract* xl6IrForeignOutputRecordGet(const XL6_IrModule* m,const XL6_IrCallTarget* target)
{
    if(!target || !xl6IrOutputRecordArraysValid(m))return NULL;
    const XL6_ForeignOutputRecordContract* result=NULL;
    for(size_t r=0;r<m->foreignOutputRecordCount;++r)if(m->foreignOutputRecords[r].declaration==target->declaration){
        if(result)return NULL;
        result=&m->foreignOutputRecords[r];}
    return result;
}
const XL6_ForeignOutputFieldContract* xl6IrForeignOutputFieldGet(const XL6_IrModule* m,const XL6_IrCallTarget* target,size_t ordinal)
{
    if(!target || !xl6IrOutputRecordArraysValid(m))return NULL;
    const XL6_ForeignOutputFieldContract* result=NULL;
    for(size_t r=0;r<m->foreignOutputFieldCount;++r)if(m->foreignOutputFields[r].declaration==target->declaration && m->foreignOutputFields[r].ordinal==ordinal){
        if(result)return NULL;
        result=&m->foreignOutputFields[r];}
    return result;
}
static int xl6IrOutputRecordPod(const XL6_IrModule* m,XL6_IrTypeId id,size_t depth)
{
    const XL6_IrType* type=xl6IrTypeGet(&m->types,id);
    if(!type || depth>16)return 0;
    if(xl6IrPackScalarCName(type))return 1;
    if(type->kind!=XL6_IR_TYPE_C_RECORD || type->cAbi!=XL6_IR_C_ABI_C_RECORD ||
        type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL || !type->foreignHeader)return 0;
    const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(m,id);
    if(!layout || layout->firstField>m->aggregateFieldCount || layout->fieldCount>m->aggregateFieldCount-layout->firstField ||
        (layout->fieldCount && !m->aggregateFields))return 0;
    for(size_t f=0;f<layout->fieldCount;++f)
        if(!xl6IrOutputRecordPod(m,m->aggregateFields[layout->firstField+f].type,depth+1))return 0;
    return 1;
}
static const XL6_IrNode* xl6IrOutputRecordReturns(const XL6_IrModule* m,XL6_IrNodeId declaration)
{
    const XL6_IrNode* node=xl6IrNodeGet(m,declaration);
    if(!node || node->kind!=XL6_IR_NODE_FUNC_DECL || !xl6ForeignValueOutputBitsValid(node->foreignCall.valueOutput) ||
        !xl6ForeignValueOutputIsRecord(node->foreignCall.valueOutput) || !node->foreignCall.outputIndexPlusOne ||
        (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC && node->sourceKind!=XL6_AST_GET) ||
        !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
        node->foreignCall.rollbackSymbol || node->foreignCall.borrowedViewResult || xl6ForeignValueOutputHasSize(node->foreignCall.valueOutput) ||
        node->firstChild>m->childCount || node->childCount>m->childCount-node->firstChild || node->childCount<3 || !m->children)return NULL;
    const XL6_IrNode* returns=xl6IrNodeGet(m,m->children[node->firstChild+2]);
    if(!returns || returns->kind!=XL6_IR_NODE_LIST || !returns->childCount || returns->childCount>64 ||
        returns->firstChild>m->childCount || returns->childCount>m->childCount-returns->firstChild)return NULL;
    XL6_IrCallTarget target={0};target.declaration=declaration;target.foreignHeaderIndexPlusOne=xl6IrForeignFunctionHeaderIndex(m,declaration);
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,node->declarationOwnerType);
    if(!xl6IrForeignFunctionHeaderGet(m,&target) && !(owner && owner->foreignHeader))return NULL;
    return returns;
}
static int xl6IrCloseOutputRecords(XL6_IrModule* m)
{
    if(!xl6IrOutputRecordArraysValid(m))return 0;
    for(size_t r=0;r<m->foreignOutputFieldCount;++r){
        XL6_ForeignOutputFieldContract* row=&m->foreignOutputFields[r];
        const XL6_IrNode* returns=xl6IrOutputRecordReturns(m,row->declaration);
        const XL6_IrNode* value=returns && row->ordinal<returns->childCount ? xl6IrNodeGet(m,m->children[returns->firstChild+row->ordinal]):NULL;
        if(!value)return 0;
        row->logicalType=value->type;
    }
    return 1;
}
static int xl6IrVerifyOutputRecords(const XL6_IrModule* m,XL6_DiagList* diags)
{
    if(!xl6IrOutputRecordArraysValid(m))goto invalid;
    for(size_t r=0;r<m->foreignOutputRecordCount;++r){
        const XL6_ForeignOutputRecordContract* row=&m->foreignOutputRecords[r];
        const XL6_IrNode* node=xl6IrNodeGet(m,row->declaration);
        if(!xl6IrOutputRecordReturns(m,row->declaration) || row->physicalIndex>=64 || row->physicalIndex+1!=node->foreignCall.outputIndexPlusOne ||
            !memchr(row->recordCName,0,sizeof(row->recordCName)) || !xl6ForeignIdentifier(row->recordCName))goto invalid;
        const XL6_IrNode* parameters=xl6IrNodeGet(m,m->children[node->firstChild+1]);
        XL6_IrCallTarget target={0};target.declaration=row->declaration;
        size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET) && !(node->flags&XL6_AST_FLAG_STATIC) ? 1u:0u;
        if(!parameters || parameters->kind!=XL6_IR_NODE_LIST)goto invalid;
        target.parameterCount=parameters->childCount+receiver;
        if(xl6IrForeignFirstOutputLogical(m,&target)==SIZE_MAX)goto invalid;
        for(size_t p=0;p<r;++p)if(m->foreignOutputRecords[p].declaration==row->declaration)goto invalid;
    }
    for(size_t r=0;r<m->foreignOutputFieldCount;++r){
        const XL6_ForeignOutputFieldContract* row=&m->foreignOutputFields[r];
        const XL6_IrNode* returns=xl6IrOutputRecordReturns(m,row->declaration);
        const XL6_IrNode* value=returns && row->ordinal<returns->childCount ? xl6IrNodeGet(m,m->children[returns->firstChild+row->ordinal]):NULL;
        if(!value || row->logicalType!=value->type || !memchr(row->field,0,sizeof(row->field)) || !xl6ForeignIdentifier(row->field) || row->field[0]=='_')goto invalid;
        XL6_IrCallTarget target={0};target.declaration=row->declaration;
        if(!xl6IrForeignOutputRecordGet(m,&target))goto invalid;
        const XL6_ForeignOutputViewContract* view=xl6IrForeignOutputViewGet(m,&target,row->ordinal);
        if(view ? view->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE:!xl6IrOutputRecordPod(m,row->logicalType,0))goto invalid;
        for(size_t p=0;p<r;++p)if(m->foreignOutputFields[p].declaration==row->declaration &&
            (m->foreignOutputFields[p].ordinal==row->ordinal || !strcmp(m->foreignOutputFields[p].field,row->field)))goto invalid;
    }
    for(size_t n=0;n<m->nodeCount;++n){
        const XL6_IrNode* node=&m->nodes[n];size_t records=0,fields=0;
        for(size_t r=0;r<m->foreignOutputRecordCount;++r)records+=m->foreignOutputRecords[r].declaration==n+1;
        for(size_t r=0;r<m->foreignOutputFieldCount;++r)fields+=m->foreignOutputFields[r].declaration==n+1;
        if(xl6ForeignValueOutputIsRecord(node->foreignCall.valueOutput)){
            const XL6_IrNode* returns=xl6IrOutputRecordReturns(m,(XL6_IrNodeId)n+1);
            if(records!=1 || !returns || fields!=returns->childCount)goto invalid;
        }else if(records || fields)goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"borrowed output records need one physical group, unique direct fields, exact POD or retained results and closed declaration/type/ownership contracts");
}
int xl6IrForeignOutputRecordsMatch(const XL6_IrModule* m,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(m,target->declaration):NULL;
    if(!node)return 0;
    const XL6_ForeignOutputRecordContract* record=xl6IrForeignOutputRecordGet(m,target);
    if(!xl6ForeignValueOutputIsRecord(node->foreignCall.valueOutput))return !record;
    const XL6_IrNode* returns=xl6IrOutputRecordReturns(m,target->declaration);
    if(!record || !returns)return 0;
    const XL6_IrType* product=xl6IrTypeGet(&m->types,target->returnType);
    if(returns->childCount>1 && (!product || product->kind!=XL6_IR_TYPE_TUPLE || product->paramCount!=returns->childCount || !product->params))return 0;
    for(size_t o=0;o<returns->childCount;++o){
        const XL6_ForeignOutputFieldContract* field=xl6IrForeignOutputFieldGet(m,target,o);
        if(!field || field->logicalType!=(returns->childCount>1 ? product->params[o]:target->returnType))return 0;
    }
    return 1;
}
