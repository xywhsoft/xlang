#include "src/runtime/xl6_array_os_argv.h"
#include "src/runtime/xl6_array_entry.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/api/xl_foreign_error.h"
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

xvalue* xl6ArrayUtf16ArgvCreateV1(uint64_t identity,uint64_t element,int count,const uint16_t* const* values)
{
    xl6_error_scope operation={0}; xvalue* owner=NULL; char* text=NULL;
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    if (!identity || !element || count<0 || (count && !values)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        for (int i=0;i<count;++i) if (!values[i]) { xrtSetErrorKind(XERR_ARGUMENT); break; }
        if (!xrtGetError()) owner=xl6ArrayEntryAcquireV1(identity,element,NULL);
        if (!owner || !xl6ArrayReserveV1(owner,identity,element,3,sizeof(xvalue*),xrtTypeValue(),count))
            xl6ErrorScopeCaptureFailure(&operation);
        for (int i=0;i<count && !operation.failed;++i) {
            size_t size=0;
            text=xrtUtf16ViewTo8(xrtUtf16View(values[i],xrtUtf16Len(values[i])),XUTF_STRICT,&size);
            xvalue* item=text ? xrtValueStringTake(&text,size) : NULL;
            if (!item || !xl6ArrayValueAppendTakeV1(owner,item) || xrtGetError())
                xl6ErrorScopeCaptureFailure(&operation);
        }
    }
    if (!owner || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    xrtFree(text);
    if (operation.failed) { xrtValueRelease(owner); owner=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return owner;
}
xvalue* xl6ArrayOsArgvCreateV1(uint64_t identity,uint64_t element,int count,char* const* values)
{
#if defined(_WIN32)
    (void)count; (void)values;
    xl6_error_scope operation={0}; xvalue* owner=NULL; int wideCount=0;
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    LPWSTR* wide=CommandLineToArgvW(GetCommandLineW(),&wideCount);
    if (!wide) {
        DWORD code=GetLastError();
        xrtSetErrorTake(xrtErrorCreate(code==ERROR_NOT_ENOUGH_MEMORY ? XERR_MEMORY : XERR_IO,
            "win32.commandLine",code,"cannot obtain Unicode argument vector"));
        xl6ErrorScopeCaptureFailure(&operation);
    } else {
        _Static_assert(sizeof(WCHAR)==sizeof(uint16_t),"Windows argv uses UTF-16 units");
        owner=xl6ArrayUtf16ArgvCreateV1(identity,element,wideCount,(const uint16_t* const*)wide);
        if (!owner || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
        if (LocalFree(wide)) {
            xrtSetErrorTake(xrtErrorCreate(XERR_IO,"win32.commandLine",GetLastError(),"cannot release argument vector"));
            xl6ErrorScopeCaptureFailure(&operation);
        }
    }
    if (operation.failed) { xrtValueRelease(owner); owner=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return owner;
#else
    return xl6ArrayArgvCreateV1(identity,element,count,values);
#endif
}
