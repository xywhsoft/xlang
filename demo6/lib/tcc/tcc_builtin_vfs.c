/*
 * Optional builtin resource VFS for TCC.
 *
 * Policy:
 *   1. Try the external filesystem first.
 *   2. If a read-only open fails, try the embedded resource table.
 *
 * The embedded table uses virtual names such as "include/stdio.h" and
 * "lib/libtcc1.a". Runtime lookup only accepts those normalized virtual names.
 */
#include "tcc_builtin_vfs.h"

#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
# include <io.h>
# include <share.h>
# include <sys/stat.h>
# include "tcc_utf8_io.h"
#else
# include <unistd.h>
# include <sys/stat.h>
#endif

#include "LzmaDec.h"

#ifndef O_BINARY
# define O_BINARY 0
#endif
#ifndef O_ACCMODE
# define O_ACCMODE 3
#endif

#define TCC_VFS_FD_BASE 0x40000000
#define TCC_VFS_MAX_OPEN 64
#define TCC_VFS_KEY_MAX 4096

typedef struct TCCVfsOpenFile {
    const unsigned char *data;
    unsigned char *owned_data;
    size_t size;
    size_t pos;
    int used;
} TCCVfsOpenFile;

static TCCVfsOpenFile tcc_vfs_open_files[TCC_VFS_MAX_OPEN];

static int tcc_vfs_is_virtual_fd(int fd)
{
    int slot = fd - TCC_VFS_FD_BASE;
    return slot >= 0
        && slot < TCC_VFS_MAX_OPEN
        && tcc_vfs_open_files[slot].used;
}

static TCCVfsOpenFile *tcc_vfs_get_file(int fd)
{
    if (!tcc_vfs_is_virtual_fd(fd))
        return NULL;
    return &tcc_vfs_open_files[fd - TCC_VFS_FD_BASE];
}

static void tcc_vfs_normalize_key(const char *path, char *out, size_t out_size)
{
    size_t i;
    size_t j;
    size_t pos;
    char segment[TCC_VFS_KEY_MAX];

    if (!out_size)
        return;
    if (!path)
        path = "";
    out[0] = '\0';
    pos = 0;
    i = 0;
    while (path[i]) {
        while (path[i] == '/' || path[i] == '\\')
            ++i;

        j = 0;
        while (path[i] && path[i] != '/' && path[i] != '\\') {
            unsigned char ch = (unsigned char)path[i++];
            if (ch >= 'A' && ch <= 'Z')
                ch = (unsigned char)(ch - 'A' + 'a');
            if (j + 1 < sizeof segment)
                segment[j++] = (char)ch;
        }
        segment[j] = '\0';

        if (j == 0 || strcmp(segment, ".") == 0)
            continue;

        if (strcmp(segment, "..") == 0) {
            if (pos > 0 && strcmp(out, "..") != 0) {
                while (pos > 0 && out[pos - 1] != '/')
                    --pos;
                if (pos > 0)
                    --pos;
                out[pos] = '\0';
            } else if (pos + 2 < out_size) {
                if (pos > 0)
                    out[pos++] = '/';
                out[pos++] = '.';
                out[pos++] = '.';
                out[pos] = '\0';
            }
            continue;
        }

        if (pos > 0) {
            if (pos + 1 >= out_size)
                break;
            out[pos++] = '/';
        }
        for (j = 0; segment[j]; ++j) {
            if (pos + 1 >= out_size)
                break;
            out[pos++] = segment[j];
        }
        out[pos] = '\0';
    }
}

static const TCCBuiltinResource *tcc_vfs_lookup_exact(const char *key)
{
    unsigned int i;

    if (!key || key[0] == '\0')
        return NULL;

    for (i = 0; i < tcc_builtin_resources_count; ++i) {
        const TCCBuiltinResource *res = &tcc_builtin_resources[i];
        if (res->name && strcmp(res->name, key) == 0)
            return res;
    }
    return NULL;
}

static const TCCBuiltinResource *tcc_vfs_lookup_resource(const char *path)
{
    char norm[TCC_VFS_KEY_MAX];

    tcc_vfs_normalize_key(path, norm, sizeof norm);
    return tcc_vfs_lookup_exact(norm);
}

static int tcc_vfs_materialize_resource(
    const TCCBuiltinResource *res,
    const unsigned char **out_data,
    unsigned char **out_owned_data,
    size_t *out_size);

static int tcc_vfs_alloc_fd(const TCCBuiltinResource *res)
{
    int i;
    const unsigned char *data;
    unsigned char *owned_data;
    size_t size;

    if (!tcc_vfs_materialize_resource(res, &data, &owned_data, &size))
        return -1;

    for (i = 0; i < TCC_VFS_MAX_OPEN; ++i) {
        TCCVfsOpenFile *vf = &tcc_vfs_open_files[i];
        if (!vf->used) {
            vf->data = data;
            vf->owned_data = owned_data;
            vf->size = size;
            vf->pos = 0;
            vf->used = 1;
            return TCC_VFS_FD_BASE + i;
        }
    }
    if (owned_data)
        free(owned_data);
    errno = EMFILE;
    return -1;
}

static int tcc_vfs_is_readonly_open(int flags)
{
    return (flags & O_ACCMODE) == O_RDONLY;
}

static void *tcc_vfs_lzma_alloc(ISzAllocPtr alloc, size_t size)
{
    (void)alloc;
    if (size == 0)
        return NULL;
    return malloc(size);
}

static void tcc_vfs_lzma_free(ISzAllocPtr alloc, void *address)
{
    (void)alloc;
    free(address);
}

static const ISzAlloc tcc_vfs_lzma_allocator = {
    tcc_vfs_lzma_alloc,
    tcc_vfs_lzma_free
};

static int tcc_vfs_decode_lzma(
    const unsigned char *src,
    size_t src_size,
    unsigned char *dst,
    size_t dst_size)
{
    SizeT in_size;
    SizeT out_size;
    ELzmaStatus status;
    SRes result;

    if (src_size < LZMA_PROPS_SIZE)
        return 0;

    in_size = (SizeT)(src_size - LZMA_PROPS_SIZE);
    out_size = (SizeT)dst_size;
    result = LzmaDecode(
        (Byte *)dst,
        &out_size,
        (const Byte *)src + LZMA_PROPS_SIZE,
        &in_size,
        (const Byte *)src,
        LZMA_PROPS_SIZE,
        LZMA_FINISH_END,
        &status,
        &tcc_vfs_lzma_allocator);

    return result == SZ_OK
        && out_size == (SizeT)dst_size
        && in_size == (SizeT)(src_size - LZMA_PROPS_SIZE);
}

static int tcc_vfs_materialize_resource(
    const TCCBuiltinResource *res,
    const unsigned char **out_data,
    unsigned char **out_owned_data,
    size_t *out_size)
{
    unsigned char *data;

    if (!out_data || !out_owned_data || !out_size)
        return 0;
    *out_data = NULL;
    *out_owned_data = NULL;
    *out_size = 0;

    if (!res || !res->data) {
        errno = ENOENT;
        return 0;
    }

    if (res->method == TCC_BUILTIN_RESOURCE_STORE) {
        *out_data = res->data;
        *out_size = res->size;
        return 1;
    }

    if (res->method != TCC_BUILTIN_RESOURCE_LZMA) {
        errno = EINVAL;
        return 0;
    }

    data = (unsigned char *)malloc(res->size ? res->size : 1u);
    if (!data) {
        errno = ENOMEM;
        return 0;
    }
    if (!tcc_vfs_decode_lzma(res->data, res->packed_size, data, res->size)) {
        free(data);
        errno = EINVAL;
        return 0;
    }

    *out_data = data;
    *out_owned_data = data;
    *out_size = res->size;
    return 1;
}

#ifdef _WIN32
static int tcc_vfs_real_open(const char *path, int flags, int mode)
{
    return tcc_utf8_open(path, flags, mode);
}

static FILE *tcc_vfs_real_fopen(const char *path, const char *mode)
{
    return tcc_utf8_fopen(path, mode);
}
#else
static int tcc_vfs_real_open(const char *path, int flags, int mode)
{
    return open(path, flags, mode);
}

static FILE *tcc_vfs_real_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}
#endif

static int tcc_vfs_is_readonly_fopen(const char *mode)
{
    return mode && mode[0] == 'r' && strchr(mode, '+') == NULL;
}

static FILE *tcc_vfs_fopen_resource(const TCCBuiltinResource *res)
{
    FILE *file;
    const unsigned char *data;
    unsigned char *owned_data;
    size_t size;

    if (!tcc_vfs_materialize_resource(res, &data, &owned_data, &size))
        return NULL;

    file = tmpfile();
    if (!file) {
        if (owned_data)
            free(owned_data);
        return NULL;
    }

    if (size && fwrite(data, 1, size, file) != size) {
        if (owned_data)
            free(owned_data);
        fclose(file);
        return NULL;
    }
    if (owned_data)
        free(owned_data);
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    return file;
}

TCC_VFS_API int tcc_vfs_open(const char *path, int flags, ...)
{
    int fd, mode = 0;
    const TCCBuiltinResource *res;
    va_list ap;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    fd = tcc_vfs_real_open(path, flags, mode);
    if (fd >= 0 || !tcc_vfs_is_readonly_open(flags))
        return fd;

    res = tcc_vfs_lookup_resource(path);
    if (!res)
        return -1;
    return tcc_vfs_alloc_fd(res);
}

TCC_VFS_API int tcc_vfs_close(int fd)
{
    TCCVfsOpenFile *vf = tcc_vfs_get_file(fd);
    if (vf) {
        if (vf->owned_data)
            free(vf->owned_data);
        memset(vf, 0, sizeof *vf);
        return 0;
    }
#ifdef _WIN32
    return _close(fd);
#else
    return close(fd);
#endif
}

TCC_VFS_API int tcc_vfs_read(int fd, void *buf, size_t count)
{
    TCCVfsOpenFile *vf = tcc_vfs_get_file(fd);
    if (vf) {
        size_t n = 0;
        if (vf->pos < vf->size) {
            n = vf->size - vf->pos;
            if (n > count)
                n = count;
        }
        if (n) {
            memcpy(buf, vf->data + vf->pos, n);
            vf->pos += n;
        }
        if (n > (size_t)INT_MAX)
            n = INT_MAX;
        return (int)n;
    }
#ifdef _WIN32
    return _read(fd, buf, (unsigned int)count);
#else
    return (int)read(fd, buf, count);
#endif
}

TCC_VFS_API long tcc_vfs_lseek(int fd, long offset, int whence)
{
    TCCVfsOpenFile *vf = tcc_vfs_get_file(fd);
    if (vf) {
        long base;
        long pos;
        switch (whence) {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = (long)vf->pos;
            break;
        case SEEK_END:
            base = (long)vf->size;
            break;
        default:
            errno = EINVAL;
            return -1;
        }
        pos = base + offset;
        if (pos < 0) {
            errno = EINVAL;
            return -1;
        }
        vf->pos = (size_t)pos;
        return pos;
    }
#ifdef _WIN32
    return _lseek(fd, offset, whence);
#else
    return (long)lseek(fd, offset, whence);
#endif
}

TCC_VFS_API FILE *tcc_vfs_fopen(const char *path, const char *mode)
{
    FILE *file;
    const TCCBuiltinResource *res;

    file = tcc_vfs_real_fopen(path, mode);
    if (file || !tcc_vfs_is_readonly_fopen(mode))
        return file;

    res = tcc_vfs_lookup_resource(path);
    return tcc_vfs_fopen_resource(res);
}

TCC_VFS_API int tcc_vfs_fclose(FILE *file)
{
    return fclose(file);
}
