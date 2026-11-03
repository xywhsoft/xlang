/* Included by xl6_ir.c. Closed native SlotMap contracts, never boxed T or
 * reconstructed source syntax. Both lowered validation and Cgen use this plan. */
typedef enum XL6_IrSlotMapRole {
    XL6_SLOT_MAP_NONE,XL6_SLOT_MAP_KEY,XL6_SLOT_MAP_ITEM,XL6_SLOT_MAP_INT,
    XL6_SLOT_MAP_BOOL,XL6_SLOT_MAP_OWNER,XL6_SLOT_MAP_ARRAY_KEY,XL6_SLOT_MAP_ARRAY_ITEM
} XL6_IrSlotMapRole;
typedef struct XL6_IrSlotMapMethod {
    XL6_IrRuntimeResourceOp source; XL6_IrRuntimeOp runtime;
    const char* name; const char* native; size_t count;
    XL6_IrSlotMapRole args[2],result; XL6_IrCallErrorPhysicalAbi failure;
} XL6_IrSlotMapMethod;
static const XL6_IrSlotMapMethod xl6SlotMapMethods[]={
#define XL6_SLOT_MAP_OP(id,name,count,a,b,result,native,failure) \
    {XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_##id,XL6_IR_RUNTIME_SLOT_MAP_##id,name,#native,count, \
    {XL6_SLOT_MAP_##a,XL6_SLOT_MAP_##b},XL6_SLOT_MAP_##result,XL6_IR_CALL_ERROR_PHYSICAL_##failure},
#include "src/abi/xl6_slot_map_ops.inc"
#undef XL6_SLOT_MAP_OP
};
static const XL6_IrSlotMapMethod* xl6IrSlotMapSpec(XL6_IrRuntimeResourceOp op)
{
    for(size_t i=0;i<sizeof(xl6SlotMapMethods)/sizeof(*xl6SlotMapMethods);++i)
        if(xl6SlotMapMethods[i].source==op) return &xl6SlotMapMethods[i];
    return NULL;
}
int xl6IrSlotMapResourceOpIs(XL6_IrRuntimeResourceOp op) { return xl6IrSlotMapSpec(op)!=NULL; }
XL6_IrRuntimeResourceOp xl6IrSlotMapResourceOp(XL6_IrRuntimeOp op)
{
    for(size_t i=0;i<sizeof(xl6SlotMapMethods)/sizeof(*xl6SlotMapMethods);++i)
        if(xl6SlotMapMethods[i].runtime==op) return xl6SlotMapMethods[i].source;
    return XL6_IR_RUNTIME_RESOURCE_OP_INVALID;
}
XL6_IrRuntimeOp xl6IrSlotMapRuntimeOp(XL6_IrRuntimeResourceOp op)
{ const XL6_IrSlotMapMethod* s=xl6IrSlotMapSpec(op); return s ? s->runtime : XL6_IR_RUNTIME_NONE; }
XL6_IrRuntimeResourceOp xl6IrSlotMapMethod(const char* name)
{
    for(size_t i=1;name && i<sizeof(xl6SlotMapMethods)/sizeof(*xl6SlotMapMethods);++i)
        if(strcmp(name,xl6SlotMapMethods[i].name)==0) return xl6SlotMapMethods[i].source;
    return XL6_IR_RUNTIME_RESOURCE_OP_INVALID;
}
const char* xl6IrSlotMapNativeSymbol(XL6_IrRuntimeOp op)
{ const XL6_IrSlotMapMethod* s=xl6IrSlotMapSpec(xl6IrSlotMapResourceOp(op)); return s ? s->native : NULL; }
XL6_IrCallErrorPhysicalAbi xl6IrSlotMapErrorPhysicalAbi(XL6_IrRuntimeOp op)
{ const XL6_IrSlotMapMethod* s=xl6IrSlotMapSpec(xl6IrSlotMapResourceOp(op)); return s ? s->failure : XL6_IR_CALL_ERROR_PHYSICAL_INVALID; }
/* Class field defaults are classified before nominal admission sets exist.
 * Storage shape is already canonical then; executable construction additionally
 * requires the closed admission proof below. Neither stage guesses payload ABI. */
static int xl6IrSlotMapStorageRecipe(const XL6_IrModule* module,XL6_IrTypeId id,XL6_IrValueCollectionRecipe* output)
{
    const XL6_IrType* t=module ? xl6IrTypeGet(&module->types,id) : NULL;
    if(!t || t->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || t->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_SLOT_MAP ||
        t->collectionAbi!=XL6_IR_COLLECTION_NONE || t->paramCount!=1 || !t->params || t->returnCount ||
        t->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || t->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
        t->nullAbi!=XL6_IR_NULL_ABI_XVALUE_SENTINEL || !t->typeValueId ||
        !t->collectionValueSchemaType || t->collectionValueSchemaType!=xl6IrValueCollectionSchemaTypeForType(&module->types,t)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,t->collectionValueSchemaType);
    if(!schema || !schema->typeValueId || schema->typeValueId==t->typeValueId ||
        schema->collectionAbi!=XL6_IR_COLLECTION_ARRAY || schema->paramCount!=1 || !schema->params ||
        schema->params[0]!=t->params[0] || schema->sequenceLayout.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT)
        return 0; /* Closure slot conversion needs its own explicit call plan. */
    XL6_IrSequenceLayout expected=xl6IrSequenceLayoutForType(&module->types,schema);
    if(!xl6IrSequenceLayoutEqual(&schema->sequenceLayout,&expected)) return 0;
    if(output) *output=(XL6_IrValueCollectionRecipe){.ownerType=id,.valueSchemaType=t->collectionValueSchemaType,
        .elementType=t->params[0],.slotAbi=schema->sequenceLayout.slotAbi,
        .codecAbi=schema->sequenceLayout.codecAbi,.transferAbi=schema->sequenceLayout.transferAbi};
    return 1;
}
int xl6IrSlotMapConstructionRecipe(const XL6_IrModule* module,XL6_IrTypeId id,XL6_IrValueCollectionRecipe* output)
{
    XL6_IrValueCollectionRecipe recipe={0};
    if(!xl6IrSlotMapStorageRecipe(module,id,&recipe) ||
        !xl6IrSequenceConstructionRecipe(module,recipe.valueSchemaType,XL6_IR_SEQUENCE_ROLE_ARRAY,
            &recipe.admissionIdentitySet)) return 0;
    if(output) *output=recipe;
    return 1;
}
int xl6IrSlotMapLoopKeyType(const XL6_IrModule* m,XL6_IrTypeId id)
{
    const XL6_IrType* t=m ? xl6IrTypeGet(&m->types,id) : NULL;
    const XL6_IrNativeValuePlan* p=xl6IrNativeValuePlanForType(t);
    return p && p->abi==XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1;
}
int xl6IrSlotMapLoopRecipe(const XL6_IrModule* m,XL6_IrTypeId owner,
    XL6_IrValueCollectionRecipe* value,XL6_IrTypeId* key,XL6_IrTypeId* keyArray)
{
    XL6_IrValueCollectionRecipe recipe={0};
    XL6_IrTypeId schema=XL6_IR_TYPE_INVALID,element=XL6_IR_TYPE_INVALID;
    if(!xl6IrSlotMapConstructionRecipe(m,owner,&recipe)) return 0;
    const XL6_IrType* source=xl6IrTypeGet(&m->types,owner);
    for(size_t i=0;i<m->types.count;++i) {
        const XL6_IrType* t=&m->types.items[i];
        if(t->collectionAbi!=XL6_IR_COLLECTION_ARRAY || t->paramCount!=1 || !t->params ||
            !xl6IrSlotMapLoopKeyType(m,t->params[0])) continue;
        if(schema || !xl6IrSequenceConstructionRecipe(m,(XL6_IrTypeId)(i+1),XL6_IR_SEQUENCE_ROLE_ARRAY,NULL) ||
            t->typeValueId==source->typeValueId || t->sequenceLayout.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT ||
            t->sequenceLayout.transferAbi!=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY) return 0;
        schema=(XL6_IrTypeId)(i+1); element=t->params[0];
    }
    if(!schema) return 0;
    if(value) *value=recipe;
    if(key) *key=element;
    if(keyArray) *keyArray=schema;
    return 1;
}
static int xl6IrSlotMapRoleMatches(const XL6_IrModule* m,XL6_IrSlotMapRole role,XL6_IrTypeId id,
    XL6_IrTypeId owner,XL6_IrTypeId item)
{
    const XL6_IrType* t=xl6IrTypeGet(&m->types,id);
    if(!t) return 0;
    switch(role) {
    case XL6_SLOT_MAP_OWNER: return id==owner;
    case XL6_SLOT_MAP_ITEM: return id==item;
    case XL6_SLOT_MAP_ARRAY_ITEM: {
        XL6_IrValueCollectionRecipe p={0};
        return xl6IrSlotMapConstructionRecipe(m,owner,&p) && id==p.valueSchemaType;
    }
    case XL6_SLOT_MAP_ARRAY_KEY: {
        const XL6_IrType* source=xl6IrTypeGet(&m->types,owner);
        if(!source || !xl6IrSequenceConstructionRecipe(m,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL) ||
            t->typeValueId==source->typeValueId || t->sequenceLayout.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT ||
            t->sequenceLayout.transferAbi!=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY) return 0;
        const XL6_IrNativeValuePlan* p=xl6IrNativeValuePlanForType(xl6IrTypeGet(&m->types,t->sequenceLayout.elementType));
        return p && p->abi==XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1;
    }
    case XL6_SLOT_MAP_KEY: {
        const XL6_IrNativeValuePlan* p=xl6IrNativeValuePlanForType(t);
        return p && p->abi==XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1;
    }
    case XL6_SLOT_MAP_BOOL: return t->kind==XL6_IR_TYPE_BUILTIN && t->cAbi==XL6_IR_C_ABI_BOOL &&
        t->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && t->nullAbi==XL6_IR_NULL_ABI_NONE;
    case XL6_SLOT_MAP_INT: return xl6IrStackBatchInteger(m,id);
    default: return 0;
    }
}
int xl6IrSlotMapSignatureCompatible(const XL6_IrModule* m,XL6_IrRuntimeResourceOp op,
    XL6_IrTypeId receiver,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    const XL6_IrSlotMapMethod* s=xl6IrSlotMapSpec(op);
    int create=op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CREATE;
    XL6_IrTypeId owner=create ? result : receiver; XL6_IrValueCollectionRecipe p={0};
    if(!s || count!=s->count+(create ? 0u : 1u) || (count && !args) ||
        (create && receiver) || (!create && args[0]!=receiver) ||
        !xl6IrSlotMapConstructionRecipe(m,owner,&p) ||
        !xl6IrSlotMapRoleMatches(m,s->result,result,owner,p.elementType)) return 0;
    for(size_t i=0;i<s->count;++i)
        if(!xl6IrSlotMapRoleMatches(m,s->args[i],args[i+1],owner,p.elementType)) return 0;
    return 1;
}
int xl6IrSlotMapIntrinsicCompatible(const XL6_IrModule* m,const XL6_IrIntrinsicCall* call,
    const XL6_IrIntrinsicOperand* args)
{
    XL6_IrTypeId types[3]={0}; XL6_IrValueCollectionRecipe p={0};
    if(!m || !call || call->operandCount>3 || (call->operandCount && !args)) return 0;
    for(size_t i=0;i<call->operandCount;++i) {
        types[i]=args[i].type; const XL6_IrType* t=xl6IrTypeGet(&m->types,types[i]);
        if(!t || args[i].abi!=(t->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED ?
            XL6_IR_INTRINSIC_OPERAND_BORROW : XL6_IR_INTRINSIC_OPERAND_VALUE)) return 0;
    }
    int create=call->runtimeResourceOp==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CREATE;
    if(!xl6IrSlotMapSignatureCompatible(m,call->runtimeResourceOp,call->receiverType,call->resultType,types,call->operandCount) ||
        !xl6IrSlotMapConstructionRecipe(m,create ? call->resultType : call->receiverType,&p)) return 0;
    return call->payloadType==p.elementType && call->runtimeResourcePayloadAbi==p.transferAbi &&
        !call->runtimeResourceKeyType && !call->runtimeResourceKeyPayloadAbi && !call->runtimeResourceKeyOrderAbi &&
        call->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE && call->resultAbi==
            xl6IrRuntimeResourceResultAbiForType(call->runtimeResourceOp,xl6IrTypeGet(&m->types,call->resultType));
}
int xl6IrSlotMapInstructionCompatible(const XL6_IrModule* m,const XL6_IrInstruction* ins)
{
    if(!m || !ins || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !ins->result ||
        ins->operandCount>3 || ins->firstOperand>m->operandCount || ins->operandCount>m->operandCount-ins->firstOperand ||
        ins->immediateText || ins->immediateData || ins->immediateSize || ins->opcode || ins->targetSymbol || ins->targetValue ||
        ins->calleeType || ins->runtimeIdentityType || ins->runtimeResourceKeyType ||
        ins->runtimeResourceKeyPayloadAbi || ins->runtimeResourceKeyOrderAbi) return 0;
    XL6_IrRuntimeResourceOp op=xl6IrSlotMapResourceOp(ins->runtimeOp);
    XL6_IrTypeId args[3]={0}; XL6_IrValueCollectionRecipe p={0};
    for(size_t i=0;i<ins->operandCount;++i) {
        const XL6_IrInstruction* def=xl6IrValueDefinition(m,m->operands[ins->firstOperand+i],NULL);
        if(!def) return 0;
        args[i]=def->type;
    }
    int create=op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CREATE;
    if(!xl6IrSlotMapSignatureCompatible(m,op,create ? 0 : args[0],ins->type,args,ins->operandCount) ||
        !xl6IrSlotMapConstructionRecipe(m,create ? ins->type : args[0],&p)) return 0;
    return ins->payloadType==p.elementType && ins->runtimeResourcePayloadAbi==p.transferAbi &&
        ins->sequenceAdmissionIdentitySet==(create ? p.admissionIdentitySet : 0);
}

/* A cursor's selected lanes are execution semantics, not an emitter guess.
 * The discard contract is frozen in typed IR before any native instructions. */
int xl6IrSlotMapIteratorOpIs(XL6_IrRuntimeOp op)
{ return op>=XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS && op<=XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEY_COPY; }
XL6_IrRuntimeOp xl6IrSlotMapLoopIteratorOp(const XL6_IrNode* loop)
{
    if(!loop || loop->kind!=XL6_IR_NODE_LOOP || loop->sourceKind!=XL6_AST_FOR ||
        (loop->loopAbi!=XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY &&
         loop->loopAbi!=XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY_VALUE)) return XL6_IR_RUNTIME_NONE;
    int key=loop->loopKeyAbi==XL6_IR_LOOP_KEY_NATIVE_VALUE_COPY;
    if(!key && loop->loopKeyAbi!=XL6_IR_LOOP_KEY_DISCARD) return XL6_IR_RUNTIME_NONE;
    int item=loop->loopAbi==XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY_VALUE &&
        loop->loopItemAbi!=XL6_IR_LOOP_ITEM_DISCARD;
    return key ? item ? XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS : XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEYS :
                 item ? XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_VALUES : XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_COUNT;
}
const char* xl6IrSlotMapIteratorNativeSymbol(XL6_IrRuntimeOp op)
{
    switch(op) {
    case XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS: return "xl6SlotMapPairIteratorCreateV1";
    case XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEYS: return "xl6SlotMapKeyIteratorCreateV1";
    case XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_VALUES: return "xl6SlotMapItemIteratorCreateV1";
    case XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_COUNT: return "xl6SlotMapCountIteratorCreateV1";
    case XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEY_COPY: return "xl6ValueIteratorKeyCopyV3";
    default: return NULL;
    }
}
/* Hidden immutable cursor storage preserves the acquisition lane. Merely
 * asserting ValueIterator<SlotMap<T>> cannot forge key-producing capability. */
static const XL6_IrInstruction* xl6IrSlotMapIteratorAcquisition(
    const XL6_IrModule* m,const XL6_IrInstruction* source)
{
    /* Evaluation cleanup can transfer a produced cursor through a pending
     * temporary before publishing the hidden local. Follow only verified,
     * single-initialization storage transfers, with an explicit cycle bound. */
    for(size_t depth=0;source && depth<=m->instructionCount;++depth) {
        if(source->kind==XL6_IR_INSTR_RUNTIME_CALL &&
            source->runtimeOp>=XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS &&
            source->runtimeOp<=XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_COUNT) return source;
        const XL6_IrStorage* storage=xl6IrStorageGet(m,source->targetSymbol);
        if((source->kind!=XL6_IR_INSTR_LOAD && source->kind!=XL6_IR_INSTR_STORAGE_TAKE) || source->operandCount ||
            !storage || (storage->kind!=XL6_IR_STORAGE_LOCAL && storage->kind!=XL6_IR_STORAGE_TEMPORARY) ||
            (source->kind==XL6_IR_INSTR_STORAGE_TAKE && storage->kind!=XL6_IR_STORAGE_TEMPORARY) ||
            storage->type!=source->type || storage->cellType) return NULL;
        const XL6_IrInstruction* result=NULL;
        for(size_t i=0;i<m->instructionCount;++i) {
            const XL6_IrInstruction* write=&m->instructions[i];
            if(write->targetSymbol!=source->targetSymbol || write->kind!=XL6_IR_INSTR_STORE) continue;
            if(result || write->type!=source->type || write->operandCount!=1 || write->firstOperand>=m->operandCount) return NULL;
            result=xl6IrValueDefinition(m,m->operands[write->firstOperand],NULL);
            if(!result || result->type!=source->type) return NULL;
        }
        source=result;
    }
    return NULL;
}
int xl6IrSlotMapIteratorInstructionCompatible(const XL6_IrModule* m,const XL6_IrInstruction* ins)
{
    if(!m || !ins || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrSlotMapIteratorOpIs(ins->runtimeOp) ||
        !ins->result || ins->operandCount!=1 || ins->firstOperand>=m->operandCount || !m->operands ||
        ins->immediateText || ins->immediateData || ins->immediateSize || ins->opcode ||
        ins->targetSymbol || ins->targetValue || ins->calleeType || ins->runtimeIdentityType ||
        ins->collectionAbi || ins->typedCollectionOp || ins->dynamicCollectionOp ||
        ins->sequenceAdmissionIdentitySet || ins->payloadType || ins->runtimeResourcePayloadAbi ||
        ins->runtimeResourceKeyType || ins->runtimeResourceKeyPayloadAbi || ins->runtimeResourceKeyOrderAbi) return 0;
    const XL6_IrInstruction* source=xl6IrValueDefinition(m,m->operands[ins->firstOperand],NULL);
    const XL6_IrType* result=xl6IrTypeGet(&m->types,ins->type);
    const XL6_IrType* input=source ? xl6IrTypeGet(&m->types,source->type) : NULL;
    if(!input || !result) return 0;
    int copy=ins->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEY_COPY;
    const XL6_IrType* cursor=copy ? input : result;
    if(cursor->kind!=XL6_IR_TYPE_VALUE_ITERATOR || cursor->cAbi!=XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR ||
        cursor->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED || cursor->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
        cursor->paramCount!=1 || !cursor->params) return 0;
    XL6_IrValueCollectionRecipe recipe={0}; XL6_IrTypeId key=0,keys=0;
    if(!xl6IrSlotMapLoopRecipe(m,cursor->params[0],&recipe,&key,&keys)) return 0;
    if(copy) {
        const XL6_IrInstruction* create=xl6IrSlotMapIteratorAcquisition(m,source);
        return ins->type==key && !ins->payloadType && !ins->runtimeResourcePayloadAbi &&
            !ins->runtimeResourceKeyType && create &&
            (create->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS ||
             create->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEYS) &&
            xl6IrSlotMapIteratorInstructionCompatible(m,create);
    }
    const XL6_IrNode* loop=xl6IrNodeGet(m,ins->origin);
    if(!loop || loop->childCount!=3 || loop->firstChild>m->childCount ||
        loop->childCount>m->childCount-loop->firstChild) return 0;
    const XL6_IrNode* iterable=xl6IrNodeGet(m,m->children[loop->firstChild+1]);
    return source->type==cursor->params[0] && iterable && iterable->storageType==source->type &&
        loop->loopResourceType==ins->type && xl6IrSlotMapLoopIteratorOp(loop)==ins->runtimeOp;
}
