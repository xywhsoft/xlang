/* CPU proxy is a process-owned test provider; resources close explicitly. */
#include "test/cases/foreign_xui_render_backend.c"

size_t webviewLiveCount(void)
{
    xmemdebugsnapshot snapshot;
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&snapshot);
    return snapshot.LiveCount;
}
