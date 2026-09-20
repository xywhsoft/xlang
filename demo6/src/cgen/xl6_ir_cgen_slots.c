/* Physical writes use one ordered resident transaction. These are renderers
 * of verified places/types/actions, never ownership or AST discovery. */
static void xl6IrCgenSlotPlanBegin(XL6_Output* out)
{ xl6OutputPuts("    { const XL6_SlotWrite xlir_writes[] = {\n", out); }
static void xl6IrCgenSlotWrite(XL6_Output* out, const char* target, const char* input)
{ xl6OutputPrintf(out, "      { &(%s), &(%s), sizeof(%s) },\n", target, input, target); }
static void xl6IrCgenSlotPlanEnd(XL6_Output* out)
{ xl6OutputPuts("    }; (xl6SlotRuntimeV1Get()->Apply)(xlir_writes, sizeof(xlir_writes)/sizeof(*xlir_writes)); }\n", out); }
static void xl6IrCgenSlotReset(XL6_Output* out, const char* type, const char* place, const char* initialized)
{
    xl6OutputPrintf(out, "      { &(%s), &(%s){0}, sizeof(%s) },\n", place, type, place);
    if (initialized != NULL) xl6IrCgenSlotWrite(out, initialized, "(bool){false}");
}
static int xl6IrCgenEmitSlotCommit(XL6_Output* out, const char* type,
    const char* place, const char* input, const char* initialized)
{
    if (!out || !type || !place || !input) return 0;
    /* Evaluate a possibly acquiring expression exactly once, before publishing
     * either the slot or its active bit. Acquisition owns its own failure ABI. */
    xl6OutputPrintf(out, "    { %s xlir_committed = %s;\n", type, input);
    xl6IrCgenSlotPlanBegin(out); xl6IrCgenSlotWrite(out,place,"xlir_committed");
    if (initialized != NULL) xl6IrCgenSlotWrite(out,initialized,"(bool){true}");
    xl6IrCgenSlotPlanEnd(out); xl6OutputPuts("    }\n",out);
    return !xl6OutputHasFailed(out);
}
static char* xl6IrCgenStoragePlace(const XL6_IrModule* module,
    const XL6_IrStorage* storage, size_t frameField)
{
    XL6_Output spelling;
    if (!storage || !xl6OutputOpenMemory(&spelling)) return NULL;
    if (frameField != SIZE_MAX) xl6OutputPrintf(&spelling,"frame->f%lu",(unsigned long)frameField);
    else {
        if (xl6IrCgenParameterAbiIsIndirect(xl6IrCgenStorageParameterAbi(module,storage,NULL)))
            xl6OutputPuts("*",&spelling);
        xl6IrCgenPutStorageName(&spelling,module,storage->symbol);
    }
    return xl6OutputTakeMemoryText(&spelling,NULL);
}
/* STORE/PRODUCT_STORE_MOVE share this spelling in sync, async and generators.
 * Private ordinary locals need no graph synchronization; shared/indirect and
 * frame slots use the same resident payload + activation commit mechanism. */
static int xl6IrCgenEmitStorageCommit(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* instruction,size_t frameField,size_t initBit,XL6_DiagList* diags)
{
    const XL6_IrStorage* storage=xl6IrStorageGet(module,instruction->targetSymbol);
    const char* type=storage?xl6IrCgenType(module,storage->type):NULL;
    char *place,input[32],initialized[64]; int ok;
    if (storage && storage->kind==XL6_IR_STORAGE_CAPTURE)
        return xl6IrCgenDiag(diags,module,instruction->origin,"XLIR mutable closure capture requires an explicit cell ABI");
    if (!storage || !type || instruction->operandCount!=1 || storage->cellType!=XL6_IR_TYPE_INVALID ||
        storage->kind==XL6_IR_STORAGE_CAPTURE ||
        module->abi.slotTransferAbi!=XL6_IR_SLOT_TRANSFER_RESIDENT_ORDERED_WRITES_V1)
        return xl6IrCgenDiag(diags,module,instruction->origin,"XLIR store has no exact physical slot transfer ABI");
    place=xl6IrCgenStoragePlace(module,storage,frameField); if (!place) return 0;
    snprintf(input,sizeof(input),"v%u",(unsigned int)module->operands[instruction->firstOperand]);
    snprintf(initialized,sizeof(initialized),"frame->i%lu",(unsigned long)initBit);
    if (frameField!=SIZE_MAX || storage->kind==XL6_IR_STORAGE_GLOBAL || storage->kind==XL6_IR_STORAGE_EXTERN_GLOBAL ||
        xl6IrCgenParameterAbiIsIndirect(xl6IrCgenStorageParameterAbi(module,storage,NULL)))
        ok=xl6IrCgenEmitSlotCommit(out,type,place,input,initBit==SIZE_MAX?NULL:initialized);
    else { xl6OutputPrintf(out,"    %s = %s;\n",place,input); ok=!xl6OutputHasFailed(out); }
    xrtFree(place); return ok;
}
