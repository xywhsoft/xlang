#define XRT_MODULE_PATH_SYSTEM
#define XRT_MODULE_STRING_FORMAT
#include "xrt.h"
#include "daemon_native.h"

#include <stdio.h>

int main(void)
{
    void *service = xdaemon_create("sample-service");
    char *temporary = xrtPathTemp();
    char *name = NULL;
    char *path = NULL;
    int result = 1;

    if (service == NULL || temporary == NULL) goto done;
    name = xrtFormat("sample-service-%lld.pid",
        (long long)xdaemon_get_pid(service));
    if (name == NULL) goto done;
    path = xrtPathJoin(temporary, name);
    if (path == NULL || !xdaemon_write_pidfile(service, path)) goto done;

    printf("%s pid=%lld\n", xdaemon_name(service),
        (long long)xdaemon_read_pidfile(path));
    printf("active=%s\n", xdaemon_is_alive(xdaemon_get_pid(service)) ? "true" : "false");
    xdaemon_request_stop(service);
    if (!xdaemon_stop_requested(service) || !xdaemon_release_pidfile(service)) goto done;
    puts("daemon example: PASS");
    result = 0;

done:
    xdaemon_destroy(service);
    xrtFree(path);
    xrtFree(name);
    xrtFree(temporary);
    return result;
}
