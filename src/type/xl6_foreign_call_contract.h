#ifndef XL6_FOREIGN_CALL_CONTRACT_H
#define XL6_FOREIGN_CALL_CONTRACT_H

#include "src/common/xl6_common.h"

/* Closed declaration facts. Const addresses exist only during one call;
 * they do not introduce a language reference or an escaping pointer. */
typedef struct XL6_ForeignCallFacts {
    uint64 constRecordAddresses;
    /* Capture-free no-raise C entries. The declaration author guarantees
     * that module code and globals outlive every native use of the address. */
    uint64 staticCallbackAddresses;
    /* Synchronous non-escaping xstrview, preserving the exact byte length. */
    uint64 textViews;
    uint32 textViewResult;
    uint32 outputIndexPlusOne;
    int32 successCode;
    uint32 rollbackSymbol;
} XL6_ForeignCallFacts;

#endif
