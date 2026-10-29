#ifndef XL6_FS_RESOURCE_H
#define XL6_FS_RESOURCE_H
#include "xl6_fs_file.h"
/* These resources own only native roots/maps and File's native reference, not
 * language File objects, generated callbacks, or arbitrary provider contexts. */
typedef struct xlir_fs_root { xroot root; } xlir_fs_root;
typedef struct xlir_fs_map { xfilemap map; xlir_fs_file* file; uint32_t flags; } xlir_fs_map;
typedef struct xlir_fs_lock { xlir_fs_file* file; uint64_t offset; uint64_t size; } xlir_fs_lock;
static inline bool xlir_fs_map_close(xlir_fs_map* owner)
{
    if(!owner || !owner->map)return true;
    xfilemap map=owner->map;xlir_fs_file* file=owner->file;owner->map=NULL;owner->file=NULL;
    bool ok=xrtFileUnmap(map);xerror* error=!ok?xrtTakeError():NULL;
    xlir_fs_file_drop(file);
    if(!ok){xrtClearError();xrtSetErrorTake(error);}
    return ok && !xrtGetError();
}
#endif
