#include "xl6_text_value.h"
#include "xl6_fs_snapshot.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void xl6_xrt_fs_dir_destroy(void* handle)
{
    xlir_fs_dir* owner=handle;xerror* prior=xrtTakeError();
    if(owner){if(owner->dir)(void)xrtDirClose(owner->dir);xrtFree(owner);}
    xrtClearError();xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsDirHandlePolicyV1Get,xl6_xrt_fs_dir_destroy)
