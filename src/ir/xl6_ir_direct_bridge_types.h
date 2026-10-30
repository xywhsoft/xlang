#ifndef XL6_IR_DIRECT_BRIDGE_TYPES_H
#define XL6_IR_DIRECT_BRIDGE_TYPES_H
/* Internal immutable catalog. Only the parameter count changes representation;
 * declaration validation keeps its existing sixteen-parameter semantic bound. */
typedef struct XL6_IrNumericEnumMemberContract {
    const char* name;
    uint64 magnitude;
    int negative;
} XL6_IrNumericEnumMemberContract;
typedef struct XL6_IrNumericEnumContract {
    const char* declarationName;
    XL6_IrCAbi cAbi;
    XL6_IrIntegerTarget integerTarget;
    const XL6_IrNumericEnumMemberContract* members;
    size_t memberCount;
} XL6_IrNumericEnumContract;

enum { XL6_IR_DIRECT_BRIDGE_PARAMETER_CAPACITY = 16 };
typedef struct XL6_IrDirectBridgeParameter {
#define XL6_DIRECT_BASE(Encoding, Type, Name)
#define XL6_DIRECT_PARAM(Encoding, Type, Name, Member) Type Member;
#define XL6_DIRECT_DETAIL(Encoding, Type, Name)
#define XL6_DIRECT_ARRAY(Encoding, Type, Name, Count)
#include "xl6_ir_direct_bridge_fields.inc"
#undef XL6_DIRECT_BASE
#undef XL6_DIRECT_PARAM
#undef XL6_DIRECT_DETAIL
#undef XL6_DIRECT_ARRAY
} XL6_IrDirectBridgeParameter;
typedef struct XL6_IrDirectBridgeDetails {
#define XL6_DIRECT_BASE(Encoding, Type, Name)
#define XL6_DIRECT_PARAM(Encoding, Type, Name, Member)
#define XL6_DIRECT_DETAIL(Encoding, Type, Name) Type Name;
#define XL6_DIRECT_ARRAY(Encoding, Type, Name, Count) Type Name[Count];
#include "xl6_ir_direct_bridge_fields.inc"
#undef XL6_DIRECT_BASE
#undef XL6_DIRECT_PARAM
#undef XL6_DIRECT_DETAIL
#undef XL6_DIRECT_ARRAY
} XL6_IrDirectBridgeDetails;
typedef struct XL6_IrDirectBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrCAbi result;
    uint32_t parameterCount;
    uint32_t nameLength;
    const XL6_IrDirectBridgeParameter* parameters;
    const XL6_IrDirectBridgeDetails* details;
} XL6_IrDirectBridgeContract;
#endif
