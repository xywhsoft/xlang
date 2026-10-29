/*
    TCC bootstrap uses the same UTF-8/VFS hooks as xl.exe, but it does not
    need embedded fallback resources. Keep this tiny table in source control
    so build.bat can bootstrap TCC with only MinGW gcc.
*/
#include "tcc_builtin_vfs.h"

const TCCBuiltinResource tcc_builtin_resources[] = {
    { "", 0, 0u, 0u, TCC_BUILTIN_RESOURCE_STORE }
};

const unsigned int tcc_builtin_resources_count = 0u;
