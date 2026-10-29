const char* xl6StdlibTlsTicketResultSource=
    "class TlsTicketResult\n    private native point handle\n"
    "    extern \"xl6TlsTicketResultDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsTicketResultStatusV1\" @pure @nonblocking get status() as TlsResult\n"
    "    extern \"xl6TlsTicketResultResumeV1\" @pure @nonblocking get resume() as TlsResume?\n"
    "end\n";
