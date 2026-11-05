#ifndef XL6_FOREIGN_NATIVE_SEQUENCE_CONTRACT_H
#define XL6_FOREIGN_NATIVE_SEQUENCE_CONTRACT_H
/* Sparse immutable output promise. The pointer/count stay stable and elements
 * remain immutable for the independently retained owner's full lifetime.
 * The ABI is an actual contiguous const T array (stride == sizeof(T)); it is
 * not a certificate for a mutable vector or temporary callback storage. */
typedef struct XL6_ForeignNativeSequenceContract {
    uint32 declaration,dataIndex,countIndex,ownerParameter;
    uint32 sequenceType,elementType,ownerType;
    uint32 retainSymbol,rollbackSymbol;
    XL6_ForeignRetainSuccess retainSuccess;
    XL6_ForeignSpanCountAbi countAbi;
    int32 successCode;
    char dataCName[96];
} XL6_ForeignNativeSequenceContract;
#endif
