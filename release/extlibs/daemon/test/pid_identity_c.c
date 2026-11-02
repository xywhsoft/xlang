/* 确定性重放：竞争者打开旧文件后，原持有者删除，第三方创建并锁定新文件。 */
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_FILE_LOCK
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_DIR_TEMP
#include "xrt.h"
#include "../include/daemon_native.h"
#include <stdio.h>
#ifndef _WIN32
#include <sys/file.h>
static void *first_owner, *new_owner;
static const char* exchange_path;
static int interleave, scheduled;
static int scheduled_flock(int descriptor, int operation)
{
    if (interleave && (operation & LOCK_EX)) {
        interleave=0;
        /* 二者都是真的包调用与内核锁；不伪造锁或文件身份查询的结果。 */
        scheduled=xdaemon_release_pidfile(first_owner) && xdaemon_write_pidfile(new_owner,exchange_path);
        if (!scheduled) return -1;
    }
    return flock(descriptor,operation);
}
#define flock scheduled_flock
#endif
#include "../daemon_native.c"
#ifndef _WIN32
#undef flock
#endif

static unsigned checks, failures;
static void require(bool ok, const char* label)
{
    ++checks;
    if (!ok) { fprintf(stderr,"daemon identity: %s\n",label); ++failures; }
}
int main(void)
{
    char* root=xrtDirTemp(NULL,"xlang-daemon-identity-",NULL);
    char* path=root ? xrtPathJoin(root,"owner.pid") : NULL;
    void* owner=xdaemon_create("owner");
    void* contender=xdaemon_create("contender");
    void* replacement=xdaemon_create("replacement");
    if (!path || !owner || !contender || !replacement) return 2;
    require(xdaemon_write_pidfile(owner,path),"initial kernel lock");
    require(!xdaemon_write_pidfile(contender,path) && xrtGetError()==NULL,"ordinary contention remains false without error");
#ifndef _WIN32
    first_owner=owner; new_owner=replacement; exchange_path=path; interleave=1;
    bool accepted=xdaemon_write_pidfile(contender,path);
    require(scheduled,"real unlink/recreate/lock interleaving executed");
    require(!accepted && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE,
        "stale opened inode rejected after acquiring its old lock");
    require(!xdaemon_holds_pidfile(contender) && xdaemon_holds_pidfile(replacement),"only current path owner remains held");
    xrtClearError();
    /* 旧实现已误接管时先真实释放，便于继续检查，不把清理失败掩成通过。 */
    if (accepted) (void)xdaemon_release_pidfile(contender);
    xrtClearError();
    require(xrtFileExists(path) && xdaemon_read_pidfile(path)==xdaemon_get_pid(replacement),"rejected stale handle cannot delete replacement");
    require(!xdaemon_write_pidfile(contender,path) && xrtGetError()==NULL,"retry while replacement holds lock remains busy");
    require(xdaemon_release_pidfile(replacement),"release replacement owner");
    char* backup=xrtPathJoin(root,"renamed.pid");
    require(backup && xdaemon_write_pidfile(owner,path) && xrtPathRename(path,backup,false),"rename a still-held PID file");
    require(xdaemon_write_pidfile(replacement,path),"replacement locks its new inode");
    require(!xdaemon_write_pidfile(owner,path) && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE,
        "same-path reentry cannot report a replaced file as owned");
    require(!xdaemon_release_pidfile(owner) && !xdaemon_holds_pidfile(owner),"release stale owner reports mismatch but retires handle");
    xrtClearError();
    require(xrtFileExists(path) && xrtFileExists(backup) && xdaemon_holds_pidfile(replacement),"stale cleanup preserves replacement and renamed file");
    require(xdaemon_release_pidfile(replacement) && xrtFileDelete(backup),"remove only actual test owners");
    xrtFree(backup);
#else
    require(xdaemon_release_pidfile(owner),"release Windows owner");
#endif
    require(xdaemon_write_pidfile(contender,path) && xdaemon_release_pidfile(contender),"same contender recovers after release");
    xdaemon_destroy(replacement); xdaemon_destroy(contender); xdaemon_destroy(owner);
    xrtClearError();
    require(!xrtFileExists(path) && xrtDirRemove(root),"own files and directory removed");
    xrtFree(path); xrtFree(root);
    printf("daemon PID identity: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
