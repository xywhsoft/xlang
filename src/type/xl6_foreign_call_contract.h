#ifndef XL6_FOREIGN_CALL_CONTRACT_H
#define XL6_FOREIGN_CALL_CONTRACT_H

#include "src/common/xl6_common.h"
#include "src/type/xl6_foreign_region_contract.h"

typedef enum XL6_ForeignLifetimeAbi {
    XL6_FOREIGN_LIFETIME_NONE=0,
    XL6_FOREIGN_LIFETIME_CALLER,
    XL6_FOREIGN_LIFETIME_OWNED,
    XL6_FOREIGN_LIFETIME_OWNER,
    XL6_FOREIGN_LIFETIME_CALLBACK
} XL6_ForeignLifetimeAbi;

typedef enum XL6_ForeignBorrowedViewResult {
    XL6_FOREIGN_VIEW_RESULT_NONE = 0,
    XL6_FOREIGN_VIEW_RESULT_TEXT,
    XL6_FOREIGN_VIEW_RESULT_BYTES,
    XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE,
    XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE,
    XL6_FOREIGN_VIEW_RESULT_BOOL_STATUS,
    XL6_FOREIGN_VIEW_RESULT_SCOPE_NATIVE,
    XL6_FOREIGN_VIEW_RESULT_INT_STATUS,
    XL6_FOREIGN_VIEW_RESULT_TEXT_FIELD,
    XL6_FOREIGN_VIEW_RESULT_BYTES_FIELD,
    XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE,
    XL6_FOREIGN_VIEW_RESULT_BORROWED_VALUE
} XL6_ForeignBorrowedViewResult;

static inline int xl6ForeignResultTextView(XL6_ForeignBorrowedViewResult kind)
{return kind==XL6_FOREIGN_VIEW_RESULT_TEXT || kind==XL6_FOREIGN_VIEW_RESULT_TEXT_FIELD;}
static inline int xl6ForeignResultBytesView(XL6_ForeignBorrowedViewResult kind)
{return kind==XL6_FOREIGN_VIEW_RESULT_BYTES || kind==XL6_FOREIGN_VIEW_RESULT_BYTES_FIELD;}
static inline int xl6ForeignResultCountedField(XL6_ForeignBorrowedViewResult kind)
{return kind==XL6_FOREIGN_VIEW_RESULT_TEXT_FIELD || kind==XL6_FOREIGN_VIEW_RESULT_BYTES_FIELD;}

typedef enum XL6_ForeignInplaceKind {
    XL6_FOREIGN_INPLACE_NONE=0,
    XL6_FOREIGN_INPLACE_STATUS,
    XL6_FOREIGN_INPLACE_VOID
} XL6_ForeignInplaceKind;

typedef enum XL6_ForeignValueOutputKind {
    XL6_FOREIGN_VALUE_OUTPUT_NONE=0,
    XL6_FOREIGN_VALUE_OUTPUT_STATUS,
    XL6_FOREIGN_VALUE_OUTPUT_VOID,
    XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS,
    XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID,
    XL6_FOREIGN_VALUE_OUTPUT_RECORD_STATUS,
    XL6_FOREIGN_VALUE_OUTPUT_RECORD_VOID
} XL6_ForeignValueOutputKind;

/* The output-mode byte authenticates sparse sizeof initializers and borrowed
 * output conversions. No additional fact slot is attached to every IR node. */
#define XL6_FOREIGN_VALUE_OUTPUT_SIZE_INITIALIZED 128u
#define XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED 64u
#define XL6_FOREIGN_VALUE_OUTPUT_KIND_MASK 7u
static inline XL6_ForeignValueOutputKind xl6ForeignValueOutputBase(uint8 mode)
{return (XL6_ForeignValueOutputKind)(mode&XL6_FOREIGN_VALUE_OUTPUT_KIND_MASK);}
static inline int xl6ForeignValueOutputHasSize(uint8 mode)
{return (mode&XL6_FOREIGN_VALUE_OUTPUT_SIZE_INITIALIZED)!=0;}
static inline int xl6ForeignValueOutputHasView(uint8 mode)
{return (mode&XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED)!=0;}
static inline int xl6ForeignValueOutputIsRecord(uint8 mode)
{return xl6ForeignValueOutputBase(mode)==XL6_FOREIGN_VALUE_OUTPUT_RECORD_STATUS || xl6ForeignValueOutputBase(mode)==XL6_FOREIGN_VALUE_OUTPUT_RECORD_VOID;}
static inline int xl6ForeignValueOutputIsVoid(uint8 mode)
{return xl6ForeignValueOutputBase(mode)==XL6_FOREIGN_VALUE_OUTPUT_VOID || xl6ForeignValueOutputBase(mode)==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID || xl6ForeignValueOutputBase(mode)==XL6_FOREIGN_VALUE_OUTPUT_RECORD_VOID;}
static inline int xl6ForeignValueOutputBitsValid(uint8 mode)
{
    return !(mode&~(XL6_FOREIGN_VALUE_OUTPUT_KIND_MASK|XL6_FOREIGN_VALUE_OUTPUT_SIZE_INITIALIZED|XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED)) &&
        xl6ForeignValueOutputBase(mode)<=XL6_FOREIGN_VALUE_OUTPUT_RECORD_VOID &&
        (!(xl6ForeignValueOutputHasSize(mode)||xl6ForeignValueOutputHasView(mode)) || xl6ForeignValueOutputBase(mode)!=XL6_FOREIGN_VALUE_OUTPUT_NONE);
}
/* Closed input-contract bits: expected callback-span count plus sparse input
 * schema presence. All decoders validate the same uint16 representation. */
#define XL6_FOREIGN_INPUT_CALLBACK_COUNT_MASK 127u
#define XL6_FOREIGN_INPUT_REGIONS_PRESENT 128u
#define XL6_FOREIGN_INPUT_PACKS_PRESENT 256u
static inline uint8 xl6ForeignInputCallbackCount(uint16 facts)
{return (uint8)(facts&XL6_FOREIGN_INPUT_CALLBACK_COUNT_MASK);}
static inline int xl6ForeignInputHasRegions(uint16 facts)
{return (facts&XL6_FOREIGN_INPUT_REGIONS_PRESENT)!=0;}
static inline int xl6ForeignInputHasPacks(uint16 facts)
{return (facts&XL6_FOREIGN_INPUT_PACKS_PRESENT)!=0;}
static inline int xl6ForeignInputMappingsValid(uint16 facts)
{return !(facts&~(XL6_FOREIGN_INPUT_CALLBACK_COUNT_MASK|XL6_FOREIGN_INPUT_REGIONS_PRESENT|XL6_FOREIGN_INPUT_PACKS_PRESENT)) && xl6ForeignInputCallbackCount(facts)<=64;}
static inline int xl6ForeignInputMappingsEmpty(uint16 facts)
{return xl6ForeignInputMappingsValid(facts) && !facts;}
static inline int xl6ForeignInputMappingsSet(size_t callbacks,int regions,int packs,uint16* output)
{
    if(callbacks>64 || (regions!=0 && regions!=1) || (packs!=0 && packs!=1) || !output)return 0;
    *output=(uint16)(callbacks|(regions ? XL6_FOREIGN_INPUT_REGIONS_PRESENT:0)|(packs ? XL6_FOREIGN_INPUT_PACKS_PRESENT:0));return 1;
}

/* Closed declaration facts. Const addresses exist only during one call;
 * they do not introduce a language reference or an escaping pointer. */
typedef struct XL6_ForeignCallFacts {
    uint64 constAggregateAddresses;
    /* Capture-free no-raise C entries. The declaration author guarantees
     * that module code and globals outlive every native use of the address. */
    uint64 staticCallbackAddresses;
    /* Synchronous non-escaping text/byte views, preserving exact byte lengths. */
    uint64 textViews;
    uint64 bytesViews;
    uint64 terminatedStrings;
    /* Exactly two C slots: span pointer/count or synchronous callback/userdata. */
    uint64 physicalPairs;
    uint64 retainedCallbacks;
    uint64 controlViews;
    /* Closed byte enums, never written through enum*. Their placement leaves
     * room for input bits while keeping the declaration record at 88 B. */
    uint8 borrowedViewResult;
    uint8 valueOutput;
    uint8 receiverDereference;
    uint8 inplaceOutput; /* XL6_ForeignInplaceKind, not a Boolean. */
    uint16 inputMappingFacts;
    uint32 resultOwnerIndexPlusOne;
    uint32 outputIndexPlusOne;
    int32 successCode;
    uint32 rollbackSymbol;
} XL6_ForeignCallFacts;

static inline int xl6ForeignCallFactsEqual(const XL6_ForeignCallFacts* a,const XL6_ForeignCallFacts* b)
{
    return a->constAggregateAddresses==b->constAggregateAddresses && a->staticCallbackAddresses==b->staticCallbackAddresses &&
        a->textViews==b->textViews && a->bytesViews==b->bytesViews && a->terminatedStrings==b->terminatedStrings &&
        a->physicalPairs==b->physicalPairs && a->retainedCallbacks==b->retainedCallbacks && a->controlViews==b->controlViews &&
        a->borrowedViewResult==b->borrowedViewResult && a->resultOwnerIndexPlusOne==b->resultOwnerIndexPlusOne &&
        a->outputIndexPlusOne==b->outputIndexPlusOne && a->successCode==b->successCode && a->rollbackSymbol==b->rollbackSymbol &&
        a->valueOutput==b->valueOutput && a->receiverDereference==b->receiverDereference && a->inplaceOutput==b->inplaceOutput &&
        xl6ForeignInputMappingsValid(a->inputMappingFacts) && xl6ForeignInputMappingsValid(b->inputMappingFacts) &&
        xl6ForeignInputCallbackCount(a->inputMappingFacts)==xl6ForeignInputCallbackCount(b->inputMappingFacts) &&
        xl6ForeignInputHasRegions(a->inputMappingFacts)==xl6ForeignInputHasRegions(b->inputMappingFacts) &&
        xl6ForeignInputHasPacks(a->inputMappingFacts)==xl6ForeignInputHasPacks(b->inputMappingFacts);
}

/* Ownership helpers have one exact raw owner input and no physical input
 * rewrites. Release may report int status through the existing checked-result
 * contract; this is independent of its logical void result. */
static inline int xl6ForeignOwnershipHelperFacts(const XL6_ForeignCallFacts* f,int retain)
{
    return f && !f->constAggregateAddresses && !f->staticCallbackAddresses && !f->textViews && !f->bytesViews &&
        !f->terminatedStrings && !f->physicalPairs && !f->retainedCallbacks && !f->controlViews &&
        !f->resultOwnerIndexPlusOne && !f->outputIndexPlusOne && !f->rollbackSymbol && !f->valueOutput &&
        !f->receiverDereference && !f->inplaceOutput && xl6ForeignInputMappingsEmpty(f->inputMappingFacts) &&
        (f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_NONE ? !f->successCode :
         !retain && f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INT_STATUS);
}

/* Each multi-output producer stores only its actual physical output slots.
 * Ordinals follow increasing physical positions and the language return list. */
typedef struct XL6_ForeignOutputContract {
    uint32 declaration,ordinal,physicalIndex;
} XL6_ForeignOutputContract;

/* Only outputs with an explicit sizeof initializer occupy a row. The typed
 * IR closes recordType and fieldDeclaration/fieldType before publication. */
typedef struct XL6_ForeignOutputSizeContract {
    uint32 declaration,physicalIndex,ordinal,recordType,fieldDeclaration,fieldType;
    char field[64];
} XL6_ForeignOutputSizeContract;

typedef enum XL6_ForeignRetainSuccess {
    XL6_FOREIGN_RETAIN_ZERO = 1,
    XL6_FOREIGN_RETAIN_POSITIVE
} XL6_ForeignRetainSuccess;
typedef enum XL6_ForeignOutputConversion {
    XL6_FOREIGN_OUTPUT_CONVERSION_INVALID=0,
    XL6_FOREIGN_OUTPUT_CONVERSION_TEXT,
    XL6_FOREIGN_OUTPUT_CONVERSION_BYTES,
    XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE
} XL6_ForeignOutputConversion;
/* Physical xstrview/xbytesview outputs become owned logical values before
 * native input/callback scopes end. Ordinary outputs occupy no row. */
typedef struct XL6_ForeignOutputViewContract {
    uint32 declaration,physicalIndex,ordinal,logicalType;
    XL6_ForeignOutputConversion conversion;
    uint32 retainSymbol,rollbackSymbol;
    XL6_ForeignRetainSuccess retainSuccess;
} XL6_ForeignOutputViewContract;

/* An explicitly borrowed native record is physical output storage, never a
 * language record. Its fields project to independent logical tuple values. */
typedef struct XL6_ForeignOutputRecordContract {
    uint32 declaration,physicalIndex;
    char recordCName[64];
} XL6_ForeignOutputRecordContract;
typedef struct XL6_ForeignOutputFieldContract {
    uint32 declaration,ordinal,logicalType;
    char field[64];
} XL6_ForeignOutputFieldContract;

/* Only parameters which explicitly add pointee constness occupy a row. */
typedef struct XL6_ForeignPointerQualifierContract {
    uint32 declaration,parameter;
    char cName[80];
} XL6_ForeignPointerQualifierContract;
typedef struct XL6_ForeignFunctionHeaderContract {
    uint32 declaration;
    char header[256];
} XL6_ForeignFunctionHeaderContract;

/* A callable C-record field is a distinct operation, never a global symbol.
 * Only declarations which request it occupy a row; ordinary nodes stay 88 B. */
typedef struct XL6_ForeignFieldCallContract {
    uint32 declaration;
    char field[64];
} XL6_ForeignFieldCallContract;

/* Sparse per-declaration physical expansions, stored only for parameters
 * which request them. No fixed array is attached to every call or IR node. */
typedef enum XL6_ForeignParameterKind {
    XL6_FOREIGN_PARAMETER_SPAN = 1,
    XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK,
    XL6_FOREIGN_PARAMETER_SYNC_CALLBACK
} XL6_ForeignParameterKind;
typedef enum XL6_ForeignSpanCountAbi {
    XL6_FOREIGN_SPAN_COUNT_SIZE = 1,
    XL6_FOREIGN_SPAN_COUNT_INT
} XL6_ForeignSpanCountAbi;
#include "src/type/xl6_foreign_pack_contract.h"
/* Only counted native field projections allocate a row. Ordinary fields,
 * calls and IR nodes carry no fixed-size C identifier/length strings. */
typedef struct XL6_ForeignFieldLengthContract {
    uint32 declaration;
    char field[64];
    XL6_ForeignSpanCountAbi countAbi;
} XL6_ForeignFieldLengthContract;

/* Only in-place record factories carry storage/adoption metadata. The record
 * name is checked against the exact pointer type by the authoritative C header;
 * native storage must be independently xrt-allocated, never language stack data. */
typedef enum XL6_ForeignStorageKind {
    XL6_FOREIGN_STORAGE_POLICY = 1,
    XL6_FOREIGN_STORAGE_NATIVE_ADOPTION
} XL6_ForeignStorageKind;
typedef struct XL6_ForeignStorageContract {
    uint32 declaration;
    uint32 adoptSymbol;
    int32 successCode;
    XL6_ForeignStorageKind kind;
    char recordCName[64];
} XL6_ForeignStorageContract;

/* A borrowed native result acquires one independent native reference before
 * input cleanup. Each admitted language root owns precisely that reference. */
typedef struct XL6_ForeignRetainContract {
    uint32 declaration,retainSymbol,rollbackSymbol;
    XL6_ForeignRetainSuccess success;
} XL6_ForeignRetainContract;

/* Two physical native callback slots become one non-null callback-epoch byte
 * view. Counts are bytes; no element-stride inference or payload ownership. */
typedef struct XL6_ForeignCallbackSpanContract {
    uint32 declaration,parameter,argument;
    XL6_ForeignSpanCountAbi countAbi;
    char pointerCName[128];
} XL6_ForeignCallbackSpanContract;

/* One logical native view supplies its complete pointee by value. Parameter
 * indices include the implicit receiver; this never expands physical slots. */
typedef struct XL6_ForeignValueContract {
    uint32 declaration,parameter;
    char cName[64];
} XL6_ForeignValueContract;

typedef enum XL6_ForeignCallbackResultKind {
    XL6_FOREIGN_CALLBACK_RESULT_TRIVIAL = 0,
    XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT
} XL6_ForeignCallbackResultKind;

typedef struct XL6_ForeignParameterContract {
    uint32 declaration;
    uint32 parameter; /* Explicit language parameter, excluding receiver. */
    XL6_ForeignParameterKind kind;
    char pointerCName[128];
    char elementCName[128];
    char header[256]; /* Optional authoritative header for span/callback C entry. */
    XL6_ForeignSpanCountAbi countAbi;
    bool readOnly;
    char callbackOwnerCName[64];
    char callbackTypeCName[80]; /* Sealed XLIR typedef, empty before normalization. */
    uint32 callbackOwnerIndexPlusOne; /* Includes the implicit receiver. */
    int32 callbackFailureCode;
    bool callbackUserFirst;
    XL6_ForeignCallbackResultKind callbackResultKind;
} XL6_ForeignParameterContract;

#endif
