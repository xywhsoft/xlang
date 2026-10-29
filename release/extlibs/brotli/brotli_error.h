#ifndef XLANG_BROTLI_ERROR_H
#define XLANG_BROTLI_ERROR_H

/* 文件层和语言薄绑定共用错误映射，不改变原有类别、域和代码。 */
static inline xerrkind brotli_error_kind(int code)
{
    switch (code) {
    case XRT_BROTLI_ERROR_ARGUMENT: return XERR_ARGUMENT;
    case XRT_BROTLI_ERROR_MEMORY: return XERR_MEMORY;
    case XRT_BROTLI_ERROR_DATA:
    case XRT_BROTLI_ERROR_TRUNCATED: return XERR_PROTOCOL;
    case XRT_BROTLI_ERROR_LIMIT: return XERR_RANGE;
    case XRT_BROTLI_ERROR_STATE: return XERR_STATE;
    case XRT_BROTLI_ERROR_CANCELLED: return XERR_CANCELLED;
    default: return XERR_IO;
    }
}

static inline void brotli_publish_error(const xrtbrotlierror* error)
{
    const char* message = error != NULL && error->sMessage[0] != 0 ?
        error->sMessage : "brotli operation failed";
    int code = error != NULL ? error->iCode : XRT_BROTLI_ERROR_IO;
    xrtSetErrorInfo(brotli_error_kind(code), "xlang.extlib.brotli", code, message);
}

#endif
