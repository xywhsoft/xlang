/* Header and call initialization consume the same sealed typed output rows. */
static int xl6IrCgenOutputSizeProof(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrAggregateLayout* layout)
{
    const XL6_IrType* record=xl6IrTypeGet(&module->types,layout->type);
    for(size_t i=0;i<module->foreignOutputSizeCount;++i){
        const XL6_ForeignOutputSizeContract* row=&module->foreignOutputSizes[i];
        if(row->recordType!=layout->type)continue;
        const char* fieldType=xl6IrCgenType(module,row->fieldType);
        if(!record || !fieldType)return 0;
        xl6OutputPrintf(out,
            "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s, %s), \"C output size field type\");\n"
            "XLIR_NATIVE_ASSERT(sizeof(%s) <= ((%s)~(%s)0), \"C output size field range\");\n",
            record->cName,row->field,fieldType,record->cName,fieldType,fieldType);
    }
    return 1;
}
static int xl6IrCgenOutputSizeInitialize(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target)
{
    size_t count=xl6IrForeignOutputCount(module,target);
    if(!xl6IrForeignOutputSizesMatch(module,target))return 0;
    if(xl6IrForeignOutputRecordGet(module,target)){
        const XL6_ForeignOutputSizeContract* row=xl6IrForeignOutputSizeGet(module,target,0);
        if(row)xl6OutputPrintf(out,"    native_raw.%s = sizeof(native_raw);\n",row->field);
        return !xl6OutputHasFailed(out);
    }
    for(size_t ordinal=0;ordinal<count;++ordinal){
        const XL6_ForeignOutputSizeContract* row=xl6IrForeignOutputSizeGet(module,target,ordinal);
        if(!row)continue;
        if(count>1)xl6OutputPrintf(out,"    native_raw.e%lu.%s = sizeof(native_raw.e%lu);\n",
            (unsigned long)ordinal,row->field,(unsigned long)ordinal);
        else xl6OutputPrintf(out,"    native_raw.%s = sizeof(native_raw);\n",row->field);
    }
    return 1;
}
