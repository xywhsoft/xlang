/* Mechanical emission of verified C owner operations. */
static int xl6IrCgenEmitCOwned(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* ins,XL6_DiagList* diags)
{
    if(!xl6IrCOwnedInstructionCompatible(module,ins))
        return xl6IrCgenDiag(diags,module,ins->origin,"C owner operation has no exact physical ABI");
    unsigned result=(unsigned)ins->result;
    unsigned source=(unsigned)module->operands[ins->firstOperand];
    if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE) {
        xl6OutputPrintf(out,"    v%u = (XL6_COwnedV1){0};\n"
            "    bool projection_ok_%u = xl6CBytesSizeTakeV1(v%u, v%u, &v%u);\n",
            result,result,source,(unsigned)module->operands[ins->firstOperand+1],result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_TAKE) {
        const XL6_IrCallTarget* releaser=xl6IrCallTargetGet(module,ins->targetSymbol);
        xl6OutputPrintf(out,"    extern void %s(void*);\n",releaser->cName);
        if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE)
            xl6OutputPrintf(out,"    v%u = xl6CStringTakeV1(v%u, %s);\n",result,source,releaser->cName);
        else xl6OutputPrintf(out,"    v%u = xl6CBytesTakeV1(v%u, 0, %s);\n",result,source,releaser->cName);
    } else {
        const char* function=ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY ? "xl6CStringCopyV1" :
            ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_BOX ? "xl6CStringBoxV1" : "xl6CBytesCopyV1";
        xl6OutputPrintf(out,"    v%u = %s(v%u);\n",result,function,source);
    }
    return !xl6OutputHasFailed(out);
}
