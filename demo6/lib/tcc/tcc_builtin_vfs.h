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

TCC_VFS_API int tcc_vfs_open(const char *path, int flags, ...);
TCC_VFS_API int tcc_vfs_close(int fd);
TCC_VFS_API int tcc_vfs_read(int fd, void *buf, size_t count);
TCC_VFS_API long tcc_vfs_lseek(int fd, long offset, int whence);
TCC_VFS_API FILE *tcc_vfs_fopen(const char *path, const char *mode);
TCC_VFS_API int tcc_vfs_fclose(FILE *file);
TCC_VFS_API int tcc_vfs_mount_memory(const char *path, const void *data, size_t size);
TCC_VFS_API void tcc_vfs_clear_dynamic(void);

#ifdef __cplusplus
}
#endif

#endif
