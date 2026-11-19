#ifndef XL6_FOREIGN_ARRAY_DECL_H
#define XL6_FOREIGN_ARRAY_DECL_H
/* Input columns may qualify the pointer elements themselves (T* const*).
 * This closed spelling permits no expressions, attributes or declarators. */
static inline int xl6ForeignArrayPointerSpelling(const char* text)
{
    if(xl6ForeignPointerSpelling(text))return 1;
    char base[64];size_t length=0,pointers=0;const char* p=text;if(!p)return 0;
    if(!strncmp(p,"const ",6))p+=6;
    if(!strncmp(p,"struct ",7))p+=7;
    while(*p && *p!=' ' && *p!='*'){if(length+1>=sizeof(base))return 0;base[length++]=*p++;}
    base[length]=0;if(!xl6ForeignIdentifier(base))return 0;
    while(*p==' ')++p;
    while(*p=='*'){
        if(++pointers>8)return 0;
        ++p;while(*p==' ')++p;
        if(!strncmp(p,"const",5) && (p[5]==' ' || p[5]=='*' || !p[5])){p+=5;while(*p==' ')++p;}
    }
    return pointers && !*p;
}

static inline XL6_ForeignArrayElementKind xl6ForeignArrayElementKind(const char* name)
{
    return !strcmp(name,"value") ? XL6_FOREIGN_ARRAY_ELEMENT_VALUE :
        !strcmp(name,"view_raw") ? XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW :
        !strcmp(name,"c_string") ? XL6_FOREIGN_ARRAY_ELEMENT_C_STRING : XL6_FOREIGN_ARRAY_ELEMENT_INVALID;
}
static inline int xl6ForeignArrayLengthSpelling(const char* text)
{return text && (!strcmp(text,"int") || !strcmp(text,"size_t") || !strcmp(text,"uint32_t") || !strcmp(text,"uint64_t"));}
static inline int xl6ForeignArraySpanPointerSpelling(const char* text)
{return text && (!strcmp(text,"const char*") || !strcmp(text,"const uint8_t*") || !strcmp(text,"const void*"));}
static inline int xl6ForeignArrayFieldNamesOverlap(const XL6_ForeignArrayFieldContract* a,const XL6_ForeignArrayFieldContract* b)
{
    return xl6ForeignPackPathsOverlap(a->field,b->field) ||
        (a->lengthField[0] && xl6ForeignPackPathsOverlap(a->lengthField,b->field)) ||
        (b->lengthField[0] && xl6ForeignPackPathsOverlap(a->field,b->lengthField)) ||
        (a->lengthField[0] && b->lengthField[0] && xl6ForeignPackPathsOverlap(a->lengthField,b->lengthField));
}
static inline int xl6ForeignArrayFieldUsesPath(const XL6_ForeignArrayFieldContract* row,const char* path)
{
    return xl6ForeignPackPathsOverlap(row->field,path) ||
        (row->lengthField[0] && xl6ForeignPackPathsOverlap(row->lengthField,path));
}
static inline int xl6ForeignArrayFieldShape(const XL6_ForeignArrayFieldContract* row)
{
    if(!memchr(row->field,0,sizeof(row->field)) || !xl6ForeignPackPath(row->field) ||
        !memchr(row->lengthField,0,sizeof(row->lengthField)) || !memchr(row->lengthCName,0,sizeof(row->lengthCName)))return 0;
    if(row->kind==XL6_FOREIGN_ARRAY_ELEMENT_SIZE)
        return row->column==UINT32_MAX && !row->arrayType && !row->elementType && !row->profile &&
            !row->physicalCName[0] && !row->lengthField[0] && xl6ForeignPackParentLength(row->field) &&
            xl6ForeignArrayLengthSpelling(row->lengthCName);
    if(row->kind!=XL6_FOREIGN_ARRAY_ELEMENT_SPAN)return !row->lengthField[0] && !row->lengthCName[0];
    return memchr(row->physicalCName,0,sizeof(row->physicalCName)) && xl6ForeignArraySpanPointerSpelling(row->physicalCName) &&
        xl6ForeignPackPath(row->lengthField) && !xl6ForeignPackPathsOverlap(row->field,row->lengthField) && xl6ForeignArrayLengthSpelling(row->lengthCName);
}
static inline int xl6ForeignArrayGroupRead(const XL6_AstNode* attr,XL6_ForeignArrayGroupContract* output)
{
    int32 physical,first,count,size;char countName[24];XL6_ForeignArrayGroupContract row={0};
    if(!attr || !attr->text || strcmp(attr->text,"c_array_group") || attr->childCount!=5 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignDecimalInt(attr->children[1],&first) || first<0 || first>=64 ||
        !xl6ForeignDecimalInt(attr->children[2],&count) || count<1 || count>63 || first>63-count ||
        !xl6ForeignQuotedText(attr->children[3],countName,sizeof(countName)) ||
        (strcmp(countName,"int") && strcmp(countName,"size_t") && strcmp(countName,"uint64_t")) ||
        !xl6ForeignDecimalInt(attr->children[4],&size) || size!=first+count)return 0;
    row.physicalIndex=(uint32)physical;row.firstParameter=(uint32)first;
    row.arrayCount=(uint32)count;row.sizeParameter=(uint32)size;
    row.countAbi=!strcmp(countName,"int") ? XL6_FOREIGN_SPAN_COUNT_INT:!strcmp(countName,"size_t") ? XL6_FOREIGN_SPAN_COUNT_SIZE:XL6_FOREIGN_SPAN_COUNT_U64;
    if(output)*output=row;
    return 1;
}
static inline int xl6ForeignArrayInputRead(const XL6_AstNode* attr,XL6_ForeignArrayInputContract* output)
{
    int32 physical,column;char mode[24];XL6_ForeignArrayInputContract row={0};
    if(!attr || !attr->text || strcmp(attr->text,"c_array_input") || attr->childCount!=4 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignDecimalInt(attr->children[1],&column) || column<0 || column>=64 ||
        !xl6ForeignQuotedText(attr->children[2],row.pointerCName,sizeof(row.pointerCName)) ||
        !xl6ForeignArrayPointerSpelling(row.pointerCName) ||
        !xl6ForeignQuotedText(attr->children[3],mode,sizeof(mode)) ||
        !(row.kind=xl6ForeignArrayElementKind(mode)))return 0;
    row.groupPhysicalIndex=(uint32)physical;row.column=(uint32)column;
    if(output)*output=row;
    return 1;
}
/* Projection IDs remain unique within a declaration. A destination changes
 * where the prepared record pointer/count are written, not the column IDs. */
static inline int xl6ForeignArrayPackRead(const XL6_AstNode* attr,uint32* group,
    uint32* physical,char* data,size_t dataCapacity,char* count,size_t countCapacity)
{
    int32 key,slot;char dataName[64],countName[64];
    if(!attr || !attr->text || strcmp(attr->text,"c_array_pack") || attr->childCount!=4 ||
        !xl6ForeignDecimalInt(attr->children[0],&key) || key<0 || key>=64 ||
        !xl6ForeignDecimalInt(attr->children[1],&slot) || slot<0 || slot>=64 ||
        !xl6ForeignQuotedText(attr->children[2],dataName,sizeof(dataName)) || !xl6ForeignPackPath(dataName) ||
        !xl6ForeignQuotedText(attr->children[3],countName,sizeof(countName)) || !xl6ForeignPackPath(countName) ||
        xl6ForeignPackPathsOverlap(dataName,countName))return 0;
    if((data && strlen(dataName)>=dataCapacity) || (count && strlen(countName)>=countCapacity))return 0;
    if(group)*group=(uint32)key;
    if(physical)*physical=(uint32)slot;
    if(data)strcpy(data,dataName);
    if(count)strcpy(count,countName);
    return 1;
}
static inline int xl6ForeignArrayPackForGroup(const XL6_AstNode* node,XL6_ForeignArrayGroupContract* group)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t found=0;
    if(!group)return 0;
    group->packPhysicalIndexPlusOne=0;group->packDataField[0]=group->packCountField[0]=0;
    for(size_t i=0;attrs && i<attrs->childCount;++i){const XL6_AstNode* attr=attrs->children[i];
        if(!attr || !attr->text || strcmp(attr->text,"c_array_pack"))continue;
        uint32 key,physical;char data[64],count[64];
        if(!xl6ForeignArrayPackRead(attr,&key,&physical,data,sizeof(data),count,sizeof(count)))return 0;
        if(key!=group->physicalIndex)continue;
        if(found++)return 0;
        group->packPhysicalIndexPlusOne=physical+1;
        strcpy(group->packDataField,data);strcpy(group->packCountField,count);
    }
    return 1;
}
static inline int xl6ForeignArrayRecordRead(const XL6_AstNode* attr,uint32* physical,char* name,size_t capacity)
{
    int32 slot;
    if(!attr || !attr->text || strcmp(attr->text,"c_array_record") || (attr->childCount!=2 && attr->childCount!=4) ||
        !xl6ForeignDecimalInt(attr->children[0],&slot) || slot<0 || slot>=64 ||
        !xl6ForeignQuotedText(attr->children[1],name,capacity) || !xl6ForeignIdentifier(name))return 0;
    if(attr->childCount==4){char field[64],ctype[24];
        if(!xl6ForeignQuotedText(attr->children[2],field,sizeof(field)) || !xl6ForeignIdentifier(field) ||
            !xl6ForeignQuotedText(attr->children[3],ctype,sizeof(ctype)) || !xl6ForeignArrayLengthSpelling(ctype))return 0;}
    if(physical)*physical=(uint32)slot;
    return 1;
}
static inline int xl6ForeignArrayRecordSizeRead(const XL6_AstNode* attr,XL6_ForeignArrayGroupContract* output)
{
    uint32 slot;char name[64];
    if(!xl6ForeignArrayRecordRead(attr,&slot,name,sizeof(name)) || attr->childCount!=4)return 0;
    if(output){strcpy(output->recordCName,name);
        if(!xl6ForeignQuotedText(attr->children[2],output->sizeField,sizeof(output->sizeField)) ||
            !xl6ForeignQuotedText(attr->children[3],output->sizeCName,sizeof(output->sizeCName)))return 0;}
    return 1;
}
static inline int xl6ForeignArrayFieldRead(const XL6_AstNode* attr,XL6_ForeignArrayFieldContract* output)
{
    int32 physical,column;char mode[24];XL6_ForeignArrayFieldContract row={0};
    if(attr && attr->text && !strcmp(attr->text,"c_array_size")){
        if(attr->childCount!=3 || !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
            !xl6ForeignQuotedText(attr->children[1],row.field,sizeof(row.field)) ||
            !xl6ForeignQuotedText(attr->children[2],row.lengthCName,sizeof(row.lengthCName)))return 0;
        row.groupPhysicalIndex=(uint32)physical;
        row.column=UINT32_MAX;
        row.kind=XL6_FOREIGN_ARRAY_ELEMENT_SIZE;
        if(!xl6ForeignArrayFieldShape(&row))return 0;
        if(output)*output=row;
        return 1;
    }
    if(attr && attr->text && !strcmp(attr->text,"c_array_span_field")){
        if(attr->childCount!=6 || !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
            !xl6ForeignQuotedText(attr->children[1],row.field,sizeof(row.field)) || !xl6ForeignPackPath(row.field) ||
            !xl6ForeignQuotedText(attr->children[2],row.lengthField,sizeof(row.lengthField)) || !xl6ForeignPackPath(row.lengthField) ||
            xl6ForeignPackPathsOverlap(row.field,row.lengthField) || !xl6ForeignDecimalInt(attr->children[3],&column) || column<0 || column>=64 ||
            !xl6ForeignQuotedText(attr->children[4],row.physicalCName,sizeof(row.physicalCName)) || !xl6ForeignArraySpanPointerSpelling(row.physicalCName) ||
            !xl6ForeignQuotedText(attr->children[5],row.lengthCName,sizeof(row.lengthCName)) || !xl6ForeignArrayLengthSpelling(row.lengthCName))return 0;
        row.groupPhysicalIndex=(uint32)physical;row.column=(uint32)column;row.kind=XL6_FOREIGN_ARRAY_ELEMENT_SPAN;
        if(output)*output=row;
        return 1;
    }
    if(!attr || !attr->text || strcmp(attr->text,"c_array_field") || attr->childCount!=4 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.field,sizeof(row.field)) || !xl6ForeignPackPath(row.field) ||
        !xl6ForeignDecimalInt(attr->children[2],&column) || column<0 || column>=64 ||
        !xl6ForeignQuotedText(attr->children[3],mode,sizeof(mode)) ||
        !(row.kind=xl6ForeignArrayElementKind(mode)))return 0;
    row.groupPhysicalIndex=(uint32)physical;row.column=(uint32)column;
    if(output)*output=row;
    return 1;
}
static inline int xl6ForeignArrayForParameter(const XL6_AstNode* node,size_t parameter,XL6_ForeignArrayGroupContract* output)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t matches=0;XL6_ForeignArrayGroupContract found={0};
    for(size_t a=0;attrs && a<attrs->childCount;++a){XL6_ForeignArrayGroupContract row;
        if(xl6ForeignArrayGroupRead(attrs->children[a],&row) && parameter>=row.firstParameter && parameter<=row.sizeParameter){found=row;++matches;}}
    if(matches!=1 || !xl6ForeignArrayPackForGroup(node,&found))return 0;
    if(output)*output=found;
    return 1;
}
static inline size_t xl6ForeignArrayNativeWidth(const XL6_AstNode* node,const XL6_ForeignArrayGroupContract* group)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t records=0;
    for(size_t a=0;attrs && a<attrs->childCount;++a){uint32 slot;char name[64];
        if(xl6ForeignArrayRecordRead(attrs->children[a],&slot,name,sizeof(name)) && slot==group->physicalIndex)++records;}
    return records>1 ? SIZE_MAX:records ? 2:(size_t)group->arrayCount+1;
}

#endif
