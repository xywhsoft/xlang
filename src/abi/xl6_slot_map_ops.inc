/* id, source method, source argument count, first/second argument role,
 * result role, native symbol, physical failure ABI. Shared by Sem/XLIR/Cgen.
 * Roles: NONE, KEY, ITEM, INT; results OWNER, KEY, ITEM, BOOL, INT,
 * ARRAY_KEY, ARRAY_ITEM. Snapshot result layouts are verified by XLIR.
 * Boolean language results are never reused as checked success flags. */
XL6_SLOT_MAP_OP(CREATE, "",         0, NONE, NONE, OWNER, xl6SlotMapCreateTakeV1, NULL_RESULT)
XL6_SLOT_MAP_OP(INSERT, "insert",   1, ITEM, NONE, KEY,   xl6SlotMapInsertV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(GET, "get",         1, KEY,  NONE, ITEM,  xl6SlotMapReadRequiredV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(SET, "set",         2, KEY,  ITEM, BOOL,  xl6SlotMapSetV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(CONTAINS, "contains",1, KEY, NONE, BOOL,  xl6SlotMapKeyV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(REMOVE, "remove",   1, KEY,  NONE, BOOL,  xl6SlotMapKeyV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(RESERVE, "reserve", 1, INT,  NONE, BOOL,  xl6SlotMapReserveV1, FALSE_RESULT)
XL6_SLOT_MAP_OP(CLEAR, "clear",     0, NONE, NONE, BOOL,  xl6SlotMapClearV1, FALSE_RESULT)
XL6_SLOT_MAP_OP(COUNT, "count",     0, NONE, NONE, INT,   xl6SlotMapSizeV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(CAPACITY, "capacity",0, NONE, NONE, INT,   xl6SlotMapSizeV1, STATUS_OUTPUT)
XL6_SLOT_MAP_OP(KEYS, "keys",       0, NONE, NONE, ARRAY_KEY, xl6SlotMapKeysV1, NULL_RESULT)
XL6_SLOT_MAP_OP(VALUES, "values",   0, NONE, NONE, ARRAY_ITEM,xl6SlotMapValuesV1, NULL_RESULT)
