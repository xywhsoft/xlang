/* Included after XL6_IrPersistenceRestoreWalk. Static members claim both
 * runtime spelling and format alias, even when excluded from restoration.
 * Disabled base-format fields are still static, never dynamic wire extras. */
static int xl6IrRestoreClaim(XL6_IrPersistenceRestoreWalk* walk,size_t field,
    XL6_IrPersistenceProjectKeyAbi abi,const char* key)
{
    XL6_IrPersistenceRestoreClaim row={field,abi,key};
    if (!key || walk->claims==SIZE_MAX) return 0;
    if (walk->verify) {
        if (walk->claims>=walk->source->persistenceRestoreClaimCount) return 0;
        const XL6_IrPersistenceRestoreClaim* actual=&walk->source->persistenceRestoreClaims[walk->claims];
        if (actual->classField!=field || actual->keyAbi!=abi || actual->key!=key) return 0;
    } else {
        if (!xl6IrGrow((void**)&walk->output->persistenceRestoreClaims,
            &walk->output->persistenceRestoreClaimCapacity,walk->claims,sizeof(row))) return 0;
        walk->output->persistenceRestoreClaims[walk->claims]=row;
        walk->output->persistenceRestoreClaimCount=walk->claims+1;
    }
    ++walk->claims; return 1;
}
static int xl6IrRestoreClaims(XL6_IrPersistenceRestoreWalk* walk,size_t index,size_t depth)
{
    const XL6_IrModule* source=walk->source;
    if (index>=source->classLayoutCount || depth>source->classLayoutCount) return 0;
    const XL6_IrClassLayout* layout=&source->classLayouts[index];
    if (layout->firstField>source->classFieldCount ||
        layout->fieldCount>source->classFieldCount-layout->firstField ||
        layout->inheritedFieldCount>layout->fieldCount) return 0;
    if (layout->baseType!=XL6_IR_TYPE_INVALID) {
        size_t base=0;
        while (base<source->classLayoutCount && source->classLayouts[base].type!=layout->baseType) ++base;
        if (!xl6IrRestoreClaims(walk,base,depth+1)) return 0;
    }
    size_t first=layout->baseDeclaration!=XL6_IR_NODE_INVALID && layout->baseType==XL6_IR_TYPE_INVALID &&
        layout->persistenceObjectAbi==XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_SCHEMA ? layout->inheritedFieldCount : 0;
    for (size_t i=first;i<layout->fieldCount;++i) {
        size_t field=layout->firstField+i;
        const XL6_IrClassField* item=&source->classFields[field];
        const char* alias=walk->format==XL6_IR_PERSIST_FORMAT_JSON ? item->jsonPersistenceKeyLiteral : item->xlonPersistenceKeyLiteral;
        if (!xl6IrRestoreClaim(walk,field,XL6_IR_PERSIST_PROJECT_KEY_FIELD_NAME,item->name) ||
            (alias && !xl6IrRestoreClaim(walk,field,XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL,alias))) return 0;
    }
    return 1;
}
