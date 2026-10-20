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
static const XL6_IrNumericEnumMemberContract xl6IrNetDatagramMetadataMembers[]={
    {"none",0,0},{"destination",1,0},{"interfaceIndex",2,0},{"hopLimit",4,0},
    {"trafficClass",8,0},{"segmentSize",16,0},{"truncated",1073741824,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetDatagramControlFlagsMembers[]={
    {"none",0,0},{"source",1,0},{"interfaceIndex",2,0},{"hopLimit",4,0},{"trafficClass",8,0},{"segmentSize",16,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetDatagramCapabilitiesMembers[]={
    {"none",0,0},{"pathMtuMode",1,0},{"pathMtuQuery",2,0},{"errorQueue",4,0},{"segmentSend",8,0},{"segmentReceive",16,0}
};
XL6_SOCKET_ENUM(NetDatagramMetadata)
XL6_SOCKET_ENUM(NetDatagramControlFlags)
XL6_SOCKET_ENUM(NetDatagramCapabilities)
static const XL6_IrNumericEnumMemberContract xl6IrNetPathMtuModeMembers[]={
    {"system",0,0},{"discover",1,0},{"fragment",2,0},{"probe",3,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetDatagramErrorOriginMembers[]={
    {"unknown",0,0},{"local",1,0},{"icmp",2,0},{"icmp6",3,0}
};
static const XL6_IrNumericEnumMemberContract xl6IrNetDatagramErrorFlagsMembers[]={
    {"none",0,0},{"remote",1,0},{"offender",2,0},{"pathMtu",4,0},{"payloadTruncated",8,0},{"metadataTruncated",16,0}
};
XL6_SOCKET_ENUM(NetPathMtuMode)
XL6_SOCKET_ENUM(NetDatagramErrorOrigin)
XL6_SOCKET_ENUM(NetDatagramErrorFlags)
#undef XL6_SOCKET_ENUM
