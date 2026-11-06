/* 单独的固定负载计时，不放进正确性门禁；包含完整parse/验证/输出释放。 */
#include "../include/cli_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef XCLI_TEST_NATIVE
#define XCLI_TEST_NATIVE "../cli_native.c"
#endif
#include XCLI_TEST_NATIVE

int main(int argc,char** argv)
{
    if (argc!=3) return 2;
    unsigned count=(unsigned)strtoul(argv[1],NULL,10),arguments=(unsigned)strtoul(argv[2],NULL,10);
    if ((count!=4u && count!=32u && count!=256u) || (arguments!=1u && arguments!=512u)) return 2;
    xvalue *spec=xrtValueObject(),*flags=xrtValueArray(),*args=xrtValueArray();
    char key[32],token[36]; int status=1;
    if (!spec || !flags || !args) goto done;
    for (unsigned i=0u;i<count;++i) {
        snprintf(key,sizeof(key),"flag-%03u",i);
        if (!xrtValueArrayAppendNew(flags,xrtValueString((xstrview){key,strlen(key)}))) goto done;
    }
    if (!xrtValueObjectSetTake(spec,XRT_STR_LITERAL("flags"),&flags)) goto done;
    snprintf(token,sizeof(token),"--%s",key);
    for (unsigned i=0u;i<arguments;++i)
        if (!xrtValueArrayAppendNew(args,xrtValueString((xstrview){token,strlen(token)}))) goto done;
    clock_t start=clock();
    for (unsigned iteration=0u;iteration<200u;++iteration) {
        xvalue* result=xcli_parse(args,spec);
        bool okay=false,active=false;
        if (!result || !xrtValueGetBool(xrtValueObjectGet(result,XRT_STR_LITERAL("ok")),&okay) || !okay ||
            !xrtValueGetBool(xrtValueObjectGet(xrtValueObjectGet(result,XRT_STR_LITERAL("flags")),
                (xstrview){key,strlen(key)}),&active) || !active) { xrtValueRelease(result); goto done; }
        xrtValueRelease(result);
    }
    printf("%u\t%u\t200\t%.3f\n",count,arguments,1000.0*(double)(clock()-start)/CLOCKS_PER_SEC);
    status=0;
done:
    xrtValueRelease(flags); xrtValueRelease(spec); xrtValueRelease(args); return status;
}
