static const XL6_IrNumericEnumMemberContract xl6IrUdpOverflowMembers[]={
    {"dropNewest",0,0},{"dropOldest",1,0},{"error",2,0}};
static const XL6_IrNumericEnumContract xl6IrUdpOverflowContract={"UdpOverflow",XL6_IR_C_ABI_I64,
    XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrUdpOverflowMembers,3};
static const XL6_IrNumericEnumMemberContract xl6IrUdpTruncationMembers[]={
    {"deliver",0,0},{"drop",1,0},{"error",2,0}};
static const XL6_IrNumericEnumContract xl6IrUdpTruncationContract={"UdpTruncation",XL6_IR_C_ABI_I64,
    XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrUdpTruncationMembers,3};
