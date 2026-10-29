const char* xl6StdlibNetBackendSource =
    "enum NetBackend as int\n"
    "    automatic = 0\n    iocp = 1\n    ioUring = 2\n    epoll = 3\n    kqueue = 4\n    select = 5\nend\n";
static const char xl6StdNetBackendCompiledSource[] =
    "extern \"xl6NetBackendCompiledV1\" @pure @no_raise @nonblocking func __xl_std_net_backend_compiled(NetBackend backend) as bool\n";
static const char xl6StdNetBackendCapabilitiesSource[] =
    "extern \"xl6NetBackendCapabilitiesV1\" @pure @no_raise @nonblocking func __xl_std_net_backend_capabilities(NetBackend backend) as int\n";
static const char xl6StdNetBackendNameSource[] =
    "extern \"xl6NetBackendNameV1\" func __xl_std_net_backend_name(NetBackend backend) as string\n";
