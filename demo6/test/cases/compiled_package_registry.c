#define XL_RUNTIME_ONLY_API
#include "include/xl_package.h"
#include "include/xruntime_decl.h"
#include "src/ast/xl6_ast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char emptyInterface[40] = {'X','L','P','1'};
static int64_t answer(void) { return 42; }
static int64_t other(void) { return 43; }

static size_t importInterface(unsigned char* out,const char* name)
{
    size_t size=strlen(name);memset(out,0,76+size);memcpy(out,"XLP1",4);
    out[20]=1;out[40]=2;out[48]=(unsigned char)size;memcpy(out+76,name,size);
    return 76+size;
}

static XL_CompiledPackage package(const char* name)
{
    XL_CompiledPackage result = {0};
    result.size = sizeof(result); result.version = XL_COMPILED_PACKAGE_VERSION;
    result.pointerBytes = sizeof(void*); result.compilerAbi = XL_COMPILED_PACKAGE_COMPILER_ABI;
    result.runtimeIdentity = xlCompiledPackageRuntimeIdentity();
    result.name = name; result.packageVersion = "1.0.0"; result.abiVersion = "1";
    result.interfaceData = emptyInterface; result.interfaceSize = sizeof(emptyInterface);
    result.interfaceHash = xlCompiledPackageInterfaceHash(emptyInterface,sizeof(emptyInterface));
    return result;
}

static void clear(XL_Context* context)
{
    xlClearLastError(context); xrtClearError();
}

static void sameMemory(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
}

static void u32(unsigned char* bytes,uint32_t value)
{
    for(unsigned i=0;i<4;++i)bytes[i]=(unsigned char)(value>>(i*8));
}

static void aggregateLimits(const XL_ContextOptions* options)
{
    XL_Context* context=xlCreateContext(options);assert(context);
    size_t size=XL_COMPILED_PACKAGE_MAX_INTERFACE/2;
    unsigned char* wire=(unsigned char*)calloc(size,1);assert(wire);
    memcpy(wire,"XLP1",4);wire[20]=1;
    u32(wire+40,XL6_AST_STRING);
    u32(wire+52,(uint32_t)(size-76));wire[72]=1;
    XL_CompiledPackage first=package("bytes_first"),second=package("bytes_second"),overflow=package("bytes_overflow");
    first.interfaceData=second.interfaceData=wire;first.interfaceSize=second.interfaceSize=size;
    first.interfaceHash=second.interfaceHash=xlCompiledPackageInterfaceHash(wire,size);
    assert(xlImportCompiledPackage(context,&first));assert(xlImportCompiledPackage(context,&first));
    assert(xlImportCompiledPackage(context,&second));
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(!xlImportCompiledPackage(context,&overflow));clear(context);sameMemory(&before);
    assert(!xlFindCompiledPackage(context,overflow.name) && xlFindCompiledPackage(context,first.name));
    xlDestroyContext(context);free(wire);

    size_t count=XL_COMPILED_PACKAGE_MAX_ENTRIES;
    XL_PackageEntry* entries=(XL_PackageEntry*)calloc(count,sizeof(*entries));
    char (*names)[32]=(char(*)[32])calloc(count,sizeof(*names));assert(entries && names);
    for(size_t i=0;i<count;++i) {
        snprintf(names[i],sizeof(names[i]),"quota_symbol_%zu",i);
        entries[i].name=names[i];entries[i].address=(const void*)answer;
    }
    first=package("entries_first");second=package("entries_second");overflow=package("entries_overflow");
    first.entries=entries;first.entryCount=count/2;
    second.entries=entries+count/2;second.entryCount=count-count/2;
    XL_PackageEntry extra={"quota_symbol_extra",(const void*)answer};overflow.entries=&extra;overflow.entryCount=1;
    context=xlCreateContext(options);assert(context);
    assert(xlImportCompiledPackage(context,&first));assert(xlImportCompiledPackage(context,&second));
    xrtMemDebugSnapshot(&before);
    assert(!xlImportCompiledPackage(context,&overflow));clear(context);sameMemory(&before);
    assert(!xlFindCompiledPackage(context,overflow.name) && xlFindCompiledPackage(context,second.name));
    xlDestroyContext(context);free(names);free(entries);
    puts("registry: exact aggregate interface/entry quotas, overflow rollback and idempotence");
}

int main(int argc,char** argv)
{
    assert(argc==3);
    XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir=".";options.tccDllPath=argv[1];options.tccLibPath=argv[2];
    assert(xrtMemDebugEnable(true));
    XL_Context* anchor = xlCreateContext(&options);
    assert(anchor);
    XL_PackageEntry entry = {"compiled_registry_answer", (const void*)answer};
    XL_CompiledPackage leaf=package("leaf"), left=package("left"), right=package("right"), root=package("root");
    leaf.entries=&entry; leaf.entryCount=1;
    const XL_CompiledPackage* leafDeps[]={&leaf};
    const XL_CompiledPackage* rootDeps[]={&left,&right};
    left.dependencies=right.dependencies=leafDeps;left.dependencyCount=right.dependencyCount=1;
    root.dependencies=rootDeps;root.dependencyCount=2;
    XL_Context* context=xlCreateContext(&options);assert(context);
    assert(xlImportCompiledPackage(context,&root));
    XL_PackageEntry defaultEntry={"xrtValueRelease",(const void*)other};
    XL_CompiledPackage defaultConflict=package("default_conflict");defaultConflict.entries=&defaultEntry;defaultConflict.entryCount=1;
    assert(!xlImportCompiledPackage(context,&defaultConflict));clear(context);
    defaultEntry.name="xlCreateContext";
    assert(!xlImportCompiledPackage(context,&defaultConflict));clear(context);
    defaultEntry.name="xrtValueRelease";defaultEntry.address=xlCompiledPackageRuntimeIdentity();
    defaultConflict.name="same_runtime";assert(xlImportCompiledPackage(context,&defaultConflict));
    assert(xlFindCompiledPackage(context,"leaf") && xlFindCompiledPackage(context,"root"));
    assert(xlFindCompiledPackage(context,"root")->dependencyCount==2);
    assert(xlImportCompiledPackage(context,&root));
    assert(!xlFindCompiledPackage(anchor,"root"));
    int ran=xlRunScriptString(context,"import leaf\nextern \"compiled_registry_answer\" func answer() as int\nassert answer()==42\n");
    if(!ran)fprintf(stderr,"%s\n",xlGetLastError(context));
    assert(ran);
    XL_CompiledPackage conflict=leaf;conflict.packageVersion="2.0.0";
    assert(!xlImportCompiledPackage(context,&conflict));clear(context);
    assert(!strcmp(xlFindCompiledPackage(context,"leaf")->packageVersion,"1.0.0"));
    XL_PackageEntry badEntry=entry;badEntry.address=(const void*)other;
    XL_CompiledPackage symbolConflict=package("collision");symbolConflict.entries=&badEntry;symbolConflict.entryCount=1;
    assert(!xlImportCompiledPackage(context,&symbolConflict));clear(context);
    assert(!xlFindCompiledPackage(context,"collision"));
    assert(!xlRegisterCFunction(context,entry.name,(void*)other));clear(context);
    assert(xlRunScriptString(context,"extern \"compiled_registry_answer\" func answer() as int\nassert answer()==42\n"));
    XL_CompiledPackage invalid=package("invalid");invalid.compilerAbi^=1;
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    invalid=package("invalid");invalid.runtimeIdentity=NULL;
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    invalid=package("invalid");invalid.pointerBytes=sizeof(void*)==8?4:8;
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    invalid=package("xlang");assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    invalid=package("invalid");invalid.entryCount=XL_COMPILED_PACKAGE_MAX_ENTRIES+1u;
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    invalid=package("invalid");invalid.interfaceSize=SIZE_MAX;
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    XL_PackageEntry duplicateEntries[]={entry,entry};
    invalid=package("duplicate");invalid.entries=duplicateEntries;invalid.entryCount=2;
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    XL_PackageEntry unordered[]={{"registry_z",(const void*)answer},{"registry_a",(const void*)other}};
    XL_CompiledPackage sorted=package("sorted");sorted.entries=unordered;sorted.entryCount=2;
    assert(xlImportCompiledPackage(context,&sorted));
    XL_PackageEntry reversed[]={unordered[1],unordered[0]};sorted.entries=reversed;
    assert(xlImportCompiledPackage(context,&sorted));
    XL_CompiledPackage cycle=package("cycle");const XL_CompiledPackage* cycleDeps[]={&cycle};
    cycle.dependencies=cycleDeps;cycle.dependencyCount=1;
    assert(!xlImportCompiledPackage(context,&cycle));assert(!xlFindCompiledPackage(context,"cycle"));clear(context);
    unsigned char cyclicWire[128];
    cycle=package("cycle");cycle.interfaceSize=importInterface(cyclicWire,"cycle");cycle.interfaceData=cyclicWire;
    cycle.interfaceHash=xlCompiledPackageInterfaceHash(cyclicWire,cycle.interfaceSize);
    assert(xlImportCompiledPackage(context,&cycle));
    assert(!xlRunScriptString(context,"import cycle\n"));clear(context);
    XL_CompiledPackage unresolved=package("unresolved");
    unresolved.interfaceSize=importInterface(cyclicWire,"compiled_demo");unresolved.interfaceData=cyclicWire;
    unresolved.interfaceHash=xlCompiledPackageInterfaceHash(cyclicWire,unresolved.interfaceSize);
    assert(xlImportCompiledPackage(context,&unresolved));
    assert(!xlRunScriptString(context,"import unresolved\n"));clear(context);
    unsigned char wire[44]={0};memcpy(wire,emptyInterface,sizeof(emptyInterface));
    for(size_t n=0;n<sizeof(emptyInterface);++n) {
        invalid=package("truncated");invalid.interfaceSize=n;
        invalid.interfaceHash=xlCompiledPackageInterfaceHash(invalid.interfaceData,n);
        assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    }
    invalid=package("trailing");invalid.interfaceData=wire;invalid.interfaceSize=sizeof(wire);
    invalid.interfaceHash=xlCompiledPackageInterfaceHash(wire,sizeof(wire));
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    wire[20]=1;memset(wire+40,255,4);invalid.name="null_child";
    invalid.interfaceHash=xlCompiledPackageInterfaceHash(wire,sizeof(wire));
    assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    XL_CompiledPackage copied=package("copied");
    unsigned char ownedWire[sizeof(emptyInterface)];memcpy(ownedWire,emptyInterface,sizeof(ownedWire));
    copied.interfaceData=ownedWire;
    assert(xlImportCompiledPackage(context,&copied));
    memset(ownedWire,0,sizeof(ownedWire));copied.name="changed";
    assert(xlFindCompiledPackage(context,"copied")->interfaceData[0]=='X');
    const XL_CompiledPackage* stable=xlFindCompiledPackage(context,"leaf");
    XL_CompiledPackage another=package("another");assert(xlImportCompiledPackage(context,&another));
    assert(stable==xlFindCompiledPackage(context,"leaf"));
    XL_Module* detached=xlCompileScriptStringToModule(context,
        "extern \"compiled_registry_answer\" func answer() as int\nassert answer()==42\n");
    assert(detached);
    puts("registry: dependency diamond, isolation, idempotence, versions, symbols, ABI/runtime and malformed interfaces");
    xlDestroyContext(context);
    assert(xlRunModuleMain(anchor,detached,NULL));xlDestroyModule(detached);

    for(int prepopulated=0;prepopulated<2;++prepopulated) {
      size_t point=0;
      for(;point<512;++point) {
        context=xlCreateContext(&options);assert(context);
        XL_CompiledPackage existing=package("existing");
        if(prepopulated)assert(xlImportCompiledPackage(context,&existing));
        clear(context);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));int ok=xlImportCompiledPackage(context,&root);
        int hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();clear(context);
        if(!ok) {
            assert(!xlFindCompiledPackage(context,"root") && !xlFindCompiledPackage(context,"leaf"));
            assert(!prepopulated || xlFindCompiledPackage(context,"existing"));
            sameMemory(&before);
            assert(xlImportCompiledPackage(context,&root));
        } else assert(!hit);
        xlDestroyContext(context);
        if(!hit)break;
      }
      assert(point<512);
      printf("registry: prepopulated=%d allocation-failure prefixes=%zu; atomic rollback and successful retry\n",prepopulated,point+1);
    }
    XL_CompiledPackage many[XL_COMPILED_PACKAGE_MAX_PACKAGES];
    const XL_CompiledPackage* chain[XL_COMPILED_PACKAGE_MAX_PACKAGES];
    char names[XL_COMPILED_PACKAGE_MAX_PACKAGES][24];
    for(size_t i=0;i<XL_COMPILED_PACKAGE_MAX_PACKAGES;++i) {
        snprintf(names[i],sizeof(names[i]),"bounded_%zu",i);many[i]=package(names[i]);chain[i]=&many[i];
        if(i){many[i].dependencies=&chain[i-1];many[i].dependencyCount=1;}
    }
    context=xlCreateContext(&options);assert(context);
    assert(xlImportCompiledPackage(context,&many[XL_COMPILED_PACKAGE_MAX_PACKAGES-1]));
    assert(xlImportCompiledPackage(context,&many[XL_COMPILED_PACKAGE_MAX_PACKAGES-1]));
    invalid=package("overflow");assert(!xlImportCompiledPackage(context,&invalid));clear(context);
    assert(xlFindCompiledPackage(context,names[0]));xlDestroyContext(context);
    puts("registry: package quota, duplicate entry rejection and order-independent identity");
    aggregateLimits(&options);
    xlDestroyContext(anchor);
    return 0;
}
