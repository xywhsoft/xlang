#include "xl6_text_value.h"
#include "xl6_fs_file.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
/* Same no-raise destructor contract as File's former generated adapter.
 * Explicit close continues to expose native errors and lease conflicts. */
XL6_TEXT_API void xl6_xrt_fs_file_destroy(void* handle)
{
    xerror* prior=xrtTakeError();xlir_fs_file_drop(handle);
    xrtClearError();xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsFileHandlePolicyV1Get,xl6_xrt_fs_file_destroy)
