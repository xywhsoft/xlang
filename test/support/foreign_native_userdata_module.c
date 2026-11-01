#include "foreign_native_userdata_module.h"
#include <assert.h>
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
#if defined(USERDATA_MODULE_BODY)
EXPORT int moduleBody(void* environment)
{
    UserdataModuleHost* host=*(UserdataModuleHost**)environment;
    ++host->calls;
    XL6_NativeCallback* old=host->slot;host->slot=NULL;
    assert(old && host->api->DropNative(old));
    host->bodyReturned=true;
    return 73;
}
#else
EXPORT int moduleAdapter(UserdataModuleHost* host,XL6_NativeCallback* record)
{
    XL6_NativeCallbackScopeV1 scope={0};
    assert(host->api->Enter(record,789,&scope,NULL,0));
    UserdataModuleBody body=(UserdataModuleBody)scope.closure.closure->invoke;
    int result=body(scope.closure.closure->environment);
    xl6_error_result outcome=host->api->Leave(&scope,(xl6_error_result){true,NULL});
    assert(outcome.ok && !outcome.error && host->bodyReturned && result==73);
    /* This code must still be mapped after the resident Leave returned. */
    host->adapterReturned=true;
    return result;
}
#endif
