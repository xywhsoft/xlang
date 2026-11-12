#ifndef XL6_FOREIGN_OWNED_OUTPUT_CONTRACT_H
#define XL6_FOREIGN_OWNED_OUTPUT_CONTRACT_H
/* Sparse contract, one row per producer. Allocation compatibility is an
 * explicit provider promise: the exact resident XRT, not merely the same CRT.
 * No user release callback or provider-code dependency survives admission. */
typedef enum XL6_ForeignOwnedOutputPointer {
    XL6_FOREIGN_OWNED_OUTPUT_VOID=1,
    XL6_FOREIGN_OWNED_OUTPUT_CHAR,
    XL6_FOREIGN_OWNED_OUTPUT_UCHAR
} XL6_ForeignOwnedOutputPointer;
typedef enum XL6_ForeignOwnedOutputSize {
    XL6_FOREIGN_OWNED_OUTPUT_SIZE=1,
    XL6_FOREIGN_OWNED_OUTPUT_U64
} XL6_ForeignOwnedOutputSize;
typedef enum XL6_ForeignOwnedOutputKind {
    XL6_FOREIGN_OWNED_OUTPUT_BYTES=1,
    XL6_FOREIGN_OWNED_OUTPUT_TEXT
} XL6_ForeignOwnedOutputKind;
typedef struct XL6_ForeignOwnedOutputContract {
    uint32 declaration,dataIndex,sizeIndex;
    XL6_ForeignOwnedOutputPointer pointerAbi;
    XL6_ForeignOwnedOutputSize sizeAbi;
    int32 successCode;
    XL6_ForeignOwnedOutputKind kind;
} XL6_ForeignOwnedOutputContract;
static inline const char* xl6ForeignOwnedOutputPointerCName(XL6_ForeignOwnedOutputPointer abi)
{return abi==XL6_FOREIGN_OWNED_OUTPUT_VOID ? "void*":abi==XL6_FOREIGN_OWNED_OUTPUT_CHAR ? "char*":abi==XL6_FOREIGN_OWNED_OUTPUT_UCHAR ? "unsigned char*":NULL;}
static inline const char* xl6ForeignOwnedOutputSizeCName(XL6_ForeignOwnedOutputSize abi)
{return abi==XL6_FOREIGN_OWNED_OUTPUT_SIZE ? "size_t":abi==XL6_FOREIGN_OWNED_OUTPUT_U64 ? "uint64_t":NULL;}
#endif
