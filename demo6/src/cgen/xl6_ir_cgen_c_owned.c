/* Mechanical emission of verified C owner operations. */
static int xl6IrCgenEmitCOwned(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* ins,XL6_DiagList* diags)
{
    if(!xl6IrCOwnedInstructionCompatible(module,ins))
        return xl6IrCgenDiag(diags,module,ins->origin,"C owner operation has no exact physical ABI");
    unsigned result=(unsigned)ins->result;
    unsigned source=(unsigned)module->operands[ins->firstOperand];
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_SET_COPY) {
        xl6OutputPrintf(out,"    v%u = xl6CStringFromStringV1(xlir_string_scalar_set_copy(xl6CStringViewV1(v%u), v%u, v%u));\n"
            "    bool projection_ok_%u = v%u.Data != NULL;\n",result,source,
            (unsigned)module->operands[ins->firstOperand+1],(unsigned)module->operands[ins->firstOperand+2],result,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SET_COPY) {
        unsigned key=(unsigned)module->operands[ins->firstOperand+1],item=(unsigned)module->operands[ins->firstOperand+2];
        xl6OutputPrintf(out,"    v%u = (XL6_COwnedV1){0};\n    { size_t index;\n"
            "      if (xlir_sequence_index(v%u.Size, v%u, &index)) {\n"
            "        unsigned char* data = (unsigned char*)xrtMalloc(v%u.Size);\n"
            "        if (data) { memcpy(data,v%u.Data,v%u.Size); data[index]=(unsigned char)v%u;\n"
            "          v%u=xl6COwnedTakeV1(data,v%u.Size,xrtFree); }\n      } }\n"
            "    bool projection_ok_%u = v%u.Owner != NULL;\n",result,source,key,source,source,source,item,result,source,result,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_C_OWNED_COPY) {
        xl6OutputPrintf(out,"    v%u = xl6COwnedCopyV1(v%u);\n    bool projection_ok_%u = !v%u.Data || v%u.Data != NULL;\n",result,source,result,source,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_FROM_STRING) {
        xl6OutputPrintf(out,"    v%u = xl6CStringFromStringV1(v%u);\n",result,source);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_FROM_BYTES) {
        xl6OutputPrintf(out,"    v%u = xl6CBytesFromBytesV1(v%u);\n    bool projection_ok_%u = v%u.Owner != NULL;\n",result,source,result,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_VIEW) {
        xl6OutputPrintf(out,"    v%u = xl6CStringViewV1(v%u);\n",result,source);
    } else if(ins->runtimeOp>=XL6_IR_RUNTIME_C_COMPARE_EQ) {
        for(unsigned i=0;i<2;++i) {
            unsigned value=(unsigned)module->operands[ins->firstOperand+i];
            const XL6_IrInstruction* def=xl6IrCgenValueDefinition(module,value,NULL);
            const XL6_IrType* type=xl6IrTypeGet(&module->types,def->type);
            xl6OutputPrintf(out,"    xbytesview c_view_%u_%u;\n",result,i);
            if(type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)
                xl6OutputPrintf(out,"    if (!xrtValueGetBytes(v%u, &c_view_%u_%u)) abort();\n",value,result,i);
            else xl6OutputPrintf(out,"    c_view_%u_%u = (xbytesview){ (const uint8*)v%u.Data, v%u.Size };\n",result,i,value,value);
        }
        const char* relation=ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_EQ ? "==" :
            ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_NE ? "!=" : ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_LT ? "<" :
            ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_LE ? "<=" : ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_GT ? ">" : ">=";
        xl6OutputPrintf(out,"    { xbytesview a=c_view_%u_0, b=c_view_%u_1; size_t n=a.Size<b.Size ? a.Size : b.Size;\n"
            "      int order=n ? memcmp(a.Data,b.Data,n) : 0; if (!order) order=(a.Size>b.Size)-(a.Size<b.Size);\n"
            "      v%u = order %s 0; }\n",result,result,result,relation);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_LENGTH) {
        xl6OutputPrintf(out,"    v%u = (int64_t)xrtUtf8Count(xl6CStringViewV1(v%u));\n",result,source);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_LENGTH) {
        xl6OutputPrintf(out,"    v%u = (int64_t)v%u.Size;\n",result,source);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_AT || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_AT) {
        unsigned key=(unsigned)module->operands[ins->firstOperand+1];
        if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_AT)
            xl6OutputPrintf(out,"    bool projection_ok_%u = xlir_string_scalar_at(xl6CStringViewV1(v%u), v%u, &v%u);\n",result,source,key,result);
        else xl6OutputPrintf(out,"    size_t c_index_%u;\n    bool projection_ok_%u = xlir_sequence_index(v%u.Size, v%u, &c_index_%u);\n"
            "    if (projection_ok_%u) v%u = ((const unsigned char*)v%u.Data)[c_index_%u];\n",
            result,result,source,key,result,result,result,source,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE) {
        xl6OutputPrintf(out,"    v%u = (XL6_COwnedV1){0};\n"
            "    bool projection_ok_%u = xl6CBytesSizeTakeV1(v%u, v%u, &v%u);\n",
            result,result,source,(unsigned)module->operands[ins->firstOperand+1],result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_TAKE) {
        const XL6_IrCallTarget* releaser=xl6IrCallTargetGet(module,ins->targetSymbol);
        xl6OutputPrintf(out,"    extern void %s(void*);\n",releaser->cName);
        if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE)
            xl6OutputPrintf(out,"    v%u = xl6CStringTakeV1(v%u, %s);\n    bool projection_ok_%u = !v%u || v%u.Owner != NULL;\n",result,source,releaser->cName,result,source,result);
        else xl6OutputPrintf(out,"    v%u = xl6CBytesTakeV1(v%u, 0, %s);\n    bool projection_ok_%u = !v%u || v%u.Owner != NULL;\n",result,source,releaser->cName,result,source,result);
    } else {
        const char* function=ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY ? "xl6CStringCopyV1" :
            ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_BOX ? "xl6CStringBoxV1" : "xl6CBytesCopyV1";
        xl6OutputPrintf(out,"    v%u = %s(v%u);\n",result,function,source);
    }
    return !xl6OutputHasFailed(out);
}
