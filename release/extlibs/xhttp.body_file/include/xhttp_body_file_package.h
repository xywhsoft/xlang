#ifndef XLANG_EXTLIB_XHTTP_BODY_FILE_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_BODY_FILE_PACKAGE_H

/* xhttp.body_file 只启用按需异步读取文件正文所需的依赖。 */
#define XHTTP_MODULE_HTTP_BODY_FILE

/* Native sources may share one compiler preprocessor. Recompute the
 * monotonic feature closure when another split HTTP package is activated. */
#undef XHTTP_FEATURES_H
#include "xhttp/features.h"
#include <xrt.h>
#include <xruntime.h>
#include <xl_extlib_abi.h>

#include "xrt/http_body.h"
#include "xrt/http_body_file.h"

#endif
