#include "xl6_vfs_private.h"
#include "xl6_fs_file.h"
XL6_TEXT_API void* xl6_xrt_fs_vfs_open(void* handle,xstrview path,int64_t flags,int64_t mode,int64_t share)
{
    xfileoptions options;
    if(flags<=0 || flags>255 || !(flags&3) || mode<0 || mode>4095 || share<0 || share>7){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xrtFileOptionsInit(&options);options.Flags=(uint32_t)flags;options.Mode=(uint32_t)mode;options.Share=(uint32_t)share;
    const char* name=xl6VfsPath(path);if(!name)return NULL;xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;
    xlir_fs_file* owner=xlir_fs_file_alloc();if(!owner)return NULL;
    owner->file=xrtVfsOpen(fs,name,&options);if(!owner->file){xrtFree(owner);return NULL;}return owner;
}
