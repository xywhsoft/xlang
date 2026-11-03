/* Closed direct owning-record calls. T/B denote the same physical families as
 * the direct bridge registry; N is deliberately the non-POD native value ABI.
 * Native record identity/ownership is granted only by its permanent profile.
 * No generated symbol-name dispatch, boxing, C callback or foreign layout. */
#define N XL6_IR_C_ABI_NATIVE_VALUE
XL6_NATIVE_VALUE_BRIDGE("xl6SlotMapKeyEqualV1",T,B,2,{N,N},
    .parameterNativeValueAbis={XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1,XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1},
    .noRaise=1,.pure=1,.nonblocking=1)
XL6_NATIVE_VALUE_BRIDGE("xl6SlotMapKeyCloneV1",T,N,1,{N},
    .resultNativeValueAbi=XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1,
    .parameterNativeValueAbis={XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1},
    .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE)
#undef N
