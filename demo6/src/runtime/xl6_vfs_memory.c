#include "xl6_vfs_private.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void* xl6_xrt_vfs_memory_create(void)
{ return xrtVfsMemoryCreate(); }
XL6_TEXT_API void xl6_xrt_vfs_memory_destroy(void* handle)
{ xerror* prior=xrtTakeError();xrtVfsMemoryDestroy(handle);xrtClearError();xrtSetErrorTake(prior); }
XL6_TEXT_API bool xl6_xrt_vfs_memory_put_bytes(void* memory,xstrview path,const xvalue* value,int64_t limit)
{
    xbytesview data={0};if(limit < -1){xrtSetErrorKind(XERR_RANGE);return false;}
    if(!xrtValueGetBytes(value,&data)){xrtSetErrorKind(XERR_TYPE);return false;}
    if(limit>=0 && (uint64_t)limit<data.Size)data.Size=(size_t)limit;
    const char* name=xl6VfsPath(path);return name && xrtVfsMemoryPutCopy(memory,name,data.Data,data.Size);
}
XL6_TEXT_API bool xl6_xrt_vfs_memory_put_string(void* memory,xstrview path,xstrview data,int64_t limit)
{
    if(limit < -1){xrtSetErrorKind(XERR_RANGE);return false;}
    if(limit>=0 && (uint64_t)limit<data.Size)data.Size=(size_t)limit;
    const char* name=xl6VfsPath(path);return name && xrtVfsMemoryPutCopy(memory,name,data.Data,data.Size);
}
XL6_TEXT_API bool xl6_xrt_vfs_memory_seal(void* memory)
{ return xrtVfsMemorySeal(memory); }
XL6_TEXT_API void* xl6_xrt_vfs_mount_memory(void* handle,void* provider,xstrview prefix,int64_t priority,int64_t mode)
{
    if(priority<INT32_MIN || priority>INT32_MAX || mode<0 || mode>1){xrtSetErrorKind(XERR_RANGE);return NULL;}
    const char* path=xl6VfsPath(prefix);if(!path)return NULL;xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;
    return xrtVfsMemoryMount(fs,path,(int32_t)priority,(xvfscase)mode,provider,0u);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6VfsMemoryHandlePolicyV1Get,xl6_xrt_vfs_memory_destroy)
