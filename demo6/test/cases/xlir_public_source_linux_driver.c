/* Linux counterpart of the public source driver. Uses the real public
 * compiler/API and genuine released TCC core; never includes xl_api.c. */
#include "xlc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char** argv)
{
    if(argc!=4) return 2;
    const char* native=getenv("XL6_SOURCE_NATIVE_ROOT"); if(!native || !*native) return 3;
    char root[4096],runtime[8192],libraries[8192],headers[4096],temporary[8192];
    if(!getcwd(root,sizeof(root))) return 3;
    ssize_t size=readlink("/proc/self/exe",headers,sizeof(headers)-1);
    if(size<=0 || (size_t)size>=sizeof(headers)-1) return 3;
    headers[size]='\0'; char* slash=strrchr(headers,'/'); if(!slash) return 3;
    if((size_t)(slash-headers)+sizeof("/include")>sizeof(headers)) return 3;
    memcpy(slash,"/include",sizeof("/include"));
    int n=snprintf(runtime,sizeof(runtime),"%s/release/libxl.so",native);
    int m=snprintf(libraries,sizeof(libraries),"%s/build/linux/tcc-dist",native);
    int t=snprintf(temporary,sizeof(temporary),"%s.temps",argv[3]);
    if(n<0 || (size_t)n>=sizeof(runtime) || m<0 || (size_t)m>=sizeof(libraries) ||
        t<0 || (size_t)t>=sizeof(temporary)) return 3;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=root; options.tempDir=temporary; options.tccDllPath=runtime; options.tccLibPath=libraries;
    XLC_Context* compiler=xlcCreateContext(&options); if(!compiler) return 4;
    int ok=xlcAddIncludePath(compiler,headers) && xlcAddSourceFile(compiler,argv[1]) && xlcCompile(compiler);
    if(ok) {
        if(!strcmp(argv[2],"cgen-default")) ok=xlcEmitC(compiler,argv[3]);
        else if(!strcmp(argv[2],"ir-typed")) ok=xlcEmitIr(compiler,argv[3],1);
        else if(!strcmp(argv[2],"ir-lowered")) ok=xlcEmitLoweredIr(compiler,argv[3],1);
        else if(!strcmp(argv[2],"run")) ok=xlcRun(compiler,0,NULL);
        else if(!strcmp(argv[2],"module")) {
            XL_Module* module=xlcRelocateModule(compiler);
            ok=module && xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL);
            xlDestroyModule(module);
        } else if(!strcmp(argv[2],"exe")) ok=xlcEmitExe(compiler,argv[3]);
        else ok=0;
    }
    int code=ok ? xlcGetLastExitCode(compiler) : 1;
    if(!ok) fprintf(stderr,"public Linux source %s failed: %s\n",argv[2],
        xlcGetLastError(compiler) ? xlcGetLastError(compiler) : "without diagnostic");
    xlcDestroyContext(compiler);
    if(ok && !code) puts("public Linux source complete");
    return code;
}
