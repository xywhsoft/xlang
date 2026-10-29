#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_array_entry.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int xl_script_main_checked(xvalue*,int*);
int xl_script_module_unit_checked(void);
int main(void)
{
    int code=97; assert(xl_script_main_checked(NULL,&code) && !code && !xrtGetError());
    xvalue* wire=xrtValueArray(); assert(wire);
    assert(xrtValueArrayAppendNew(wire,xrtValueString((xstrview){"a\0\xe4\xbd\xa0",5})));
    unsigned char bytes[]={0,65,255};
    assert(xrtValueArrayAppendNew(wire,xrtValueBytes((xbytesview){bytes,3})));
    assert(xrtValueArrayAppendNew(wire,xrtValueNull()));
    assert(xl_script_main_checked(wire,&code) && !code && !xrtGetError());
    xstrview text={0}; assert(xrtValueGetString(xrtValueArrayGet(wire,0),&text) && text.Size==5);
    xvalue* native=xl6ArrayEntryAcquireV1(UINT64_C(0x275BD96A834D5C08),UINT64_C(0x3594F07E9DD4DB58),wire); assert(native);
    assert(xl_script_main_checked(native,&code) && !code && !xrtGetError());
    xvalue* item=xl6ArrayErasedReadV1(native,0);
    assert(item && xrtValueGetString(item,&text) && text.Size==7 && !memcmp(text.Data,"changed",7));
    xrtValueRelease(item); xrtValueRelease(native); xrtValueRelease(wire);
    code=97; assert(!xl_script_main_checked(xrtValueNull(),&code) && code==1);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    assert(xl_script_main_checked(NULL,&code) && !code && !xrtGetError());
    assert(xl_script_module_unit_checked() && !xrtGetError());
    puts("native entry host passed"); return 0;
}
