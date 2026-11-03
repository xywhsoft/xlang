#ifndef XL6_IR_DIRECT_BRIDGE_SCHEMA_H
#define XL6_IR_DIRECT_BRIDGE_SCHEMA_H
#include "xl6_ir_direct_bridge_types.h"
/* Build-time authoring form only. This table is never linked into a product. */
typedef struct XL6_IrDirectBridgeDeclaration {
#define XL6_DIRECT_BASE(Encoding, Type, Name) Type Name;
#define XL6_DIRECT_PARAM(Encoding, Type, Name, Member) Type Name[XL6_IR_DIRECT_BRIDGE_PARAMETER_CAPACITY];
#define XL6_DIRECT_DETAIL(Encoding, Type, Name) Type Name;
#define XL6_DIRECT_ARRAY(Encoding, Type, Name, Count) Type Name[Count];
#include "xl6_ir_direct_bridge_fields.inc"
#undef XL6_DIRECT_BASE
#undef XL6_DIRECT_PARAM
#undef XL6_DIRECT_DETAIL
#undef XL6_DIRECT_ARRAY
} XL6_IrDirectBridgeDeclaration;
#endif
