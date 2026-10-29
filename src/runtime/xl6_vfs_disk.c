#include "xl6_vfs_private.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void* xl6_xrt_vfs_disk_create(xstrview root,int64_t access)
{
    if(access<=0 || (uint64_t)access>XVFS_DISK_ACCESS){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    const char* path=xl6VfsPath(root);return path ? xrtVfsDiskCreate(path,(uint32_t)access):NULL;
}
XL6_TEXT_API void xl6_xrt_vfs_disk_destroy(void* handle)
{ xerror* prior=xrtTakeError();xrtVfsDiskDestroy(handle);xrtClearError();xrtSetErrorTake(prior); }
XL6_TEXT_API void* xl6_xrt_vfs_mount_disk(void* handle,void* provider,xstrview prefix,int64_t priority,int64_t mode)
{
    if(priority<INT32_MIN || priority>INT32_MAX || mode<0 || mode>1){xrtSetErrorKind(XERR_RANGE);return NULL;}
    const char* path=xl6VfsPath(prefix);if(!path)return NULL;xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;
    return xrtVfsDiskMount(fs,path,(int32_t)priority,(xvfscase)mode,provider,0u);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6VfsDiskHandlePolicyV1Get,xl6_xrt_vfs_disk_destroy)
