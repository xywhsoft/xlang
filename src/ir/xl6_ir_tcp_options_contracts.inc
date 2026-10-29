#define TS "xl6TcpStreamOptionsDestroyV1"
#define TD "xl6TcpDialOptionsDestroyV1"
#define TL "xl6TcpListenOptionsDestroyV1"
#define TA "xl6NetAddressDestroyV1"
#define TF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TG .pure=1,.noRaise=1,.nonblocking=1
{"xl6TcpStreamOptionsCreateV1",T,V,0,{0},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpStreamOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TS},.nativeHandlePolicy="xl6TcpStreamOptionsHandlePolicyV1Get",.noRaise=1},
#define XL6_TCP_STREAM_SIZE(Field,Name) {"xl6TcpStreamOptions" #Field "V1",T,U,1,{V},.parameterOwners={TS},TG},
#define XL6_TCP_STREAM_BOOL(Field,Name) {"xl6TcpStreamOptions" #Field "V1",T,B,1,{V},.parameterOwners={TS},TG},
#define XL6_TCP_LISTEN_INT(Field,Name) {"xl6TcpListenOptions" #Field "V1",T,I,1,{V},.parameterOwners={TL},TG},
#define XL6_TCP_LISTEN_BOOL(Field,Name) {"xl6TcpListenOptions" #Field "V1",T,B,1,{V},.parameterOwners={TL},TG},
#include "src/runtime/xl6_tcp_options_fields.inc"
#undef XL6_TCP_STREAM_SIZE
#undef XL6_TCP_STREAM_BOOL
#undef XL6_TCP_LISTEN_INT
#undef XL6_TCP_LISTEN_BOOL
{"xl6TcpStreamOptionsConnectTimeoutV1",T,I,1,{V},.parameterOwners={TS},TG},
{"xl6TcpStreamOptionsReadModeV1",T,I,1,{V},.parameterOwners={TS},.resultEnum=&xl6IrTcpReadModeContract,TG},
{"xl6TcpStreamOptionsWithReadBufferV1",T,V,3,{V,U,U},.parameterOwners={TS},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpStreamOptionsWithWriteWatermarksV1",T,V,4,{V,U,U,U},.parameterOwners={TS},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpStreamOptionsWithConnectTimeoutV1",T,V,2,{V,I},.parameterOwners={TS},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpStreamOptionsWithReadModeV1",T,V,2,{V,I},.parameterOwners={TS},.parameterEnums={NULL,&xl6IrTcpReadModeContract},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpStreamOptionsWithNoDelayV1",T,V,2,{V,B},.parameterOwners={TS},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpStreamOptionsWithKeepAliveV1",T,V,2,{V,B},.parameterOwners={TS},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpDialOptionsCreateV1",T,V,0,{0},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpDialOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TD},.nativeHandlePolicy="xl6TcpDialOptionsHandlePolicyV1Get",.noRaise=1},
{"xl6TcpDialOptionsStreamV1",T,V,1,{V},.parameterOwners={TD},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpDialOptionsWithStreamV1",T,V,2,{V,V},.parameterOwners={TD,TS},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpDialOptionsFamilyV1",T,I,1,{V},.parameterOwners={TD},TG},
{"xl6TcpDialOptionsAffinityV1",T,U,1,{V},.parameterOwners={TD},TG},
{"xl6TcpDialOptionsTimeoutV1",T,I,1,{V},.parameterOwners={TD},TG},
{"xl6TcpDialOptionsFallbackDelayV1",T,I,1,{V},.parameterOwners={TD},TG},
{"xl6TcpDialOptionsMaxAttemptsV1",T,I,1,{V},.parameterOwners={TD},TG},
{"xl6TcpDialOptionsWithFamilyV1",T,V,2,{V,I},.parameterOwners={TD},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpDialOptionsWithAffinityV1",T,V,2,{V,U},.parameterOwners={TD},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpDialOptionsWithTimeoutV1",T,V,2,{V,I},.parameterOwners={TD},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpDialOptionsWithFallbackDelayV1",T,V,2,{V,I},.parameterOwners={TD},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpDialOptionsWithMaxAttemptsV1",T,V,2,{V,I},.parameterOwners={TD},.resultOwner=TD,.nonblocking=1,TF},
{"xl6TcpListenOptionsCreateV1",T,V,0,{0},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsCreateAddressV1",T,V,1,{V},.parameterOwners={TA},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TL},.nativeHandlePolicy="xl6TcpListenOptionsHandlePolicyV1Get",.noRaise=1},
{"xl6TcpListenOptionsAddressV1",T,V,1,{V},.parameterOwners={TL},.resultOwner=TA,.nonblocking=1,TF},
{"xl6TcpListenOptionsWithAddressV1",T,V,2,{V,V},.parameterOwners={TL,TA},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsStreamV1",T,V,1,{V},.parameterOwners={TL},.resultOwner=TS,.nonblocking=1,TF},
{"xl6TcpListenOptionsWithStreamV1",T,V,2,{V,V},.parameterOwners={TL,TS},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsAffinityV1",T,U,1,{V},.parameterOwners={TL},TG},
{"xl6TcpListenOptionsDistributionV1",T,I,1,{V},.parameterOwners={TL},.resultEnum=&xl6IrTcpAcceptDistributionContract,TG},
{"xl6TcpListenOptionsWithAffinityV1",T,V,2,{V,U},.parameterOwners={TL},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsWithAcceptQueueV1",T,V,3,{V,I,I},.parameterOwners={TL},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsWithBacklogV1",T,V,2,{V,I},.parameterOwners={TL},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsWithDistributionV1",T,V,2,{V,I},.parameterOwners={TL},.parameterEnums={NULL,&xl6IrTcpAcceptDistributionContract},.resultOwner=TL,.nonblocking=1,TF},
{"xl6TcpListenOptionsWithBindingV1",T,V,5,{V,B,B,B,B},.parameterOwners={TL},.resultOwner=TL,.nonblocking=1,TF},
#undef TG
#undef TF
#undef TA
#undef TL
#undef TD
#undef TS
