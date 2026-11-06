/* 结果字典中的false也表示已声明；复用其键查找不能改变解析/首错/寿命。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/cli_native.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#define XL_VALUE_READ_WIRE_ONLY
#define XL_VALUE_READ_WIRE_IMPLEMENTATION
#include "../../_common/xl_value_read_wire.h"
#endif

#ifndef XCLI_TEST_NATIVE
#define XCLI_TEST_NATIVE "../cli_native.c"
#endif
#include XCLI_TEST_NATIVE

static unsigned failures, cases, faults;
static void check(bool okay, cstr message)
{
    if (!okay) { if (failures < 8u) fprintf(stderr,"cli flag lookup: %s\n",message); ++failures; }
}
static xstrview view(cstr text) { return (xstrview){text,strlen(text)}; }
static xvalue* field(xvalue* value,cstr key) { return xrtValueObjectGet(value,view(key)); }
static bool push(xvalue* array,cstr text) { return xrtValueArrayAppendNew(array,xrtValueString(view(text))); }
static void name(char output[64],unsigned index)
{
    if (index < 2u) snprintf(output,64,"%s",index ? "Z" : "q");
    else snprintf(output,64,index % 2u ? "Flag-%03u" : "开关-%03u",index);
}
static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
static xvalue* specification(unsigned count)
{
    xvalue* spec=xrtValueObject();
    xvalue* flags=xrtValueArray();
    if (!spec || !flags) goto failed;
    for (unsigned i=0u;i<count;++i) {
        char key[64]; name(key,i);
        if (!push(flags,key)) goto failed;
    }
    if (xrtValueObjectSetTake(spec,XRT_STR_LITERAL("flags"),&flags)) return spec;
failed:
    xrtValueRelease(flags); xrtValueRelease(spec); return NULL;
}
static bool add_flag(xvalue* args,bool expected[256],unsigned index)
{
    char key[64],token[68]; name(key,index); snprintf(token,sizeof(token),"--%s",key);
    expected[index]=true; return push(args,token);
}
static xvalue* arguments(unsigned count,unsigned pattern,bool expected[256],char error[128])
{
    xvalue* args=xrtValueArray();
    if (!args) return NULL;
    error[0]=0;
    if (pattern==1u && count) {
        if (!add_flag(args,expected,0u) || !add_flag(args,expected,count-1u)) goto failed;
    } else if (pattern==2u) {
        for (unsigned i=count;i>0u;--i) if (!add_flag(args,expected,i-1u)) goto failed;
    } else if (pattern==3u && count) {
        for (unsigned i=0u;i<64u;++i) if (!add_flag(args,expected,count-1u)) goto failed;
    } else if (pattern==4u && count>=2u) {
        if (!push(args,"-qZq")) goto failed;
        expected[0]=expected[1]=true;
    } else if (pattern>=5u) {
        if (count && !add_flag(args,expected,count-1u)) goto failed;
        if (pattern==5u) {
            if (!push(args,"-?")) goto failed;
            snprintf(error,128,"unknown short option -?");
        } else if (pattern==6u) {
            if (!push(args,"--missing")) goto failed;
            snprintf(error,128,"unknown option --missing");
        } else if (count) {
            char key[64],token[80]; name(key,0u); snprintf(token,sizeof(token),"--%s=bad",key);
            if (!push(args,token)) goto failed;
            snprintf(error,128,"flag --%s does not take a value",key);
        } else {
            if (!push(args,"--missing=bad")) goto failed;
            snprintf(error,128,"unknown option --missing");
        }
        /* 首错之后的旗标不可生效，即使同一token本身语法正确。 */
        if (count && !push(args,"--q")) goto failed;
    }
    return args;
failed:
    xrtValueRelease(args); return NULL;
}
static bool result_is(xvalue* result,unsigned count,const bool expected[256],cstr error)
{
    bool okay;
    xstrview message;
    xvalue* flags=result ? field(result,"flags") : NULL;
    if (!result || xrtGetError()!=NULL || !xrtValueGetBool(field(result,"ok"),&okay) ||
        okay!=(error[0]==0) || !xrtValueGetString(field(result,"error"),&message) ||
        message.Size!=strlen(error) || memcmp(message.Data,error,message.Size) ||
        !flags || xrtValueCount(flags)!=count || xrtValueCount(field(result,"options")) ||
        xrtValueCount(field(result,"positional")) || xrtValueCount(field(result,"rest"))) return false;
    for (unsigned i=0u;i<count;++i) {
        char key[64]; bool active; name(key,i);
        if (!xrtValueGetBool(field(flags,key),&active) || active!=expected[i]) return false;
    }
    return true;
}
static void exercise(unsigned count,unsigned pattern,bool costs)
{
    xmemdebugsnapshot outer,before,after;
    bool expected[256]={false}; char error[128];
    xrtMemDebugSnapshot(&outer);
    xvalue* spec=specification(count);
    xvalue* args=arguments(count,pattern,expected,error);
    check(spec && args,"fixture allocation");
    if (!spec || !args) goto done;
    xrtMemDebugSnapshot(&before);
    xvalue* held=xcli_parse(args,spec);
    xrtMemDebugSnapshot(&after);
    check(result_is(held,count,expected,error),"result differs");
    if (costs) printf("%u\t%u\t%" PRIu64 "\n",count,pattern,(uint64_t)(after.AllocCount-before.AllocCount));
    /* 三条命中/拒绝路径枚举每个输出分配点，不用大矩阵反复注入相同故障。 */
    if (count==8u && (pattern==1u || pattern==5u || pattern==7u)) {
        xrtMemDebugSnapshot(&before);
        bool success=false;
        for (unsigned budget=0u;budget<256u;++budget) {
            check(xrtMemDebugFailAfter(budget),"fault setup");
            xvalue* result=xcli_parse(args,spec);
            bool triggered=xrtMemDebugFailTriggered();
            xerrkind kind=xrtErrorKind(xrtGetError());
            xrtMemDebugFailClear();
            if (triggered) {
                check(result==NULL && kind==XERR_MEMORY,"allocation failure contract"); ++faults;
                xrtValueRelease(result); result=xcli_parse(args,spec);
            }
            check(result_is(result,count,expected,error) && result_is(held,count,expected,error),"recovery or held result changed");
            xrtValueRelease(result);
            check(balanced(&before),"fault memory balance");
            if (!triggered) { success=true; break; }
        }
        check(success,"fault sweep did not finish");
    }
    xrtValueRelease(args); args=NULL; xrtValueRelease(spec); spec=NULL;
    check(result_is(held,count,expected,error),"result borrows released input");
    xrtValueRelease(held);
done:
    xrtValueRelease(args); xrtValueRelease(spec);
    check(balanced(&outer),"case memory balance"); ++cases;
}
int main(int argc,char** argv)
{
    static const unsigned counts[]={0u,1u,2u,8u,64u,256u};
    bool costs=argc==2 && strcmp(argv[1],"--costs")==0;
    if (argc>1 && !costs) return 2;
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE,"test.cli",0,"warm diagnostics"); xrtClearError();
    for (unsigned c=0u;c<sizeof(counts)/sizeof(counts[0]);++c)
        for (unsigned pattern=0u;pattern<8u;++pattern) exercise(counts[c],pattern,costs);
    if (!costs) printf("cli flag lookup: %u cases, %u allocation faults, %u failures\n",cases,faults,failures);
    return failures ? 1 : 0;
}
