#ifndef XL6_PATH_BUILD_H
#define XL6_PATH_BUILD_H
#include "xl6_text_value.h"
/* Synchronous Array input transaction. Native admitted Arrays and explicit
 * wire Arrays share exact UTF-8 spans; every part must be string without NUL.
 * Owns the returned sentinel-terminated text. Failure returns typed empty and
 * propagates checked failure independently of diagnostic allocation. */
XL6_TEXT_API xstrview xl6_xrt_fs_path_build(const xvalue*,int64_t);
#endif
