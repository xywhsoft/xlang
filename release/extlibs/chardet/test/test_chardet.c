/* 独立 C：准确字节长度、非法 Unicode、借用返回、零分配和错误恢复。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/chardet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned char data[16];
    size_t size;
    const char* name;
    int confidence, bom;
    bool utf8, ascii;
} sample;

static sample samples[] = {
    {{0},0,"UNKNOWN",0,0,true,true},
    {{'h','i'},2,"UTF-8",40,0,true,true},
    {{0xe4,0xbd,0xa0,0xe5,0xa5,0xbd},6,"UTF-8",90,0,true,false},
    {{0xef,0xbb,0xbf,'A'},4,"UTF-8-BOM",100,3,true,false},
    {{0xff,0xfe,'A',0},4,"UTF-16LE",100,2,false,false},
    {{0xfe,0xff,0,'A'},4,"UTF-16BE",100,2,false,false},
    {{0xff,0xfe,0,0,'A',0,0,0},8,"UTF-32LE",100,4,false,false},
    {{0,0,0xfe,0xff,0,0,0,'A'},8,"UTF-32BE",100,4,false,false},
    {{0xef,0xbb,0xbf},3,"UTF-8-BOM",100,3,true,false},
    {{0xef,0xbb,0xbf,0xff},4,"UTF-8-BOM",100,3,false,false},
    {{'A',0,'B',0,'C',0,'D',0},8,"UTF-16LE",85,0,true,true},
    {{0,'A',0,'B',0,'C',0,'D'},8,"UTF-16BE",85,0,true,true},
    {{'A',0,0,0,'B',0,0,0},8,"UTF-32LE",80,0,true,true},
    {{0,0,0,'A',0,0,0,'B'},8,"UTF-32BE",80,0,true,true},
    {{'A',0,'B'},3,"UTF-8",50,0,true,true},
    {{'A',0,0xff},3,"UNKNOWN",0,0,false,false},
    {{0xe4,0xbd},2,"UNKNOWN",0,0,false,false},
    {{0xc0,0xaf},2,"UNKNOWN",0,0,false,false},
    {{0xed,0xa0,0x80},3,"UNKNOWN",0,0,false,false},
    {{0xf4,0x90,0x80,0x80},4,"UNKNOWN",0,0,false,false},
    {{0xff},1,"UNKNOWN",0,0,false,false},
    {{0},1,"UTF-8",50,0,true,true}
};

static void check(sample* s)
{
    unsigned char before[16];
    const char* p = (const char*)s->data;
    memcpy(before, s->data, sizeof(before));
    assert(strcmp(xchardet_detect_name_n(s->data,s->size),s->name)==0);
    assert(xchardet_detect_conf_n(s->data,s->size)==s->confidence);
    assert(xchardet_detect_bom_size_n(s->data,s->size)==s->bom);
    assert(xchardet_is_utf8_n(s->data,s->size)==s->utf8);
    assert(xchardet_is_ascii_n(s->data,s->size)==s->ascii);
    assert(strcmp(xchardet_detect_name_str(p,s->size),s->name)==0);
    assert(xchardet_detect_conf_str(p,s->size)==s->confidence);
    assert(xchardet_detect_bom_size_str(p,s->size)==s->bom);
    assert(xchardet_is_utf8_str(p,s->size)==s->utf8);
    assert(xchardet_is_ascii_str(p,s->size)==s->ascii);
    assert(memcmp(before,s->data,sizeof(before))==0);
    assert(xrtGetError()==NULL);
}

static void argument_error(void)
{
    const xerror* error = xrtGetError();
    assert(error && xrtErrorKind(error)==XERR_ARGUMENT);
    check(&samples[1]);
}

int main(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<sizeof(samples)/sizeof(samples[0]);++i) check(&samples[i]);
    (void)xchardet_detect_name_n(NULL,1); argument_error();
    (void)xchardet_detect_conf_n(NULL,1); argument_error();
    (void)xchardet_detect_bom_size_n(NULL,1); argument_error();
    (void)xchardet_is_utf8_n(NULL,1); argument_error();
    (void)xchardet_is_ascii_n(NULL,1); argument_error();
    (void)xchardet_detect_name_str(NULL,1); argument_error();
    (void)xchardet_detect_conf_str(NULL,1); argument_error();
    (void)xchardet_detect_bom_size_str(NULL,1); argument_error();
    (void)xchardet_is_utf8_str(NULL,1); argument_error();
    (void)xchardet_is_ascii_str(NULL,1); argument_error();
    assert(strcmp(xchardet_detect_name_n(NULL,0),"UNKNOWN")==0);
    assert(xchardet_is_utf8_n(NULL,0) && xchardet_is_ascii_n(NULL,0));

    /* 探测不分配内存；失败注入不应改变任何正常结论。 */
    assert(xrtMemDebugFailAfter(0));
    for(int round=0;round<1000;++round) {
        for(size_t i=0;i<sizeof(samples)/sizeof(samples[0]);++i) check(&samples[i]);
    }
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.OverflowCount==after.OverflowCount && before.UnderflowCount==after.UnderflowCount);
    puts("chardet C: 22 vectors, 10 ABIs, invalid pointers, zero allocation and ownership PASS");
    return 0;
}
