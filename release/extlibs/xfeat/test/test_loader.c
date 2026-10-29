#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
#include "xrt.h"
#include "fake_ort.h"
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "xfeat.h"

typedef struct LoadCase {
    atomic_int* start;
    int kind;
    int ok;
} LoadCase;

static void load_worker(LoadCase* test)
{
    const char* expected = test->kind ? "invalid XFeat match parameters" : "empty XFeat features";
    int i;
    while (!atomic_load(test->start)) { }
    test->ok = xfeat_available_native();
    for (i = 0; test->ok && i < 100; ++i) {
        if (!xfeat_stats_add_native(test->kind ? 1 : 2, 1.0, 2.0, 3.0)) test->ok = 0;
        if (xfeat_match_native(NULL, NULL, 0.8, test->kind ? 0 : 3, 2.0, 1.0) != NULL ||
            strcmp(xfeat_last_error_native(), expected) != 0) test->ok = 0;
    }
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI load_thread(LPVOID context)
{
    load_worker((LoadCase*)context);
    return 0;
}
#else
static void* load_thread(void* context)
{
    load_worker((LoadCase*)context);
    return NULL;
}
#endif

static int require(int condition, const char* what)
{
    if (condition) return 0;
    fprintf(stderr, "xfeat bridge: %s (%s)\n", what, xfeat_last_error_native());
    return 1;
}

int main(void)
{
    enum { THREADS = 8 };
    LoadCase cases[THREADS];
    atomic_int start = ATOMIC_VAR_INIT(0);
#if defined(_WIN32) || defined(_WIN64)
    HANDLE threads[THREADS];
#else
    pthread_t threads[THREADS];
#endif
    float query_rows[4 * 67] = {0};
    float target_rows[4 * 67] = {0};
    FakeOrtResult query_result = {"[4,67]", 4 * 67, query_rows};
    FakeOrtResult target_result = {"[1,4,67]", 4 * 67, target_rows};
    const float points[8] = {10, 10, 80, 10, 10, 80, 80, 80};
    void* query;
    void* target;
    void* location;
    char* json;
    char* saved;
    char* stats;
    char long_name[5001];
    int i;
    int stats_failures = 0;
    int stats_successes = 0;
    int location_failures = 0;
    int location_successes = 0;

    if (require(xfeat_stats_reset_native(), "reset concurrent statistics")) return 1;
    for (i = 0; i < THREADS; ++i) {
        cases[i].start = &start;
        cases[i].kind = i & 1;
        cases[i].ok = 0;
#if defined(_WIN32) || defined(_WIN64)
        threads[i] = CreateThread(NULL, 0, load_thread, &cases[i], 0, NULL);
        if (require(threads[i] != NULL, "create load worker")) return 1;
#else
        if (require(pthread_create(&threads[i], NULL, load_thread, &cases[i]) == 0,
                    "create load worker")) return 1;
#endif
    }
    atomic_store(&start, 1);
    for (i = 0; i < THREADS; ++i) {
#if defined(_WIN32) || defined(_WIN64)
        if (require(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0,
                    "join load worker")) return 1;
        CloseHandle(threads[i]);
#else
        if (require(pthread_join(threads[i], NULL) == 0, "join load worker")) return 1;
#endif
        if (require(cases[i].ok, "UTF-8 bridge should load on all workers")) return 1;
    }
    stats = xfeat_stats_json_native();
    if (require(stats != NULL && xrtJsonValid(xrtStrView(stats)) &&
                strstr(stats, "\"all\":{\"count\":800") != NULL &&
                strstr(stats, "\"query\":{\"count\":400") != NULL &&
                strstr(stats, "\"target\":{\"count\":400") != NULL &&
                xfeat_last_error_native()[0] == '\0', "thread-local errors and statistics")) return 1;
    xrtFree(stats);
    for (i = 0; i < 4; ++i) {
        query_rows[i * 67] = points[i * 2];
        query_rows[i * 67 + 1] = points[i * 2 + 1];
        target_rows[i * 67] = points[i * 2] + 20.0f;
        target_rows[i * 67 + 1] = points[i * 2 + 1] + 30.0f;
        query_rows[i * 67 + 2] = target_rows[i * 67 + 2] = 1.0f;
        query_rows[i * 67 + 3 + i] = target_rows[i * 67 + 3 + i] = 1.0f;
    }
    query = xfeat_features_from_result_native(&query_result, 100, 100, 100, 100);
    target = xfeat_features_from_result_native(&target_result, 300, 300, 300, 300);
    if (require(query != NULL && target != NULL, "feature rows should parse")) return 1;
    if (require(xfeat_features_count_native(query) == 4 &&
                xfeat_features_count_native(target) == 4, "feature count")) return 1;
    location = xfeat_match_native(query, target, 0.8, 3, 2.0, 1.0);
    if (require(location != NULL && xfeat_location_found_native(location), "translation match")) return 1;
    if (require(xfeat_location_bbox_x_native(location) == 20 &&
                xfeat_location_bbox_y_native(location) == 30 &&
                xfeat_location_bbox_w_native(location) == 99 &&
                xfeat_location_bbox_h_native(location) == 99, "translation bounds")) return 1;
    json = xfeat_location_json_native(location, "target.png", "match.png");
    if (require(json != NULL && strstr(json, "\"found\":true") != NULL &&
                xrtJsonValid(xrtStrView(json)), "location JSON")) return 1;
    xrtFree(json);

    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[0] = '"';
    long_name[1] = '\n';
    long_name[sizeof(long_name) - 1] = '\0';
    saved = xfeat_location_json_native(location, long_name, "first");
    json = xfeat_location_json_native(location, "second", "second");
    if (require(saved != NULL && json != NULL && strlen(saved) > 5000 &&
                xrtJsonValid(xrtStrView(saved)) && xrtJsonValid(xrtStrView(json)) &&
                strstr(saved, "\\n") != NULL && strstr(saved, "first") != NULL &&
                strstr(json, "second") != NULL, "owned long JSON outputs")) return 1;
    xrtFree(json);
    xrtFree(saved);

    if (require(xfeat_stats_reset_native() &&
                xfeat_stats_add_native(1, 1.0, 2.0, 3.0), "statistics update")) return 1;
    stats = xfeat_stats_json_native();
    if (require(stats != NULL && xrtJsonValid(xrtStrView(stats)) &&
                strstr(stats, "\"all\":{\"count\":1") != NULL &&
                strstr(stats, "\"query\":{\"count\":1") != NULL &&
                strstr(stats, "\"target\":{\"count\":0") != NULL,
                "all/query statistics")) return 1;
    xrtFree(stats);
    xfeat_location_release_native(location);
    xfeat_features_release_native(target);
    xfeat_features_release_native(query);

    query_rows[0] = 110.0f;
    query = xfeat_features_from_result_native(&query_result, 100, 100, 128, 128);
    if (require(query != NULL && xfeat_features_count_native(query) == 3,
                "padded image features should be filtered")) return 1;
    xfeat_features_release_native(query);
    query_rows[0] = 10.0f;
    if (require(xfeat_features_from_result_native(&query_result, 100, 100, 96, 128) == NULL,
                "padded dimensions cannot be smaller than the image")) return 1;

    query_result.shape = "[4,68]";
    if (require(xfeat_features_from_result_native(&query_result, 100, 100, 100, 100) == NULL,
                "wrong feature width should fail")) return 1;
    query_result.shape = "[4,67]";
    query_result.count--;
    if (require(xfeat_features_from_result_native(&query_result, 100, 100, 100, 100) == NULL,
                "shape/count mismatch should fail")) return 1;
    query_result.shape = "[0,67]";
    query_result.count = 0;
    query = xfeat_features_from_result_native(&query_result, 100, 100, 100, 100);
    if (require(query != NULL && xfeat_features_count_native(query) == 0,
                "zero features should be valid")) return 1;
    xfeat_features_release_native(query);
    query_result.shape = "[4,67]";
    query_result.count = 4 * 67;
    query_rows[0] = NAN;
    if (require(xfeat_features_from_result_native(&query_result, 100, 100, 100, 100) == NULL,
                "non-finite coordinates should fail")) return 1;
    query_rows[0] = 10.0f;
    query_rows[3] = NAN;
    if (require(xfeat_features_from_result_native(&query_result, 100, 100, 100, 100) == NULL,
                "non-finite descriptors should fail")) return 1;
    for (i = 0; i < 32; ++i) {
        bool triggered;
        if (require(xrtMemDebugFailAfter((uint64)i), "JSON fault setup")) return 1;
        stats = xfeat_stats_json_native();
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (require(triggered ? stats == NULL : stats != NULL,
                    "statistics JSON allocation failure")) return 1;
        if (triggered) stats_failures++;
        else stats_successes++;
        xrtFree(stats);

        if (require(xrtMemDebugFailAfter((uint64)i), "location fault setup")) return 1;
        json = xfeat_location_json_native(NULL, "target.png", "");
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (require(triggered ? json == NULL : json != NULL,
                    "location JSON allocation failure")) return 1;
        if (triggered) location_failures++;
        else location_successes++;
        xrtFree(json);
    }
    if (require(stats_failures > 0 && stats_successes > 0 &&
                location_failures > 0 && location_successes > 0,
                "JSON fault injection coverage")) return 1;
    puts("xfeat UTF-8 bridge and feature rows: PASS");
    return 0;
}
