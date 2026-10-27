/* One complete value-domain recipe drives distinct native List/Set/Dict factories.
 * No container algorithm, type spelling recovery or wire conversion is emitted. */
static int xl6IrCgenEmitValueCollectionTargetCreateDepth(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,XL6_IrCollectionAbi family,const char* result,XL6_IrNominalIdentitySetId admissionSet,size_t depth,
    const char* importChain,size_t importCount)
{
    XL6_IrValueCollectionRecipe recipe={0};
    if (depth>64 || !result || !xl6IrValueCollectionConstructionRecipe(module,id,&recipe) ||
        recipe.family!=family || admissionSet!=recipe.admissionIdentitySet) return -1;
    XL6_IrTypeId schemaId=recipe.valueSchemaType;
    XL6_IrNominalIdentitySetId expectedSet=recipe.admissionIdentitySet;
    const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(family);
    if (!target) return -1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,id);
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    const XL6_IrSequenceLayout* plan=&schema->sequenceLayout;
    XL6_IrTypeId element=plan->elementType;
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,element);
    const char* slot=plan->codecAbi==XL6_IR_SEQUENCE_CODEC_ERROR_VALUE_V1 ||
        plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 ||
        plan->codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1 ?
        "xvalue*" : xl6IrCgenType(module,element);
    const char* descriptor=xl6IrCgenNativeSlotTypeExpr(plan->slotAbi);
    const char* transfer=xl6IrCgenSequenceTransferAbiToken(plan->transferAbi);
    const char* converter=plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ?
        "NULL" : xl6IrCgenNativeValueConverterExpr(module,element);
    if (!payload || !slot || !descriptor || !transfer || !converter) return -1;
    /* Whole recipe preflight precedes the first output write. A refused domain
     * cannot leave a partial target/layout or introduce an ANY fallback. */
    xl6OutputPuts("    { xvalue* xlir_value_collection_value_schema = NULL;\n"
        "      xl6_error_scope xlir_value_collection_target_scope = {0};\n"
        "      if (xl6ErrorScopeBegin(&xlir_value_collection_target_scope)) {\n",out);
    if (xl6IrCgenEmitSequenceCreateDepth(out,module,schemaId,"xlir_value_collection_value_schema",
        expectedSet,depth,importChain,importCount,true,false)<0) return -1;
    xl6OutputPrintf(out,
        "      if (!xlir_value_collection_value_schema) xl6ErrorScopeCaptureFailure(&xlir_value_collection_target_scope);\n"
        "      const %s xlir_value_collection_target_layout = {0x%016llXULL,"
        "{0x%016llXULL,0x%016llXULL,%s,sizeof(%s),%s}};\n"
        "      %s = %s(&xlir_value_collection_target_layout, xlir_value_collection_value_schema);\n"
        "      if (%s) %s = %s(%s, &xlir_value_collection_target_layout, %s);\n"
        "      if (!%s || xrtGetError()) xl6ErrorScopeCaptureFailure(&xlir_value_collection_target_scope);\n"
        "      if (xlir_value_collection_target_scope.failed) { xrtValueRelease(%s); %s = NULL; }\n"
        "      xl6_error_result xlir_value_collection_target_outcome = xl6ErrorScopeEnd(&xlir_value_collection_target_scope);\n"
        "      if (!xlir_value_collection_target_outcome.ok) xrtSetErrorTake(xlir_value_collection_target_outcome.error);\n"
        "      } else { %s = NULL; } }\n",
        target->layout,(unsigned long long)owner->typeValueId,(unsigned long long)schema->typeValueId,
        (unsigned long long)payload->typeValueId,transfer,slot,descriptor,
        result,target->create,result,result,target->project,result,converter,result,result,result,result);
    /* The virtual Array witness is not a collection-chain node. Only the
     * actual projected owner is retained as this depth's transaction seed. */
    if (importChain && depth<importCount)
        xl6OutputPrintf(out,"    %s[%u] = %s ? xrtValueRetain(%s) : NULL;\n",
            importChain,(unsigned)depth,result,result);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
static int xl6IrCgenEmitListTargetCreateDepth(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,const char* result,XL6_IrNominalIdentitySetId admissionSet,size_t depth,
    const char* importChain,size_t importCount)
{ return xl6IrCgenEmitValueCollectionTargetCreateDepth(out,module,id,XL6_IR_COLLECTION_LIST,
    result,admissionSet,depth,importChain,importCount); }
static int xl6IrCgenEmitListTargetCreate(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,const char* result,XL6_IrNominalIdentitySetId admissionSet)
{ return xl6IrCgenEmitListTargetCreateDepth(out,module,id,result,admissionSet,0,NULL,0); }
static int xl6IrCgenEmitSetTargetCreate(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,const char* result,XL6_IrNominalIdentitySetId admissionSet)
{ return xl6IrCgenEmitValueCollectionTargetCreateDepth(out,module,id,XL6_IR_COLLECTION_SET,
    result,admissionSet,0,NULL,0); }
static int xl6IrCgenEmitDictTargetCreate(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,const char* result,XL6_IrNominalIdentitySetId admissionSet)
{ return xl6IrCgenEmitValueCollectionTargetCreateDepth(out,module,id,XL6_IR_COLLECTION_DICT,
    result,admissionSet,0,NULL,0); }
