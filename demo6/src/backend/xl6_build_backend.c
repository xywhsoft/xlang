#include "src/backend/xl6_build_backend.h"

#include <string.h>

void xl6BuildBackendInitDefault(XL6_BuildBackend* backend)
{
    if ( backend == NULL ) {
        return;
    }
    memset(backend, 0, sizeof(*backend));
    backend->kind = XL6_BUILD_BACKEND_TCC;
    backend->name = "tcc";
    backend->ccPath = "..\\demo5\\lib\\tcc\\tcc.exe";
    backend->commandPrefix =
        "..\\demo5\\lib\\tcc\\tcc.exe "
        "-Irelease\\tcc\\include_win "
        "-I..\\demo5\\lib\\tcc\\include "
        "-I..\\demo5\\release\\tcc\\include_win "
        "-I..\\demo5\\release\\tcc\\include_win\\winapi "
        "-Ilib\\xrt "
        "-Irelease\\tmp "
        "-L..\\demo5\\release\\tcc\\lib";
}

const char* xl6BuildBackendName(const XL6_BuildBackend* backend)
{
    return backend != NULL && backend->name != NULL ? backend->name : "tcc";
}

const char* xl6BuildBackendCcPath(const XL6_BuildBackend* backend)
{
    return backend != NULL && backend->ccPath != NULL ? backend->ccPath : "..\\demo5\\lib\\tcc\\tcc.exe";
}

const char* xl6BuildBackendCommandPrefix(const XL6_BuildBackend* backend)
{
    return backend != NULL && backend->commandPrefix != NULL ? backend->commandPrefix :
        "..\\demo5\\lib\\tcc\\tcc.exe -Irelease\\tcc\\include_win -I..\\demo5\\lib\\tcc\\include -I..\\demo5\\release\\tcc\\include_win -I..\\demo5\\release\\tcc\\include_win\\winapi -Ilib\\xrt -Irelease\\tmp -L..\\demo5\\release\\tcc\\lib";
}
