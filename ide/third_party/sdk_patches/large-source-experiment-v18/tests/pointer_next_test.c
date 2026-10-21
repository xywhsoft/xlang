#define _WIN32_WINNT 0x0601
#include "xui_internal.h"
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int old_xuiInternalTextGraphemeNext(const char*,int,int);
int old_xuiInternalTextGraphemeNextRead(xui_internal_text_read_proc,void*,int,int);
typedef struct reader {const unsigned char* text;int length,failAt;uint64_t reads;} reader;
static uint64_t assertions,cases,callbackFailureCases,scalarCases,pairCases,guardCases;
#define CHECK(v) do {++assertions;if(!(v)) {fprintf(stderr,"pointer grapheme:%d: %s; case=%llu\n",__LINE__,#v,(unsigned long long)cases);exit(1);}} while(0)
static int readByte(void* user,int offset,unsigned char* out) {
    reader* r=user;++r->reads;
    if(offset<0 || offset>=r->length || offset==r->failAt)return 0;
    *out=r->text[offset];return 1;
}
static void compare(const unsigned char* text,int length,int offset) {
    int actualLength=length<0 ? (int)strlen((const char*)text) : length;
    reader a={text,actualLength,-1,0},b=a;
    int old=old_xuiInternalTextGraphemeNext((const char*)text,length,offset);
    int current=xuiInternalTextGraphemeNext((const char*)text,length,offset);
    ++cases;CHECK(old==current);
    CHECK(old_xuiInternalTextGraphemeNextRead(readByte,&a,actualLength,offset)==old);
    CHECK(xuiInternalTextGraphemeNextRead(readByte,&b,actualLength,offset)==current);
    CHECK(a.reads==b.reads);
}
static int encode(uint32_t cp,unsigned char* out) {
    if(cp<0x80) {out[0]=(unsigned char)cp;return 1;}
    if(cp<0x800) {out[0]=(unsigned char)(0xc0|(cp>>6));out[1]=(unsigned char)(0x80|(cp&63));return 2;}
    if(cp<0x10000) {out[0]=(unsigned char)(0xe0|(cp>>12));out[1]=(unsigned char)(0x80|((cp>>6)&63));out[2]=(unsigned char)(0x80|(cp&63));return 3;}
    out[0]=(unsigned char)(0xf0|(cp>>18));out[1]=(unsigned char)(0x80|((cp>>12)&63));out[2]=(unsigned char)(0x80|((cp>>6)&63));out[3]=(unsigned char)(0x80|(cp&63));return 4;
}
static void partials(const unsigned char* text,int length) {
    for(int n=0;n<=length;++n)
        for(int offset=-1;offset<=n+1;++offset)compare(text,n,offset);
    for(int offset=-1;offset<=length+1;++offset)compare(text,-1,offset);
}
static uint32_t randomState=0x754cae13u;
static uint32_t randomValue(void) {
    randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;
}
static double elapsed(LARGE_INTEGER start,LARGE_INTEGER frequency) {
    LARGE_INTEGER end;QueryPerformanceCounter(&end);return (end.QuadPart-start.QuadPart)*1000.0/frequency.QuadPart;
}
int main(void) {
    static const char* corpus[]={"","a","\r\n","a\tbc\r\n\x00\x7f","e\xcc\x81Z",
        "\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x98\x80", "\xf0\x9f\x87\xa8\xf0\x9f\x87\xb3\xf0\x9f\x87\xba",
        "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa7",
        "\xe1\x84\x80\xe1\x85\xa1\xe1\x86\xa8", "\xd8\x80" "a\xcc\x81\xe2\x80\x8d" "b",
        "\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7", "\xff\x80\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80"};
    for(size_t i=0;i<sizeof(corpus)/sizeof(corpus[0]);++i)partials((const unsigned char*)corpus[i],(int)strlen(corpus[i]));
    for(uint32_t cp=0;cp<=0x10ffffu;++cp) {
        unsigned char text[16]={0};int n=encode(cp,text);
        memcpy(text+n,"\xcc\x81Z",3);compare(text,n+3,0);compare(text,n+3,n);++scalarCases;
    }
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b) {
        unsigned char text[4]={(unsigned char)a,(unsigned char)b,'A',0};
        for(int n=1;n<=3;++n)for(int offset=-1;offset<=n+1;++offset)compare(text,n,offset);
        ++pairCases;
    }
    for(int i=0;i<4096;++i) {
        unsigned char text[98]={0};int n=(int)(randomValue()%97u);
        for(int j=0;j<n;++j)text[j]=(unsigned char)randomValue();
        for(int offset=-1;offset<=n+1;++offset)compare(text,n,offset);
        for(int j=0;j<8;++j) {int partial=(int)(randomValue()%(unsigned)(n+1));compare(text,partial,(int)(randomValue()%(unsigned)(partial+3))-1);}
    }
    {
        unsigned char text[258]={0};text[0]='e';
        for(int i=1;i<255;i+=2) {text[i]=0xcc;text[i+1]=0x81;}
        text[255]='Z';partials(text,256);
    }
    for(int i=0;i<256;++i) {
        unsigned char text[33]={0};for(int j=0;j<32;++j)text[j]=(unsigned char)randomValue();
        for(int failure=0;failure<32;++failure)for(int offset=-1;offset<=32;++offset) {
            reader a={text,32,failure,0},b=a;
            CHECK(old_xuiInternalTextGraphemeNextRead(readByte,&a,32,offset)==xuiInternalTextGraphemeNextRead(readByte,&b,32,offset));
            CHECK(a.reads==b.reads);++callbackFailureCases;
        }
    }
    CHECK(old_xuiInternalTextGraphemeNext(NULL,-1,0)==xuiInternalTextGraphemeNext(NULL,-1,0));
    CHECK(old_xuiInternalTextGraphemeNextRead(NULL,NULL,1,0)==xuiInternalTextGraphemeNextRead(NULL,NULL,1,0));
    {
        SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
        unsigned char* memory=VirtualAlloc(NULL,page*2,MEM_RESERVE,PAGE_NOACCESS);CHECK(memory!=NULL);
        CHECK(VirtualAlloc(memory,page,MEM_COMMIT,PAGE_READWRITE)==memory);
        for(int n=0;n<=128;++n) {
            unsigned char* text=memory+page-n;
            for(int i=0;i<n;++i)text[i]=(unsigned char)(i%7==0 ? 0xf0 : i%3==0 ? 0xcc : 0x81);
            for(int offset=-1;offset<=n+1;++offset) {compare(text,n,offset);++guardCases;}
        }
        CHECK(VirtualFree(memory,0,MEM_RELEASE));
    }
    {
        static const char motif[]="\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x98\x80 e\xcc\x81 0123456789 ";
        const int length=16*1024*1024;const size_t bytes=sizeof(motif)-1;
        unsigned char* text=malloc((size_t)length+1u);CHECK(text!=NULL);
        for(int at=0;at<length;) {int n=length-at;if(n>(int)bytes)n=(int)bytes;memcpy(text+at,motif,(size_t)n);at+=n;}
        text[length]=0;LARGE_INTEGER frequency,start;QueryPerformanceFrequency(&frequency);
        uint64_t oldHash=0,newHash=0;int oldCount=0,newCount=0;
        QueryPerformanceCounter(&start);
        for(int at=0;at<length;) {int next=old_xuiInternalTextGraphemeNext((const char*)text,length,at);CHECK(next>at);oldHash=oldHash*1099511628211ull+(unsigned)next;at=next;++oldCount;}
        double oldMs=elapsed(start,frequency);QueryPerformanceCounter(&start);
        for(int at=0;at<length;) {int next=xuiInternalTextGraphemeNext((const char*)text,length,at);CHECK(next>at);newHash=newHash*1099511628211ull+(unsigned)next;at=next;++newCount;}
        double newMs=elapsed(start,frequency);CHECK(oldHash==newHash);CHECK(oldCount==newCount);
        printf("{\"event\":\"traversal-observation\",\"bytes\":%d,\"oldMs\":%.3f,\"newMs\":%.3f,\"graphemes\":%d,\"hash\":\"%016llx\"}\n",length,oldMs,newMs,newCount,(unsigned long long)newHash);free(text);
    }
    printf("pointer grapheme comparison passed: %llu assertions; %llu boundary cases; %llu scalar cases; %llu byte pairs; %llu callback failure cases; %llu guard-page cases;\n",
        (unsigned long long)assertions,(unsigned long long)cases,(unsigned long long)scalarCases,
        (unsigned long long)pairCases,(unsigned long long)callbackFailureCases,(unsigned long long)guardCases);
    return 0;
}
