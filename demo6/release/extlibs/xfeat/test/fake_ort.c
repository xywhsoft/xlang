#if defined(_WIN32) || defined(_WIN64)
#define TEST_EXPORT __declspec(dllexport)
#else
#define TEST_EXPORT __attribute__((visibility("default")))
#endif
#include "fake_ort.h"
#include <stdlib.h>
#include <string.h>

static void* released_sessions[32];
static int released_count;

TEST_EXPORT void* onnxruntime_xl_create_session_options(void)
{
    return malloc(1u);
}

TEST_EXPORT void onnxruntime_xl_release_session_options(void* options)
{
    free(options);
}

TEST_EXPORT int onnxruntime_xl_session_options_set_intra_op_num_threads(void* options, int value)
{
    return options != NULL && value > 0;
}

TEST_EXPORT int onnxruntime_xl_session_options_set_inter_op_num_threads(void* options, int value)
{
    return options != NULL && value > 0;
}

TEST_EXPORT int onnxruntime_xl_session_options_set_graph_optimization_level(void* options, int value)
{
    return options != NULL && value >= 0;
}

TEST_EXPORT int onnxruntime_xl_session_options_set_execution_mode(void* options, int value)
{
    return options != NULL && value >= 0;
}

TEST_EXPORT void* onnxruntime_xl_create_session(const char* path, void* options)
{
    void* session;
    int i;
    (void)options;
    if (path == NULL || path[0] == '\0') return NULL;
    session = malloc(1u);
    if (session == NULL) return NULL;
    for (i = 0; i < released_count; ++i) {
        if (released_sessions[i] == session) {
            released_sessions[i] = released_sessions[--released_count];
            break;
        }
    }
    return session;
}

TEST_EXPORT void onnxruntime_xl_release_session(void* session)
{
    int i;
    for (i = 0; i < released_count; ++i) {
        if (released_sessions[i] == session) abort();
    }
    if (released_count >= 32) abort();
    released_sessions[released_count++] = session;
    free(session);
}

TEST_EXPORT void* onnxruntime_xl_run_json(void* session, const char* input, const char* output)
{
    (void)session;
    (void)input;
    (void)output;
    return NULL;
}

TEST_EXPORT const char* onnxruntime_xl_result_float_shape_json(void* handle)
{
	return ((const FakeOrtResult*)handle)->shape;
}

TEST_EXPORT int onnxruntime_xl_result_float_count(void* handle)
{
	return ((const FakeOrtResult*)handle)->count;
}

TEST_EXPORT float onnxruntime_xl_result_float_at(void* handle, int index)
{
	return ((const FakeOrtResult*)handle)->data[index];
}

TEST_EXPORT const float* onnxruntime_xl_result_float_data(void* handle)
{
	return ((const FakeOrtResult*)handle)->data;
}
