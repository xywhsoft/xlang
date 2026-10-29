#include "xl6_text_value.h"
#include "xl6_fs_resource.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void xl6_xrt_fs_lock_destroy(void* handle)
{
    xlir_fs_lock* owner=handle;xerror* prior=xrtTakeError();
    if(owner){
        if(owner->file){(void)xrtFileUnlockRange(owner->file->file,owner->offset,owner->size);
            xrtAtomic32Store(&owner->file->lock,0,XMEMORY_RELEASE);xlir_fs_file_drop(owner->file);}
        xrtFree(owner);
    }
    xrtClearError();xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsLockHandlePolicyV1Get,xl6_xrt_fs_lock_destroy)
