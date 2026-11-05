#ifndef XL6_FOREIGN_REGION_CONTRACT_H
#define XL6_FOREIGN_REGION_CONTRACT_H

/* Sparse fixed-pointer regions. A region occupies one physical C input slot;
 * unlike a span it never appends a count argument. */
typedef enum XL6_ForeignRegionAccess {
    XL6_FOREIGN_REGION_ACCESS_INVALID=0,
    XL6_FOREIGN_REGION_ACCESS_READ,
    XL6_FOREIGN_REGION_ACCESS_WRITE
} XL6_ForeignRegionAccess;
typedef enum XL6_ForeignRegionSelectorKind {
    XL6_FOREIGN_REGION_SELECTOR_INVALID=0,
    XL6_FOREIGN_REGION_SELECTOR_INPUT,
    XL6_FOREIGN_REGION_SELECTOR_INPUT_FIELD,
    XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD
} XL6_ForeignRegionSelectorKind;
typedef struct XL6_ForeignRegionSelector {
    XL6_ForeignRegionSelectorKind kind;
    uint32 physicalIndex,parameter,recordType,fieldDeclaration,fieldType;
    char field[64];
} XL6_ForeignRegionSelector;
typedef struct XL6_ForeignRegionContract {
    uint32 declaration,physicalIndex,parameter,logicalType,unitBytes;
    XL6_ForeignRegionAccess access;
    uint8 nullable;
    XL6_ForeignRegionSelector width,height,stride;
} XL6_ForeignRegionContract;
#define XL6_FOREIGN_REGION_OBSERVER_MAX_INPUTS 4
typedef struct XL6_ForeignRegionObserverContract {
    uint32 declaration,regionPhysicalIndex,querySymbol,queryDeclaration,recordType;
    uint32 physicalInputs[XL6_FOREIGN_REGION_OBSERVER_MAX_INPUTS];
    uint32 parameters[XL6_FOREIGN_REGION_OBSERVER_MAX_INPUTS];
    uint8 inputCount;
} XL6_ForeignRegionObserverContract;

#endif
