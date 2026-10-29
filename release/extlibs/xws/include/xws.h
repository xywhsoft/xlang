#ifndef XLANG_EXTLIB_XWS_H
#define XLANG_EXTLIB_XWS_H

/* xlang 的 xws 基础包固定提供连接、TLS、压缩、Future、Writer 和 Group。 */
#define XWS_MODULE_WEBSOCKET_WRITER_DEFLATE
#define XWS_MODULE_WEBSOCKET_WRITER_REF
#define XWS_MODULE_WEBSOCKET_GROUP_FUTURE
#define XWS_MODULE_WEBSOCKET_CONNECTION_TLS

#include "xws/features.h"
#include "xrt/websocket_runtime.h"
#include "xrt/websocket_group.h"

#endif
