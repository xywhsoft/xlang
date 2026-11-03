const char* xl6StdlibTcpVectorMembersSource=
    "    extern \"xl6TcpStreamSendVectorTextV1\" @nonblocking func sendVector(array<string> data, int size = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamSendVectorBytesV1\" @nonblocking func sendVector(array<bytes> data, int size = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamSendVectorBufferV1\" @nonblocking func sendVector(array<Buffer> data, int size = -1) as NetStatus\n";
#define TD_RECEIVE_MEMBERS(Name,Type) \
    "    extern \"xl6TcpStreamRecvInto" #Name "V1\" func recvInto(" #Type " output, int maxBytes = 65536, int timeoutMillis = -1) as int\n" \
    "    extern \"xl6TcpStreamRecvInto" #Name "AsyncV1\" @nonblocking func recvIntoAsync(" #Type " output, int maxBytes = 65536) as future<int>\n"
const char* xl6StdlibTcpNetBufferMembersSource=
    "    extern \"xl6TcpStreamSendNetBufferV1\" @nonblocking func send(NetBuffer data, int size = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamSendChainMoveV1\" @nonblocking func sendMove(NetBuffer data) as NetStatus\n"
    TD_RECEIVE_MEMBERS(NetBuffer,NetBuffer);
const char* xl6StdlibTcpLineMembersSource=TD_RECEIVE_MEMBERS(Line,NetLineFramer);
const char* xl6StdlibTcpLengthMembersSource=TD_RECEIVE_MEMBERS(Length,NetLengthFramer);
#undef TD_RECEIVE_MEMBERS
const char* xl6StdlibTcpFileMembersSource=
    "    extern \"xl6TcpStreamSendFileV1\" func sendFile(File data, uint64 offset = uint64(0), int size = -1) as NetStatus\n";
