#include <stddef.h>

/* 只提供旧版两个入口，验证不完整后端不会被当作完整插件。 */
__declspec(dllexport) void* onnxruntime_xl_create_session(const char* path, void* options)
{
    (void)path;
    (void)options;
    return NULL;
}

__declspec(dllexport) void* onnxruntime_xl_run_json(void* session,
    const char* input_names, const char* input_json)
{
    (void)session;
    (void)input_names;
    (void)input_json;
    return NULL;
}
