/* Included after canonical message construction and owning optional results.
 * Closed prerequisite for the public Channel cutover. No AST chase, output
 * variable or payload-codec guess is permitted in this plan. */
static const XL6_IrChannelReceiveSpec xl6ChannelReceiveSpecs[]={
#define XL6_CHANNEL_RECEIVE_OP(Id,Name,Mode,Timed) \
    {XL6_IR_CHANNEL_RECEIVE_##Id,Name,Mode,Timed},
#include "src/abi/xl6_channel_receive_ops.inc"
#undef XL6_CHANNEL_RECEIVE_OP
};

const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveSpec(XL6_IrChannelReceiveKind kind)
{
    for (size_t i=0;i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (xl6ChannelReceiveSpecs[i].kind==kind) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveMethod(const char* name)
{
    for (size_t i=0;name && i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (!strcmp(name,xl6ChannelReceiveSpecs[i].name)) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
int xl6IrChannelReceivePlanBuild(const XL6_IrModule* module,XL6_IrChannelReceiveKind kind,
    XL6_IrTypeId owner,XL6_IrTypeId result,XL6_IrChannelReceivePlan* output)
{
    XL6_IrChannelReceivePlan plan={.spec=xl6IrChannelReceiveSpec(kind),
        .ownerType=owner,.resultType=result,.errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE,
        .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT};
    if (!plan.spec || !xl6IrChannelConstructionRecipe(module,owner,&plan.schemaType,
        &plan.admissionIdentitySet)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,plan.schemaType);
    plan.elementType=schema->sequenceLayout.elementType;
    plan.transferAbi=schema->sequenceLayout.transferAbi;
    plan.slotAbi=schema->sequenceLayout.slotAbi;
    if (!xl6IrOptionalTryResultCompatible(module,result,plan.elementType,XL6_IR_C_ABI_I64)) return 0;
    const XL6_IrType* tuple=xl6IrTypeGet(&module->types,result);
    if (!xl6IrConcurrentQueueIntegerType(module,tuple->params[0])) return 0;
    plan.resultValueType=tuple->params[1];
    const XL6_IrType* value=xl6IrTypeGet(&module->types,plan.resultValueType);
    plan.valueTagged=plan.resultValueType!=plan.elementType && value->cAbi==XL6_IR_C_ABI_TAGGED_NULLABLE;
    if (output) *output=plan;
    return 1;
}
int xl6IrChannelReceiveSignatureCompatible(const XL6_IrModule* module,XL6_IrChannelReceiveKind kind,
    XL6_IrTypeId owner,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    XL6_IrChannelReceivePlan plan={0};
    return xl6IrChannelReceivePlanBuild(module,kind,owner,result,&plan) && args &&
        count==(plan.spec->timed ? 2u : 1u) && args[0]==owner &&
        (!plan.spec->timed || xl6IrConcurrentQueueIntegerType(module,args[1]));
}
