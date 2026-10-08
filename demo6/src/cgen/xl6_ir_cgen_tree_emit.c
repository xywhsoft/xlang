/* Included after the Sequence factory renderer. Tree semantics, storage,
 * output atomicity and graph participation live in the resident provider. */
static int xl6IrCgenEmitTreeLayout(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId ownerId,const char* name)
{
    XL6_IrTypeId keyId=0,valueId=0;
    if (!xl6IrAvlConstructionRecipe(module,ownerId,&keyId,&valueId,NULL,NULL)) return -1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    const XL6_IrType* key=xl6IrTypeGet(&module->types,keyId);
    const XL6_IrType* value=xl6IrTypeGet(&module->types,valueId);
    const XL6_IrSequenceLayout* k=&key->sequenceLayout;
    const XL6_IrSequenceLayout* v=&value->sequenceLayout;
    xl6OutputPrintf(out,
        "      const XL6_TreeLayoutV1 %s = {0x%016llXULL, %u,\n"
        "      {0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s},\n"
        "      {0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s}};\n",
        name,(unsigned long long)owner->typeValueId,(unsigned)xl6IrAvlKeyOrderAbi(module,ownerId),
        (unsigned long long)key->typeValueId,
        (unsigned long long)xl6IrTypeGet(&module->types,k->elementType)->typeValueId,
        xl6IrCgenSequenceTransferAbiToken(k->transferAbi),xl6IrCgenType(module,k->elementType),
        xl6IrCgenNativeSlotTypeExpr(k->slotAbi),
        (unsigned long long)value->typeValueId,
        (unsigned long long)xl6IrTypeGet(&module->types,v->elementType)->typeValueId,
        xl6IrCgenSequenceTransferAbiToken(v->transferAbi),xl6IrCgenType(module,v->elementType),
        xl6IrCgenNativeSlotTypeExpr(v->slotAbi));
    return xl6OutputHasFailed(out) ? -1 : 1;
}
static int xl6IrCgenEmitTreeReferenceCreateDepth(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId sequenceId,const char* result,size_t depth)
{
    const XL6_IrType* sequence=xl6IrTypeGet(&module->types,sequenceId);
    if (depth>64 || !sequence || sequence->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1) return -1;
    const XL6_IrSequenceLayout* plan=&sequence->sequenceLayout;
    XL6_IrTypeId keyId=0,valueId=0; XL6_IrNominalIdentitySetId keySet=0,valueSet=0;
    if (!xl6IrAvlConstructionRecipe(module,plan->baseElementType,&keyId,&valueId,&keySet,&valueSet)) return -1;
    char keys[64],values[64],layout[64],scope[64],outcome[64];
    /* A nested caller can itself be preparing a Tree domain. Its result
     * variable must never be shadowed by the child's factory temporaries. */
    snprintf(keys,sizeof(keys),"xlir_tree_schema_keys_%u",(unsigned)depth);
    snprintf(values,sizeof(values),"xlir_tree_schema_values_%u",(unsigned)depth);
    snprintf(layout,sizeof(layout),"xlir_tree_schema_layout_%u",(unsigned)depth);
    snprintf(scope,sizeof(scope),"xlir_tree_schema_scope_%u",(unsigned)depth);
    snprintf(outcome,sizeof(outcome),"xlir_tree_schema_outcome_%u",(unsigned)depth);
    xl6OutputPrintf(out,"    { xvalue* %s = NULL; xvalue* %s = NULL;\n",keys,values);
    if (xl6IrCgenEmitTreeLayout(out,module,plan->baseElementType,layout)<0) return -1;
    xl6OutputPrintf(out,"      xl6_error_scope %s = {0};\n"
        "      if (!xl6ErrorScopeBegin(&%s)) abort();\n",scope,scope);
    if (xl6IrCgenEmitSequenceCreateDepth(out,module,keyId,keys,keySet,depth+1,NULL,0,false)<0) return -1;
    xl6OutputPrintf(out,"      if (!%s) xl6ErrorScopeCaptureFailure(&%s);\n",keys,scope);
    if (xl6IrCgenEmitSequenceCreateDepth(out,module,valueId,values,valueSet,depth+1,NULL,0,false)<0) return -1;
    xl6OutputPrintf(out,
        "      if (!%s) xl6ErrorScopeCaptureFailure(&%s);\n"
        "      %s = xl6SequenceTreeCreateTakeV1(%u, 0x%016llXULL, 0x%016llXULL, &%s, %s, %s, %s);\n"
        "      if (!%s) xl6ErrorScopeCaptureFailure(&%s);\n"
        "      xl6_error_result %s = xl6ErrorScopeEnd(&%s);\n"
        "      if (!%s.ok) xl6ErrorScopePropagateFailure(%s); }\n",values,scope,
        result,(unsigned)plan->role,(unsigned long long)sequence->typeValueId,
        (unsigned long long)xl6IrTypeGet(&module->types,plan->elementType)->typeValueId,
        layout,keys,values,plan->nullable ? "true" : "false",result,scope,outcome,scope,outcome,outcome);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
static const char* xl6IrCgenTreeNativeMethod(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_AVL_TREE_CREATE: return "xl6TreeCreateTakeV1";
    case XL6_IR_RUNTIME_AVL_TREE_SET: return "xl6TreeSetV1";
    case XL6_IR_RUNTIME_AVL_TREE_GET_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_TAKE_INTO: return "xl6TreeGetV1";
    case XL6_IR_RUNTIME_AVL_TREE_CONTAINS:
    case XL6_IR_RUNTIME_AVL_TREE_REMOVE: return "xl6TreeKeyV1";
    case XL6_IR_RUNTIME_AVL_TREE_FIRST_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_LAST_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_LOWER_BOUND_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_UPPER_BOUND_INTO: return "xl6TreeEndpointV1";
    case XL6_IR_RUNTIME_AVL_TREE_EQUALS: return "xl6TreeEqualsV1";
    case XL6_IR_RUNTIME_AVL_TREE_MERGE: return "xl6TreeMergeV1";
    case XL6_IR_RUNTIME_AVL_TREE_CLEAR: return "xl6TreeClearV1";
    case XL6_IR_RUNTIME_AVL_TREE_COUNT: return "xl6TreeCountV1";
    case XL6_IR_RUNTIME_AVL_TREE_TRIM:
    case XL6_IR_RUNTIME_AVL_TREE_TRIM_TO: return "xl6TreeTrimV1";
    case XL6_IR_RUNTIME_AVL_TREE_CLONE: return "xl6TreeCloneV1";
    case XL6_IR_RUNTIME_AVL_TREE_KEYS:
    case XL6_IR_RUNTIME_AVL_TREE_VALUES: return "xl6TreeProjectTakeV1";
    default: return NULL;
    }
}
static int xl6IrCgenEmitAvlRuntime(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (!instruction || !xl6IrCgenAvlRuntimeOpIs(instruction->runtimeOp)) return 0;
    if (!xl6IrCgenRuntimeAvlAbi(module,instruction)) return -1;
    bool creates=instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_CREATE;
    const XL6_IrInstruction* receiver=creates ? NULL :
        xl6IrCgenValueDefinition(module,module->operands[instruction->firstOperand],NULL);
    XL6_IrTypeId ownerId=creates ? instruction->type : receiver->type;
    XL6_IrTypeId keyId=0,valueId=0;
    XL6_IrNominalIdentitySetId keySet=0,valueSet=0;
    if (!xl6IrAvlConstructionRecipe(module,ownerId,&keyId,&valueId,&keySet,&valueSet) ||
        (creates && instruction->sequenceAdmissionIdentitySet!=valueSet)) return -1;
    const XL6_IrType* key=xl6IrTypeGet(&module->types,keyId);
    const XL6_IrType* value=xl6IrTypeGet(&module->types,valueId);
    const XL6_IrSequenceLayout* k=&key->sequenceLayout;
    const XL6_IrSequenceLayout* v=&value->sequenceLayout;
    const char* method=xl6IrCgenTreeNativeMethod(instruction->runtimeOp);
    if (!method) return -1;
    unsigned output=(unsigned)instruction->result;
    unsigned args[4]={0};
    for (size_t i=0;i<instruction->operandCount;++i)
        args[i]=(unsigned)module->operands[instruction->firstOperand+i];
    xl6OutputPrintf(out,"    v%u = (%s){0};\n    {\n",output,xl6IrCgenType(module,instruction->type));
    if (xl6IrCgenEmitTreeLayout(out,module,ownerId,"tree_layout")<0) return -1;
    switch (instruction->runtimeOp) {
    case XL6_IR_RUNTIME_AVL_TREE_CREATE:
        xl6OutputPuts("      xvalue* tree_keys = NULL; xvalue* tree_values = NULL;\n"
            "      xl6_error_scope tree_scope = {0};\n"
            "      if (!xl6ErrorScopeBegin(&tree_scope)) abort();\n",out);
        if (xl6IrCgenEmitSequenceCreateDepth(out,module,keyId,"tree_keys",keySet,1,NULL,0,false)<0) return -1;
        xl6OutputPuts("      if (!tree_keys) xl6ErrorScopeCaptureFailure(&tree_scope);\n",out);
        if (xl6IrCgenEmitSequenceCreateDepth(out,module,valueId,"tree_values",valueSet,1,NULL,0,false)<0) return -1;
        xl6OutputPrintf(out,"      if (!tree_values) xl6ErrorScopeCaptureFailure(&tree_scope);\n"
            "      v%u = %s(&tree_layout, tree_keys, tree_values);\n"
            "      if (!v%u) xl6ErrorScopeCaptureFailure(&tree_scope);\n"
            "      xl6_error_result tree_outcome = xl6ErrorScopeEnd(&tree_scope);\n"
            "      if (!tree_outcome.ok) xl6ErrorScopePropagateFailure(tree_outcome);\n",
            output,method,output);
        break;
    case XL6_IR_RUNTIME_AVL_TREE_SET:
        xl6OutputPrintf(out,"      v%u = %s(v%u, &tree_layout, &v%u, &v%u);\n",
            output,method,args[0],args[1],args[2]); break;
    case XL6_IR_RUNTIME_AVL_TREE_GET_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_TAKE_INTO:
        xl6OutputPrintf(out,"      (void)%s(v%u, &tree_layout, &v%u, v%u, %s, &v%u);\n",
            method,args[0],args[1],args[2],instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_TAKE_INTO ? "true" : "false",output);
        break;
    case XL6_IR_RUNTIME_AVL_TREE_CONTAINS:
    case XL6_IR_RUNTIME_AVL_TREE_REMOVE:
        xl6OutputPrintf(out,"      (void)%s(v%u, &tree_layout, &v%u, %s, &v%u);\n",
            method,args[0],args[1],instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_REMOVE ? "true" : "false",output);
        break;
    case XL6_IR_RUNTIME_AVL_TREE_FIRST_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_LAST_INTO:
        xl6OutputPrintf(out,"      (void)%s(v%u, &tree_layout, %u, NULL, v%u, v%u, &v%u);\n",
            method,args[0],instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_LAST_INTO ? 1u : 0u,args[1],args[2],output);
        break;
    case XL6_IR_RUNTIME_AVL_TREE_LOWER_BOUND_INTO:
    case XL6_IR_RUNTIME_AVL_TREE_UPPER_BOUND_INTO:
        xl6OutputPrintf(out,"      (void)%s(v%u, &tree_layout, %u, &v%u, v%u, v%u, &v%u);\n",
            method,args[0],instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_UPPER_BOUND_INTO ? 3u : 2u,args[1],args[2],args[3],output);
        break;
    case XL6_IR_RUNTIME_AVL_TREE_EQUALS:
        xl6OutputPrintf(out,"      (void)%s(v%u, v%u, &tree_layout, &v%u);\n",method,args[0],args[1],output);
        break;
    case XL6_IR_RUNTIME_AVL_TREE_MERGE:
        xl6OutputPrintf(out,"      v%u = %s(v%u, v%u, &tree_layout, v%u);\n",output,method,args[0],args[1],args[2]); break;
    case XL6_IR_RUNTIME_AVL_TREE_CLEAR:
    case XL6_IR_RUNTIME_AVL_TREE_COUNT:
    case XL6_IR_RUNTIME_AVL_TREE_CLONE:
        xl6OutputPrintf(out,"      v%u = %s(v%u, &tree_layout);\n",output,method,args[0]); break;
    case XL6_IR_RUNTIME_AVL_TREE_TRIM:
        xl6OutputPrintf(out,"      v%u = %s(v%u, &tree_layout, 0);\n",output,method,args[0]); break;
    case XL6_IR_RUNTIME_AVL_TREE_TRIM_TO:
        xl6OutputPrintf(out,"      v%u = %s(v%u, &tree_layout, v%u);\n",output,method,args[0],args[1]); break;
    case XL6_IR_RUNTIME_AVL_TREE_KEYS:
    case XL6_IR_RUNTIME_AVL_TREE_VALUES: {
        bool keys=instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_KEYS;
        XL6_IrTypeId arrayId=keys ? keyId : valueId;
        const XL6_IrSequenceLayout* plan=keys ? k : v;
        if (instruction->type!=arrayId || instruction->sequenceAdmissionIdentitySet!=(keys ? keySet : valueSet)) return -1;
        xl6OutputPuts("      xvalue* tree_array = NULL;\n",out);
        if (xl6IrCgenEmitSequenceCreateDepth(out,module,arrayId,"tree_array",keys ? keySet : valueSet,0,NULL,0,false)<0) return -1;
        xl6OutputPrintf(out,"      v%u = %s(v%u, &tree_layout, %s, tree_array, %s);\n",
            output,method,args[0],keys ? "true" : "false",
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ?
                "xl6ArrayReferenceConverterV1()" : xl6IrCgenNativeValueConverterExpr(module,plan->elementType));
        break;
    }
    default: return -1;
    }
    xl6OutputPuts("    }\n",out);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
