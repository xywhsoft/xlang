#define UR "xl6UdpDestroyV1"
#define UA "xl6NetAddressDestroyV1"
#define UC "xl6NetDatagramControlDestroyV1"
#define UP "xl6UdpPacketDestroyV1"
#define UX "xl6UdpErrorPacketDestroyV1"
#define UB "xl6UdpBatchDestroyV1"
#define US "xl6UdpStatsDestroyV1"
#define UE .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define UN .nonblocking=1,UE
#define UG .pure=1,.noRaise=1,.nonblocking=1
#define UT .resultEnum=&xl6IrNetStatusEnumContract
#define UQ .nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1
{"xl6UdpBindDefaultV1",T,V,2,{V,V},.parameterOwners={"xl6NetEngineDestroyV1",UA},.resultOwner=UR,UQ,UE},
{"xl6UdpBindV1",T,V,4,{V,V,V,U},.parameterOwners={"xl6NetEngineDestroyV1",UA,"xl6UdpConfigDestroyV1"},.resultOwner=UR,UQ,UE},
{"xl6UdpConnectDefaultV1",T,V,2,{V,V},.parameterOwners={"xl6NetEngineDestroyV1",UA},.resultOwner=UR,UQ,UE},
{"xl6UdpConnectV1",T,V,4,{V,V,V,U},.parameterOwners={"xl6NetEngineDestroyV1",UA,"xl6UdpConfigDestroyV1"},.resultOwner=UR,UQ,UE},
{"xl6UdpOpenV1",T,V,5,{V,V,V,V,U},.parameterOwners={"xl6NetEngineDestroyV1",UA,UA,"xl6UdpConfigDestroyV1"},.resultOwner=UR,UQ,UE},
{"xl6UdpDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UR},.nativeHandlePolicy="xl6UdpHandlePolicyV1Get",.noRaise=1},
{"xl6UdpCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UR},UN},
{"xl6UdpAbortV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UR},UN},
{"xl6UdpStateV1",T,I,1,{V},.parameterOwners={UR},.resultEnum=&xl6IrUdpStateContract,UN},
{"xl6UdpConnectedV1",T,B,1,{V},.parameterOwners={UR},UN},
{"xl6UdpSendControlAvailableV1",T,I,1,{V},.parameterOwners={UR},.resultEnum=&xl6IrNetDatagramControlFlagsEnumContract,UN},
{"xl6UdpLocalV1",T,V,1,{V},.parameterOwners={UR},.resultOwner=UA,UN},
{"xl6UdpPeerV1",T,V,1,{V},.parameterOwners={UR},.resultOwner=UA,UN},
{"xl6UdpTryReceiveV1",T,V,1,{V},.parameterOwners={UR},.resultOwner=UP,.nullableResult=1,UN},
{"xl6UdpTryReceiveErrorV1",T,V,1,{V},.parameterOwners={UR},.resultOwner=UX,.nullableResult=1,UN},
{"xl6UdpPacketDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UP},.nativeHandlePolicy="xl6UdpPacketHandlePolicyV1Get",.noRaise=1},
{"xl6UdpPacketLengthV1",T,U,1,{V},.parameterOwners={UP},UG},
{"xl6UdpPacketTruncatedV1",T,B,1,{V},.parameterOwners={UP},UG},
{"xl6UdpPacketBytesV1",T,Y,3,{V,I,I},.parameterOwners={UP},UN},
{"xl6UdpPacketRemoteV1",T,V,1,{V},.parameterOwners={UP},.resultOwner=UA,UN},
{"xl6UdpPacketInfoV1",T,V,1,{V},.parameterOwners={UP},.resultOwner="xl6NetDatagramInfoDestroyV1",UN},
{"xl6UdpErrorPacketDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UX},.nativeHandlePolicy="xl6UdpErrorPacketHandlePolicyV1Get",.noRaise=1},
{"xl6UdpErrorPacketLengthV1",T,U,1,{V},.parameterOwners={UX},UG},
{"xl6UdpErrorPacketBytesV1",T,Y,3,{V,I,I},.parameterOwners={UX},UN},
{"xl6UdpErrorPacketInfoV1",T,V,1,{V},.parameterOwners={UX},.resultOwner="xl6NetDatagramErrorInfoDestroyV1",UN},
{"xl6UdpBatchDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={UB},.nativeHandlePolicy="xl6UdpBatchHandlePolicyV1Get",.noRaise=1},
{"xl6UdpBatchCountV1",T,U,1,{V},.parameterOwners={UB},.noRaise=1,.nonblocking=1},
{"xl6UdpBatchTakeV1",T,V,2,{V,I},.parameterOwners={UB},.resultOwner=UP,.nullableResult=1,UN},
{"xl6UdpSnapshotV1",T,V,1,{V},.parameterOwners={UR},.resultOwner=US,UN},
{"xl6UdpStatsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={US},.nativeHandlePolicy="xl6UdpStatsHandlePolicyV1Get",.noRaise=1},
{"xl6UdpStatsStateV1",T,I,1,{V},.parameterOwners={US},.resultEnum=&xl6IrUdpStateContract,UG},
{"xl6UdpStatsConnectedV1",T,B,1,{V},.parameterOwners={US},UG},
#define XL6_UDP_STAT(Field,Name) {"xl6UdpStats" #Field "V1",T,U,1,{V},.parameterOwners={US},UG},
#include "src/runtime/xl6_udp_stats_fields.h"
#undef XL6_UDP_STAT
#define SEND(Name,Type,Owner) \
{"xl6UdpSend" #Name "V1",T,I,3,{V,Type,I},.parameterOwners={UR,Owner},UT,UN}, \
{"xl6UdpSendTo" #Name "V1",T,I,4,{V,V,Type,I},.parameterOwners={UR,UA,Owner},UT,UN}, \
{"xl6UdpSendMessage" #Name "V1",T,I,5,{V,V,V,Type,I},.parameterOwners={UR,UA,UC,Owner},UT,UN}, \
{"xl6UdpSendConnectedMessage" #Name "V1",T,I,4,{V,V,Type,I},.parameterOwners={UR,UC,Owner},UT,UN},
SEND(Text,S,NULL)
SEND(Bytes,Y,NULL)
SEND(Buffer,V,"xl6BufferDestroyV1")
#undef SEND
#define REF(Name,Owner) \
{"xl6UdpSendRef" #Name "V1",T,I,4,{V,V,I,I},.parameterOwners={UR,Owner},UT,UN}, \
{"xl6UdpSendRefTo" #Name "V1",T,I,5,{V,V,V,I,I},.parameterOwners={UR,UA,Owner},UT,UN}, \
{"xl6UdpSendRefMessage" #Name "V1",T,I,6,{V,V,V,V,I,I},.parameterOwners={UR,UA,UC,Owner},UT,UN}, \
{"xl6UdpSendRefConnectedMessage" #Name "V1",T,I,5,{V,V,V,I,I},.parameterOwners={UR,UC,Owner},UT,UN},
REF(Bytes,"xl6NetBytesDestroyV1")
REF(Packet,UP)
#undef REF
{"xl6UdpSendMoveV1",T,I,2,{V,V},.parameterOwners={UR,"xl6BufferDestroyV1"},UT,UN},
{"xl6UdpSendMoveToV1",T,I,3,{V,V,V},.parameterOwners={UR,UA,"xl6BufferDestroyV1"},UT,UN},
{"xl6UdpSendMoveMessageV1",T,I,4,{V,V,V,V},.parameterOwners={UR,UA,UC,"xl6BufferDestroyV1"},UT,UN},
{"xl6UdpSendMoveConnectedMessageV1",T,I,3,{V,V,V},.parameterOwners={UR,UC,"xl6BufferDestroyV1"},UT,UN},
#define WAIT(Name,Enum) \
{"xl6Udp" #Name "V1",T,I,3,{V,I,I},.parameterOwners={UR},.parameterEnums={NULL,Enum},UT,UE}, \
{"xl6Udp" #Name "CancelV1",T,I,4,{V,V,I,I},.parameterOwners={UR,"xl6CancellationTokenDestroyV1"},.parameterEnums={NULL,NULL,Enum},UT,UE}, \
{"xl6Udp" #Name "UntilV1",T,I,3,{V,U,I},.parameterOwners={UR},.parameterEnums={NULL,NULL,Enum},UT,UE}, \
{"xl6Udp" #Name "UntilCancelV1",T,I,4,{V,U,V,I},.parameterOwners={UR,NULL,"xl6CancellationTokenDestroyV1"},.parameterEnums={NULL,NULL,NULL,Enum},UT,UE},
WAIT(Wait,&xl6IrUdpWaitContract)
WAIT(WaitWritable,NULL)
#undef WAIT
#define RECEIVE(Name,Owner) \
{"xl6Udp" #Name "V1",T,V,2,{V,I},.parameterOwners={UR},.resultOwner=Owner,UE}, \
{"xl6Udp" #Name "CancelV1",T,V,3,{V,V,I},.parameterOwners={UR,"xl6CancellationTokenDestroyV1"},.resultOwner=Owner,UE}, \
{"xl6Udp" #Name "UntilV1",T,V,2,{V,U},.parameterOwners={UR},.resultOwner=Owner,UE}, \
{"xl6Udp" #Name "UntilCancelV1",T,V,3,{V,U,V},.parameterOwners={UR,NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=Owner,UE},
RECEIVE(Receive,UP)
RECEIVE(ReceiveError,UX)
#undef RECEIVE
{"xl6UdpReceiveBatchV1",T,V,3,{V,I,I},.parameterOwners={UR},.resultOwner=UB,UE},
{"xl6UdpReceiveBatchCancelV1",T,V,4,{V,V,I,I},.parameterOwners={UR,"xl6CancellationTokenDestroyV1"},.resultOwner=UB,UE},
{"xl6UdpReceiveBatchUntilV1",T,V,3,{V,U,I},.parameterOwners={UR},.resultOwner=UB,UE},
{"xl6UdpReceiveBatchUntilCancelV1",T,V,4,{V,U,V,I},.parameterOwners={UR,NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=UB,UE},
#undef UQ
#undef UT
#undef UG
#undef UN
#undef UE
#undef US
#undef UB
#undef UX
#undef UP
#undef UC
#undef UA
#undef UR
