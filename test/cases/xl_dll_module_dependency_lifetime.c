#include "release/include/xl.h"

#include <stdio.h>
#include <windows.h>

typedef long long (*XL_TestValueProc)(void);

static DWORD WINAPI testWatchdog(LPVOID ignored)
{
    (void)ignored;
    Sleep(30000u);
    fputs("xl.dll module dependency lifetime timed out\n", stderr);
    ExitProcess(124u);
    return 0;
}

int main(void)
{
    XL_ContextOptions options = xlDefaultContextOptions();
    XL_Context* ctx;
    XL_Module* module;
    XL_TestValueProc valueProc;
    HANDLE watchdog;
    const char* source =
        "extern long long xl6_dll_loadlib_value(void);\n"
        "long long module_dependency_value(void) {\n"
        "    return xl6_dll_loadlib_value();\n"
        "}\n";

    watchdog = CreateThread(NULL, 0, testWatchdog, NULL, 0, NULL);
    if ( watchdog == NULL ) {
        fprintf(stderr, "cannot create module lifetime watchdog\n");
        return 10;
    }

    options.workDir = ".";
    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create module lifetime context\n");
        return 1;
    }
    if ( !xlLoadDependencyLibrary(ctx, "build\\tmp\\xl_dll_loadlib_host.dll") ||
         !xlAddLinkLibrary(ctx, "build\\tmp\\xl_dll_loadlib_host.def") ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 2;
    }
    module = xlCompileCSourceStringToModule(ctx, source);
    if ( module == NULL ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 3;
    }
    valueProc = (XL_TestValueProc)xlModuleGetSymbol(module, "module_dependency_value");
    if ( valueProc == NULL ) {
        fprintf(stderr, "module dependency symbol missing\n");
        xlDestroyModule(module);
        xlDestroyContext(ctx);
        return 4;
    }

    /* 模块应独立保活依赖库，销毁创建上下文后仍可安全调用。 */
    xlDestroyContext(ctx);
    if ( valueProc() != 42 ) {
        fprintf(stderr, "module dependency was unloaded with context\n");
        xlDestroyModule(module);
        return 5;
    }
    xlDestroyModule(module);
    CloseHandle(watchdog);
    printf("xl.dll module dependency lifetime ok\n");
    return 0;
}
