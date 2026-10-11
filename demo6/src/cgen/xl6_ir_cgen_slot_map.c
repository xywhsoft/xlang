/* The IR plan supplies owner/schema/slot/transfer identities. This emitter
 * does no type spelling lookup, boxing or container algorithm generation. */
static int xl6IrCgenEmitSlotMap(XL6_Output* out,const XL6_IrModule* module,const XL6_IrInstruction* ins)
{
    XL6_IrRuntimeResourceOp op=xl6IrSlotMapResourceOp(ins->runtimeOp);
    if(!xl6IrSlotMapResourceOpIs(op)) return 0;
    if(!xl6IrSlotMapInstructionCompatible(module,ins)) return -1;
    int create=op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CREATE;
    XL6_IrValueId first=create ? 0 : module->operands[ins->firstOperand];
    const XL6_IrInstruction* def=create ? NULL : xl6IrCgenValueDefinition(module,first,NULL);
    XL6_IrTypeId id=create ? ins->type : def->type; XL6_IrValueCollectionRecipe recipe={0};
    if(!xl6IrSlotMapConstructionRecipe(module,id,&recipe)) return -1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,id);
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,recipe.valueSchemaType);
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,recipe.elementType);
    const char* slot=xl6IrCgenType(module,recipe.elementType);
    const char* descriptor=xl6IrCgenNativeSlotTypeExpr(recipe.slotAbi);
    const char* transfer=xl6IrCgenSequenceTransferAbiToken(recipe.transferAbi);
    if(!slot || !descriptor || !transfer) return -1;
    xl6OutputPuts("    {\n",out);
    if(create) {
        xl6OutputPuts("      xvalue* slot_map_schema = NULL;\n",out);
        if(xl6IrCgenEmitSequenceCreateDepth(out,module,recipe.valueSchemaType,"slot_map_schema",
            recipe.admissionIdentitySet,0,NULL,0,false,true)<0) return -1;
    }
    xl6OutputPrintf(out,"      const XL6_SlotMapLayoutV1 slot_map_layout = {0x%016llXULL,"
        "{0x%016llXULL,0x%016llXULL,%s,sizeof(%s),%s}};\n",
        (unsigned long long)owner->typeValueId,(unsigned long long)schema->typeValueId,
        (unsigned long long)payload->typeValueId,transfer,slot,descriptor);
    /* Output publication uses the same physical checked ABI for both primitive
     * and owning records; false normal booleans never enter the error lane. */
    if(create) xl6OutputPrintf(out,"      v%u = xl6SlotMapCreateTakeV1(&slot_map_layout,slot_map_schema);\n",(unsigned)ins->result);
    else {
        XL6_IrValueId a=ins->operandCount>1 ? module->operands[ins->firstOperand+1] : 0;
        XL6_IrValueId b=ins->operandCount>2 ? module->operands[ins->firstOperand+2] : 0;
        XL6_IrCallErrorPhysicalAbi physical=xl6IrSlotMapErrorPhysicalAbi(ins->runtimeOp);
        if(physical==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT) {
            xl6OutputPrintf(out,"      v%u = (%s){0};\n",(unsigned)ins->result,xl6IrCgenType(module,ins->type));
            /* The outcome is declared in the surrounding block, so ERROR_TEST
             * can observe it after the temporary layout's lexical scope ends. */
            xl6OutputPrintf(out,"      projection_ok_%u = %s(v%u,&slot_map_layout,",
                (unsigned)ins->result,xl6IrSlotMapNativeSymbol(ins->runtimeOp),(unsigned)first);
            if(op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_COUNT || op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CAPACITY)
                xl6OutputPrintf(out,"%s,",op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CAPACITY ? "true" : "false");
            else if(op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_INSERT) xl6OutputPrintf(out,"&v%u,",(unsigned)a);
            else xl6OutputPrintf(out,"v%u,",(unsigned)a);
            if(op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_SET) xl6OutputPrintf(out,"&v%u,",(unsigned)b);
            if(op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_CONTAINS || op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_REMOVE)
                xl6OutputPrintf(out,"%s,",op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_REMOVE ? "true" : "false");
            xl6OutputPrintf(out,"&v%u);\n",(unsigned)ins->result);
        } else {
            xl6OutputPrintf(out,"      v%u = %s(v%u,&slot_map_layout",(unsigned)ins->result,
                xl6IrSlotMapNativeSymbol(ins->runtimeOp),(unsigned)first);
            if(op==XL6_IR_RUNTIME_RESOURCE_OP_SLOT_MAP_RESERVE) xl6OutputPrintf(out,",v%u",(unsigned)a);
            xl6OutputPuts(");\n",out);
        }
    }
    xl6OutputPuts("    }\n",out);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
