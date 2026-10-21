#define XRT_MODULE_ATOMIC
#define XRT_MODULE_FILE_LOCK
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "daemon_native.h"

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

static int fail(const char *message)
{
    fprintf(stderr, "daemon C test failed: %s\n", message);
    return 1;
}

static int check_pid_text(const char *path, const char *text, size_t size,
    int64_t expected)
{
    xfile file = xrtOpen(path, XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
    int written, closed;
    if (file == NULL) return 0;
    written = xrtWriteAtFull(file, 0u, text, size, NULL);
    closed = xrtClose(file);
    if (!written || !closed) return 0;
    return xdaemon_read_pidfile(path) == expected && xrtGetError() == NULL;
}

int main(void)
{
    char *temporary = xrtPathTemp();
    char *file_one = NULL;
    char *file_two = NULL;
    char *name = NULL;
    char unterminated_name[256];
    char unterminated_path[4097];
    void *service = NULL;
    void *contender = NULL;
    int result = 1;
    int stage = 0;

    if (temporary == NULL) return fail("temporary directory");
    service = xdaemon_create("测试服务");
    if (service == NULL || xdaemon_get_pid(service) <= 0 ||
        strcmp(xdaemon_name(service), "测试服务") != 0 ||
        !xdaemon_is_alive(xdaemon_get_pid(service))) goto done;
    memset(unterminated_name, 'a', sizeof(unterminated_name));
    if (xdaemon_create(unterminated_name) != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) goto done;
    unterminated_name[255] = '\0';
    contender = xdaemon_create(unterminated_name);
    if (contender == NULL) goto done;
    xdaemon_destroy(contender);
    contender = NULL;
    memset(unterminated_path, 'a', sizeof(unterminated_path));
    if (xdaemon_write_pidfile(service, unterminated_path) ||
        xdaemon_read_pidfile(unterminated_path) != -1 || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) goto done;
    name = xrtFormat("xl-daemon-c-测试-%lld.pid", (long long)xdaemon_get_pid(service));
    if (name == NULL) goto done;
    file_one = xrtPathJoin(temporary, name);
    xrtFree(name);
    name = xrtFormat("xl-daemon-c-%lld-secondary.pid", (long long)xdaemon_get_pid(service));
    if (name == NULL) goto done;
    file_two = xrtPathJoin(temporary, name);
    if (file_one == NULL || file_two == NULL) goto done;
    stage = 1;
    if (xdaemon_create("") != NULL) goto done;

    if (!xdaemon_write_pidfile(service, file_one) ||
        xrtGetError() != NULL ||
        !xdaemon_holds_pidfile(service) ||
        xdaemon_read_pidfile(file_one) != xdaemon_get_pid(service) ||
        !xdaemon_write_pidfile(service, file_one)) goto done;
    stage = 2;
#ifdef _WIN32
    contender = xdaemon_create("contender");
    if (contender == NULL || xdaemon_write_pidfile(contender, file_one)) goto done;
    xrtClearError();
#else
    {
        pid_t child = fork();
        int status_code = 0;
        if (child < 0) goto done;
        if (child == 0) {
            void *other = xdaemon_create("contender");
            int acquired = other != NULL && xdaemon_write_pidfile(other, file_one);
            xdaemon_destroy(other);
            _exit(acquired ? 1 : 0);
        }
        if (waitpid(child, &status_code, 0) != child ||
            !WIFEXITED(status_code) || WEXITSTATUS(status_code) != 0) {
            fprintf(stderr, "lock contender status=%d path=%s\n", status_code, file_one);
            goto done;
        }
    }
#endif
    stage = 3;
    if (xdaemon_write_pidfile(service, "") ||
        !xdaemon_holds_pidfile(service)) goto done;

    if (xdaemon_write_pidfile(service, file_two) ||
        !xdaemon_holds_pidfile(service)) goto done;
    if (!xdaemon_release_pidfile(service) ||
        xrtGetError() != NULL ||
        xrtFileExists(file_one) ||
        !xdaemon_write_pidfile(service, file_two) ||
        xdaemon_read_pidfile(file_two) != xdaemon_get_pid(service)) goto done;
    xdaemon_request_stop(service);
    if (!xdaemon_stop_requested(service) ||
        !xdaemon_release_pidfile(service) ||
        !xdaemon_release_pidfile(service) ||
        xrtFileExists(file_two)) goto done;
    if (xdaemon_read_pidfile(file_two) != -1 || xrtGetError() != NULL) goto done;
    if (!check_pid_text(file_two, "42\n", 3u, 42) ||
        !check_pid_text(file_two, "9223372036854775807\n", 20u, INT64_MAX) ||
        !check_pid_text(file_two, "9223372036854775808\n", 20u, -1) ||
        !check_pid_text(file_two, "0\n", 2u, -1) ||
        !check_pid_text(file_two, "-42\n", 4u, -1) ||
        !check_pid_text(file_two, "", 0u, -1) ||
        !check_pid_text(file_two,
            "1111111111111111111111111111111111111111111111111111111111111111",
            64u, -1) ||
        !check_pid_text(file_two, "42\0junk\n", 8u, -1) ||
        !check_pid_text(file_two, "+42\n", 4u, -1) ||
        !check_pid_text(file_two, "42\n\n", 4u, -1) ||
        !check_pid_text(file_two, "7", 1u, 7) ||
        !xrtFileDelete(file_two)) goto done;
    if (!xdaemon_write_pidfile(service, file_one)) goto done;
    xrtSetErrorInfo(XERR_VALUE, "daemon.test", 0, "sentinel before destroy");
    xdaemon_destroy(service);
    service = NULL;
    if (xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE ||
        strcmp(xrtErrorMessage(xrtGetError()), "sentinel before destroy") != 0)
        goto done;
    xrtClearError();
    if (xrtFileExists(file_one)) goto done;
#ifndef _WIN32
    {
        FILE *replacement;
        int write_result, close_result;
        service = xdaemon_create("replacement test");
        if (service == NULL || !xdaemon_write_pidfile(service, file_one) ||
            rename(file_one, file_two) != 0) goto done;
        replacement = fopen(file_one, "wb");
        if (replacement == NULL) goto done;
        write_result = fputs("999999\n", replacement);
        close_result = fclose(replacement);
        if (write_result < 0 || close_result != 0) goto done;
        stage = 4;
        if (xdaemon_release_pidfile(service) || xrtGetError() == NULL ||
            !xrtFileExists(file_one) || !xrtFileExists(file_two) ||
            xdaemon_holds_pidfile(service)) goto done;
        xdaemon_destroy(service);
        service = NULL;
        if (unlink(file_one) != 0 || unlink(file_two) != 0) goto done;
    }
#endif
    result = 0;

done:
    xdaemon_destroy(contender);
    xdaemon_destroy(service);
#ifndef _WIN32
    if (file_one != NULL) unlink(file_one);
    if (file_two != NULL) unlink(file_two);
#endif
    xrtFree(name);
    xrtFree(file_two);
    xrtFree(file_one);
    xrtFree(temporary);
    if (result) {
        fprintf(stderr, "daemon C test stage: %d\n", stage);
        return fail("lifecycle, PID file or cleanup");
    }
    puts("daemon C test: PASS");
    return 0;
}
