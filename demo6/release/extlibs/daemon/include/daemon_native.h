#ifndef XL_DAEMON_NATIVE_H
#define XL_DAEMON_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *xdaemon_create(const char *name);
void xdaemon_destroy(void *handle);
int64_t xdaemon_write_pidfile(void *handle, const char *path);
int64_t xdaemon_release_pidfile(void *handle);
int64_t xdaemon_read_pidfile(const char *path);
int64_t xdaemon_is_alive(int64_t pid);
int64_t xdaemon_get_pid(void *handle);
const char *xdaemon_name(void *handle);
int64_t xdaemon_stop_requested(void *handle);
void xdaemon_request_stop(void *handle);
int64_t xdaemon_holds_pidfile(void *handle);

#ifdef __cplusplus
}
#endif

#endif
