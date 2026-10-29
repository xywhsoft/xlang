#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../../onnxruntime/include/onnxruntime.h"

extern int opencv_available_native(void);
extern const char* opencv_backend_native(void);

enum { WORKERS = 16 };
static HANDLE g_start;
static volatile LONG g_failures;
static HANDLE g_error_ready;
static volatile LONG g_error_count;

static DWORD WINAPI probe_worker(void* unused)
{
    int i;
    (void)unused;
    if ( WaitForSingleObject(g_start, 10000u) != WAIT_OBJECT_0 ) return 1u;
    for ( i = 0; i < 10; ++i ) {
        if ( !opencv_available_native() || !ort_available_native() ||
             opencv_backend_native()[0] == '\0' || ort_backend_native()[0] == '\0' ) {
            InterlockedIncrement(&g_failures);
            return 1u;
        }
    }
    return 0u;
}

static DWORD WINAPI error_worker(void* which)
{
    const char* expected;
    if ( which == NULL ) {
        if ( ort_tensor_from_opencv_native(NULL, 1.0, 0, 0, 0, 1, 1, 1) != NULL ) return 1u;
        expected = "invalid OpenCV image handle";
    } else {
        if ( ort_tensor_from_opencv_batch_native(0, NULL, NULL, NULL, NULL, NULL, NULL,
                                                  1.0, 0, 0, 0, 1, 1, 1) != NULL ) return 2u;
        expected = "invalid OpenCV batch count";
    }
    if ( InterlockedIncrement(&g_error_count) == 2 ) SetEvent(g_error_ready);
    if ( WaitForSingleObject(g_error_ready, 10000u) != WAIT_OBJECT_0 ) return 3u;
    return strcmp(ort_last_error_native(), expected) == 0 ? 0u : 4u;
}

int main(void)
{
    HANDLE workers[WORKERS];
    HANDLE errors[2];
    DWORD code;
    int i;

    g_start = CreateEventW(NULL, TRUE, FALSE, NULL);
    if ( g_start == NULL ) return 2;
    for ( i = 0; i < WORKERS; ++i ) {
        workers[i] = CreateThread(NULL, 0, probe_worker, NULL, 0, NULL);
        if ( workers[i] == NULL ) {
            SetEvent(g_start);
            while ( i > 0 ) { --i; WaitForSingleObject(workers[i], INFINITE); CloseHandle(workers[i]); }
            CloseHandle(g_start);
            return 3;
        }
    }
    SetEvent(g_start);
    for ( i = 0; i < WORKERS; ++i ) {
        if ( WaitForSingleObject(workers[i], 30000u) != WAIT_OBJECT_0 ) return 4;
        if ( !GetExitCodeThread(workers[i], &code) || code != 0u ) InterlockedIncrement(&g_failures);
        CloseHandle(workers[i]);
    }
    CloseHandle(g_start);
    if ( g_failures != 0 ) {
        fprintf(stderr, "cold loader failures: %ld\n", (long)g_failures);
        return 1;
    }
    g_error_ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if ( g_error_ready == NULL ) return 5;
    errors[0] = CreateThread(NULL, 0, error_worker, NULL, 0, NULL);
    errors[1] = CreateThread(NULL, 0, error_worker, (void*)1, 0, NULL);
    if ( errors[0] == NULL || errors[1] == NULL ) return 6;
    for ( i = 0; i < 2; ++i ) {
        if ( WaitForSingleObject(errors[i], 15000u) != WAIT_OBJECT_0 ||
             !GetExitCodeThread(errors[i], &code) || code != 0u ) return 7;
        CloseHandle(errors[i]);
    }
    CloseHandle(g_error_ready);
    puts("opencv/onnxruntime C cold loader: PASS");
    return 0;
}
