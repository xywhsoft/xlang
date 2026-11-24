#ifndef XL6_FOREIGN_PACK_CONTRACT_H
#define XL6_FOREIGN_PACK_CONTRACT_H

#include <string.h>

/* Only descriptor projections use this logical bound. Physical slot and
 * ordinary function-attribute masks still have 64 entries. */
enum { XL6_FOREIGN_PACK_LOGICAL_LIMIT = 128 };

static inline size_t xl6ForeignPackParentLength(const char* path)
{const char* dot=strrchr(path,'.');return dot ? (size_t)(dot-path):0;}

/* A physical-only immutable C descriptor. Its inputs remain ordinary typed
 * language parameters, evaluated once and borrowed only during this call. */
typedef struct XL6_ForeignPackContract {
    uint32 declaration,physicalIndex,firstParameter,parameterCount;
    char recordCName[64],header[256];
    /* Optional retained callback group. A distinct physical owner slot has no
     * logical language argument; its record ABI is derived from the members. */
    uint32 callbackOwnerPhysicalIndexPlusOne;
    uint32 callbackGroupOptional;
    char callbackUserField[64],callbackGroupOwnerCName[64];
} XL6_ForeignPackContract;

typedef enum XL6_ForeignPackFieldKind {
    XL6_FOREIGN_PACK_FIELD_INVALID=0,
    XL6_FOREIGN_PACK_FIELD_VALUE,
    XL6_FOREIGN_PACK_FIELD_VIEW_RAW,
    XL6_FOREIGN_PACK_FIELD_TEXT_DATA,
    XL6_FOREIGN_PACK_FIELD_TEXT_SIZE,
    XL6_FOREIGN_PACK_FIELD_SPAN_DATA,
    XL6_FOREIGN_PACK_FIELD_SPAN_COUNT,
    XL6_FOREIGN_PACK_FIELD_C_STRING,
    XL6_FOREIGN_PACK_FIELD_SIZEOF,
    XL6_FOREIGN_PACK_FIELD_RECORD,
    /* A synchronous callback and its userdata share one logical parameter.
     * The invocation remains scoped by the existing callback contract. */
    XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK,
    XL6_FOREIGN_PACK_FIELD_CALLBACK_USER,
    XL6_FOREIGN_PACK_FIELD_OWNED_CALLBACK
} XL6_ForeignPackFieldKind;

enum {
    XL6_FOREIGN_PACK_SLOT_USED = 1u,
    XL6_FOREIGN_PACK_SLOT_TEXT_DATA = 2u,
    XL6_FOREIGN_PACK_SLOT_TEXT_SIZE = 4u,
    XL6_FOREIGN_PACK_SLOT_SPAN_DATA = 8u,
    XL6_FOREIGN_PACK_SLOT_SPAN_COUNT = 16u,
    XL6_FOREIGN_PACK_SLOT_SYNC = 32u,
    XL6_FOREIGN_PACK_SLOT_USER = 64u,
    XL6_FOREIGN_PACK_SLOT_OWNED = 128u,
    XL6_FOREIGN_PACK_SLOT_ARRAY = 256u
};

/* The normalized IR can include one implicit receiver in its ordinals. */
typedef struct XL6_ForeignPackSlots {
    uint16 flags[XL6_FOREIGN_PACK_LOGICAL_LIMIT+1];
} XL6_ForeignPackSlots;

static inline int xl6ForeignPackUsesWideOrdinal(XL6_ForeignPackFieldKind kind)
{
    return kind==XL6_FOREIGN_PACK_FIELD_VALUE || kind==XL6_FOREIGN_PACK_FIELD_VIEW_RAW ||
        kind==XL6_FOREIGN_PACK_FIELD_OWNED_CALLBACK;
}

static inline int xl6ForeignPackSlotArray(XL6_ForeignPackSlots* slots,size_t first,size_t last)
{
    if(first>last || last>=64)return 0;
    for(size_t p=first;p<=last;++p){
        if(slots->flags[p])return 0;
        slots->flags[p]=XL6_FOREIGN_PACK_SLOT_USED|XL6_FOREIGN_PACK_SLOT_ARRAY;
    }
    return 1;
}

static inline int xl6ForeignPackSlotField(XL6_ForeignPackSlots* slots,size_t p,XL6_ForeignPackFieldKind kind)
{
    if(p>XL6_FOREIGN_PACK_LOGICAL_LIMIT)return 0;
    unsigned flag=0;
    switch(kind){
        case XL6_FOREIGN_PACK_FIELD_TEXT_DATA: flag=XL6_FOREIGN_PACK_SLOT_TEXT_DATA;break;
        case XL6_FOREIGN_PACK_FIELD_TEXT_SIZE: flag=XL6_FOREIGN_PACK_SLOT_TEXT_SIZE;break;
        case XL6_FOREIGN_PACK_FIELD_SPAN_DATA: flag=XL6_FOREIGN_PACK_SLOT_SPAN_DATA;break;
        case XL6_FOREIGN_PACK_FIELD_SPAN_COUNT: flag=XL6_FOREIGN_PACK_SLOT_SPAN_COUNT;break;
        case XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK: flag=XL6_FOREIGN_PACK_SLOT_SYNC;break;
        case XL6_FOREIGN_PACK_FIELD_CALLBACK_USER: flag=XL6_FOREIGN_PACK_SLOT_USER;break;
        case XL6_FOREIGN_PACK_FIELD_OWNED_CALLBACK: flag=XL6_FOREIGN_PACK_SLOT_OWNED;break;
        default:break;
    }
    if(slots->flags[p]&(flag|XL6_FOREIGN_PACK_SLOT_ARRAY))return 0;
    slots->flags[p]|=(uint16)(flag|XL6_FOREIGN_PACK_SLOT_USED);
    return 1;
}

static inline int xl6ForeignPackSlotsComplete(const XL6_ForeignPackSlots* slots,size_t first,size_t count,int ownsCallbacks)
{
    if(first>XL6_FOREIGN_PACK_LOGICAL_LIMIT || count>XL6_FOREIGN_PACK_LOGICAL_LIMIT+1-first)return 0;
    int owned=0;
    for(size_t p=first;p<first+count;++p){
        unsigned flags=slots->flags[p];
        if(!(flags&XL6_FOREIGN_PACK_SLOT_USED) ||
            !!(flags&XL6_FOREIGN_PACK_SLOT_TEXT_DATA)!=!!(flags&XL6_FOREIGN_PACK_SLOT_TEXT_SIZE) ||
            !!(flags&XL6_FOREIGN_PACK_SLOT_SPAN_DATA)!=!!(flags&XL6_FOREIGN_PACK_SLOT_SPAN_COUNT) ||
            !!(flags&XL6_FOREIGN_PACK_SLOT_SYNC)!=!!(flags&XL6_FOREIGN_PACK_SLOT_USER))return 0;
        owned|=!!(flags&XL6_FOREIGN_PACK_SLOT_OWNED);
    }
    return owned==!!ownsCallbacks;
}

typedef struct XL6_ForeignPackFieldContract {
    uint32 declaration,packPhysicalIndex,parameter,logicalType;
    /* Closed sparse-row identity of the containing record; zero is the root. */
    uint32 recordParentIndexPlusOne;
    XL6_ForeignPackFieldKind kind;
    XL6_ForeignSpanCountAbi countAbi;
    char field[64],physicalCName[128];
} XL6_ForeignPackFieldContract;

typedef enum XL6_ForeignFixedInputKind {
    XL6_FOREIGN_FIXED_TYPED_NULL = 1,
    XL6_FOREIGN_FIXED_MODULE_CODE_OWNER
} XL6_ForeignFixedInputKind;
typedef struct XL6_ForeignFixedInputContract {
    uint32 declaration,physicalIndex;
    XL6_ForeignFixedInputKind kind;
    char pointerCName[128];
} XL6_ForeignFixedInputContract;

/* A physical step is one native input group or one output pointer. A span
 * step still has its inseparable pointer/count ABI. A pack step consumes its
 * complete logical parameter group and emits one const descriptor pointer. */
typedef enum XL6_ForeignPhysicalStepKind {
    XL6_FOREIGN_PHYSICAL_STEP_INVALID=0,
    XL6_FOREIGN_PHYSICAL_STEP_INPUT,
    XL6_FOREIGN_PHYSICAL_STEP_OUTPUT,
    XL6_FOREIGN_PHYSICAL_STEP_PACK,
    XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP,
    XL6_FOREIGN_PHYSICAL_STEP_FIXED,
    XL6_FOREIGN_PHYSICAL_STEP_CALLBACK_GROUP_OWNER
} XL6_ForeignPhysicalStepKind;
typedef struct XL6_ForeignPhysicalStep {
    XL6_ForeignPhysicalStepKind kind;
    uint32 parameter,ordinal,physicalIndex;
} XL6_ForeignPhysicalStep;

#endif
