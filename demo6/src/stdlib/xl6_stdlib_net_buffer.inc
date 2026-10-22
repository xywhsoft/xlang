const char* xl6StdlibNetBufferSource=
    "class NetBuffer\n    private native point handle\n"
    "    extern \"xl6NetBufferCreateV1\" @nonblocking create(int maxBytes = 67108864)\n"
    "    extern \"xl6NetBufferDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetBufferLengthV1\" @pure @nonblocking get length() as int\n"
    "    extern \"xl6NetBufferSpanCountV1\" @pure @nonblocking get spanCount() as int\n"
    "    extern \"xl6NetBufferLimitV1\" @pure @no_raise @nonblocking get maxBytes() as int\n"
    "    extern \"xl6NetBufferClearV1\" @nonblocking func clear()\n"
    "    extern \"xl6NetBufferCompactV1\" @nonblocking func compact(int size = -1)\n"
    "    extern \"xl6NetBufferAppendMoveV1\" @nonblocking func appendMove(NetBuffer source)\n"
    "    extern \"xl6NetBufferConsumeV1\" @nonblocking func consume(int size = -1) as int\n"
    "    extern \"xl6NetBufferFindV1\" @pure @nonblocking func find(int value, int offset = 0) as int\n"
    "    extern \"xl6NetBufferPeekV1\" @nonblocking func peek(int offset = 0, int size = -1) as bytes\n"
    "    extern \"xl6NetBufferReadV1\" @nonblocking func read(int size = -1) as bytes\n"
    "    extern \"xl6NetBufferPeekIntoV1\" @nonblocking func peekInto(Buffer output, int offset = 0, int size = -1) as int\n"
    "    extern \"xl6NetBufferReadIntoV1\" @nonblocking func readInto(Buffer output, int size = -1) as int\n"
#define XL6_NET_BUFFER_FEED(Verb,Name) \
    "    extern \"xl6NetBuffer" #Verb "TextV1\" @nonblocking func " #Name "(string input, int size = -1)\n" \
    "    extern \"xl6NetBuffer" #Verb "BytesV1\" @nonblocking func " #Name "(bytes input, int size = -1)\n" \
    "    extern \"xl6NetBuffer" #Verb "BufferV1\" @nonblocking func " #Name "(Buffer input, int size = -1)\n"
XL6_NET_BUFFER_FEED(Append,append)
XL6_NET_BUFFER_FEED(Prepend,prepend)
#undef XL6_NET_BUFFER_FEED
    "";
const char* xl6StdlibNetBufferRefMembersSource=
    "    extern \"xl6NetBufferAppendRefV1\" @nonblocking func appendRef(NetBytes data, int size = -1, int offset = 0)\n";
const char* xl6StdlibNetFrameResultSource=
    "enum NetFrameStatus as int\n    more = 0\n    ready = 1\nend\n"
    "class NetFrameResult\n    private native point handle\n"
    "    extern \"xl6NetFrameResultDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetFrameResultStatusV1\" @pure @no_raise @nonblocking get status() as NetFrameStatus\n"
    "    extern \"xl6NetFrameResultPayloadOffsetV1\" @pure @nonblocking get payloadOffset() as int\n"
    "    extern \"xl6NetFrameResultPayloadSizeV1\" @pure @nonblocking get payloadSize() as int\n"
    "    extern \"xl6NetFrameResultFrameSizeV1\" @pure @nonblocking get frameSize() as int\n"
    "    extern \"xl6NetFrameResultDeclaredV1\" @pure @nonblocking get declared() as uint64\n"
    "    extern \"xl6NetFrameResultBytesV1\" @nonblocking func bytes() as bytes\n"
    "    extern \"xl6NetFrameResultTextV1\" @nonblocking func text() as string\n"
    "    extern \"xl6NetFrameResultCopyIntoV1\" @nonblocking func copyInto(Buffer output)\n"
    "    get ready() as bool\n        return self.status == NetFrameStatus.ready\n    end\n"
    "    get more() as bool\n        return self.status == NetFrameStatus.more\n    end\nend\n";
#define XL6_NET_FRAMER_METHODS(Name) \
    "    extern \"xl6Net" #Name "FramerDestroyV1\" @no_raise destroy()\n" \
    "    extern \"xl6Net" #Name "FramerResetV1\" @nonblocking func reset()\n" \
    "    extern \"xl6Net" #Name "FramerClearV1\" @nonblocking func clear()\n" \
    "    extern \"xl6Net" #Name "FramerBufferedV1\" @pure @nonblocking get buffered() as int\n" \
    "    extern \"xl6Net" #Name "FramerLimitV1\" @pure @no_raise @nonblocking get maxBufferedBytes() as int\n" \
    "    extern \"xl6Net" #Name "FramerFailedV1\" @pure @nonblocking get failed() as bool\n" \
    "    private extern \"xl6Net" #Name "FramerNextV1\" @nonblocking func peekNative() as NetFrameResult\n" \
    "    private extern \"xl6Net" #Name "FramerCommitV1\" @nonblocking func commitNative(NetFrameResult result)\n" \
    "    func next(bool consume = true) as NetFrameResult\n" \
    "        auto result = self.peekNative()\n" \
    "        if consume and result.ready then self.commitNative(result) end\n" \
    "        return result\n    end\n" \
    "    extern \"xl6Net" #Name "FramerNextIntoV1\" @nonblocking func nextInto(Buffer output, bool consume = true) as bool\n" \
    "    extern \"xl6Net" #Name "FramerAppendTextV1\" @nonblocking func append(string input, int size = -1)\n" \
    "    extern \"xl6Net" #Name "FramerAppendBytesV1\" @nonblocking func append(bytes input, int size = -1)\n" \
    "    extern \"xl6Net" #Name "FramerAppendBufferV1\" @nonblocking func append(Buffer input, int size = -1)\n" \
    "    extern \"xl6Net" #Name "FramerAppendMoveV1\" @nonblocking func appendMove(NetBuffer input)\n"
const char* xl6StdlibNetLineFramerSource=
    "class NetLineFramer\n    private native point handle\n"
    "    extern \"xl6NetLineFramerCreateTextV1\" @nonblocking create(string delimiter = \"\\n\", int maxPayload = 8192, bool includeDelimiter = false, int maxBufferedBytes = 67108864, int size = -1)\n"
    "    extern \"xl6NetLineFramerCreateBytesV1\" @nonblocking create(bytes delimiter, int maxPayload = 8192, bool includeDelimiter = false, int maxBufferedBytes = 67108864, int size = -1)\n"
    XL6_NET_FRAMER_METHODS(Line)
    "end\n";
const char* xl6StdlibNetLengthFramerSource=
    "class NetLengthFramer\n    private native point handle\n"
    "    extern \"xl6NetLengthFramerCreateV1\" @nonblocking create(int lengthOffset = 0, int lengthSize = 4, int adjustment = 0, int strip = 4, int maxFrame = 1048576, bool littleEndian = false, int maxBufferedBytes = 67108864)\n"
    XL6_NET_FRAMER_METHODS(Length)
    "end\n";
#undef XL6_NET_FRAMER_METHODS
