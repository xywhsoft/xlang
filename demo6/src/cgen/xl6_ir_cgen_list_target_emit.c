/* Closed native List factory. No List algorithm, source/type spelling recovery
 * or wire conversion is emitted: the IR recipe is the complete producer. */
static int xl6IrCgenEmitListTargetCreate(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,const char* result,XL6_IrNominalIdentitySetId admissionSet)
{
    XL6_IrTypeId schemaId=XL6_IR_TYPE_INVALID;
    XL6_IrNominalIdentitySetId expectedSet=XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
    if (!result || !xl6IrListConstructionRecipe(module,id,&schemaId,&expectedSet) ||
        admissionSet!=expectedSet) return -1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,id);
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    const XL6_IrSequenceLayout* plan=&schema->sequenceLayout;
    XL6_IrTypeId element=plan->elementType;
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,element);
    const char* slot=plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 ?
        "xvalue*" : xl6IrCgenType(module,element);
    const char* descriptor=xl6IrCgenNativeSlotTypeExpr(plan->slotAbi);
    const char* transfer=xl6IrCgenSequenceTransferAbiToken(plan->transferAbi);
    const char* converter=plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ?
        "NULL" : xl6IrCgenNativeValueConverterExpr(module,element);
    if (!payload || !slot || !descriptor || !transfer || !converter) return -1;
    /* Whole recipe preflight precedes the first output write. A refused domain
     * cannot leave a partial target/layout or introduce an ANY fallback. */
    xl6OutputPuts("    { xvalue* xlir_list_value_schema = NULL;\n"
        "      xl6_error_scope xlir_list_target_scope = {0};\n"
        "      if (xl6ErrorScopeBegin(&xlir_list_target_scope)) {\n",out);
    if (xl6IrCgenEmitSequenceCreateDepth(out,module,schemaId,"xlir_list_value_schema",
        expectedSet,0,NULL,0,true)<0) return -1;
    xl6OutputPrintf(out,
        "      if (!xlir_list_value_schema) xl6ErrorScopeCaptureFailure(&xlir_list_target_scope);\n"
        "      const XL6_ListLayoutV1 xlir_list_target_layout = {0x%016llXULL,"
        "{0x%016llXULL,0x%016llXULL,%s,sizeof(%s),%s}};\n"
        "      %s = xl6ListCreateTakeV1(&xlir_list_target_layout, xlir_list_value_schema);\n"
        "      %s = xl6ListProjectionTakeV1(%s, &xlir_list_target_layout, %s);\n"
        "      if (!%s || xrtGetError()) xl6ErrorScopeCaptureFailure(&xlir_list_target_scope);\n"
        "      if (xlir_list_target_scope.failed) { xrtValueRelease(%s); %s = NULL; }\n"
        "      xl6_error_result xlir_list_target_outcome = xl6ErrorScopeEnd(&xlir_list_target_scope);\n"
        "      if (!xlir_list_target_outcome.ok) xl6ErrorScopePropagateFailure(xlir_list_target_outcome);\n"
        "      } else { %s = NULL; xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); } }\n",
        (unsigned long long)owner->typeValueId,(unsigned long long)schema->typeValueId,
        (unsigned long long)payload->typeValueId,transfer,slot,descriptor,
        result,result,result,converter,result,result,result,result);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
