const char* xl6StdlibNetBytesClassSource=
    "class NetBytes\n    private native point handle\n"
    "    extern \"xl6NetBytesCreateV1\" @nonblocking create()\n"
    "    extern \"xl6NetBytesFromTextV1\" @nonblocking create(string input, int size = -1)\n"
    "    extern \"xl6NetBytesFromBytesV1\" @nonblocking create(bytes input, int size = -1)\n"
    "    extern \"xl6NetBytesFromBufferV1\" @nonblocking create(Buffer input, int size = -1)\n"
    "    extern \"xl6NetBytesDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetBytesLengthV1\" @pure @no_raise @nonblocking get length() as int\n"
    "    extern \"xl6NetBytesByteAtV1\" @pure @nonblocking func byteAt(int index) as int\n"
    "    extern \"xl6NetBytesToBytesV1\" @nonblocking func toBytes(int size = -1, int offset = 0) as bytes\n"
    "    extern \"xl6NetBytesAppendToV1\" @nonblocking func appendTo(Buffer output, int size = -1, int offset = 0) as int\n"
    "end\n";
/* Temporary producer-side bridge for the old TLS receive implementation.
 * Not a public constructor or borrowed data escape; removed with that
 * producer's P6 migration. The owning raw return is adopted exactly once. */
const char* xl6StdlibNetBytesAdoptSource=
    "extern \"xl6NetBytesAdoptV1\" @nonblocking func __xl_std_runtime_take_net_bytes(point owned) as NetBytes\n";
const char* xl6StdlibTcpSendRefMembersSource=
    "    extern \"xl6TcpStreamSendRefV1\" @nonblocking func sendRef(NetBytes data, int size = -1, int offset = 0) as NetStatus\n";
const char* xl6StdlibTcpSendMoveMembersSource=
    "    extern \"xl6TcpStreamSendMoveV1\" @nonblocking func sendMove(Buffer data) as NetStatus\n";
