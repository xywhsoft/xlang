#ifndef XS_HANDLER_H
#define XS_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * xs 动态处理器的稳定 C ABI。
 *
 * 所有结构都以 struct_size 和 abi_version 开头。新增字段只能追加到结构
 * 尾部，读取方必须先检查 struct_size，不能假定新字段一定存在。
 */

#define XS_HANDLER_ABI_VERSION 1u
#define XS_HANDLER_ENTRY_NAME "xs_handler"

typedef struct xvalue_struct* xvalue;
typedef struct xs_handler_frame xs_handler_frame;
typedef struct xs_handler_host_api xs_handler_host_api;

typedef enum xs_handler_kind {
    XS_HANDLER_KIND_NONE = 0,
    XS_HANDLER_KIND_HTTP = 1,
    XS_HANDLER_KIND_TCP_OPEN = 2,
    XS_HANDLER_KIND_TCP_DATA = 3,
    XS_HANDLER_KIND_TCP_CLOSE = 4,
    XS_HANDLER_KIND_UDP = 5,
    XS_HANDLER_KIND_INIT = 6,
    XS_HANDLER_KIND_START = 7,
    XS_HANDLER_KIND_STOP = 8,
    XS_HANDLER_KIND_UNIT = 9
} xs_handler_kind;

typedef enum xs_handler_result {
    XS_HANDLER_OK = 0,
    XS_HANDLER_ERROR = 1,
    XS_HANDLER_BAD_REQUEST = 2,
    XS_HANDLER_UNAUTHORIZED = 3,
    XS_HANDLER_FORBIDDEN = 4,
    XS_HANDLER_NOT_FOUND = 5,
    XS_HANDLER_TIMEOUT = 6,
    XS_HANDLER_OVERLOADED = 7,
    XS_HANDLER_UNAVAILABLE = 8
} xs_handler_result;

typedef enum xs_handler_frame_flags {
    XS_HANDLER_FRAME_NONE = 0,
    XS_HANDLER_FRAME_TLS = 1u << 0,
    XS_HANDLER_FRAME_CANCELLED = 1u << 1,
    XS_HANDLER_FRAME_RESPONSE_STARTED = 1u << 2
} xs_handler_frame_flags;

typedef enum xs_handler_log_level {
    XS_HANDLER_LOG_TRACE = 0,
    XS_HANDLER_LOG_DEBUG = 1,
    XS_HANDLER_LOG_INFO = 2,
    XS_HANDLER_LOG_WARN = 3,
    XS_HANDLER_LOG_ERROR = 4,
    XS_HANDLER_LOG_FATAL = 5
} xs_handler_log_level;

/*
 * 宿主 helper 表只提供与 handler 生命周期相关的窄接口。
 * HTTP、TCP、UDP 的完整能力仍通过 frame 中的 XRT 句柄和公开 XRT API 使用。
 */
struct xs_handler_host_api {
    uint32_t struct_size;
    uint32_t abi_version;

    uint64_t (*now_ms)(void);
    int (*is_cancelled)(const xs_handler_frame* frame);
    int (*set_output)(xs_handler_frame* frame, xvalue value);
    int (*set_output_text)(xs_handler_frame* frame, const char* text);
    void (*log)(const xs_handler_frame* frame, int level, const char* message);

    /* ABI v1 追加字段：二进制响应不经过 C 字符串。 */
    int (*append_output)(xs_handler_frame* frame, const void* data, size_t size);
    int (*clear_output)(xs_handler_frame* frame);
};

struct xs_handler_frame {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t kind;
    uint32_t flags;

    uint64_t request_id;
    uint64_t deadline_ms;

    const char* service_name;
    const char* route_name;

    void* request;
    void* response;

    xvalue input;
    xvalue output;

    const xs_handler_host_api* api;
    void* host;

    /*
     * request/response 是协议原生句柄；data/data_size 是当前数据片段。
     * response 由宿主管理，处理器应通过 host_api 写入，不能释放。
     */
    const void* data;
    size_t data_size;

    const char* remote_address;
    uint16_t remote_port;
    uint16_t reserved_port;
};

typedef int (*xs_handler_proc)(xs_handler_frame* frame);

/* 动态模块必须导出同签名、名为 XS_HANDLER_ENTRY_NAME 的函数。 */
int xs_handler(xs_handler_frame* frame);

static inline int xsHandlerFrameIsValid(const xs_handler_frame* frame)
{
    return frame != NULL &&
           frame->struct_size >= (uint32_t)offsetof(xs_handler_frame, host) + (uint32_t)sizeof(frame->host) &&
           frame->abi_version == XS_HANDLER_ABI_VERSION;
}

static inline int xsHandlerApiIsValid(const xs_handler_host_api* api)
{
    return api != NULL &&
           api->struct_size >= (uint32_t)offsetof(xs_handler_host_api, log) + (uint32_t)sizeof(api->log) &&
           api->abi_version == XS_HANDLER_ABI_VERSION;
}

static inline int xsHandlerFrameHasData(const xs_handler_frame* frame)
{
    return xsHandlerFrameIsValid(frame) &&
           frame->struct_size >= (uint32_t)offsetof(xs_handler_frame, data_size) + (uint32_t)sizeof(frame->data_size);
}

static inline int xsHandlerApiHasBinaryOutput(const xs_handler_host_api* api)
{
    return xsHandlerApiIsValid(api) &&
           api->struct_size >= (uint32_t)offsetof(xs_handler_host_api, clear_output) + (uint32_t)sizeof(api->clear_output) &&
           api->append_output != NULL;
}

#ifdef __cplusplus
}
#endif

#endif
