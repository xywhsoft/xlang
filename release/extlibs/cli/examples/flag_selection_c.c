#include "../include/cli_native.h"
#include <stdio.h>
#include <string.h>
#ifdef __TINYC__
#define XL_VALUE_READ_WIRE_ONLY
#define XL_VALUE_READ_WIRE_IMPLEMENTATION
#include "../../_common/xl_value_read_wire.h"
#include "../cli_native.c"
#endif

static xstrview view(cstr text) { return (xstrview){text,strlen(text)}; }
static xvalue* field(xvalue* value,cstr key) { return xrtValueObjectGet(value,view(key)); }
static xvalue* strings(cstr const* items,size_t count)
{
    xvalue* result=xrtValueArray();
    if (!result) return NULL;
    for (size_t i=0u;i<count;++i) {
        if (!xrtValueArrayAppendNew(result,xrtValueString(view(items[i])))) {
            xrtValueRelease(result); return NULL;
        }
    }
    return result;
}
static bool boolean(xvalue* value,cstr key,bool wanted)
{
    bool actual;
    return xrtValueGetBool(field(value,key),&actual) && actual==wanted;
}
int main(void)
{
    static cstr const names[]={"quiet","Quiet","q","Z","开关","pending"};
    static cstr const tokens[]={"--Quiet","-qZ","--开关"};
    static cstr const invalid[]={"--Quiet","--missing","--quiet"};
    xvalue *spec=xrtValueObject(),*args=strings(tokens,3u),*bad=strings(invalid,3u);
    xvalue *result=NULL,*rejected=NULL;
    int status=1;
    if (!spec || !args || !bad || !xrtValueObjectSetNew(spec,view("flags"),strings(names,6u))) goto done;
    result=xcli_parse(args,spec);
    xrtValueRelease(args); args=NULL;
    if (!result || !boolean(result,"ok",true)) goto done;
    xvalue* flags=field(result,"flags");
    if (!boolean(flags,"quiet",false) || !boolean(flags,"Quiet",true)) goto done;
    puts("case sensitive: true");
    if (!boolean(flags,"q",true) || !boolean(flags,"Z",true)) goto done;
    puts("short flags: true");
    if (!boolean(flags,"开关",true)) goto done;
    puts("unicode flag: true");
    if (!boolean(flags,"pending",false)) goto done;
    puts("unselected: false");
    rejected=xcli_parse(bad,spec);
    xstrview error;
    if (!rejected || !boolean(rejected,"ok",false) ||
        !xrtValueGetString(field(rejected,"error"),&error) ||
        error.Size!=strlen("unknown option --missing") || memcmp(error.Data,"unknown option --missing",error.Size) ||
        !boolean(field(rejected,"flags"),"Quiet",true) || !boolean(field(rejected,"flags"),"quiet",false) ||
        !boolean(result,"ok",true) || !boolean(flags,"开关",true)) goto done;
    puts("first error: true"); status=0;
done:
    xrtValueRelease(rejected); xrtValueRelease(result); xrtValueRelease(bad); xrtValueRelease(args); xrtValueRelease(spec);
    return status;
}
