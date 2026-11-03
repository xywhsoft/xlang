#define XL6_NET_VECTOR_OVERLOADS(Type,Name) \
    "    extern \"xl6NetSocketSendVector" #Name "V1\" func sendVector(array<" #Type "> data, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendVectorTo" #Name "V1\" func sendVectorTo(array<" #Type "> data, NetAddress target, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendMessageVector" #Name "V1\" func sendMessageVector(array<" #Type "> data, NetAddress target, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendConnectedMessageVector" #Name "V1\" func sendMessageVector(array<" #Type "> data, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n"
#define XL6_NET_SOCKET_VECTOR_METHODS \
    XL6_NET_VECTOR_OVERLOADS(string,Text) \
    XL6_NET_VECTOR_OVERLOADS(bytes,Bytes) \
    XL6_NET_VECTOR_OVERLOADS(Buffer,Buffer) \
    "    extern \"xl6NetSocketReceiveVectorIntoV1\" func receiveVectorInto(array<Buffer> output, int maxBytesPerBuffer = 65536) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketReceiveFromVectorIntoV1\" func receiveFromVectorInto(array<Buffer> output, NetDatagramInfo info, int maxBytesPerBuffer = 65536) as (NetStatus, uint64)\n"
