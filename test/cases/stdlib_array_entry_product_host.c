/* Actual runtime/compiler product, embedded VFS, original Array entry values
 * fixture. Binary and embedded-NUL values cannot be supplied through OS argv. */
#include "xl.h"
#include "xruntime_decl.h"
#include "__xl_runtime/xl6_array_entry.h"
#include "__xl_runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void invoke(XL_Context* context,XL_Module* module,xvalue* value)
{
    int ok=xlRunModuleMain(context,module,value);
    if(!ok)fprintf(stderr,"entry main: %s\n",xlGetLastError(context));
    assert(ok && xlGetLastExitCode(context)==0 && !xrtGetError());
}
static void transaction(const char* runtime,const char* source,const char* temporary)
{
    assert(xlRuntimeInit());
    XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir=".";options.tempDir=temporary;options.tccDllPath=runtime;
    XL_Context* context=xlCreateContext(&options);assert(context);
    XL_Module* module=xlCompileScriptFileToModule(context,source);
    if(!module)fprintf(stderr,"entry compile: %s\n",xlGetLastError(context));
    assert(module);invoke(context,module,NULL);
    xvalue* wire=xrtValueArray();assert(wire);
    assert(xrtValueArrayAppendNew(wire,xrtValueString((xstrview){"a\0\xe4\xbd\xa0",5})));
    const unsigned char bytes[]={0,65,255};
    assert(xrtValueArrayAppendNew(wire,xrtValueBytes((xbytesview){bytes,3})));
    assert(xrtValueArrayAppendNew(wire,xrtValueNull()));
    invoke(context,module,wire);
    xstrview text={0};assert(xrtValueGetString(xrtValueArrayGet(wire,0),&text) && text.Size==5);
    xvalue* native=xl6ArrayEntryAcquireV1(UINT64_C(0x275BD96A834D5C08),UINT64_C(0x3594F07E9DD4DB58),wire);assert(native);
    invoke(context,module,native);
    xvalue* item=xl6ArrayErasedReadV1(native,0);
    assert(item && xrtValueGetString(item,&text) && text.Size==7 && !memcmp(text.Data,"changed",7));
    xrtValueRelease(item);xrtValueRelease(native);xrtValueRelease(wire);
    assert(!xlRunModuleMain(context,module,xrtValueNull()));
    assert(xlGetLastExitCode(context)==1 && xrtErrorKind(xrtGetError())==XERR_TYPE);xrtClearError();
    invoke(context,module,NULL);
    xlDestroyModule(module);xlDestroyContext(context);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(5000);
    assert(drained.ready && drained.ok && !xrtGetError());xlRuntimeUnit();assert(!xrtGetError());
}
int main(int argc,char** argv)
{
    assert(argc==4 && xrtMemDebugEnable(true));
    transaction(argv[1],argv[2],argv[3]);transaction(argv[1],argv[2],argv[3]);
    for(unsigned round=0;round<8;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        transaction(argv[1],argv[2],argv[3]);xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
            before.UseAfterFreeCount==after.UseAfterFreeCount);
    }
    puts("entry product: NULL, boxed NUL/bytes/null, native Array mutation, invalid input; 8 balanced lifecycles");
    return 0;
}
