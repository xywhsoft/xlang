#include "services/program_version.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include "xide_version.h"
#include <windows.h>
#include <stdlib.h>
const char* xideProgramVersion(void) { return XIDE_VERSION; }
int64_t xideInstalledIterationAt(const char* root)
{
    if(!root) return 0;
    /* Missing/corrupt optional receipts must not leave a TLS error that later
       XUI callbacks would interpret as their own failure. Preserve the caller's
       existing error, just like best-effort preference reads. */
    xerror* previous=xrtTakeError();
    char* path=xidePathResolve(root,".xlang-update/installed.json");
    size_t size=0; unsigned char* bytes=path ? xideIoReadAllLimit(path,16u*1024u*1024u,&size) : NULL;
    free(path);
    xvalue* receipt=bytes ? xrtJsonParse((xstrview){(const char*)bytes,size}) : NULL;
    xrtFree(bytes);
    int64_t iteration=0;
    xvalue* v=receipt ? xrtValueObjectGet(receipt,(xstrview){"version",7}) : NULL;
    if(!v || !xrtValueGetInt(v,&iteration) || iteration<1 || iteration>2147483647) iteration=0;
    xrtValueRelease(receipt);
    xrtErrorFree(xrtTakeError()); xrtSetErrorTake(previous); return iteration;
}
int64_t xideInstalledIteration(void)
{
    wchar_t binary[32768]; DWORD n=GetModuleFileNameW(NULL,binary,32768);
    if(!n || n>=32768) return 0;
    const wchar_t* module=xideStartupResourceModule(binary);
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,module,-1,NULL,0,NULL,NULL);
    char* utf8=size ? malloc((size_t)size) : NULL;
    if(!utf8) return 0;
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,module,-1,utf8,size,NULL,NULL)) { free(utf8); return 0; }
    char* root=xidePathDirectory(utf8); free(utf8);
    int64_t iteration=xideInstalledIterationAt(root); free(root); return iteration;
}
