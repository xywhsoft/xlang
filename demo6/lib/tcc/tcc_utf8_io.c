#define TCC_UTF8_IO_NO_REMAP

#include "tcc_utf8_io.h"

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdlib.h>
#include <windows.h>

static wchar_t *tcc_utf8_to_wide(const char *text)
{
    int count;
    wchar_t *wide;

    if (text == NULL) {
        text = "";
    }

    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (count <= 0) {
        errno = EINVAL;
        return NULL;
    }

    wide = (wchar_t *)malloc((size_t)count * sizeof(wchar_t));
    if (wide == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count) <= 0) {
        free(wide);
        errno = EINVAL;
        return NULL;
    }
    return wide;
}

int tcc_utf8_open(const char *path, int flags, ...)
{
    wchar_t *wide;
    int mode;
    int fd;

    mode = 0;
    if ((flags & O_CREAT) != 0) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    wide = tcc_utf8_to_wide(path);
    if (wide == NULL) {
        return -1;
    }

    fd = _wopen(wide, flags, mode);
    free(wide);
    return fd;
}

FILE *tcc_utf8_fopen(const char *path, const char *mode)
{
    wchar_t *widePath;
    wchar_t *wideMode;
    FILE *file;

    widePath = tcc_utf8_to_wide(path);
    wideMode = tcc_utf8_to_wide(mode);
    if (widePath == NULL || wideMode == NULL) {
        free(widePath);
        free(wideMode);
        return NULL;
    }

    file = _wfopen(widePath, wideMode);
    free(widePath);
    free(wideMode);
    return file;
}

int tcc_utf8_unlink(const char *path)
{
    wchar_t *wide;
    int rc;

    wide = tcc_utf8_to_wide(path);
    if (wide == NULL) {
        return -1;
    }

    rc = _wunlink(wide);
    free(wide);
    return rc;
}
