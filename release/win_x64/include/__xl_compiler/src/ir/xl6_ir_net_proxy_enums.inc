static const XL6_IrNumericEnumMemberContract xl6IrNetProxyTypeMembers[]={{"socks5",1,0},{"httpConnect",2,0}};
static const XL6_IrNumericEnumContract xl6IrNetProxyTypeContract={"NetProxyType",XL6_IR_C_ABI_I64,
    XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrNetProxyTypeMembers,2};
static const XL6_IrNumericEnumMemberContract xl6IrNetProxyAuthMembers[]={{"automatic",0,0},{"none",1,0},{"required",2,0},{"optional",3,0}};
static const XL6_IrNumericEnumContract xl6IrNetProxyAuthContract={"NetProxyAuth",XL6_IR_C_ABI_I64,
    XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrNetProxyAuthMembers,4};
static const XL6_IrNumericEnumMemberContract xl6IrNetProxyHandshakeStateMembers[]={{"write",1,0},{"read",2,0},{"ready",3,0},{"failed",4,0}};
static const XL6_IrNumericEnumContract xl6IrNetProxyHandshakeStateContract={"NetProxyHandshakeState",XL6_IR_C_ABI_I64,
    XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrNetProxyHandshakeStateMembers,4};
