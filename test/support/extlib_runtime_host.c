/* 扩展库 DLL 门禁：只使用公开 SDK，在同一上下文重复编译和运行包用例。 */
#define XL_RUNTIME_ONLY_API 1
#include "xl.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    XL_ContextOptions options=xlDefaultContextOptions();
    XL_Context* context;
    int success=1;
    if(argc!=3) {
        fprintf(stderr,"usage: extlib-runtime-host work-directory source.xl\n");
        return 2;
    }
    options.workDir=argv[1];
    options.saveTemps=0;
    context=xlCreateContext(&options);
    if(!context) return 3;
    for(int i=0;i<3;++i) {
        if(!xlRunScriptFile(context,argv[2]) || xlGetLastExitCode(context)!=0) {
            fprintf(stderr,"extension runtime round %d: %s\n",i,
                xlGetLastError(context) ? xlGetLastError(context) : "nonzero exit");
            success=0;
            break;
        }
    }
    xlDestroyContext(context);
    return success ? 0 : 1;
}
