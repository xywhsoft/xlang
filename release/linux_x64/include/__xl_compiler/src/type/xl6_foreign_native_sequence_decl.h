#ifndef XL6_FOREIGN_NATIVE_SEQUENCE_DECL_H
#define XL6_FOREIGN_NATIVE_SEQUENCE_DECL_H
static inline int xl6ForeignNativeSequenceRead(const XL6_AstNode* attr,
    XL6_ForeignNativeSequenceContract* output)
{
    XL6_ForeignNativeSequenceContract row={0};int32 data,count,owner;char countName[24];
    if(!attr || !attr->text || strcmp(attr->text,"c_owned_sequence") || attr->childCount!=5 ||
        !xl6ForeignDecimalInt(attr->children[0],&data) || data<0 || data>=64 ||
        !xl6ForeignDecimalInt(attr->children[1],&count) || count<0 || count>=64 || data==count ||
        !xl6ForeignDecimalInt(attr->children[2],&owner) || owner<0 || owner>=64 ||
        !xl6ForeignQuotedText(attr->children[3],countName,sizeof(countName)) ||
        !xl6ForeignDecimalInt(attr->children[4],&row.successCode))return 0;
    row.countAbi=!strcmp(countName,"int") ? XL6_FOREIGN_SPAN_COUNT_INT :
        !strcmp(countName,"size_t") ? XL6_FOREIGN_SPAN_COUNT_SIZE:0;
    if(!row.countAbi)return 0;
    row.dataIndex=(uint32)data;row.countIndex=(uint32)count;row.ownerParameter=(uint32)owner;
    if(output)*output=row;
    return 1;
}
#endif
