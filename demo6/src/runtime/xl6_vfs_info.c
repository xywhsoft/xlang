#include "xl6_vfs_private.h"
#include "xl6_fs_snapshot.h"
XL6_TEXT_API void* xl6_xrt_fs_info_vfs(void* handle,xstrview path,bool follow)
{
    const char* name=xl6VfsPath(path);if(!name)return NULL;xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;
    xlir_fs_info* info=xlir_fs_info_alloc(name);if(!info)return NULL;
    if(!xrtVfsStat(fs,name,follow,&info->info)){xlir_fs_info_drop(info);return NULL;}return info;
}
