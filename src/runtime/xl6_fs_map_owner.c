#include "xl6_text_value.h"
#include "xl6_fs_resource.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void xl6_xrt_fs_map_destroy(void* handle)
{
    xerror* prior=xrtTakeError();(void)xlir_fs_map_close(handle);xrtFree(handle);
    xrtClearError();xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsMapHandlePolicyV1Get,xl6_xrt_fs_map_destroy)
