#define _WIN32_WINNT 0x0601
#include "xui.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; fprintf(stderr,"find_escape_test:%d: %s\n",__LINE__,#e); } } while (0)

static void replacement(const char* escape, const char* expected, uint32_t flags)
{
    char* output=NULL; int length=-1, count=-1; char error[160]={0};
    int ret=xuiFindReplaceAllText("before target after",-1,"target",escape,0,-1,XUI_FIND_ESCAPE|flags,
                                &output,&length,&count,error,sizeof(error));
    if (expected) {
        CHECK(ret==XUI_OK && output && count==1 && length==(int)strlen(expected));
        CHECK(output && !strcmp(output,expected) && error[0]=='\0');
    } else {
        CHECK(ret==XUI_ERROR_INVALID_ARGUMENT && !output && error[0]!='\0');
    }
    printf("replacement %s flags=%u: ret=%d length=%d count=%d error=%s\n",escape,flags,ret,length,count,error);
    xuiFindFreeText(output);
}

static int guard(const char* pattern, int replace)
{
    SYSTEM_INFO info; GetSystemInfo(&info);
    size_t page=info.dwPageSize, length=strlen(pattern)+1;
    unsigned char* pages=VirtualAlloc(NULL,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!pages || length>page) return 2;
    DWORD old=0; if (!VirtualProtect(pages+page,page,PAGE_NOACCESS,&old)) return 2;
    char* at=(char*)pages+page-length; memcpy(at,pattern,length);
    char error[160]={0}; int ret;
    /* This is the actual public DLL API; the NUL is the last readable byte. */
    if (replace) {
        char* output=NULL; int n=0, count=0;
        ret=xuiFindReplaceAllText("target",-1,"target",at,0,-1,XUI_FIND_ESCAPE|(replace==2?XUI_FIND_REGEX:0),
                                 &output,&n,&count,error,sizeof(error));
        CHECK(ret==XUI_ERROR_INVALID_ARGUMENT && !output && error[0]!='\0');
        xuiFindFreeText(output);
    } else {
        xui_find_result_t result={.iSize=sizeof(result)};
        ret=xuiFindText("target",-1,at,0,0,-1,XUI_FIND_ESCAPE,&result,error,sizeof(error));
        CHECK(ret==XUI_ERROR_INVALID_ARGUMENT && error[0]!='\0');
    }
    printf("guard %s %s: ret=%d error=%s\n",replace?"replacement":"pattern",pattern,ret,error);
    VirtualFree(pages,0,MEM_RELEASE); return failures?1:0;
}

int main(int argc,char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if (argc==4 && !strcmp(argv[1],"guard")) return guard(argv[2],atoi(argv[3]));
    if (argc!=1 && argc!=2) return 2;
    replacement("\\0tail",NULL,0);
    replacement("\\0tail","before targettail after",XUI_FIND_REGEX);
    for (int regex=0;regex<2;++regex) {
        uint32_t flags=regex?XUI_FIND_REGEX:0;
        replacement("\\x00tail",NULL,flags);
        replacement("\\u0000tail",NULL,flags);
        replacement("\\uD800",NULL,flags);
        replacement("\\uDC00",NULL,flags);
        replacement("\\uD83D\\u0041",NULL,flags);
        replacement("\\uD83D",NULL,flags);
        replacement("\\uD83D\\uDE00","before 😀 after",flags);
        replacement("\\uD800\\uDC00","before 𐀀 after",flags);
        replacement("\\uDBFF\\uDFFF","before \xF4\x8F\xBF\xBF after",flags);
        replacement("\\u4E2D\\u6587","before 中文 after",flags);
        replacement("\\xC3\\xA9","before é after",flags);
        replacement("\\t\\n\\r\\\\","before \t\n\r\\ after",flags);
    }
    xui_find_result_t result={.iSize=sizeof(result)}; char error[160]={0};
    CHECK(xuiFindText("中😀 after",-1,"\\u4E2D\\uD83D\\uDE00",0,0,-1,XUI_FIND_ESCAPE,
                      &result,error,sizeof(error))==XUI_OK && result.iStart==0 && result.iEnd==7);
    const char* invalid[]={"\\x00","\\u0000","\\uD800","\\uDC00","\\uD800x","\\uD800\\u0000"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
        CHECK(xuiFindText("text",-1,invalid[i],0,0,-1,XUI_FIND_ESCAPE,&result,error,sizeof(error))==XUI_ERROR_INVALID_ARGUMENT && error[0] && strcmp(error,"not found"));
    char* output=NULL;int length=0,count=0;
    CHECK(xuiFindReplaceAllText("中 item12 ITEM34 item56 后",-1,"item([0-9]+)","value$1",0,-1,XUI_FIND_REGEX|XUI_FIND_CASE_SENSITIVE,
                               &output,&length,&count,error,sizeof(error))==XUI_OK && output && count==2 && !strcmp(output,"中 value12 ITEM34 value56 后"));
    xuiFindFreeText(output);
    output=NULL;
    CHECK(xuiFindReplaceAllText("no match",-1,"target","\\uD800",0,-1,XUI_FIND_REGEX|XUI_FIND_ESCAPE,
                               &output,&length,&count,error,sizeof(error))==XUI_ERROR_INVALID_ARGUMENT && !output && length==0 && count==0 && error[0]);
    CHECK(xuiFindReplaceAllText("item12",-1,"item(\\\\d+)","\\1\\uD83D\\uDE00",0,-1,XUI_FIND_REGEX|XUI_FIND_ESCAPE,
                               &output,&length,&count,error,sizeof(error))==XUI_OK && output && !strcmp(output,"12😀"));
    xuiFindFreeText(output);
    printf("find_escape_test: %d assertions, %d failures\n",checks,failures);
    return failures?1:0;
}
