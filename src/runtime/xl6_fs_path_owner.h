#ifndef XL6_FS_PATH_OWNER_H
#define XL6_FS_PATH_OWNER_H
#include "xl6_text_value.h"
XL6_TEXT_API void* xl6_xrt_fs_path_parts_create(xstrview,int64_t);
XL6_TEXT_API void xl6_xrt_fs_path_parts_destroy(void*);
XL6_TEXT_API xstrview xl6_xrt_fs_path_parts_text(void*,int64_t);
XL6_TEXT_API int64_t xl6_xrt_fs_path_parts_number(void*,int64_t);
XL6_TEXT_API void* xl6_xrt_fs_path_iter_create(xstrview,int64_t);
XL6_TEXT_API void xl6_xrt_fs_path_iter_destroy(void*);
XL6_TEXT_API bool xl6_xrt_fs_path_iter_advance(void*);
XL6_TEXT_API xstrview xl6_xrt_fs_path_iter_text(void*);
XL6_TEXT_API int64_t xl6_xrt_fs_path_iter_kind(void*);
#endif
