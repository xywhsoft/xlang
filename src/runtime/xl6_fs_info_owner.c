#include "xl6_text_value.h"
#include "xl6_fs_snapshot.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
XL6_TEXT_API void xl6_xrt_fs_info_destroy(void* handle)
{ xlir_fs_info_drop(handle); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsInfoHandlePolicyV1Get,xl6_xrt_fs_info_destroy)
