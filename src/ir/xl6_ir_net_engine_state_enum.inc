static const XL6_IrNumericEnumMemberContract xl6IrNetEngineStateMembers[]={
    {"stopped",0,0},{"starting",1,0},{"running",2,0},{"stopping",3,0},{"retiring",4,0}
};
static const XL6_IrNumericEnumContract xl6IrNetEngineStateEnumContract={
    "NetEngineState",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,
    xl6IrNetEngineStateMembers,sizeof(xl6IrNetEngineStateMembers)/sizeof(xl6IrNetEngineStateMembers[0])
};
