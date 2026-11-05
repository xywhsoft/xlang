#ifndef XL6_FOREIGN_PACK_CONTRACT_H
#define XL6_FOREIGN_PACK_CONTRACT_H

/* A physical-only immutable C descriptor. Its inputs remain ordinary typed
 * language parameters, evaluated once and borrowed only during this call. */
typedef struct XL6_ForeignPackContract {
    uint32 declaration,physicalIndex,firstParameter,parameterCount;
    char recordCName[64],header[256];
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
    XL6_FOREIGN_PACK_FIELD_SIZEOF
} XL6_ForeignPackFieldKind;

typedef struct XL6_ForeignPackFieldContract {
    uint32 declaration,packPhysicalIndex,parameter,logicalType;
    XL6_ForeignPackFieldKind kind;
    XL6_ForeignSpanCountAbi countAbi;
    char field[64],physicalCName[128];
} XL6_ForeignPackFieldContract;

/* A physical step is one native input group or one output pointer. A span
 * step still has its inseparable pointer/count ABI. A pack step consumes its
 * complete logical parameter group and emits one const descriptor pointer. */
typedef enum XL6_ForeignPhysicalStepKind {
    XL6_FOREIGN_PHYSICAL_STEP_INVALID=0,
    XL6_FOREIGN_PHYSICAL_STEP_INPUT,
    XL6_FOREIGN_PHYSICAL_STEP_OUTPUT,
    XL6_FOREIGN_PHYSICAL_STEP_PACK,
    XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP
} XL6_ForeignPhysicalStepKind;
typedef struct XL6_ForeignPhysicalStep {
    XL6_ForeignPhysicalStepKind kind;
    uint32 parameter,ordinal,physicalIndex;
} XL6_ForeignPhysicalStep;

#endif
