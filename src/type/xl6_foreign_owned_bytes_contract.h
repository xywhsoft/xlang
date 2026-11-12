#ifndef XL6_FOREIGN_OWNED_BYTES_CONTRACT_H
#define XL6_FOREIGN_OWNED_BYTES_CONTRACT_H
/* Sparse contract, one row per producer. Allocation compatibility is an
 * explicit provider promise: the exact resident XRT, not merely the same CRT.
 * No user release callback or provider-code dependency survives admission. */
typedef enum XL6_ForeignOwnedBytesPointer {
    XL6_FOREIGN_OWNED_BYTES_VOID=1,
    XL6_FOREIGN_OWNED_BYTES_CHAR,
    XL6_FOREIGN_OWNED_BYTES_UCHAR
} XL6_ForeignOwnedBytesPointer;
typedef enum XL6_ForeignOwnedBytesSize {
    XL6_FOREIGN_OWNED_BYTES_SIZE=1,
    XL6_FOREIGN_OWNED_BYTES_U64
} XL6_ForeignOwnedBytesSize;
typedef struct XL6_ForeignOwnedBytesContract {
    uint32 declaration,dataIndex,sizeIndex;
    XL6_ForeignOwnedBytesPointer pointerAbi;
    XL6_ForeignOwnedBytesSize sizeAbi;
    int32 successCode;
} XL6_ForeignOwnedBytesContract;
static inline const char* xl6ForeignOwnedBytesPointerCName(XL6_ForeignOwnedBytesPointer abi)
{return abi==XL6_FOREIGN_OWNED_BYTES_VOID ? "void*":abi==XL6_FOREIGN_OWNED_BYTES_CHAR ? "char*":abi==XL6_FOREIGN_OWNED_BYTES_UCHAR ? "unsigned char*":NULL;}
static inline const char* xl6ForeignOwnedBytesSizeCName(XL6_ForeignOwnedBytesSize abi)
{return abi==XL6_FOREIGN_OWNED_BYTES_SIZE ? "size_t":abi==XL6_FOREIGN_OWNED_BYTES_U64 ? "uint64_t":NULL;}
#endif
