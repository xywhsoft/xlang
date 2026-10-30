/*
 * Optional builtin resource VFS for TCC.
 *
 * Policy:
 *   1. Try the dynamic per-compile memory VFS first.
 *   2. Try the external filesystem.
 *   3. If a read-only open still fails, try the embedded resource table.
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
#include <stdint.h>

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

#ifndef EOVERFLOW
# define EOVERFLOW EINVAL
#endif

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
    const TCCBuiltinResource *resource;
    unsigned char *owned_data;
    size_t size;
    size_t pos;
    int used;
} TCCVfsOpenFile;

typedef struct TCCVfsDynamicResource {
    char *name;
    unsigned char *data;
    size_t size;
} TCCVfsDynamicResource;

static TCCVfsOpenFile tcc_vfs_open_files[TCC_VFS_MAX_OPEN];
static TCCVfsDynamicResource *tcc_vfs_dynamic_resources;
static unsigned int tcc_vfs_dynamic_count;
static unsigned int tcc_vfs_dynamic_capacity;
static TCCVfsDiskReader tcc_vfs_disk_reader;
static void *tcc_vfs_disk_reader_opaque;

void tcc_vfs_set_disk_reader(TCCVfsDiskReader reader, void *opaque)
{
    tcc_vfs_disk_reader = reader;
    tcc_vfs_disk_reader_opaque = opaque;
}

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
    unsigned int low=0,high=tcc_builtin_resources_count;
    if(!key||!key[0])return NULL;
    while(low<high){
        unsigned int middle=low+(high-low)/2;
        const TCCBuiltinResource* resource=&tcc_builtin_resources[middle];
        const char* name=tcc_builtin_resource_name(resource);
        if(!name){errno=EINVAL;return NULL;}
        int compared=strcmp(key,name);
        if(!compared)return resource;
        if(compared<0)high=middle;else low=middle+1;
    }
    return NULL;
}

static const TCCBuiltinResource *tcc_vfs_lookup_resource(const char *path)
{
    char norm[TCC_VFS_KEY_MAX];

    tcc_vfs_normalize_key(path, norm, sizeof norm);
    return tcc_vfs_lookup_exact(norm);
}

static char *tcc_vfs_copy_key(const char *key)
{
    size_t len;
    char *copy;

    if (!key)
        return NULL;
    len = strlen(key);
    copy = (char *)malloc(len + 1);
    if (!copy)
        return NULL;
    memcpy(copy, key, len + 1);
    return copy;
}

static int tcc_vfs_find_dynamic_key(const char *key)
{
    unsigned int i;

    if (!key || key[0] == '\0')
        return -1;
    for (i = 0; i < tcc_vfs_dynamic_count; ++i) {
        if (tcc_vfs_dynamic_resources[i].name &&
            strcmp(tcc_vfs_dynamic_resources[i].name, key) == 0)
            return (int)i;
    }
    return -1;
}

static int tcc_vfs_lookup_fixed_object(const char *path,const TCCBuiltinResource **result)
{
    char key[TCC_VFS_KEY_MAX];
    if(!path)return 0;
    tcc_vfs_normalize_key(path,key,sizeof(key));
    if(strncmp(key,"__xl_objects/",13)!=0)return 0;
    *result=tcc_vfs_lookup_exact(key);
    if(!*result)errno=ENOENT;
    return 1;
}

static const TCCVfsDynamicResource *tcc_vfs_lookup_dynamic(const char *path)
{
    char norm[TCC_VFS_KEY_MAX];
    int index;

    tcc_vfs_normalize_key(path, norm, sizeof norm);
    index = tcc_vfs_find_dynamic_key(norm);
    return index >= 0 ? &tcc_vfs_dynamic_resources[index] : NULL;
}

static void tcc_vfs_free_dynamic_resource(TCCVfsDynamicResource *res)
{
    if (!res)
        return;
    free(res->name);
    free(res->data);
    memset(res, 0, sizeof *res);
}

static void tcc_vfs_clear_cache(void);

TCC_VFS_API void tcc_vfs_clear_dynamic(void)
{
    unsigned int i;

    for (i = 0; i < tcc_vfs_dynamic_count; ++i)
        tcc_vfs_free_dynamic_resource(&tcc_vfs_dynamic_resources[i]);
    free(tcc_vfs_dynamic_resources);
    tcc_vfs_dynamic_resources = NULL;
    tcc_vfs_dynamic_count = 0;
    tcc_vfs_dynamic_capacity = 0;
    tcc_vfs_clear_cache();
}

TCC_VFS_API int tcc_vfs_mount_memory(const char *path, const void *data, size_t size)
{
    char norm[TCC_VFS_KEY_MAX];
    char *name_copy;
    unsigned char *data_copy;
    TCCVfsDynamicResource *new_items;
    unsigned int new_capacity;
    int index;

    tcc_vfs_normalize_key(path, norm, sizeof norm);
    if (norm[0] == '\0' || (size > 0 && !data)) {
        errno = EINVAL;
        return 0;
    }

    name_copy = tcc_vfs_copy_key(norm);
    if (!name_copy) {
        errno = ENOMEM;
        return 0;
    }

    data_copy = NULL;
    if (size > 0) {
        data_copy = (unsigned char *)malloc(size);
        if (!data_copy) {
            free(name_copy);
            errno = ENOMEM;
            return 0;
        }
        memcpy(data_copy, data, size);
    }

    index = tcc_vfs_find_dynamic_key(norm);
    if (index >= 0) {
        TCCVfsDynamicResource *res = &tcc_vfs_dynamic_resources[index];
        free(res->name);
        free(res->data);
        res->name = name_copy;
        res->data = data_copy;
        res->size = size;
        return 1;
    }

    if (tcc_vfs_dynamic_count == tcc_vfs_dynamic_capacity) {
        new_capacity = tcc_vfs_dynamic_capacity == 0 ? 8u : tcc_vfs_dynamic_capacity * 2u;
        new_items = (TCCVfsDynamicResource *)realloc(
            tcc_vfs_dynamic_resources,
            new_capacity * sizeof(TCCVfsDynamicResource));
        if (!new_items) {
            free(name_copy);
            free(data_copy);
            errno = ENOMEM;
            return 0;
        }
        memset(new_items + tcc_vfs_dynamic_capacity, 0,
               (new_capacity - tcc_vfs_dynamic_capacity) * sizeof(TCCVfsDynamicResource));
        tcc_vfs_dynamic_resources = new_items;
        tcc_vfs_dynamic_capacity = new_capacity;
    }

    tcc_vfs_dynamic_resources[tcc_vfs_dynamic_count].name = name_copy;
    tcc_vfs_dynamic_resources[tcc_vfs_dynamic_count].data = data_copy;
    tcc_vfs_dynamic_resources[tcc_vfs_dynamic_count].size = size;
    tcc_vfs_dynamic_count += 1;
    return 1;
}

static int tcc_vfs_materialize_resource(
    const TCCBuiltinResource *res,
    const unsigned char **out_data,
    unsigned char **out_owned_data,
    size_t *out_size);

static int tcc_vfs_alloc_materialized_fd(const unsigned char *data, unsigned char *owned_data, size_t size)
{
    int i;

    for (i = 0; i < TCC_VFS_MAX_OPEN; ++i) {
        TCCVfsOpenFile *vf = &tcc_vfs_open_files[i];
        if (!vf->used) {
            vf->data = data;
            vf->resource = NULL;
            vf->owned_data = owned_data;
            vf->size = size;
            vf->pos = 0;
            vf->used = 1;
            return TCC_VFS_FD_BASE + i;
        }
    }

    free(owned_data);
    errno = EMFILE;
    return -1;
}

static int tcc_vfs_alloc_bytes_fd(const unsigned char *data, size_t size, int copy)
{
    unsigned char *owned_data;

    owned_data = NULL;
    if (copy && size > 0) {
        owned_data = (unsigned char *)malloc(size);
        if (!owned_data) {
            errno = ENOMEM;
            return -1;
        }
        memcpy(owned_data, data, size);
        data = owned_data;
    }
    return tcc_vfs_alloc_materialized_fd(data, owned_data, size);
}

static int tcc_vfs_alloc_dynamic_fd(const TCCVfsDynamicResource *res)
{
    if (!res) {
        errno = ENOENT;
        return -1;
    }
    return tcc_vfs_alloc_bytes_fd(res->data, res->size, 1);
}

static int tcc_vfs_alloc_fd(const TCCBuiltinResource *res)
{
    int fd;
    if (res->size>SIZE_MAX) { errno=EOVERFLOW; return -1; }
    fd = tcc_vfs_alloc_materialized_fd(NULL, NULL, (size_t)res->size);
    TCCVfsOpenFile *vf = tcc_vfs_get_file(fd);
    if (vf) vf->resource = res;
    return fd;
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

/* Fixed capacity: 8 x 1 MiB decoded blocks (8 MiB maximum). File
 * descriptors retain catalog extents, never an evictable cache address. */
#define TCC_VFS_CACHE_SLOTS 8
#define TCC_VFS_CACHE_BLOCK_MAX 1048576u
typedef struct TCCVfsCacheEntry {
    const TCCBuiltinBlob *blob;
    unsigned char *data;
    uint64_t age;
} TCCVfsCacheEntry;
static TCCVfsCacheEntry tcc_vfs_cache[TCC_VFS_CACHE_SLOTS];
static uint64_t tcc_vfs_cache_age;
static void tcc_vfs_clear_cache(void)
{
    unsigned int i;
    for (i=0;i<TCC_VFS_CACHE_SLOTS;++i) {
        free(tcc_vfs_cache[i].data);
        memset(&tcc_vfs_cache[i],0,sizeof(tcc_vfs_cache[i]));
    }
    tcc_vfs_cache_age=0;
}
static const unsigned char *tcc_vfs_blob_data(const TCCBuiltinBlob *blob, unsigned char **temporary)
{
    unsigned int i, victim=0;
    unsigned char *data;
    *temporary=NULL;
    if (!blob || !blob->data) { errno=EINVAL; return NULL; }
    if (blob->method==TCC_BUILTIN_RESOURCE_STORE) {
        if (blob->packed_size!=blob->size) { errno=EINVAL; return NULL; }
        return blob->data;
    }
    if (blob->method!=TCC_BUILTIN_RESOURCE_LZMA) { errno=EINVAL; return NULL; }
    if (++tcc_vfs_cache_age==0) {
        for (i=0;i<TCC_VFS_CACHE_SLOTS;++i) tcc_vfs_cache[i].age=0;
        tcc_vfs_cache_age=1;
    }
    for (i=0;i<TCC_VFS_CACHE_SLOTS;++i) {
        if (tcc_vfs_cache[i].blob==blob) {
            tcc_vfs_cache[i].age=tcc_vfs_cache_age;
            return tcc_vfs_cache[i].data;
        }
        if (tcc_vfs_cache[i].age<tcc_vfs_cache[victim].age) victim=i;
    }
    data=(unsigned char*)malloc(blob->size?blob->size:1u);
    if (!data) { errno=ENOMEM; return NULL; }
    if (!tcc_vfs_decode_lzma(blob->data,blob->packed_size,data,blob->size)) {
        free(data); errno=EINVAL; return NULL;
    }
    if (blob->size>TCC_VFS_CACHE_BLOCK_MAX) { *temporary=data; return data; }
    free(tcc_vfs_cache[victim].data);
    tcc_vfs_cache[victim].blob=blob;
    tcc_vfs_cache[victim].data=data;
    tcc_vfs_cache[victim].age=tcc_vfs_cache_age;
    return data;
}
static int tcc_vfs_resource_read(const TCCBuiltinResource *res, size_t pos, void *buffer, size_t size)
{
    size_t total=0, copied=0;
    unsigned int i;
    if (!res || pos>res->size || size>res->size-pos || (size&&!buffer) ||
        (res->first_span>tcc_builtin_spans_count ||
         res->span_count>tcc_builtin_spans_count-res->first_span)) { errno=EINVAL; return 0; }
    for (i=0;i<res->span_count;++i) {
        const TCCBuiltinSpan *span=&tcc_builtin_spans[res->first_span+i];
        const TCCBuiltinBlob* blob=tcc_builtin_span_blob(span);
        if (!blob || span->offset>blob->size ||
            span->size>blob->size-span->offset || span->size>SIZE_MAX-total) {
            errno=EINVAL; return 0;
        }
        total+=span->size;
    }
    if (total!=res->size) { errno=EINVAL; return 0; }
    total=0;
    for (i=0;i<res->span_count && copied<size;++i) {
        const TCCBuiltinSpan *span=&tcc_builtin_spans[res->first_span+i];
        size_t end=total+span->size;
        if (pos<end) {
            size_t offset=pos>total?pos-total:0;
            size_t n=span->size-offset;
            unsigned char *temporary=NULL;
            const unsigned char *data;
            if (n>size-copied) n=size-copied;
            data=tcc_vfs_blob_data(tcc_builtin_span_blob(span),&temporary);
            if (!data) return 0;
            memcpy((unsigned char*)buffer+copied,data+span->offset+offset,n);
            free(temporary);
            copied+=n; pos+=n;
        }
        total=end;
    }
    return copied==size;
}
static int tcc_vfs_materialize_resource(const TCCBuiltinResource *res,
    const unsigned char **out_data,unsigned char **out_owned_data,size_t *out_size)
{
    unsigned char *data;
    *out_data=NULL; *out_owned_data=NULL; *out_size=0;
    if (!res) { errno=ENOENT; return 0; }
    if (res->size>SIZE_MAX) { errno=EOVERFLOW; return 0; }
    data=(unsigned char*)malloc(res->size?res->size:1u);
    if (!data) { errno=ENOMEM; return 0; }
    if (!tcc_vfs_resource_read(res,0,data,res->size)) { free(data); return 0; }
    *out_data=data; *out_owned_data=data; *out_size=res->size; return 1;
}
int tcc_vfs_read_builtin(const char *path,void *buffer,size_t capacity,size_t *size)
{
    const TCCBuiltinResource *res;
    if (size) *size=0;
    if (!path || !size || strlen(path)>=TCC_VFS_KEY_MAX || (!buffer&&capacity)) { errno=EINVAL; return 0; }
    res=tcc_vfs_lookup_resource(path);
    if (!res) { errno=ENOENT; return 0; }
    if (res->size>SIZE_MAX) { errno=EOVERFLOW; return 0; }
    *size=(size_t)res->size;
    if (!buffer) return 1;
    if (capacity<res->size) { errno=ENOSPC; return 0; }
    return tcc_vfs_resource_read(res,0,buffer,res->size);
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

static FILE *tcc_vfs_fopen_bytes(const unsigned char *data, size_t size, unsigned char *owned_data)
{
    FILE *file;

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

static FILE *tcc_vfs_fopen_dynamic(const TCCVfsDynamicResource *res)
{
    unsigned char *data;

    if (!res) {
        errno = ENOENT;
        return NULL;
    }
    data = NULL;
    if (res->size > 0) {
        data = (unsigned char *)malloc(res->size);
        if (!data) {
            errno = ENOMEM;
            return NULL;
        }
        memcpy(data, res->data, res->size);
    }
    return tcc_vfs_fopen_bytes(data, res->size, data);
}

static FILE *tcc_vfs_fopen_resource(const TCCBuiltinResource *res)
{
    const unsigned char *data;
    unsigned char *owned_data;
    size_t size;

    if (!tcc_vfs_materialize_resource(res, &data, &owned_data, &size))
        return NULL;
    return tcc_vfs_fopen_bytes(data, size, owned_data);
}

TCC_VFS_API int tcc_vfs_open(const char *path, int flags, ...)
{
    int fd, mode = 0;
    const TCCBuiltinResource *res;
    const TCCVfsDynamicResource *dynamic_res;
    va_list ap;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    if (tcc_vfs_is_readonly_open(flags)) {
        if (tcc_vfs_is_readonly_open(flags) && tcc_vfs_lookup_fixed_object(path,&res))
        return res?tcc_vfs_alloc_fd(res):-1;
    dynamic_res = tcc_vfs_lookup_dynamic(path);
        if (dynamic_res)
            return tcc_vfs_alloc_dynamic_fd(dynamic_res);
        if (tcc_vfs_disk_reader) {
            const unsigned char *data = NULL;
            size_t size = 0;
            int observed = tcc_vfs_disk_reader(tcc_vfs_disk_reader_opaque, path, &data, &size);
            if (observed < 0 || (observed > 0 && size && !data)) { errno = EIO; return -1; }
            if (observed > 0) return tcc_vfs_alloc_materialized_fd(data, NULL, size);
            res = tcc_vfs_lookup_resource(path);
            if (!res) { errno = ENOENT; return -1; }
            return tcc_vfs_alloc_fd(res);
        }
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
        if (n > (size_t)INT_MAX) n=INT_MAX;
        if (n) {
            if (vf->resource) {
                if (!tcc_vfs_resource_read(vf->resource,vf->pos,buf,n)) return -1;
            } else memcpy(buf, vf->data + vf->pos, n);
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
        if ((offset>0&&base>LONG_MAX-offset)||(offset<0&&base<LONG_MIN-offset)) { errno=EINVAL; return -1; }
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
    const TCCVfsDynamicResource *dynamic_res;

    if (tcc_vfs_is_readonly_fopen(mode)) {
        if (tcc_vfs_is_readonly_fopen(mode) && tcc_vfs_lookup_fixed_object(path,&res))
        return res?tcc_vfs_fopen_resource(res):NULL;
    dynamic_res = tcc_vfs_lookup_dynamic(path);
        if (dynamic_res)
            return tcc_vfs_fopen_dynamic(dynamic_res);
        if (tcc_vfs_disk_reader) {
            const unsigned char *data = NULL;
            size_t size = 0;
            int observed = tcc_vfs_disk_reader(tcc_vfs_disk_reader_opaque, path, &data, &size);
            if (observed < 0 || (observed > 0 && size && !data)) { errno = EIO; return NULL; }
            if (observed > 0) return tcc_vfs_fopen_bytes(data, size, NULL);
            res = tcc_vfs_lookup_resource(path);
            if (!res) { errno = ENOENT; return NULL; }
            return tcc_vfs_fopen_resource(res);
        }
    }

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

/* Catalog code is generated on demand for a nested host. Physical compressed
 * bytes are stored once in each host; logical aliases keep their own names. */
typedef struct TCCVfsSourceWriter { char *buffer; size_t capacity, used; int ok; } TCCVfsSourceWriter;
static void tcc_vfs_source_append(TCCVfsSourceWriter *w,const char *text,size_t size)
{
    if (!w->ok || size>SIZE_MAX-w->used) { w->ok=0; return; }
    if (w->buffer && size && w->used<w->capacity) {
        size_t n=w->capacity-w->used;
        if (n>size) n=size;
        memcpy(w->buffer+w->used,text,n);
    }
    w->used+=size;
}
static void tcc_vfs_source_format(TCCVfsSourceWriter *w,const char *format,...)
{
    char text[256]; int size; va_list args;
    va_start(args,format); size=vsnprintf(text,sizeof(text),format,args); va_end(args);
    if (size<0 || (size_t)size>=sizeof(text)) { w->ok=0; return; }
    tcc_vfs_source_append(w,text,(size_t)size);
}
static int tcc_vfs_catalog_selected(const TCCBuiltinResource *r,int compiler)
{
    if (compiler || strcmp(tcc_builtin_resource_name(r),"__xl_objects/static-native/xrt.o")==0 ||
        strcmp(tcc_builtin_resource_name(r),"__xl_objects/shared-native/xrt.o")==0) return 1;
    return strncmp(tcc_builtin_resource_name(r),"__xl_compiler/",14)!=0 &&
        strncmp(tcc_builtin_resource_name(r),"__xl_objects/",13)!=0 &&
        strcmp(tcc_builtin_resource_name(r),"include/__xl_runtime/xrt.h")!=0 &&
        strcmp(tcc_builtin_resource_name(r),"include/__xl_runtime/xruntime.h")!=0 &&
        strcmp(tcc_builtin_resource_name(r),"include/__xl_runtime/xruntime_features.h")!=0;
}
/* xlang 2026-10-30: serialize the indexed v3 catalog without changing
 * decoded payloads, logical aliases or the bounded decompression cache. */
static void tcc_vfs_source_name(TCCVfsSourceWriter* w,const char* text)
{
    const unsigned char* name=(const unsigned char*)text;
    for(;*name;++name){
        if(*name=='"'||*name=='\\')tcc_vfs_source_format(w,"\\%c",*name);
        else if(*name>=32&&*name<127)tcc_vfs_source_append(w,(const char*)name,1);
        else tcc_vfs_source_format(w,"\\%03o",*name);
    }
}
int tcc_vfs_write_catalog_source(int compiler,char *buffer,size_t capacity,size_t *size)
{
    uint32_t* map=NULL;
    size_t count=0,b,i,j,span_count=0,name_bytes=0,resource_count=0;
    TCCVfsSourceWriter w={buffer,capacity,0,1};
    if(size)*size=0;
    if(!size||(!buffer&&capacity)){errno=EINVAL;return 0;}
    if((size_t)tcc_builtin_blobs_count>SIZE_MAX/sizeof(*map)){errno=EOVERFLOW;return 0;}
    map=malloc((tcc_builtin_blobs_count?tcc_builtin_blobs_count:1)*sizeof(*map));
    if(!map){errno=ENOMEM;return 0;}
    for(i=0;i<tcc_builtin_blobs_count;++i)map[i]=UINT32_MAX;
    for(i=0;i<tcc_builtin_resources_count;++i){
        const TCCBuiltinResource* resource=&tcc_builtin_resources[i];
        const char* name=tcc_builtin_resource_name(resource);size_t length,total=0;
        if(!name){errno=EINVAL;goto failed;}
        if(!tcc_vfs_catalog_selected(resource,compiler))continue;
        length=strlen(name);
        if(resource->first_span>tcc_builtin_spans_count ||
           resource->span_count>tcc_builtin_spans_count-resource->first_span ||
           resource->span_count>UINT32_MAX-span_count || length>=UINT32_MAX-name_bytes){errno=EOVERFLOW;goto failed;}
        span_count+=resource->span_count;name_bytes+=length+1;++resource_count;
        for(j=0;j<resource->span_count;++j){
            const TCCBuiltinSpan* span=&tcc_builtin_spans[resource->first_span+j];
            const TCCBuiltinBlob* blob=tcc_builtin_span_blob(span);
            if(!blob||span->offset>blob->size||span->size>blob->size-span->offset||span->size>SIZE_MAX-total){errno=EINVAL;goto failed;}
            total+=span->size;
            if(map[span->blob_index]==UINT32_MAX)map[span->blob_index]=(uint32_t)count++;
        }
        if(total!=resource->size){errno=EINVAL;goto failed;}
    }
    tcc_vfs_source_format(&w,"#include \"tcc_builtin_vfs.h\"\n#if TCC_BUILTIN_CATALOG_VERSION != 3\n#error catalog_version\n#endif\n");
    /* Emit original bytes; map order is first use in the sorted directory. */
    for(i=0;i<tcc_builtin_blobs_count;++i){
        const TCCBuiltinBlob* blob=&tcc_builtin_blobs[i];const char hex[]="0123456789abcdef";
        if(map[i]==UINT32_MAX)continue;
        b=map[i];tcc_vfs_source_format(&w,"static const unsigned char vd%llu[]={\n",(unsigned long long)b);
        for(size_t at=0;at<blob->packed_size;){
            char line[81];size_t n=0;
            for(j=0;j<16&&at<blob->packed_size;++j,++at){
                unsigned int byte=blob->data[at];line[n++]='0';line[n++]='x';
                line[n++]=hex[byte>>4];line[n++]=hex[byte&15];line[n++]=',';
            }
            line[n++]='\n';tcc_vfs_source_append(&w,line,n);
        }
        if(!blob->packed_size)tcc_vfs_source_format(&w,"0\n");
        tcc_vfs_source_format(&w,"};\n");
    }
    tcc_vfs_source_format(&w,"const TCCBuiltinBlob tcc_builtin_blobs[]={\n");
    for(b=0;b<count;++b)for(i=0;i<tcc_builtin_blobs_count;++i)if(map[i]==b){
        const TCCBuiltinBlob* blob=&tcc_builtin_blobs[i];
        tcc_vfs_source_format(&w,"{vd%llu,%u,%u,%u},\n",(unsigned long long)b,blob->packed_size,blob->size,blob->method);break;
    }
    if(!count)tcc_vfs_source_format(&w,"{0}\n");
    tcc_vfs_source_format(&w,"};\nconst unsigned int tcc_builtin_blobs_count=%llu;\nconst TCCBuiltinSpan tcc_builtin_spans[]={\n",(unsigned long long)count);
    for(i=0;i<tcc_builtin_resources_count;++i){
        const TCCBuiltinResource* resource=&tcc_builtin_resources[i];
        if(!tcc_vfs_catalog_selected(resource,compiler))continue;
        for(j=0;j<resource->span_count;++j){
            const TCCBuiltinSpan* span=&tcc_builtin_spans[resource->first_span+j];
            tcc_vfs_source_format(&w,"{%u,%u,%u},\n",map[span->blob_index],span->offset,span->size);
        }
    }
    if(!span_count)tcc_vfs_source_format(&w,"{0}\n");
    tcc_vfs_source_format(&w,"};\nconst unsigned int tcc_builtin_spans_count=%llu;\nconst char tcc_builtin_names[]=\n",(unsigned long long)span_count);
    for(i=0;i<tcc_builtin_resources_count;++i){
        const TCCBuiltinResource* resource=&tcc_builtin_resources[i];
        if(!tcc_vfs_catalog_selected(resource,compiler))continue;
        tcc_vfs_source_format(&w,"\"");tcc_vfs_source_name(&w,tcc_builtin_resource_name(resource));
        tcc_vfs_source_format(&w,"\\000\"\n");
    }
    if(!resource_count)tcc_vfs_source_format(&w,"\"\"\n");
    tcc_vfs_source_format(&w,";\nconst unsigned int tcc_builtin_names_size=%llu;\nconst TCCBuiltinResource tcc_builtin_resources[]={\n",(unsigned long long)(name_bytes+1));
    span_count=0;name_bytes=0;
    for(i=0;i<tcc_builtin_resources_count;++i){
        const TCCBuiltinResource* resource=&tcc_builtin_resources[i];
        if(!tcc_vfs_catalog_selected(resource,compiler))continue;
        tcc_vfs_source_format(&w,"{%lluull,%lluu,%lluu,%u},\n",(unsigned long long)resource->size,(unsigned long long)name_bytes,(unsigned long long)span_count,resource->span_count);
        span_count+=resource->span_count;name_bytes+=strlen(tcc_builtin_resource_name(resource))+1;
    }
    if(!resource_count)tcc_vfs_source_format(&w,"{0}\n");
    tcc_vfs_source_format(&w,"};\nconst unsigned int tcc_builtin_resources_count=%llu;\n",(unsigned long long)resource_count);
    free(map);*size=w.used;
    if(!w.ok){errno=EOVERFLOW;return 0;}
    if(buffer){if(capacity<=w.used){errno=ENOSPC;return 0;}buffer[w.used]=0;}
    return 1;
failed:
    free(map);return 0;
}
