/* 与 restart.xl 同一行为和固定输出；仅操作自己的独占临时目录。 */
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
#include "xrt.h"
#include "../include/daemon_native.h"
#ifdef __TINYC__
#include "../daemon_native.c"
#endif
#include <stdio.h>

int main(void)
{
    char* root=xrtDirTemp(NULL,"xlang-daemon-restart-",NULL);
    char* path=root ? xrtPathJoin(root,"服务.pid") : NULL;
    char* other=root ? xrtPathJoin(root,"other.pid") : NULL;
    void* worker=xdaemon_create("worker");
    void* successor=xdaemon_create("successor");
    int result=1;
    if (!path || !other || !worker || !successor) goto done;
    if (!xdaemon_write_pidfile(worker,path) || !xdaemon_holds_pidfile(worker) ||
        xdaemon_read_pidfile(path)!=xdaemon_get_pid(worker)) goto done;
    puts("held: true");
    if (xdaemon_write_pidfile(successor,path) || xrtGetError()) goto done;
    puts("contender: busy");
    if (xdaemon_write_pidfile(worker,other) || !xrtGetError() || xrtErrorKind(xrtGetError())!=XERR_STATE ||
        !xdaemon_holds_pidfile(worker) || xdaemon_read_pidfile(path)!=xdaemon_get_pid(worker)) goto done;
    puts("failed switch: held");
    if (!xdaemon_release_pidfile(worker) || !xdaemon_write_pidfile(successor,path) ||
        !xdaemon_holds_pidfile(successor) || !xdaemon_release_pidfile(successor)) goto done;
    puts("handoff: true");
    xdaemon_request_stop(worker);
    if (!xdaemon_stop_requested(worker)) goto done;
    puts("stop: true"); result=0;
done:
    xdaemon_destroy(successor); xdaemon_destroy(worker);
    xrtClearError();
    if (path && xrtFileExists(path) && !xrtFileDelete(path)) result=1;
    if (root && !xrtDirRemove(root)) result=1;
    xrtFree(other); xrtFree(path); xrtFree(root);
    return result;
}
