/*
 * Optional builtin resource VFS for TCC.
 *
 * This file intentionally stays independent from tcc.h so it can be built as
 * a small side module and keep upstream TCC edits narrow.
 */
#ifndef TCC_BUILTIN_VFS_H
#define TCC_BUILTIN_VFS_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TCC_VFS_API
# if defined(_WIN32) && defined(LIBTCC_AS_DLL)
#  define TCC_VFS_API __declspec(dllexport)
# else
#  define TCC_VFS_API
# endif
#endif

#define TCC_BUILTIN_RESOURCE_STORE 0u
#define TCC_BUILTIN_RESOURCE_LZMA  1u

typedef struct TCCBuiltinResource {
    const char *name;
    const unsigned char *data;
    size_t packed_size;
    size_t size;
    unsigned int method;
} TCCBuiltinResource;

extern const TCCBuiltinResource tcc_builtin_resources[];
extern const unsigned int tcc_builtin_resources_count;

/* Exact builtin lookup only: ignores disk files and dynamic mounts. A NULL
 * buffer with zero capacity queries size. No allocation crosses API heaps. */
TCC_VFS_API int tcc_vfs_read_builtin(const char *path, void *buffer, size_t capacity, size_t *size);
TCC_VFS_API int tcc_vfs_open(const char *path, int flags, ...);
TCC_VFS_API int tcc_vfs_close(int fd);
TCC_VFS_API int tcc_vfs_read(int fd, void *buf, size_t count);
TCC_VFS_API long tcc_vfs_lseek(int fd, long offset, int whence);
TCC_VFS_API FILE *tcc_vfs_fopen(const char *path, const char *mode);
TCC_VFS_API int tcc_vfs_fclose(FILE *file);
TCC_VFS_API int tcc_vfs_mount_memory(const char *path, const void *data, size_t size);
TCC_VFS_API void tcc_vfs_clear_dynamic(void);

/* Optional disk transport for one serialized compiler operation. Called after
 * dynamic-memory lookup and before disk/embedded lookup. Return 1 for borrowed
 * immutable bytes, 0 for a missing disk candidate (embedded lookup continues),
 * or -1 to reject the read. Bytes must outlive every open descriptor. The host
 * owns synchronization, limits, failed-candidate records and final validation.
 * This hook never sees writes or changes dynamic/embedded resource priority. */
typedef int (*TCCVfsDiskReader)(void *opaque, const char *path,
    const unsigned char **data, size_t *size);
TCC_VFS_API void tcc_vfs_set_disk_reader(TCCVfsDiskReader reader, void *opaque);

#ifdef __cplusplus
}
#endif

#endif
