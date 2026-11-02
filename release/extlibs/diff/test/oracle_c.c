/* 长度前缀语料由独立Python参照生成；C只验证真实公开结果，不生成期望。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FILE_WHOLE
#include "xrt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef XDIFF_FORCE_COLLISION
static uint64 collision_hash(const void* data,size_t size)
{(void)data;(void)size;return 0;}
#define xrtHash64 collision_hash
#endif
#include "../diff_native.c"

static uint32_t u32(const unsigned char* p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtMemDebugEnabled())return 2;
    size_t size=0;bytes data=xrtFileReadAllLimit(argv[1],64u*1024u*1024u,&size);
    if(!data || size<12 || memcmp(data,"XDIFF01\0",8)){xrtFree(data);return 2;}
    uint32_t count=u32(data+8);size_t cursor=12;unsigned failures=0,checks=0;
    if(!count){xrtFree(data);return 2;}
    char* warm=xdiff_lines("","");xrtFree(warm);warm=xdiff_stats("","");xrtFree(warm);xrtClearError();
    xmemdebugsnapshot whole;xrtMemDebugSnapshot(&whole);
    for(uint32_t record=0;record<count;++record){
        uint32_t lengths[4];const unsigned char* fields[4];
        if(size-cursor<16){failures=1;break;}
        for(unsigned field=0;field<4;++field)lengths[field]=u32(data+cursor+field*4);
        cursor+=16;
        bool valid=true;
        for(unsigned field=0;field<4;++field){
            if(lengths[field]>size-cursor){valid=false;break;}
            fields[field]=data+cursor;cursor+=lengths[field];
        }
        if(!valid || lengths[0]>8u*1024u*1024u || lengths[1]>8u*1024u*1024u ||
            memchr(fields[0],0,lengths[0]) || memchr(fields[1],0,lengths[1])){failures=1;break;}
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        char *old=xrtMalloc((size_t)lengths[0]+1),*new=xrtMalloc((size_t)lengths[1]+1);
        if(!old || !new){xrtFree(old);xrtFree(new);failures=1;break;}
        memcpy(old,fields[0],lengths[0]);old[lengths[0]]=0;memcpy(new,fields[1],lengths[1]);new[lengths[1]]=0;
        char* output=xdiff_lines(old,new);
        bool outputOk=output && !xrtGetError() && strlen(output)==lengths[2] && !memcmp(output,fields[2],lengths[2]);++checks;
        char* stats=xdiff_stats(old,new);
        bool statsOk=stats && !xrtGetError() && strlen(stats)==lengths[3] && !memcmp(stats,fields[3],lengths[3]);++checks;
        xrtFree(stats);xrtFree(output);xrtFree(old);xrtFree(new);
        bool memoryOk=balanced(&before);++checks;
        if(!outputOk || !statsOk || !memoryOk){
            if(failures<8)fprintf(stderr,"diff oracle mismatch case=%u output=%d stats=%d memory=%d\n",record,outputOk,statsOk,memoryOk);
            ++failures;
        }
    }
    if(cursor!=size || !balanced(&whole))++failures;
    xrtFree(data);
    printf("diff independent oracle: %u cases, %u checks, %u failures\n",count,checks,failures);
    return failures ? 1 : 0;
}
