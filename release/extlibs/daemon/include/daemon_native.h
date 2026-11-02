#ifndef XL_DAEMON_NATIVE_H
#define XL_DAEMON_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *xdaemon_create(const char *name);
/* 析构保留进入时的 XRT 首因；清理失败仍消耗句柄。 */
void xdaemon_destroy(void *handle);
/* 写入和同路径重入核对当前文件身份；失败不自动重试新路径。 */
int64_t xdaemon_write_pidfile(void *handle, const char *path);
/* 继续尝试删除/解锁/关闭，保留首错；调用后不再持有原文件。 */
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
