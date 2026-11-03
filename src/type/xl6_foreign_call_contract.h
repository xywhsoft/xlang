#ifndef XL6_FOREIGN_CALL_CONTRACT_H
#define XL6_FOREIGN_CALL_CONTRACT_H

#include "src/common/xl6_common.h"

typedef enum XL6_ForeignLifetimeAbi {
    XL6_FOREIGN_LIFETIME_NONE=0,
    XL6_FOREIGN_LIFETIME_CALLER,
    XL6_FOREIGN_LIFETIME_OWNED,
    XL6_FOREIGN_LIFETIME_OWNER
} XL6_ForeignLifetimeAbi;

typedef enum XL6_ForeignBorrowedViewResult {
    XL6_FOREIGN_VIEW_RESULT_NONE = 0,
    XL6_FOREIGN_VIEW_RESULT_TEXT,
    XL6_FOREIGN_VIEW_RESULT_BYTES,
    XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE,
    XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE,
    XL6_FOREIGN_VIEW_RESULT_BOOL_STATUS
} XL6_ForeignBorrowedViewResult;

/* Closed declaration facts. Const addresses exist only during one call;
 * they do not introduce a language reference or an escaping pointer. */
typedef struct XL6_ForeignCallFacts {
    uint64 constRecordAddresses;
    /* Capture-free no-raise C entries. The declaration author guarantees
     * that module code and globals outlive every native use of the address. */
    uint64 staticCallbackAddresses;
    /* Synchronous non-escaping text/byte views, preserving exact byte lengths. */
    uint64 textViews;
    uint64 bytesViews;
    uint64 terminatedStrings;
    uint64 spanParameters;
    uint64 retainedCallbacks;
    uint64 controlViews;
    XL6_ForeignBorrowedViewResult borrowedViewResult;
    uint32 resultOwnerIndexPlusOne;
    uint32 outputIndexPlusOne;
    int32 successCode;
    uint32 rollbackSymbol;
} XL6_ForeignCallFacts;

/* The factory output index counts actual C slots. Expanded pointer/count
 * and callback/userdata/owner groups remain indivisible parameter contracts. */
static inline size_t xl6ForeignFactoryOutputLogical(const XL6_ForeignCallFacts* facts,size_t count)
{
    if(!facts || !facts->outputIndexPlusOne)return SIZE_MAX;
    size_t physical=0;
    for(size_t p=0;p<=count;++p) {
        if(physical==facts->outputIndexPlusOne-1)return p;
        if(p<count)physical+=p<64 && (facts->spanParameters&(UINT64_C(1)<<p)) ? 2 :
            p<64 && (facts->retainedCallbacks&(UINT64_C(1)<<p)) ? 3 : 1;
    }
    return SIZE_MAX;
}

/* Sparse per-declaration physical expansions, stored only for parameters
 * which request them. No fixed array is attached to every call or IR node. */
typedef enum XL6_ForeignParameterKind {
    XL6_FOREIGN_PARAMETER_SPAN = 1,
    XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK
} XL6_ForeignParameterKind;
typedef enum XL6_ForeignSpanCountAbi {
    XL6_FOREIGN_SPAN_COUNT_SIZE = 1,
    XL6_FOREIGN_SPAN_COUNT_INT
} XL6_ForeignSpanCountAbi;
typedef struct XL6_ForeignParameterContract {
    uint32 declaration;
    uint32 parameter; /* Explicit language parameter, excluding receiver. */
    XL6_ForeignParameterKind kind;
    char pointerCName[128];
    char elementCName[128];
    char header[256]; /* Optional authoritative header for a split C span. */
    XL6_ForeignSpanCountAbi countAbi;
    bool readOnly;
    char callbackOwnerCName[64];
    char callbackTypeCName[80]; /* Sealed XLIR typedef, empty before normalization. */
    uint32 callbackOwnerIndexPlusOne; /* Includes the implicit receiver. */
    int32 callbackFailureCode;
    bool callbackUserFirst;
} XL6_ForeignParameterContract;

#endif
