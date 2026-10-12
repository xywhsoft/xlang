/* Closed immutable native owning-value profiles. Consumed by semantic builtin
 * registration and IR; a source spelling or a foreign layout grants nothing.
 * Keep physical declarations and resident implementation out of Cgen. */
XL6_NATIVE_VALUE_PROFILE(SLOT_MAP_KEY_V1, "SlotMapKey", "xlang.runtime.slot-map-key.v1",
    "XL6_SlotMapKeyV1", "__xl_runtime/xl6_slot_map_key_abi.h", "xlir_result_slot_map_key_v1",
    "xl6SlotMapKeyCopyValueV1", "xl6SlotMapKeyDropValueV1", "xl6SlotMapKeyEqualV1",
    "xl6SlotMapKeyBoxV1", "xl6SlotMapKeyUnboxV1", NO_RAISE,
    xl6SlotMapKeyTypeV1, xl6SlotMapKeyConverterV1, "xl6SequenceSlotMapKeyCreateV1",
    "xl6SlotMapKeyValueProfileV1Get", XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,
    "__xl_runtime/xl6_slot_map_key_capture.h", "xl6SlotMapKeyCaptureProfileV1Get")
