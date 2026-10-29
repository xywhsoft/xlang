#ifndef XLANG_EXTLIB_XHTTP_FORM_DATA_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_FORM_DATA_PACKAGE_H

/* FormData 同时启用拥有型容器、multipart 编码和完整正文解析。 */
#define XHTTP_MODULE_FORM_DATA_MULTIPART
#define XHTTP_MODULE_FORM_DATA_PARSE

#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif

/* Native sources may share one compiler preprocessor. Recompute the
 * monotonic feature closure when another split HTTP package is activated. */
#undef XHTTP_FEATURES_H
#include "xhttp/features.h"
#include <xrt.h>

#include "../../xhttp.body/include/xhttp_body_package.h"
#include "../../xhttp.multipart/include/xhttp_multipart_package.h"

#include "xrt/form_data.h"

#endif
