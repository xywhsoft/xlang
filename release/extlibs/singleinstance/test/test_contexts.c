#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "xl.h"

typedef struct runner {
    XL_Context* Context;
    const char* Script;
    int Result;
    atomic_int Done;
} runner;

static void* run_script(void* data)
{
    runner* job = (runner*)data;
    job->Result = xlRunScriptFile(job->Context, job->Script) &&
        xlGetLastExitCode(job->Context) == 0;
    atomic_store(&job->Done, 1);
    return NULL;
}

int main(int argc, char** argv)
{
    XL_ContextOptions options;
    XL_Context* first;
    XL_Context* second;
    pthread_t hold_thread;
    pthread_t probe_thread;
    runner hold;
    runner probe;
    char name[80];
    char done_path[128];
    int i;
    if (argc != 5) return 2;
    snprintf(name, sizeof(name), "xlang_si_contexts_%ld", (long)getpid());
    snprintf(done_path, sizeof(done_path), "/tmp/xlang_si_contexts_%ld.done", (long)getpid());
    unlink(done_path);
    if (setenv("XLANG_SI_TEST_NAME", name, 1) != 0 ||
        setenv("XLANG_SI_TEST_DONE_PATH", done_path, 1) != 0 ||
        unsetenv("XLANG_SI_TEST_READY") != 0) return 1;
    options = xlDefaultContextOptions();
    options.workDir = argv[1];
    first = xlCreateContext(&options);
    second = xlCreateContext(&options);
    if (first == NULL || second == NULL) return 1;
    hold.Context = first;
    hold.Script = argv[2];
    hold.Result = 0;
    atomic_init(&hold.Done, 0);
    if (pthread_create(&hold_thread, NULL, run_script, &hold) != 0) return 1;
    for (i = 0; i < 300 && getenv("XLANG_SI_TEST_READY") == NULL; ++i) {
        struct timespec pause = {0, 10000000};
        nanosleep(&pause, NULL);
    }
    if (getenv("XLANG_SI_TEST_READY") == NULL) {
        fprintf(stderr, "singleinstance holder did not become ready\n");
        return 1;
    }
    probe.Context = second;
    probe.Script = argv[3];
    probe.Result = 0;
    atomic_init(&probe.Done, 0);
    if (pthread_create(&probe_thread, NULL, run_script, &probe) != 0) return 1;
    for (i = 0; i < 1000 && !atomic_load(&probe.Done); ++i) {
        struct timespec pause = {0, 10000000};
        nanosleep(&pause, NULL);
    }
    int concurrent = atomic_load(&probe.Done);
    int signal_fd = open(done_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (signal_fd < 0) return 1;
    close(signal_fd);
    pthread_join(hold_thread, NULL);
    pthread_join(probe_thread, NULL);
    unlink(done_path);
    if (!concurrent) {
        fprintf(stderr, "blocked-upstream: second XL_Context did not execute during first context's script\n");
        return 2;
    }
    if (!probe.Result) {
        fprintf(stderr, "singleinstance context probe: %s\n", xlGetLastError(second));
        return 1;
    }
    if (!hold.Result || !xlRunScriptFile(second, argv[4]) ||
        xlGetLastExitCode(second) != 0) {
        fprintf(stderr, "singleinstance context after release: %s\n", xlGetLastError(second));
        return 1;
    }
    xlDestroyContext(second);
    xlDestroyContext(first);
    puts("singleinstance XL_Context isolation: PASS");
    return 0;
}
