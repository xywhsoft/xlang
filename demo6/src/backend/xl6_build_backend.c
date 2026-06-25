#include "src/backend/xl6_build_backend.h"
#include "src/backend/xl6_toolchain.h"

#include <string.h>

void xl6BuildBackendInitDefault(XL6_BuildBackend* backend)
{
    if ( backend == NULL ) {
        return;
    }
    memset(backend, 0, sizeof(*backend));
    backend->kind = XL6_BUILD_BACKEND_TCC;
    backend->name = "tcc";
    backend->ccPath = XL6_TCC_EXE_PATH;
    backend->commandPrefix = XL6_TCC_DEFAULT_COMMAND_PREFIX;
}

const char* xl6BuildBackendName(const XL6_BuildBackend* backend)
{
    return backend != NULL && backend->name != NULL ? backend->name : "tcc";
}

const char* xl6BuildBackendCcPath(const XL6_BuildBackend* backend)
{
    return backend != NULL && backend->ccPath != NULL ? backend->ccPath : XL6_TCC_EXE_PATH;
}

const char* xl6BuildBackendCommandPrefix(const XL6_BuildBackend* backend)
{
    return backend != NULL && backend->commandPrefix != NULL ? backend->commandPrefix : XL6_TCC_DEFAULT_COMMAND_PREFIX;
}
