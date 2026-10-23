/* Mechanical emission of verified C owner operations. */
static int xl6IrCgenEmitCOwned(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* ins,XL6_DiagList* diags)
{
    if(!xl6IrCOwnedInstructionCompatible(module,ins))
        return xl6IrCgenDiag(diags,module,ins->origin,"C owner operation has no exact physical ABI");
    unsigned result=(unsigned)ins->result;
    unsigned source=(unsigned)module->operands[ins->firstOperand];
    if(ins->runtimeOp==XL6_IR_RUNTIME_C_PRINT || ins->runtimeOp==XL6_IR_RUNTIME_C_PRINTLN) {
        xl6OutputPrintf(out,"    XL6_CPrintPartV1 c_print_%u[%lu] = {0};\n",result,(unsigned long)ins->operandCount);
        for(size_t i=0;i<ins->operandCount;++i) {
            unsigned value=(unsigned)module->operands[ins->firstOperand+i];
            const XL6_IrInstruction* def=xl6IrCgenValueDefinition(module,value,NULL);
            const XL6_IrType* type=xl6IrTypeGet(&module->types,def->type);
            int optional=type->kind==XL6_IR_TYPE_NULLABLE;
            if(optional) {
                if(xl6IrTypeIsCOwned(type))xl6OutputPrintf(out,"    if(xl6COwnedIsNoneV1(v%u)) {\n",value);
                else if(type->cAbi==XL6_IR_C_ABI_MANAGED_STRING)xl6OutputPrintf(out,"    if(!v%u.Data) {\n",value);
                else if(type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)xl6OutputPrintf(out,"    if(!v%u || !xrtValueType(v%u)) {\n",value,value);
                else optional=0;
                if(optional)xl6OutputPrintf(out,"    c_print_%u[%lu].Kind=3; c_print_%u[%lu].Value=xrtValueNull();\n    } else {\n",result,(unsigned long)i,result,(unsigned long)i);
            }
            if(xl6IrTypeIsCOwned(type) || type->cAbi==XL6_IR_C_ABI_MANAGED_STRING)
                xl6OutputPrintf(out,"    c_print_%u[%lu].Kind=%d; c_print_%u[%lu].Data=v%u.Data; c_print_%u[%lu].Size=v%u.Size;\n",
                    result,(unsigned long)i,type->cAbi==XL6_IR_C_ABI_C_BYTES ? 2 : 1,result,(unsigned long)i,value,result,(unsigned long)i,value);
            else if(type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)
                xl6OutputPrintf(out,"    { xbytesview bytes={0}; if(!xrtValueGetBytes(v%u,&bytes))abort();\n"
                    "      c_print_%u[%lu].Kind=2; c_print_%u[%lu].Data=bytes.Data; c_print_%u[%lu].Size=bytes.Size; }\n",
                    value,result,(unsigned long)i,result,(unsigned long)i,result,(unsigned long)i);
            else xl6OutputPrintf(out,"    c_print_%u[%lu].Kind=3; c_print_%u[%lu].Value=v%u;\n",result,(unsigned long)i,result,(unsigned long)i,value);
            if(optional)xl6OutputPuts("    }\n",out);
        }
        xl6OutputPrintf(out,"    v%u = xl6CPrintV1(c_print_%u,%lu,%s);\n",result,result,(unsigned long)ins->operandCount,ins->runtimeOp==XL6_IR_RUNTIME_C_PRINTLN ? "true" : "false");
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_SET_COPY) {
        const XL6_IrInstruction* input=xl6IrCgenValueDefinition(module,source,NULL);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,input->type);
        xl6OutputPrintf(out,"    v%u = xl6CStringFromStringV1(xlir_string_scalar_set_copy(%s(v%u), v%u, v%u));\n"
            "    bool projection_ok_%u = v%u.Data != NULL;\n",result,type->kind==XL6_IR_TYPE_NULLABLE ? "xl6CStringNullableViewV1" : "xl6CStringViewV1",source,
            (unsigned)module->operands[ins->firstOperand+1],(unsigned)module->operands[ins->firstOperand+2],result,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SET_COPY) {
        unsigned key=(unsigned)module->operands[ins->firstOperand+1],item=(unsigned)module->operands[ins->firstOperand+2];
        const XL6_IrInstruction* input=xl6IrCgenValueDefinition(module,source,NULL);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,input->type);
        char length[64];
        if(type->kind==XL6_IR_TYPE_NULLABLE) {
            xl6OutputPrintf(out,"    size_t c_write_size_%u=xl6COwnedIsNoneV1(v%u) ? 0 : v%u.Size;\n",result,source,source);
            snprintf(length,sizeof(length),"c_write_size_%u",result);
        } else snprintf(length,sizeof(length),"v%u.Size",source);
        xl6OutputPrintf(out,"    v%u = (XL6_COwnedV1){0};\n    { size_t index;\n"
            "      if (xlir_sequence_index(%s, v%u, &index)) {\n"
            "        unsigned char* data = (unsigned char*)xrtMalloc(%s);\n"
            "        if (data) { memcpy(data,v%u.Data,%s); data[index]=(unsigned char)v%u;\n"
            "          v%u=xl6COwnedTakeV1(data,%s,xrtFree); }\n      } }\n"
            "    bool projection_ok_%u = v%u.Owner != NULL;\n",result,length,key,length,source,length,item,result,length,result,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_FORMAT) {
        xl6OutputPrintf(out,"    v%u=(xlir_string){0};\n"
            "    { xxlonwriteconfig config; xrtXlonWriteConfigInit(&config); xxlonwriter* writer=xrtXlonWriterCreate(&config);\n"
            "      if(writer && xrtXlonWriterBytes(writer,xl6CBytesViewV1(v%u)) && xrtXlonWriterFinish(writer)) {\n"
            "        size_t size=0; char* text=xrtXlonWriterTake(writer,&size); v%u=(xlir_string){text,text ? size : 0}; }\n"
            "      xrtXlonWriterFree(writer); }\n",result,source,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_C_OWNED_COPY) {
        xl6OutputPrintf(out,"    v%u = xl6COwnedCopyV1(v%u);\n    bool projection_ok_%u = !v%u.Data || v%u.Data != NULL;\n",result,source,result,source,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_FROM_STRING) {
        const XL6_IrInstruction* definition=xl6IrCgenValueDefinition(module,source,NULL);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,definition->type);
        xl6OutputPrintf(out,"    v%u = %s(v%u);\n",result,type->kind==XL6_IR_TYPE_NULLABLE ? "xl6CStringFromNullableStringV1" : "xl6CStringFromStringV1",source);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_FROM_BYTES) {
        const XL6_IrInstruction* definition=xl6IrCgenValueDefinition(module,source,NULL);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,definition->type);
        if(type->kind==XL6_IR_TYPE_NULLABLE)
            xl6OutputPrintf(out,"    v%u=xl6CBytesFromNullableBytesV1(v%u);\n    bool projection_ok_%u=xl6COwnedIsNoneV1(v%u) || v%u.Owner!=NULL;\n",result,source,result,result,result);
        else xl6OutputPrintf(out,"    v%u = xl6CBytesFromBytesV1(v%u);\n    bool projection_ok_%u = v%u.Owner != NULL;\n",result,source,result,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_C_BORROW_VIEW) {
        const XL6_IrInstruction* def=xl6IrCgenValueDefinition(module,module->operands[ins->firstOperand],NULL);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,def->type);
        if(type->cAbi==XL6_IR_C_ABI_MANAGED_STRING)
            xl6OutputPrintf(out,"    v%u=(XL6_COwnedV1){(void*)v%u.Data,v%u.Size,NULL};\n",result,source,source);
        else xl6OutputPrintf(out,"    { xbytesview bytes; if(!xrtValueGetBytes(v%u,&bytes))abort();\n"
            "      v%u=(XL6_COwnedV1){(void*)bytes.Data,bytes.Size,NULL}; }\n",source,result);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_VIEW) {
        const XL6_IrInstruction* definition=xl6IrCgenValueDefinition(module,source,NULL);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,definition->type);
        xl6OutputPrintf(out,"    v%u = %s(v%u);\n",result,type->kind==XL6_IR_TYPE_NULLABLE ? "xl6CStringNullableViewV1" : "xl6CStringViewV1",source);
    } else if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_CONCAT || ins->runtimeOp>=XL6_IR_RUNTIME_C_COMPARE_EQ) {
        int nullableComparison=0;
        for(size_t i=0;i<2;++i) {
            const XL6_IrInstruction* value=xl6IrCgenValueDefinition(module,module->operands[ins->firstOperand+i],NULL);
            nullableComparison|=xl6IrTypeGet(&module->types,value->type)->kind==XL6_IR_TYPE_NULLABLE;
        }
        for(unsigned i=0;i<2;++i) {
            unsigned value=(unsigned)module->operands[ins->firstOperand+i];
            const XL6_IrInstruction* def=xl6IrCgenValueDefinition(module,value,NULL);
            const XL6_IrType* type=xl6IrTypeGet(&module->types,def->type);
            xl6OutputPrintf(out,nullableComparison ? "    xbytesview c_view_%u_%u={0};\n" : "    xbytesview c_view_%u_%u;\n",result,i);
            if(type->kind==XL6_IR_TYPE_NULLABLE) {
                if(xl6IrTypeIsCOwned(type))xl6OutputPrintf(out,"    bool c_none_%u_%u=xl6COwnedIsNoneV1(v%u);\n",result,i,value);
                else if(type->cAbi==XL6_IR_C_ABI_MANAGED_STRING)xl6OutputPrintf(out,"    bool c_none_%u_%u=!v%u.Data;\n",result,i,value);
                else xl6OutputPrintf(out,"    bool c_none_%u_%u=!v%u || !xrtValueType(v%u);\n",result,i,value,value);
                xl6OutputPrintf(out,"    if(!c_none_%u_%u) {\n",result,i);
            } else if(nullableComparison)xl6OutputPrintf(out,"    bool c_none_%u_%u=false;\n",result,i);
            if(type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)
                xl6OutputPrintf(out,"    if (!xrtValueGetBytes(v%u, &c_view_%u_%u)) abort();\n",value,result,i);
            else xl6OutputPrintf(out,"    c_view_%u_%u = (xbytesview){ (const uint8*)v%u.Data, v%u.Size };\n",result,i,value,value);
            if(type->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("    }\n",out);
        }
        if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_CONCAT) {
            xl6OutputPrintf(out,"    v%u=(XL6_COwnedV1){0};\n"
                "    { xbytesview a=c_view_%u_0,b=c_view_%u_1;\n"
                "      if(a.Size>SIZE_MAX-b.Size)xrtSetErrorKind(XERR_RANGE); else {\n"
                "        size_t size=a.Size+b.Size; unsigned char* data=(unsigned char*)xrtMalloc(size ? size : 1u);\n"
                "        if(data) { if(a.Size)memcpy(data,a.Data,a.Size); if(b.Size)memcpy(data+a.Size,b.Data,b.Size);\n"
                "          v%u=xl6COwnedTakeV1(data,size,xrtFree); } } }\n"
                "    bool projection_ok_%u = v%u.Owner != NULL;\n",result,result,result,result,result,result);
        } else {
        const char* relation=ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_EQ ? "==" :
            ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_NE ? "!=" : ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_LT ? "<" :
            ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_LE ? "<=" : ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_GT ? ">" : ">=";
        xl6OutputPrintf(out,"    { xbytesview a=c_view_%u_0, b=c_view_%u_1; size_t n=a.Size<b.Size ? a.Size : b.Size;\n"
            "      int order=n ? memcmp(a.Data,b.Data,n) : 0; if (!order) order=(a.Size>b.Size)-(a.Size<b.Size);\n"
            "      v%u = order %s 0; }\n",result,result,result,relation);
        if(nullableComparison && (ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_EQ || ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_NE))
            xl6OutputPrintf(out,"    if(c_none_%u_0 || c_none_%u_1) v%u=(c_none_%u_0 %s c_none_%u_1);\n",
                result,result,result,result,ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_EQ ? "==" : "!=",result);
        }
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
        if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY) {
            const XL6_IrInstruction* definition=xl6IrCgenValueDefinition(module,source,NULL);
            const XL6_IrType* type=xl6IrTypeGet(&module->types,definition->type);
            if(type->kind==XL6_IR_TYPE_NULLABLE)
                xl6OutputPrintf(out,"    bool projection_ok_%u=xl6COwnedIsNoneV1(v%u) || v%u.Data!=NULL;\n",result,source,result);
            else xl6OutputPrintf(out,"    bool projection_ok_%u=v%u.Data!=NULL;\n",result,result);
        }
    }
    return !xl6OutputHasFailed(out);
}
