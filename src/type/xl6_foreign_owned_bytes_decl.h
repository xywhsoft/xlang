#ifndef XL6_FOREIGN_OWNED_BYTES_DECL_H
#define XL6_FOREIGN_OWNED_BYTES_DECL_H
static inline int xl6ForeignOwnedBytesRead(const XL6_AstNode* attr,XL6_ForeignOwnedBytesContract* output)
{
    XL6_ForeignOwnedBytesContract row={0};int32 data,size;char pointer[32],count[24],allocator[16];
    if(!attr || !attr->text || strcmp(attr->text,"c_output_bytes") || attr->childCount!=6 ||
       !xl6ForeignDecimalInt(attr->children[0],&data) || data<0 || data>=64 ||
       !xl6ForeignDecimalInt(attr->children[1],&size) || size<=data || size>=64 ||
       !xl6ForeignQuotedText(attr->children[2],pointer,sizeof(pointer)) ||
       !xl6ForeignQuotedText(attr->children[3],count,sizeof(count)) ||
       !xl6ForeignQuotedText(attr->children[4],allocator,sizeof(allocator)) || strcmp(allocator,"xrt") ||
       !xl6ForeignDecimalInt(attr->children[5],&row.successCode))return 0;
    row.pointerAbi=!strcmp(pointer,"void*") ? XL6_FOREIGN_OWNED_BYTES_VOID:
        !strcmp(pointer,"char*") ? XL6_FOREIGN_OWNED_BYTES_CHAR:
        !strcmp(pointer,"unsigned char*") ? XL6_FOREIGN_OWNED_BYTES_UCHAR:0;
    row.sizeAbi=!strcmp(count,"size_t") ? XL6_FOREIGN_OWNED_BYTES_SIZE:
        !strcmp(count,"uint64_t") ? XL6_FOREIGN_OWNED_BYTES_U64:0;
    if(!row.pointerAbi || !row.sizeAbi)return 0;
    row.dataIndex=(uint32)data;row.sizeIndex=(uint32)size;if(output)*output=row;return 1;
}
#endif
