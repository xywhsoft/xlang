const char* xl6StdlibOnceSource =
    "class Once\n"
    "    private native point handle\n"
    "    extern \"xl6OnceCreateV1\" @nonblocking create()\n"
    "    extern \"xl6OnceDestroyV1\" destroy()\n"
    "    extern \"xl6OnceRunV1\" func run(func() initializer)\n"
    "    extern \"xl6OnceTryRunV1\" func tryRun(func() as bool initializer) as bool\n"
    "end\n";
const char* xl6StdlibEventSource =
    "class Event\n"
    "    private native point handle\n"
    "    extern \"xl6EventCreateV1\" create(bool manualReset = false, bool signaled = false)\n"
    "    extern \"xl6EventDestroyV1\" destroy()\n"
    "    extern \"xl6EventSetV1\" func set()\n"
    "    extern \"xl6EventResetV1\" func reset()\n"
    "    extern \"xl6EventWaitV1\" func wait(int timeoutMillis = -1) as bool\n"
    "    extern \"xl6EventTryWaitV1\" @nonblocking func tryWait() as bool\n"
    "end\n";
