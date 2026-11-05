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
static inline int xl6ForeignArrayGroupRead(const XL6_AstNode* attr,XL6_ForeignArrayGroupContract* output)
{
    int32 physical,first,count,size;char countName[24];XL6_ForeignArrayGroupContract row={0};
    if(!attr || !attr->text || strcmp(attr->text,"c_array_group") || attr->childCount!=5 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignDecimalInt(attr->children[1],&first) || first<0 || first>=64 ||
        !xl6ForeignDecimalInt(attr->children[2],&count) || count<1 || count>63 || first>63-count ||
        !xl6ForeignQuotedText(attr->children[3],countName,sizeof(countName)) ||
        (strcmp(countName,"int") && strcmp(countName,"size_t")) ||
        !xl6ForeignDecimalInt(attr->children[4],&size) || size!=first+count)return 0;
    row.physicalIndex=(uint32)physical;row.firstParameter=(uint32)first;
    row.arrayCount=(uint32)count;row.sizeParameter=(uint32)size;
    row.countAbi=!strcmp(countName,"int") ? XL6_FOREIGN_SPAN_COUNT_INT:XL6_FOREIGN_SPAN_COUNT_SIZE;
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
static inline int xl6ForeignArrayRecordRead(const XL6_AstNode* attr,uint32* physical,char* name,size_t capacity)
{
    int32 slot;
    if(!attr || !attr->text || strcmp(attr->text,"c_array_record") || attr->childCount!=2 ||
        !xl6ForeignDecimalInt(attr->children[0],&slot) || slot<0 || slot>=64 ||
        !xl6ForeignQuotedText(attr->children[1],name,capacity) || !xl6ForeignIdentifier(name))return 0;
    if(physical)*physical=(uint32)slot;
    return 1;
}
static inline int xl6ForeignArrayFieldRead(const XL6_AstNode* attr,XL6_ForeignArrayFieldContract* output)
{
    int32 physical,column;char mode[24];XL6_ForeignArrayFieldContract row={0};
    if(!attr || !attr->text || strcmp(attr->text,"c_array_field") || attr->childCount!=4 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.field,sizeof(row.field)) || !xl6ForeignIdentifier(row.field) ||
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
    if(matches!=1)return 0;
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
