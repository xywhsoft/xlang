static const XL6_IrNumericEnumMemberContract xl6IrTcpStreamStateMembers[]={
    {"connecting",0,0},{"open",1,0},{"closing",2,0},{"closed",3,0}};
static const XL6_IrNumericEnumMemberContract xl6IrTcpListenerStateMembers[]={
    {"open",0,0},{"closing",1,0},{"closed",2,0}};
static const XL6_IrNumericEnumMemberContract xl6IrTcpWaitMembers[]={
    {"open",0,0},{"readable",1,0},{"writable",2,0},{"drain",3,0},{"close",4,0}};
#define XL6_TCP_ENUM(Name) static const XL6_IrNumericEnumContract xl6Ir##Name##Contract={ \
    #Name,XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6Ir##Name##Members, \
    sizeof(xl6Ir##Name##Members)/sizeof(xl6Ir##Name##Members[0])};
XL6_TCP_ENUM(TcpStreamState)
XL6_TCP_ENUM(TcpListenerState)
XL6_TCP_ENUM(TcpWait)
#undef XL6_TCP_ENUM
