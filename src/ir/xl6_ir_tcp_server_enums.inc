static const XL6_IrNumericEnumMemberContract xl6IrTcpServerStateMembers[]={
    {"starting",0,0},{"open",1,0},{"closing",2,0},{"closed",3,0}};
static const XL6_IrNumericEnumMemberContract xl6IrTcpServerModeMembers[]={
    {"shared",0,0},{"reusePort",1,0}};
#define XL6_TCP_SERVER_ENUM(Name) static const XL6_IrNumericEnumContract xl6Ir##Name##Contract={ \
    #Name,XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6Ir##Name##Members, \
    sizeof(xl6Ir##Name##Members)/sizeof(xl6Ir##Name##Members[0])};
XL6_TCP_SERVER_ENUM(TcpServerState)
XL6_TCP_SERVER_ENUM(TcpServerMode)
#undef XL6_TCP_SERVER_ENUM
