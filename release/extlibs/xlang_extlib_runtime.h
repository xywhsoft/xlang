#ifndef XLANG_EXTLIB_RUNTIME_H
#define XLANG_EXTLIB_RUNTIME_H

/*
 * Shared glue for source extension libraries.
 *
 * Include this after the package's XRT header.  It deliberately relies only
 * on the public current XRT API, so package code does not carry legacy xvo,
 * xrtSetError(message, take), or xrtBufferAdopt contracts forward.
 */

static void xl6ExtlibSetError(xerrkind kind, const char *domain,
    const char *message)
{
    xrtSetErrorInfo(kind, domain, 0, message != NULL ? message : "extension failure");
}

static void *xl6ExtlibBufferTake(bytes data, size_t size)
{
    xbuffer *buffer;

    if (size == 0u) {
        xrtFree(data);
        return xrtBufferCreate();
    }

    buffer = xrtBufferCreateTake(&data, size, size);
    if (buffer == NULL && data != NULL) {
        xrtFree(data);
    }
    return buffer;
}

#endif
