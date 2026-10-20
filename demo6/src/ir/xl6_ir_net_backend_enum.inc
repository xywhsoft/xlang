/* Closed declarative vocabulary shared by input and result contracts. No
 * integer/enum interchange or source-name based Cgen behavior is authorized. */
static const XL6_IrNumericEnumMemberContract xl6IrNetBackendMembers[]={
    {"automatic",0,0},{"iocp",1,0},{"ioUring",2,0},{"epoll",3,0},{"kqueue",4,0},{"select",5,0}
};
static const XL6_IrNumericEnumContract xl6IrNetBackendEnumContract={
    "NetBackend",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,
    xl6IrNetBackendMembers,sizeof(xl6IrNetBackendMembers)/sizeof(xl6IrNetBackendMembers[0])
};
