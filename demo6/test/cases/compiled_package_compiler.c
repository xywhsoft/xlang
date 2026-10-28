#include "include/xlc.h"
#include "include/xl_package.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const unsigned char emptyInterface[40]={'X','L','P','1'};
static int64_t answer(void) { return 42; }

int main(int argc,char** argv)
{
    assert(argc==4);
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=".";options.tccDllPath=argv[1];options.tccLibPath=argv[2];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    XL_Context* runtime=xlcRuntimeContext(compiler);assert(runtime);
    XL_PackageEntry entry={"compiled_compiler_answer",(const void*)answer};
    XL_CompiledPackage package={sizeof(package),XL_COMPILED_PACKAGE_VERSION,sizeof(void*),
        XL_COMPILED_PACKAGE_COMPILER_ABI,NULL,"compiled_consumer","1.0.0","1",
        emptyInterface,sizeof(emptyInterface),0,&entry,1,NULL,0};
    package.runtimeIdentity=xlCompiledPackageRuntimeIdentity();
    package.interfaceHash=xlCompiledPackageInterfaceHash(emptyInterface,sizeof(emptyInterface));
    assert(xlImportCompiledPackage(runtime,&package));
    assert(xlcAddSourceString(compiler,"compiled_consumer.xl",
        "import compiled_consumer\nextern \"compiled_compiler_answer\" func answer() as int\nassert answer()==42\n"));
    int ok=xlcCompile(compiler);
    if(!ok)fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    assert(ok);
    assert(xlcEmitC(compiler,argv[3]));
    XL_Module* module=xlcRelocateModule(compiler);assert(module);
    assert(xlRunModuleMain(runtime,module,NULL));xlDestroyModule(module);
    assert(!xlcEmitExe(compiler,argv[3]));
    if(!strstr(xlcGetLastError(compiler),"process-local"))fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    assert(strstr(xlcGetLastError(compiler),"process-local"));
    assert(xlcAddSourceString(compiler,"compiled_consumer_shared.xl",
        "import compiled_consumer\nextern \"compiled_compiler_answer\" func answer() as int\n"
        "export \"compiled_consumer_value\" func value() as int\nreturn answer()\nend\n"));
    assert(xlcCompile(compiler));
    assert(!xlcEmitSharedLibrary(compiler,argv[3]));
    assert(strstr(xlcGetLastError(compiler),"process-local"));
    xlcDestroyContext(compiler);
    compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcAddSourceString(compiler,"compiled_provider_shared.xl",
        "import xlang\nimport compiled_demo\n"
        "export \"compiled_provider\" func provider() as point\n"
        "return xlang.compiledPackage(\"compiled_demo\")\nend\n"));
    if(!xlcCompile(compiler)) {
        fprintf(stderr,"%s\n",xlcGetLastError(compiler));assert(0);
    }
    assert(!xlcEmitSharedLibrary(compiler,argv[3]));
    if(!strstr(xlcGetLastError(compiler),"process-resident"))fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    assert(strstr(xlcGetLastError(compiler),"process-resident"));
    xlcDestroyContext(compiler);
    puts("compiler: mounted package checks, C emission and relocation; external binary address embedding rejected");
    puts("compiler: shared-library package provider rejected before publication");
    return 0;
}
