#include "handler_generation.h"

#include "../include/xs_handler.h"
#include "../../../include/xl.h"
#include "xrt.h"

#include <string.h>

/* ------------------------------------ 数据结构 ------------------------------------ */

typedef struct xs2_handler_instance {
    XL_Context* context;
    XL_Module* module;
    xs_handler_proc entry;
    int busy;
} xs2_handler_instance;

struct xs2_handler_generation {
    xs2_handler_instance* instances;
    int instance_count;
    int queue_capacity;
    int queue_timeout_ms;
    int execution_timeout_ms;
    int waiting_count;
    int closing;
    xmutex mutex;
    xcond changed;
};

typedef struct xs2_handler_call_state {
    xs2_handler_generation* generation;
    xbuffer response;
} xs2_handler_call_state;

/* ------------------------------------ 基础工具 ------------------------------------ */

static char* xs2CopyText(const char* text)
{
    if ( text == NULL ) {
        text = "";
    }
    return (char*)xrtCopyStr((str)(void*)text, 0u);
}

static void xs2SetError(char** error_text, const char* text)
{
    if ( error_text == NULL ) {
        return;
    }
    if ( *error_text != NULL ) {
        xrtFree(*error_text);
    }
    *error_text = xs2CopyText(text);
}

static uint64_t xs2NowMs(void)
{
    double now = xrtTimer();
    if ( now <= 0.0 ) {
        return 0u;
    }
    return (uint64_t)(now * 1000.0);
}

static int xs2PathIsC(const char* path)
{
    const char* extension;

    if ( path == NULL ) {
        return 0;
    }
    extension = strrchr(path, '.');
    return extension != NULL &&
           (extension[1] == 'c' || extension[1] == 'C') &&
           extension[2] == '\0';
}

/* ------------------------------------ Handler Host API ------------------------------------ */

static int xs2HostIsCancelled(const xs_handler_frame* frame)
{
    if ( frame == NULL ) {
        return 1;
    }
    if ( (frame->flags & XS_HANDLER_FRAME_CANCELLED) != 0u ) {
        return 1;
    }
    return frame->deadline_ms != 0u && xs2NowMs() >= frame->deadline_ms;
}

static int xs2HostSetOutput(xs_handler_frame* frame, xvalue value)
{
    if ( frame == NULL ) {
        return 0;
    }
    frame->output = value;
    return 1;
}

static int xs2HostClearOutput(xs_handler_frame* frame)
{
    xs2_handler_call_state* state;

    if ( frame == NULL || frame->host == NULL ) {
        return 0;
    }
    state = (xs2_handler_call_state*)frame->host;
    if ( state->response == NULL ) {
        return 0;
    }
    state->response->Length = 0u;
    return 1;
}

static int xs2HostAppendOutput(xs_handler_frame* frame, const void* data, size_t size)
{
    xs2_handler_call_state* state;

    if ( frame == NULL || frame->host == NULL || (data == NULL && size != 0u) || size > UINT32_MAX ) {
        return 0;
    }
    state = (xs2_handler_call_state*)frame->host;
    if ( state->response == NULL ) {
        return 0;
    }
    if ( size == 0u ) {
        return 1;
    }
    if ( !xrtBufferAppend(state->response, (void*)data, (uint32)size, XBUF_BINARY) ) {
        return 0;
    }
    frame->flags |= XS_HANDLER_FRAME_RESPONSE_STARTED;
    return 1;
}

static int xs2HostSetOutputText(xs_handler_frame* frame, const char* text)
{
    if ( !xs2HostClearOutput(frame) ) {
        return 0;
    }
    if ( text == NULL ) {
        text = "";
    }
    return xs2HostAppendOutput(frame, text, strlen(text));
}

static void xs2HostLog(const xs_handler_frame* frame, int level, const char* message)
{
    xloglevel log_level;

    (void)frame;
    if ( level < XS_HANDLER_LOG_TRACE ) {
        level = XS_HANDLER_LOG_TRACE;
    } else if ( level > XS_HANDLER_LOG_FATAL ) {
        level = XS_HANDLER_LOG_FATAL;
    }
    log_level = (xloglevel)level;
    xlogWrite(xlogDefault(), log_level, __FILE__, (uint32)__LINE__, __func__, "%s", message != NULL ? message : "");
}

static const xs_handler_host_api xs2HostApi = {
    (uint32_t)sizeof(xs_handler_host_api),
    XS_HANDLER_ABI_VERSION,
    xs2NowMs,
    xs2HostIsCancelled,
    xs2HostSetOutput,
    xs2HostSetOutputText,
    xs2HostLog,
    xs2HostAppendOutput,
    xs2HostClearOutput
};

/* ------------------------------------ 生命周期调用 ------------------------------------ */

static int xs2InvokeLifecycle(xs2_handler_instance* instance, int kind)
{
    xs_handler_frame frame;
    xs2_handler_call_state state;
    xbuffer response;
    int result;

    if ( instance == NULL || instance->entry == NULL ) {
        return 0;
    }
    response = xrtBufferCreate(256u);
    if ( response == NULL ) {
        return 0;
    }
    memset(&state, 0, sizeof(state));
    state.response = response;
    memset(&frame, 0, sizeof(frame));
    frame.struct_size = (uint32_t)sizeof(frame);
    frame.abi_version = XS_HANDLER_ABI_VERSION;
    frame.kind = (uint32_t)kind;
    frame.response = response;
    frame.api = &xs2HostApi;
    frame.host = &state;
    result = instance->entry(&frame);
    xrtBufferDestroy(response);
    return result == XS_HANDLER_OK;
}

static void xs2InstanceUnit(xs2_handler_instance* instance)
{
    if ( instance == NULL ) {
        return;
    }
    if ( instance->entry != NULL ) {
        (void)xs2InvokeLifecycle(instance, XS_HANDLER_KIND_STOP);
        (void)xs2InvokeLifecycle(instance, XS_HANDLER_KIND_UNIT);
    }
    if ( instance->module != NULL && instance->context != NULL ) {
        (void)xlDeactivateModule(instance->context, instance->module);
    }
    if ( instance->module != NULL ) {
        xlDestroyModule(instance->module);
    }
    if ( instance->context != NULL ) {
        xlDestroyContext(instance->context);
    }
    memset(instance, 0, sizeof(*instance));
}

static int xs2InstanceInit(
    xs2_handler_instance* instance,
    const char* source_path,
    const char* work_dir,
    const char* include_dir,
    char** error_text)
{
    XL_ContextOptions options;
    int is_c;

    memset(instance, 0, sizeof(*instance));
    options = xlDefaultContextOptions();
    options.workDir = work_dir;
    instance->context = xlCreateContext(&options);
    if ( instance->context == NULL ) {
        xs2SetError(error_text, "cannot create xlang handler context");
        return 0;
    }
    if ( include_dir != NULL && include_dir[0] != '\0' && !xlAddIncludePath(instance->context, include_dir) ) {
        xs2SetError(error_text, xlGetLastError(instance->context));
        return 0;
    }

    is_c = xs2PathIsC(source_path);
    if ( is_c ) {
        instance->module = xlCompileCSourceFileToModule(instance->context, source_path);
    } else {
        instance->module = xlCompileScriptFileToModule(instance->context, source_path);
    }
    if ( instance->module == NULL ) {
        xs2SetError(error_text, xlGetLastError(instance->context));
        return 0;
    }

    if ( !is_c && !xlActivateModule(instance->context, instance->module, NULL) ) {
        xs2SetError(error_text, xlGetLastError(instance->context));
        return 0;
    }
    instance->entry = (xs_handler_proc)xlModuleGetSymbol(instance->module, XS_HANDLER_ENTRY_NAME);
    if ( instance->entry == NULL ) {
        xs2SetError(error_text, "handler does not export xs_handler");
        return 0;
    }
    if ( !xs2InvokeLifecycle(instance, XS_HANDLER_KIND_INIT) ) {
        xs2SetError(error_text, "handler INIT lifecycle failed");
        return 0;
    }
    if ( !xs2InvokeLifecycle(instance, XS_HANDLER_KIND_START) ) {
        xs2SetError(error_text, "handler START lifecycle failed");
        return 0;
    }
    return 1;
}

/* ------------------------------------ Generation ------------------------------------ */

xs2_handler_generation* xs2GenerationCreate(
    const char* source_path,
    const char* work_dir,
    const char* include_dir,
    int pool_size,
    int queue_capacity,
    int queue_timeout_ms,
    int execution_timeout_ms,
    char** error_text)
{
    xs2_handler_generation* generation;
    int i;

    if ( error_text != NULL ) {
        *error_text = NULL;
    }
    if ( source_path == NULL || source_path[0] == '\0' || pool_size < 1 || pool_size > 256 ||
         queue_capacity < 0 || queue_capacity > 65536 || queue_timeout_ms < 0 || execution_timeout_ms < 0 ) {
        xs2SetError(error_text, "invalid handler generation options");
        return NULL;
    }

    generation = (xs2_handler_generation*)xrtCalloc(1u, sizeof(*generation));
    if ( generation == NULL ) {
        xs2SetError(error_text, "cannot allocate handler generation");
        return NULL;
    }
    generation->mutex = xrtMutexCreate();
    generation->changed = xrtCondCreate();
    generation->instances = (xs2_handler_instance*)xrtCalloc((size_t)pool_size, sizeof(xs2_handler_instance));
    if ( generation->mutex == NULL || generation->changed == NULL || generation->instances == NULL ) {
        xs2SetError(error_text, "cannot allocate handler generation synchronization state");
        xs2GenerationDestroy(generation);
        return NULL;
    }
    generation->instance_count = pool_size;
    generation->queue_capacity = queue_capacity;
    generation->queue_timeout_ms = queue_timeout_ms;
    generation->execution_timeout_ms = execution_timeout_ms;

    for ( i = 0; i < pool_size; ++i ) {
        if ( !xs2InstanceInit(&generation->instances[i], source_path, work_dir, include_dir, error_text) ) {
            xs2GenerationDestroy(generation);
            return NULL;
        }
    }
    return generation;
}

void xs2GenerationDestroy(xs2_handler_generation* generation)
{
    int i;

    if ( generation == NULL ) {
        return;
    }
    if ( generation->mutex != NULL ) {
        xrtMutexLock(generation->mutex);
        generation->closing = 1;
        if ( generation->changed != NULL ) {
            xrtCondBroadcast(generation->changed);
        }
        xrtMutexUnlock(generation->mutex);
    }
    if ( generation->instances != NULL ) {
        for ( i = 0; i < generation->instance_count; ++i ) {
            xs2InstanceUnit(&generation->instances[i]);
        }
        xrtFree(generation->instances);
    }
    if ( generation->changed != NULL ) {
        xrtCondDestroy(generation->changed);
    }
    if ( generation->mutex != NULL ) {
        xrtMutexDestroy(generation->mutex);
    }
    xrtFree(generation);
}

static int xs2GenerationAcquire(xs2_handler_generation* generation)
{
    uint64_t deadline;
    int i;
    int wait_result;

    deadline = generation->queue_timeout_ms > 0 ? xs2NowMs() + (uint64_t)generation->queue_timeout_ms : 0u;
    xrtMutexLock(generation->mutex);
    for ( ;; ) {
        if ( generation->closing ) {
            xrtMutexUnlock(generation->mutex);
            return -1;
        }
        for ( i = 0; i < generation->instance_count; ++i ) {
            if ( !generation->instances[i].busy ) {
                generation->instances[i].busy = 1;
                xrtMutexUnlock(generation->mutex);
                return i;
            }
        }
        if ( generation->queue_capacity == 0 || generation->waiting_count >= generation->queue_capacity ) {
            xrtMutexUnlock(generation->mutex);
            return -2;
        }
        if ( generation->queue_timeout_ms == 0 ) {
            xrtMutexUnlock(generation->mutex);
            return -3;
        }

        generation->waiting_count += 1;
        wait_result = xrtCondWaitTimeout(generation->changed, generation->mutex, (uint32)generation->queue_timeout_ms);
        generation->waiting_count -= 1;
        if ( wait_result == XRT_WAIT_TIMEOUT || (deadline != 0u && xs2NowMs() >= deadline) ) {
            xrtMutexUnlock(generation->mutex);
            return -3;
        }
        if ( wait_result == XRT_WAIT_ERROR ) {
            xrtMutexUnlock(generation->mutex);
            return -1;
        }
    }
}

static void xs2GenerationRelease(xs2_handler_generation* generation, int index)
{
    xrtMutexLock(generation->mutex);
    generation->instances[index].busy = 0;
    xrtCondSignal(generation->changed);
    xrtMutexUnlock(generation->mutex);
}

int xs2GenerationCall(
    xs2_handler_generation* generation,
    uint64_t request_id,
    int kind,
    const char* service_name,
    const char* route_name,
    void* request_handle,
    void* response_handle,
    const void* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    void* response_buffer,
    char** error_text)
{
    xs2_handler_call_state state;
    xs_handler_frame frame;
    uint64_t started;
    uint64_t elapsed;
    int index;
    int result;

    if ( error_text != NULL ) {
        *error_text = NULL;
    }
    if ( generation == NULL || response_buffer == NULL || (data == NULL && data_size != 0u) || remote_port > 65535u ) {
        xs2SetError(error_text, "invalid handler call arguments");
        return XS_HANDLER_BAD_REQUEST;
    }
    index = xs2GenerationAcquire(generation);
    if ( index == -2 ) {
        xs2SetError(error_text, "handler queue is full");
        return XS_HANDLER_OVERLOADED;
    }
    if ( index == -3 ) {
        xs2SetError(error_text, "handler queue wait timed out");
        return XS_HANDLER_TIMEOUT;
    }
    if ( index < 0 ) {
        xs2SetError(error_text, "handler generation is unavailable");
        return XS_HANDLER_UNAVAILABLE;
    }

    memset(&state, 0, sizeof(state));
    state.generation = generation;
    state.response = (xbuffer)response_buffer;
    state.response->Length = 0u;

    memset(&frame, 0, sizeof(frame));
    frame.struct_size = (uint32_t)sizeof(frame);
    frame.abi_version = XS_HANDLER_ABI_VERSION;
    frame.kind = (uint32_t)kind;
    frame.request_id = request_id;
    frame.service_name = service_name != NULL ? service_name : "";
    frame.route_name = route_name != NULL ? route_name : "";
    frame.request = request_handle;
    frame.response = response_handle;
    frame.api = &xs2HostApi;
    frame.host = &state;
    frame.data = data;
    frame.data_size = data_size;
    frame.remote_address = remote_address != NULL ? remote_address : "";
    frame.remote_port = (uint16_t)remote_port;
    started = xs2NowMs();
    if ( generation->execution_timeout_ms > 0 ) {
        frame.deadline_ms = started + (uint64_t)generation->execution_timeout_ms;
    }

    result = generation->instances[index].entry(&frame);
    elapsed = xs2NowMs() - started;
    if ( generation->execution_timeout_ms > 0 && elapsed > (uint64_t)generation->execution_timeout_ms ) {
        state.response->Length = 0u;
        xs2SetError(error_text, "handler execution exceeded its deadline");
        result = XS_HANDLER_TIMEOUT;
    } else if ( result < XS_HANDLER_OK || result > XS_HANDLER_UNAVAILABLE ) {
        xrtBufferClear(state.response);
        xs2SetError(error_text, "handler returned an invalid result code");
        result = XS_HANDLER_ERROR;
    }
    xs2GenerationRelease(generation, index);
    return result;
}

int xs2GenerationPoolSize(const xs2_handler_generation* generation)
{
    return generation != NULL ? generation->instance_count : 0;
}
