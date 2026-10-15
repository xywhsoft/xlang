#include "xl6_vfs_private.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void* xl6_xrt_vfs_create(void)
{
    XL6_VfsOwner* owner=xrtCalloc(1,sizeof(*owner));if(!owner)return NULL;
    owner->native=xrtVfsCreate();if(!owner->native){xrtFree(owner);return NULL;}return owner;
}
XL6_TEXT_API void xl6_xrt_vfs_destroy(void* handle)
{
    XL6_VfsOwner* owner=handle;xerror* prior=xrtTakeError();
    if(owner){xrtVfsDestroy(owner->native);xrtFree(owner);}
    xrtClearError();xrtSetErrorTake(prior);
}
XL6_TEXT_API xvalue* xl6_xrt_vfs_read(void* handle,xstrview path,int64_t limit)
{
    if(limit<0 || (uint64_t)limit>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    const char* name=xl6VfsPath(path);if(!name)return NULL;
    xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;size_t size=0;
    unsigned char* data=xrtVfsReadAllLimit(fs,name,(size_t)limit,&size);if(!data)return NULL;
    xvalue* result=xrtValueBytesTake(&data,size);xrtFree(data);return result;
}
XL6_TEXT_API void xl6_xrt_vfs_mount_destroy(void* handle)
{ xerror* prior=xrtTakeError();xrtVfsMountDestroy(handle);xrtClearError();xrtSetErrorTake(prior); }
XL6_TEXT_API bool xl6_xrt_vfs_unmount(void* mount)
{ return xrtVfsUnmount(mount); }
XL6_TEXT_API uint64_t xl6_xrt_vfs_mount_id(void* mount)
{ return xrtVfsMountId(mount); }
/* Builtin providers own native bytes/files/roots only; neither namespace nor
 * mount stores a language value or generated callback. Native references are
 * still released by XRT, including mounts that outlive the language Vfs. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6VfsHandlePolicyV1Get,xl6_xrt_vfs_destroy)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6VfsMountHandlePolicyV1Get,xl6_xrt_vfs_mount_destroy)
