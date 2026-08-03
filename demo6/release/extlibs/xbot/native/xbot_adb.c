/*
 * xbot_adb.c - ADB 子进程管理
 * 通过 CreateProcess + 管道执行 ADB 命令，支持文本和二进制输出。
 * TCC 兼容，纯 Win32 API。
 */
#include "xbot_native.h"

#define ADB_BUF_SIZE 65536
static char adb_buf[ADB_BUF_SIZE];

/* 动态缓冲区 */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} AdbBuffer;

static void adb_buf_init(AdbBuffer *b) {
    b->cap = 4096;
    b->len = 0;
    b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = '\0';
}

static void adb_buf_append(AdbBuffer *b, const char *chunk, size_t n) {
    if (!b->data) return;
    while (b->len + n + 1 > b->cap) {
        b->cap *= 2;
        b->data = (char *)realloc(b->data, b->cap);
        if (!b->data) return;
    }
    memcpy(b->data + b->len, chunk, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void adb_buf_free(AdbBuffer *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
}

/*
 * 构建完整命令行: "adb_path" -s serial args
 * 返回 malloc 的字符串，调用方负责 free。
 */
static char *build_command(const char *adb_path, const char *serial, const char *args) {
    size_t len = strlen(adb_path) + 8;
    if (serial && *serial) len += strlen(serial) + 8;
    if (args && *args) len += strlen(args) + 2;

    char *cmd = (char *)malloc(len);
    if (!cmd) return NULL;

    snprintf(cmd, len, "\"%s\"", adb_path);
    if (serial && *serial) {
        strcat(cmd, " -s ");
        strcat(cmd, serial);
    }
    if (args && *args) {
        strcat(cmd, " ");
        strcat(cmd, args);
    }
    return cmd;
}

/*
 * 执行子进程并捕获 stdout/stderr。
 * 返回 exit code，stdout 和 stderr 写入对应缓冲区。
 */
static int run_process(const char *cmdline, long long timeout_ms,
                       AdbBuffer *out_buf, AdbBuffer *err_buf) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hOutRead = NULL, hOutWrite = NULL;
    HANDLE hErrRead = NULL, hErrWrite = NULL;

    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) return -1;
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&hErrRead, &hErrWrite, &sa, 0)) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        return -1;
    }
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hOutWrite;
    si.hStdError = hErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));

    /* CreateProcess 需要可写的命令行 */
    char *cmd_copy = _strdup(cmdline);
    BOOL ok = CreateProcessA(NULL, cmd_copy, NULL, NULL, TRUE,
                              CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd_copy);

    CloseHandle(hOutWrite);
    CloseHandle(hErrWrite);

    if (!ok) {
        CloseHandle(hOutRead);
        CloseHandle(hErrRead);
        return -1;
    }

    /* 等待进程结束 */
    DWORD wait_result = WaitForSingleObject(pi.hProcess, (DWORD)(timeout_ms > 0 ? timeout_ms : 30000));
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 3000);
    }

    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    /* 读取 stdout */
    char chunk[4096];
    DWORD bytes_read = 0;
    while (ReadFile(hOutRead, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0) {
        adb_buf_append(out_buf, chunk, bytes_read);
    }

    /* 读取 stderr */
    while (ReadFile(hErrRead, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0) {
        adb_buf_append(err_buf, chunk, bytes_read);
    }

    CloseHandle(hOutRead);
    CloseHandle(hErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}

/* ─── 公开 API ─── */

const char *xxbot_adb_run(const char *adb_path, const char *serial,
                           const char *args, long long timeout_ms) {
    if (!adb_path || !*adb_path) {
        xrtSetError("xbot_adb: empty adb path", 0);
        return "";
    }

    char *cmd = build_command(adb_path, serial, args);
    if (!cmd) { xrtSetError("xbot_adb: out of memory", 0); return ""; }

    AdbBuffer out_buf, err_buf;
    adb_buf_init(&out_buf);
    adb_buf_init(&err_buf);

    int exit_code = run_process(cmd, timeout_ms, &out_buf, &err_buf);
    free(cmd);

    if (exit_code < 0) {
        adb_buf_free(&out_buf);
        adb_buf_free(&err_buf);
        xrtSetError("xbot_adb: CreateProcess failed", 0);
        return "";
    }

    /* 组装结果: "EXIT_CODE\r\nSTDOUT\r\nSTDERR" */
    size_t total = 32 + out_buf.len + err_buf.len + 4;
    char *result = (char *)xrtTempMemory(total);
    if (!result) {
        adb_buf_free(&out_buf);
        adb_buf_free(&err_buf);
        xrtSetError("xbot_adb: out of memory", 0);
        return "";
    }

    snprintf(result, total, "%d\r\n%s\r\n%s",
             exit_code,
             out_buf.data ? out_buf.data : "",
             err_buf.data ? err_buf.data : "");

    adb_buf_free(&out_buf);
    adb_buf_free(&err_buf);
    return result;
}

long long xxbot_adb_run_binary(const char *adb_path, const char *serial,
                                const char *args, const char *save_path,
                                long long timeout_ms) {
    if (!adb_path || !*adb_path) {
        xrtSetError("xbot_adb: empty adb path", 0);
        return -1;
    }
    if (!save_path || !*save_path) {
        xrtSetError("xbot_adb: empty save path", 0);
        return -1;
    }

    char *cmd = build_command(adb_path, serial, args);
    if (!cmd) { xrtSetError("xbot_adb: out of memory", 0); return -1; }

    /* 对于二进制输出，我们直接将 stdout 重定向到文件 */
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hFile = CreateFileA(save_path, GENERIC_WRITE, 0, &sa,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(cmd);
        xrtSetError("xbot_adb: cannot create output file", 0);
        return -1;
    }

    HANDLE hErrRead = NULL, hErrWrite = NULL;
    CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hFile;
    si.hStdError = hErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));

    char *cmd_copy = _strdup(cmd);
    BOOL ok = CreateProcessA(NULL, cmd_copy, NULL, NULL, TRUE,
                              CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd_copy);
    free(cmd);

    CloseHandle(hErrWrite);
    CloseHandle(hFile);

    if (!ok) {
        CloseHandle(hErrRead);
        xrtSetError("xbot_adb: CreateProcess failed for binary", 0);
        return -1;
    }

    DWORD wait_result = WaitForSingleObject(pi.hProcess, (DWORD)(timeout_ms > 0 ? timeout_ms : 30000));
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 3000);
    }

    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    /* 读取 stderr 用于错误报告 */
    AdbBuffer err_buf;
    adb_buf_init(&err_buf);
    char chunk[4096];
    DWORD bytes_read = 0;
    while (ReadFile(hErrRead, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0) {
        adb_buf_append(&err_buf, chunk, bytes_read);
    }
    CloseHandle(hErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        snprintf(adb_buf, sizeof(adb_buf), "xbot_adb: binary command failed (exit %lu): %s",
                 exit_code, err_buf.data ? err_buf.data : "");
        xrtSetError(adb_buf, 0);
        adb_buf_free(&err_buf);
        return -1;
    }

    adb_buf_free(&err_buf);

    /* 返回文件大小 */
    HANDLE hCheck = CreateFileA(save_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                 OPEN_EXISTING, 0, NULL);
    if (hCheck == INVALID_HANDLE_VALUE) return 0;
    DWORD file_size = GetFileSize(hCheck, NULL);
    CloseHandle(hCheck);
    return (long long)file_size;
}
