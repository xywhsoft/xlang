/* Test the real Request component without the legacy TCP/TLS facade, which
 * still uses retired raw stdlib constructors. No replacement implementations. */
#include "../../release/extlibs/xhttp.client/src/http/http_client_request.c"
#include "../../release/extlibs/xhttp.client/src/http/http_client_request_trailers.c"
#include "../../release/extlibs/xhttp.client/src/http/http_client_request_te.c"
#include "../../release/extlibs/xhttp.client/src/http/http_client_request_query.c"
#include "../../release/extlibs/xhttp.client/src/http/http_client_request_form.c"
#include "../../release/extlibs/xhttp.client/src/http/http_client_request_form_data.c"
#include "../../release/extlibs/xhttp.client/src/http/http_client_prepare.c"
#include "../../release/extlibs/xhttp.client/src/xlang/xhttp_client_native_common.c"
#include "../../release/extlibs/xhttp.client/src/xlang/xhttp_client_native_request.c"
