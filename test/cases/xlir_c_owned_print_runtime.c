#include <assert.h>
#include <string.h>
#include "include/xruntime_decl.h"
static const char* original;
static bool sawOriginal, failWrite;
static char output[512];
static size_t outputSize;
static bool writePart(xconsolestream stream,xstrview text)
{
    assert(stream==XCONSOLE_STDOUT);
    if(failWrite)return false;
    if(text.Data==original)sawOriginal=true;
    assert(text.Size<=sizeof(output)-outputSize);
    memcpy(output+outputSize,text.Data,text.Size);outputSize+=text.Size;
    return true;
}
static bool writeLine(xconsolestream stream,xstrview text)
{
    return writePart(stream,text) && writePart(stream,(xstrview){"\n",1});
}
#define xrtConsoleWrite writePart
#define xrtConsoleWriteLine writeLine
#include "src/runtime/xl6_c_print.h"
#undef xrtConsoleWrite
#undef xrtConsoleWriteLine
int main(void)
{
    assert(xrtMemDebugEnable(true));
    char text[]="hello";original=text;
    XL6_CPrintPartV1 part={.Kind=1,.Data=text,.Size=5};
    xmemdebugsnapshot before={0},after={0};
    xrtMemDebugSnapshot(&before);
    assert(xl6CPrintV1(&part,1,true));
    xrtMemDebugSnapshot(&after);
    assert(sawOriginal && outputSize==6 && !memcmp(output,"hello\n",6));
    assert(before.AllocCount==after.AllocCount);
    const unsigned char bytes[]={0,255,17,42};
    part=(XL6_CPrintPartV1){.Kind=2,.Data=bytes,.Size=sizeof(bytes)};
    outputSize=0;
    assert(xl6CPrintV1(&part,1,false));
    assert(outputSize==17 && !memcmp(output,"bytes(\"AP8RKg==\")",17));
    xvalue* number=xrtValueInt(42);assert(number);
    XL6_CPrintPartV1 parts[]={{.Kind=1,.Data=text,.Size=5},{.Kind=3,.Value=number}};
    outputSize=0;
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6CPrintV1(parts,2,false) && outputSize==0);
    xrtMemDebugFailClear();xrtClearError();xrtValueRelease(number);
    failWrite=true;part=(XL6_CPrintPartV1){.Kind=1,.Data=text,.Size=5};
    assert(!xl6CPrintV1(&part,1,false) && xrtGetError());
    xrtClearError();
    return 0;
}
