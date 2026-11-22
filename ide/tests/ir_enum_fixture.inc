/* Test-owned native rows; no production relocation code is used here. */
static const char* enumRecordNames[3]={"enumLayouts","enumVariants","enumPayloads"};
static const size_t enumRecordWidths[3]={sizeof(XL6_IrEnumLayout),sizeof(XL6_IrEnumVariant),sizeof(XL6_IrEnumPayload)};
static inline void* enumRecordRows(XL6_IrModule* m,size_t table,size_t* count)
{switch(table){case 0:*count=m->enumLayoutCount;return m->enumLayouts;case 1:*count=m->enumVariantCount;return m->enumVariants;case 2:*count=m->enumPayloadCount;return m->enumPayloads;default:*count=0;return NULL;}}
static inline void enumRecordSet(XL6_IrModule* m,size_t table,void* rows,size_t count)
{switch(table){case 0:m->enumLayouts=rows;m->enumLayoutCount=m->enumLayoutCapacity=count;break;case 1:m->enumVariants=rows;m->enumVariantCount=m->enumVariantCapacity=count;break;case 2:m->enumPayloads=rows;m->enumPayloadCount=m->enumPayloadCapacity=count;break;}}
