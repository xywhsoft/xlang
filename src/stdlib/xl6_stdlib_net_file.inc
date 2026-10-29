const char* xl6StdlibNetFileClassSource=
    "class NetFile\n    private native point handle\n"
    "    extern \"xl6NetFileCreateV1\" create(NetEngine engine, string path, int flags = 1, int mode = 438, int share = 7, int worker = 0)\n"
    "    extern \"xl6NetFileDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetFileReadV1\" @nonblocking func readAt(uint64 offset, int size) as future<bytes>\n"
    "    extern \"xl6NetFileReadCancelV1\" @nonblocking func readAtCancel(uint64 offset, int size, CancellationToken cancel) as future<bytes>\n"
    "    extern \"xl6NetFileWriteBytesV1\" @nonblocking func writeAt(uint64 offset, bytes data, int size = -1) as future<int>\n"
    "    extern \"xl6NetFileWriteTextV1\" @nonblocking func writeAt(uint64 offset, string data, int size = -1) as future<int>\n"
    "    extern \"xl6NetFileWriteBytesCancelV1\" @nonblocking func writeAtCancel(uint64 offset, bytes data, CancellationToken cancel, int size = -1) as future<int>\n"
    "    extern \"xl6NetFileWriteTextCancelV1\" @nonblocking func writeAtCancel(uint64 offset, string data, CancellationToken cancel, int size = -1) as future<int>\n"
    "    extern \"xl6NetFileWriteBufferV1\" @nonblocking func writeBufferAt(uint64 offset, Buffer data, int size = -1) as future<int>\n"
    "    extern \"xl6NetFileWriteBufferCancelV1\" @nonblocking func writeBufferAtCancel(uint64 offset, Buffer data, CancellationToken cancel, int size = -1) as future<int>\n"
    "    extern \"xl6NetFileCloseV1\" @nonblocking func close() as future<null>\n"
    "end\n";
