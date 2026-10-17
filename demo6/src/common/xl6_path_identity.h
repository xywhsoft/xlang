#ifndef XL6_PATH_IDENTITY_H
#define XL6_PATH_IDENTITY_H

#include "src/common/xl6_common.h"
#ifdef _WIN32
#include <windows.h>
#endif

/* Lexical path identity: 1 equal, 0 different, -1 conversion/resource error.
 * A failed comparison is never evidence that a snapshot is absent. */
static inline int xl6PathIdentityEqual(const char* a, const char* b)
{
    if (!a || !b) return -1;
#ifdef _WIN32
    wchar_t localA[256], localB[256];
    wchar_t *wa = localA, *wb = localB;
    int result = -1;
    int na = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a, -1, NULL, 0);
    int nb;
    if (na <= 0) return -1;
    if (!strcmp(a, b)) return 1; /* Strictly valid, identical UTF-8; no heap. */
    nb = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b, -1, NULL, 0);
    if (nb <= 0 || (size_t)na > SIZE_MAX / sizeof(*wa) ||
        (size_t)nb > SIZE_MAX / sizeof(*wb)) return -1;
    if ((size_t)na > XL6_ARRAY_COUNT(localA)) {
        wa = (wchar_t*)xrtMalloc((size_t)na * sizeof(*wa));
        if (!wa) return -1;
    }
    if ((size_t)nb > XL6_ARRAY_COUNT(localB)) {
        wb = (wchar_t*)xrtMalloc((size_t)nb * sizeof(*wb));
        if (!wb) goto done;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a, -1, wa, na) &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b, -1, wb, nb)) {
        int compared = CompareStringOrdinal(wa, -1, wb, -1, TRUE);
        if (compared) result = compared == CSTR_EQUAL;
    }
done:
    if (wa != localA) xrtFree(wa);
    if (wb != localB) xrtFree(wb);
    return result;
#else
    return !strcmp(a, b);
#endif
}

#endif
