/* Named C aggregates are values. A nested write evaluates its address keys
 * once, validates every fixed extent, then publishes a rebuilt root value.
 * Reload after key evaluation preserves changes made by reentrant key code;
 * only the selected leaf is replaced. No raw address escapes into XLIR. */
typedef struct XL6_IrLowerAggregatePlaceStep {
    const XL6_IrNode* node;
    XL6_IrValueId key, proof, container;
} XL6_IrLowerAggregatePlaceStep;

static XL6_IrValueId xl6IrLowerAggregatePlaceRead(
    XL6_IrLowerContext* ctx, XL6_IrNodeId origin,
    XL6_IrLowerAggregatePlaceStep* step, XL6_IrValueId container)
{
    XL6_IrValueId value=0;
    const XL6_IrNode* node=step->node;
    if(node->kind==XL6_IR_NODE_MEMBER) {
        if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_AGGREGATE_GET,origin,node->storageType,
                0,&container,1,&value))return 0;
        ctx->module->instructions[ctx->module->instructionCount-1].elementIndex=node->unionField;
    } else {
        XL6_IrValueId operands[]={container,step->key,step->proof};
        if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_C_ARRAY_GET,origin,node->storageType,
                0,operands,3,&value))return 0;
    }
    return value;
}

static int xl6IrLowerAggregatePlaceAssign(XL6_IrLowerContext* ctx,
    XL6_IrNodeId origin,const XL6_IrNode* lhs,XL6_IrValueId replacement)
{
    XL6_IrNodeId rootId=xl6IrAggregatePlaceRootGet(ctx->module,lhs);
    if(!rootId || lhs->kind==XL6_IR_NODE_NAME)return 0;
    const XL6_IrNode* root=xl6IrNodeGet(ctx->module,rootId);
    const XL6_IrNode* node=lhs;
    size_t count=0;
    while(node!=root) {
        ++count;node=xl6IrNodeGet(ctx->module,ctx->module->children[node->firstChild]);
    }
    if(count>SIZE_MAX/sizeof(XL6_IrLowerAggregatePlaceStep))return -1;
    XL6_IrLowerAggregatePlaceStep* steps=xrtCalloc(count,sizeof(*steps));
    if(!steps)return -1;
    int result=-1;bool indexed=false;
    XL6_IrLowerStorageTargetPlan storage;
    if(xl6IrLowerPrepareStorageTargetPlan(ctx,origin,root,&storage)!=1)goto done;
    node=lhs;
    for(size_t i=0;i<count;++i) {
        steps[i].node=node;
        if(node->kind==XL6_IR_NODE_INDEX)indexed=true;
        node=xl6IrNodeGet(ctx->module,ctx->module->children[node->firstChild]);
    }
    XL6_IrValueId original=xl6IrLowerReadStorageTargetPlan(ctx,origin,&storage);
    XL6_IrValueId current=original;
    if(!current)goto done;
    for(size_t i=indexed ? count:0;i>0;--i) {
        XL6_IrLowerAggregatePlaceStep* step=&steps[i-1];
        node=step->node;
        if(node->kind==XL6_IR_NODE_INDEX) {
            XL6_IrNodeId indexId=node==lhs ? origin
                : (XL6_IrNodeId)(node-ctx->module->nodes+1);
            XL6_IrNodeId keyId=ctx->module->children[node->firstChild+1];
            const XL6_IrNode* key=xl6IrNodeGet(ctx->module,keyId);
            XL6_IrTypeId boolean=xl6IrLowerFindTypeByCAbi(ctx->module,XL6_IR_C_ABI_BOOL);
            step->key=xl6IrLowerNativeArrayKey(ctx,indexId,keyId,key);
            XL6_IrValueId operands[]={current,step->key};
            if(!step->key || !boolean ||
               !xl6IrLowerEmit(ctx,XL6_IR_INSTR_C_ARRAY_BOUNDS,indexId,boolean,0,operands,2,&step->proof) ||
               !xl6IrLowerEmitCheckedWrite(ctx,indexId,step->proof,
                    "RangeError","fixed C array index is out of bounds"))goto done;
        }
        if(i>1) {
            current=xl6IrLowerAggregatePlaceRead(ctx,origin,step,current);
            if(!current)goto done;
        }
    }
    current=indexed ? xl6IrLowerReadStorageTargetPlan(ctx,origin,&storage):original;
    if(!current)goto done;
    for(size_t i=count;i>0;--i) {
        XL6_IrLowerAggregatePlaceStep* step=&steps[i-1];
        step->container=current;
        if(i>1) {
            current=xl6IrLowerAggregatePlaceRead(ctx,origin,step,current);
            if(!current)goto done;
        }
    }
    for(size_t i=0;i<count;++i) {
        XL6_IrLowerAggregatePlaceStep* step=&steps[i];
        const XL6_IrTypeId containerType=xl6IrLowerValueType(ctx,step->container);
        XL6_IrValueId updated=0;
        if(step->node->kind==XL6_IR_NODE_MEMBER) {
            XL6_IrValueId operands[]={step->container,replacement};
            if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_AGGREGATE_SET,origin,containerType,
                    0,operands,2,&updated))goto done;
            ctx->module->instructions[ctx->module->instructionCount-1].elementIndex=step->node->unionField;
        } else {
            XL6_IrValueId operands[]={step->container,step->key,replacement,step->proof};
            if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_C_ARRAY_SET,origin,containerType,
                    0,operands,4,&updated))goto done;
        }
        replacement=updated;
    }
    result=xl6IrLowerWriteStorageTargetPlan(ctx,origin,&storage,replacement) ? 1:-1;
done:
    xrtFree(steps);
    if(result<0 && !ctx->failed)xl6IrLowerDiag(ctx,origin,"XLIR C aggregate place has no complete write-back transaction");
    return result;
}
