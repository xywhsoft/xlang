const char* xl6StdlibTlsResumeRequestSource=
    "class TlsResumeRequest\n    private native point handle\n"
    "    extern \"xl6TlsResumeRequestDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsResumeRequestNameV1\" @pure @nonblocking get serverName() as bytes\n"
    "    extern \"xl6TlsResumeRequestProtocolCountV1\" @pure @nonblocking get protocolCount() as int\n"
    "    extern \"xl6TlsResumeRequestProtocolAtV1\" @pure @nonblocking func protocolAt(int index) as bytes\n"
    "    extern \"xl6TlsResumeRequestTicketV1\" @pure @nonblocking get ticket() as bytes\n"
    "    extern \"xl6TlsResumeRequestAgeV1\" @pure @nonblocking get age() as uint64\n"
    "end\n";
