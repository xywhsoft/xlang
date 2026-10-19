/* Included after canonical message-schema construction. A logical callable
 * or union is not its boxed storage slot. All users share this exact proof. */
int xl6IrChannelValuePlanFromSchema(const XL6_IrModule* module,XL6_IrTypeId schemaId,
    XL6_IrChannelValuePlan* output)
{
    XL6_IrChannelValuePlan plan={.schemaType=schemaId};
    if (!xl6IrSequenceConstructionRecipe(module,schemaId,XL6_IR_SEQUENCE_ROLE_ARRAY,
            &plan.admissionIdentitySet)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    const XL6_IrSequenceLayout* layout=&schema->sequenceLayout;
    const XL6_IrType* element=xl6IrTypeGet(&module->types,layout->elementType);
    const XL6_IrType* base=xl6IrTypeGet(&module->types,layout->baseElementType);
    if (!element || !base || layout->transferAbi<XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY ||
        layout->transferAbi>XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY) return 0;
    if (layout->codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT) {
        if (layout->slotAbi!=XL6_IR_NATIVE_SLOT_XVALUE_CARRIER ||
            layout->transferAbi!=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN) return 0;
        if (layout->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1) {
            if (base->kind!=XL6_IR_TYPE_FUNCTION || base->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR ||
                !xl6IrRuntimeCallableAdapterSignatureSupportedDepth(&module->types,layout->baseElementType,1,0)) return 0;
        } else if (layout->codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1) {
            if (base->kind!=XL6_IR_TYPE_UNION || base->cAbi!=XL6_IR_C_ABI_UNION ||
                base->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
                !xl6IrUnionLayoutGet(module,layout->baseElementType)) return 0;
        } else return 0;
    }
    plan.elementType=layout->elementType;plan.baseElementType=layout->baseElementType;
    plan.codecAbi=layout->codecAbi;plan.transferAbi=layout->transferAbi;plan.slotAbi=layout->slotAbi;
    plan.nullable=layout->nullable;plan.logicalTagged=element->nullAbi==XL6_IR_NULL_ABI_TAGGED_VALUE;
    if (output) *output=plan;
    return 1;
}
int xl6IrChannelValuePlanBuild(const XL6_IrModule* module,XL6_IrTypeId owner,
    XL6_IrChannelValuePlan* output)
{
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,owner) : NULL;
    XL6_IrTypeId schema=0;XL6_IrChannelValuePlan plan={0};
    if (!type || type->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_CHANNEL ||
        !xl6IrMessageConstructionRecipeDepth(module,owner,NULL,&schema,NULL,0) ||
        !xl6IrChannelValuePlanFromSchema(module,schema,&plan)) return 0;
    plan.ownerType=owner;
    if (output) *output=plan;
    return 1;
}
