#include "xui.h"
#include <stdio.h>
#include <string.h>
static int checks,failed;
#define CHECK(e) do { ++checks;if(!(e)){++failed;fprintf(stderr,"find_boundaries_test:%d: %s\n",__LINE__,#e);} } while(0)
static void replace(const char* text,const char* pattern,const char* value,int start,int end,const char* expected,int expectedCount)
{
    char* output=NULL;int count=0,length=0;char error[160]={0};
    int ret=xuiFindReplaceAllText(text,-1,pattern,value,start,end,XUI_FIND_REGEX,&output,&length,&count,error,sizeof(error));
    CHECK(ret==XUI_OK && output && count==expectedCount && length==(int)strlen(expected));
    CHECK(output && !strcmp(output,expected) && xrtUtf8Valid((xstrview){output,(size_t)length},NULL));
    printf("pattern=%s count=%d ret=%d length=%d\n",pattern,count,ret,length);xuiFindFreeText(output);
}
int main(int argc,char** argv)
{
    (void)argv;if(argc!=1 && argc!=2)return 2;
    replace("中😀 tail","$","!",0,-1,"中😀 tail!",1);
    replace("中😀 tail","^","!",0,-1,"!中😀 tail",1);
    replace("中😀","^|$","!",0,-1,"!中😀!",2);
    replace("中😀","()","!",0,-1,"!中!😀!",3);
    replace("中😀 é","()","!",0,-1,"!中!😀! !e!́!",6);
    replace("前中😀后","()","!",3,10,"前!中!😀!后",3);
    replace("中😀","$","",0,-1,"中😀",1);
    replace("","$","!",0,-1,"!",1);
    replace("","()","!",0,-1,"!",1);
    char error[160]={0};xui_find_result_t results[16]={0};int count=0;
    CHECK(xuiFindCollectText("中😀",-1,"()",0,-1,XUI_FIND_REGEX,results,16,&count,error,sizeof(error))==XUI_OK && count==3);
    int positions[]={0,3,7};
    for(int i=0;i<3 && i<count;++i)CHECK(results[i].iStart==positions[i] && results[i].iEnd==positions[i]);
    CHECK(xuiFindCollectText("前中😀后",-1,"()",3,10,XUI_FIND_REGEX,results,16,&count,error,sizeof(error))==XUI_OK && count==3);
    for(int i=0;i<3 && i<count;++i)CHECK(results[i].iStart==positions[i]+3 && results[i].iEnd==positions[i]+3);
    xui_find_result_t result={.iSize=sizeof(result)};
    CHECK(xuiFindText("中😀",-1,"()",7,0,-1,XUI_FIND_REGEX|XUI_FIND_BACKWARD,&result,error,sizeof(error))==XUI_OK && result.iStart==7 && result.iEnd==7);
    CHECK(xuiFindText("中😀",-1,"()",3,0,-1,XUI_FIND_REGEX|XUI_FIND_BACKWARD,&result,error,sizeof(error))==XUI_OK && result.iStart==3 && result.iEnd==3);
    const char* invalid[]={"\\xFF","\\x80","\\xC0\\xAF","\\xED\\xA0\\x80","\\xF4\\x90\\x80\\x80","\\xE4\\xB8"};
    for(int regex=0;regex<2;++regex)for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){
        char* output=NULL;int length=0;count=0;
        CHECK(xuiFindReplaceAllText("target",-1,"target",invalid[i],0,-1,XUI_FIND_ESCAPE|(regex?XUI_FIND_REGEX:0),&output,&length,&count,error,sizeof(error))==XUI_ERROR_INVALID_ARGUMENT && !output && !length && !count && error[0]);
        xuiFindFreeText(output);
        CHECK(xuiFindText("中😀",-1,invalid[i],0,0,-1,XUI_FIND_ESCAPE|(regex?XUI_FIND_REGEX:0),&result,error,sizeof(error))==XUI_ERROR_INVALID_ARGUMENT && error[0]);
    }
    char* output=NULL;int length=0;count=0;
    CHECK(xuiFindReplaceAllText("target",-1,"target","\\xE4\\xB8\\xAD\\xF0\\x9F\\x98\\x80",0,-1,XUI_FIND_ESCAPE,&output,&length,&count,error,sizeof(error))==XUI_OK && output && !strcmp(output,"中😀"));
    xuiFindFreeText(output);
    printf("find_boundaries_test: %d assertions, %d failures\n",checks,failed);return failed?1:0;
}
