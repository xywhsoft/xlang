static const XL6_IrNumericEnumMemberContract xl6IrNetStatusMembers[]={
    {"failed",1,1},{"ok",0,0},{"again",1,0},{"closed",2,0},{"truncated",3,0},{"timeout",4,0},{"cancelled",5,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetSocketKindMembers[]={
    {"stream",1,0},{"datagram",2,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetShutdownMembers[]={
    {"read",1,0},{"write",2,0},{"both",3,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetSocketOptionMembers[]={
#define XL6_NET_SOCKET_OPTION(Name,Value) {#Name,Value,0},
#include "src/runtime/xl6_net_socket_options.inc"
#undef XL6_NET_SOCKET_OPTION
};
#define XL6_SOCKET_ENUM(Name) static const XL6_IrNumericEnumContract xl6Ir##Name##EnumContract={ \
    #Name,XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6Ir##Name##Members, \
    sizeof(xl6Ir##Name##Members)/sizeof(xl6Ir##Name##Members[0])};
XL6_SOCKET_ENUM(NetStatus)
XL6_SOCKET_ENUM(NetSocketKind)
XL6_SOCKET_ENUM(NetShutdown)
XL6_SOCKET_ENUM(NetSocketOption)
#undef XL6_SOCKET_ENUM
