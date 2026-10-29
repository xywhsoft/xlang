#define UC "xl6UdpConfigDestroyV1"
#define UE .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define UG .pure=1,.noRaise=1,.nonblocking=1
#define UW .parameterOwners={UC},.resultOwner=UC,.nonblocking=1,UE
{"xl6UdpConfigCreateV1",T,V,0,{0},.resultOwner=UC,.nonblocking=1,UE},
{"xl6UdpConfigDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UC},.nativeHandlePolicy="xl6UdpConfigHandlePolicyV1Get",.noRaise=1},
#define XL6_UDP_CONFIG_SIZE(Field,Name) {"xl6UdpConfig" #Field "V1",T,U,1,{V},.parameterOwners={UC},UG},
#define XL6_UDP_CONFIG_UINT(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
#define XL6_UDP_CONFIG_INT(Field,Name) {"xl6UdpConfig" #Field "V1",T,I,1,{V},.parameterOwners={UC},UG},
#define XL6_UDP_CONFIG_BOOL(Field,Name) {"xl6UdpConfig" #Field "V1",T,B,1,{V},.parameterOwners={UC},UG},
#include "src/runtime/xl6_udp_config_fields.h"
#undef XL6_UDP_CONFIG_SIZE
#undef XL6_UDP_CONFIG_UINT
#undef XL6_UDP_CONFIG_INT
#undef XL6_UDP_CONFIG_BOOL
{"xl6UdpConfigReceiveMetadataV1",T,I,1,{V},.parameterOwners={UC},.resultEnum=&xl6IrNetDatagramMetadataEnumContract,UG},
{"xl6UdpConfigOverflowV1",T,I,1,{V},.parameterOwners={UC},.resultEnum=&xl6IrUdpOverflowContract,UG},
{"xl6UdpConfigTruncationV1",T,I,1,{V},.parameterOwners={UC},.resultEnum=&xl6IrUdpTruncationContract,UG},
{"xl6UdpConfigErrorOverflowV1",T,I,1,{V},.parameterOwners={UC},.resultEnum=&xl6IrUdpOverflowContract,UG},
{"xl6UdpConfigPathMtuV1",T,I,1,{V},.parameterOwners={UC},.resultEnum=&xl6IrNetPathMtuModeEnumContract,UG},
{"xl6UdpConfigWithReceiveV1",T,V,4,{V,U,U,U},UW},
{"xl6UdpConfigWithReceiveQueueV1",T,V,4,{V,U,U,I},.parameterEnums={NULL,NULL,NULL,&xl6IrUdpOverflowContract},UW},
{"xl6UdpConfigWithReceiveMetadataV1",T,V,2,{V,I},.parameterEnums={NULL,&xl6IrNetDatagramMetadataEnumContract},UW},
{"xl6UdpConfigWithTruncationV1",T,V,2,{V,I},.parameterEnums={NULL,&xl6IrUdpTruncationContract},UW},
{"xl6UdpConfigWithSendQueueV1",T,V,5,{V,U,U,U,U},UW},
{"xl6UdpConfigWithSendConcurrencyV1",T,V,2,{V,U},UW},
{"xl6UdpConfigWithSocketBuffersV1",T,V,3,{V,I,I},UW},
{"xl6UdpConfigWithTrafficV1",T,V,3,{V,I,I},UW},
{"xl6UdpConfigWithPathMtuV1",T,V,2,{V,I},.parameterEnums={NULL,&xl6IrNetPathMtuModeEnumContract},UW},
{"xl6UdpConfigWithBindingV1",T,V,5,{V,B,B,B,B},UW},
{"xl6UdpConfigWithBroadcastV1",T,V,2,{V,B},UW},
{"xl6UdpConfigWithErrorQueueV1",T,V,6,{V,B,U,U,U,I},.parameterEnums={NULL,NULL,NULL,NULL,NULL,&xl6IrUdpOverflowContract},UW},
#undef UW
#undef UG
#undef UE
#undef UC
