/* Each split package must activate its modules even after shared helper
 * guards were initialized by another package in the same translation unit. */
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#if defined(XL_TEST_HTTP_REVERSE)
#include "../../release/extlibs/xhttp.protocol/src/internal/xhttp_internal.h"
#include "../../release/extlibs/xhttp.body/src/internal/xhttp_internal.h"
#else
#include "../../release/extlibs/xhttp.body/src/internal/xhttp_internal.h"
#include "../../release/extlibs/xhttp.protocol/src/internal/xhttp_internal.h"
#endif
#include "../../release/extlibs/xhttp/include/xhttp_package.h"
#include "../../release/extlibs/xhttp.multipart/include/xhttp_multipart_package.h"
#include "../../release/extlibs/xhttp.form_data/include/xhttp_form_data_package.h"
#include "../../release/extlibs/xhttp.client/include/xhttp_client_package.h"
#if !defined(XHTTP_FEATURE_URL) || !defined(XHTTP_FEATURE_HTTP_BODY) || \
    !defined(XHTTP_FEATURE_HTTP_HEADERS) || !defined(XHTTP_FEATURE_MULTIPART) || \
    !defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)
#error Split HTTP package feature activation depends on include order
#endif
int main(void) { return 0; }
