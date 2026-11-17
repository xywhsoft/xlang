#ifndef XL6_FOREIGN_ARRAY_CONTRACT_H
#define XL6_FOREIGN_ARRAY_CONTRACT_H

/* A synchronous physical projection of typed Array columns. Source indices
 * include the receiver after normalization. The final logical size input is
 * consumed by this group; it never becomes a second C count argument. */
typedef struct XL6_ForeignArrayGroupContract {
    uint32 declaration,physicalIndex,firstParameter,arrayCount,sizeParameter;
    XL6_ForeignSpanCountAbi countAbi;
    char recordCName[64],header[256],sizeField[64],sizeCName[24];
    /* An embedded projection keeps its group key for columns, but contributes
     * no independent C argument. The enclosing pack owns its logical inputs. */
    uint32 packPhysicalIndexPlusOne;
    char packDataField[64],packCountField[64];
} XL6_ForeignArrayGroupContract;
typedef enum XL6_ForeignArrayElementKind {
    XL6_FOREIGN_ARRAY_ELEMENT_INVALID=0,
    XL6_FOREIGN_ARRAY_ELEMENT_VALUE,
    XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW,
    XL6_FOREIGN_ARRAY_ELEMENT_C_STRING,
    XL6_FOREIGN_ARRAY_ELEMENT_SPAN
} XL6_ForeignArrayElementKind;
typedef enum XL6_ForeignArrayProfile {
    XL6_FOREIGN_ARRAY_PROFILE_INVALID=0,
    XL6_FOREIGN_ARRAY_PROFILE_SCALAR_READ_PIN,
    XL6_FOREIGN_ARRAY_PROFILE_VALUE_SNAPSHOT,
    XL6_FOREIGN_ARRAY_PROFILE_VIEW_CALL_PIN,
    XL6_FOREIGN_ARRAY_PROFILE_STRING_READ_PIN,
    XL6_FOREIGN_ARRAY_PROFILE_BYTES_READ_PIN
} XL6_ForeignArrayProfile;
typedef struct XL6_ForeignArrayInputContract {
    uint32 declaration,groupPhysicalIndex,column,arrayType,elementType;
    XL6_ForeignArrayElementKind kind;
    XL6_ForeignArrayProfile profile;
    char pointerCName[128],elementCName[128];
} XL6_ForeignArrayInputContract;
typedef struct XL6_ForeignArrayFieldContract {
    uint32 declaration,groupPhysicalIndex,column,arrayType,elementType;
    XL6_ForeignArrayElementKind kind;
    XL6_ForeignArrayProfile profile;
    char field[64],physicalCName[128],lengthField[64],lengthCName[24];
} XL6_ForeignArrayFieldContract;

#endif
