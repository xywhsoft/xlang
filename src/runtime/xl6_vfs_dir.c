#include "xl6_vfs_private.h"
#include "xl6_fs_snapshot.h"
XL6_TEXT_API void* xl6_xrt_fs_vfs_dir_open(void* handle,xstrview path,int64_t flags)
{
    if(flags<0 || flags>7){xrtSetErrorKind(XERR_RANGE);return NULL;}
    const char* name=xl6VfsPath(path);if(!name)return NULL;xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;
    xlir_fs_dir* owner=xrtCalloc(1,sizeof(*owner));if(!owner)return NULL;
    owner->dir=xrtVfsDirOpen(fs,name,(uint32_t)flags);if(!owner->dir){xrtFree(owner);return NULL;}return owner;
}
