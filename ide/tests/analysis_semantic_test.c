#include "analysis/semantic.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_semantic_test:%d: %s\n",__LINE__,#value); return 1; } } while(0)
static int cancelled(void* user) { unsigned* calls=user; return ++*calls>=2; }
static xls_semantic_completion* query(const char* entry,const char* path,const char* source,
    const char* marker,const xls_semantic_input* inputs,size_t count)
{
    char error[256]; const char* at=strstr(source,marker);
    xls_semantic_completion* result=at ? xlsSemanticCompletionCreate(entry,path,
        (size_t)(at-source)+strlen(marker),inputs,count,NULL,NULL,error,sizeof(error)) : NULL;
    if(!result) fprintf(stderr,"query failed: %s\n",at ? error : "missing fixture cursor");
    return result;
}
static int has(xls_semantic_completion* result,const char* label,const char* detail)
{
    xls_completion_info info=xlsSemanticCompletionInfo(result);
    for(size_t i=0;i<info.count;++i) {
        xls_completion_item item;
        if(xlsSemanticCompletionItem(result,i,&item) && !strcmp(item.label,label) &&
           (!detail || !strcmp(item.detail,detail))) return 1;
    }
    return 0;
}
int main(void)
{
    char *path=xrtPathAbs("native-completion-memory.xl"),*dep=xrtPathAbs("dep.xl");
    CHECK(path && dep);
    const char* source="func main(...) as int\n int zzResult = 7\n return zzRes\nend\n";
    xls_semantic_input inputs[2]={{path,source,strlen(source)},{0}};
    xls_semantic_completion* first=query(path,path,source,"return zzRes",inputs,1);
    CHECK(first && xlsSemanticCompletionInfo(first).available && has(first,"zzResult","int"));
    xls_completion_info info=xlsSemanticCompletionInfo(first);
    CHECK(info.count==1 && !strcmp(info.prefix,"zzRes") && info.start==info.offset-5 && info.end==info.offset);
    xls_completion_item item;
    CHECK(xlsSemanticCompletionItem(first,0,&item) && item.hasSelection && item.symbol && !strcmp(item.selectionPath,path));
    CHECK(item.selectionEnd-item.selectionStart==strlen("zzResult"));
    size_t size=0; char* json=xlsSemanticCompletionJson(first,&size);
    CHECK(json && size==strlen(json) && strstr(json,"\"phase\":\"completion\"") && strstr(json,"\"origin\":\"overlay\""));
    xlsSemanticStringFree(json);

    const char* shadow="int value = 1\nfunc main(...) as int\n string value = \"s\"\n val\n return 0\nend\n";
    inputs[0]=(xls_semantic_input){path,shadow,strlen(shadow)};
    xls_semantic_completion* second=query(path,path,shadow,"\n val",inputs,1);
    CHECK(second && xlsSemanticCompletionInfo(second).available && has(second,"value","string"));
    CHECK(has(first,"zzResult","int") && !has(first,"value",NULL));
    xlsSemanticCompletionDestroy(second);

    const char* members="class Box\n int value\n func getValue() as int\n return self.value\n end\nend\nfunc main(...) as int\n auto box = Box()\n return box.\nend\n";
    inputs[0]=(xls_semantic_input){path,members,strlen(members)};
    second=query(path,path,members,"return box.",inputs,1);
    CHECK(second && xlsSemanticCompletionInfo(second).available && has(second,"value","int") && has(second,"getValue",NULL));
    CHECK(xlsSemanticCompletionInfo(second).count==2 && xlsSemanticCompletionInfo(second).start==xlsSemanticCompletionInfo(second).end);
    xlsSemanticCompletionDestroy(second);

    const char* entry="import dep\nfunc main(...) as int\n return dep.value()\nend\n";
    const char* dependency="export func value() as int\n int member = 7\n return mem\nend\n";
    inputs[0]=(xls_semantic_input){path,entry,strlen(entry)};
    inputs[1]=(xls_semantic_input){dep,dependency,strlen(dependency)};
    second=query(path,dep,dependency,"return mem",inputs,2);
    CHECK(second && xlsSemanticCompletionInfo(second).available && has(second,"member","int"));
    CHECK(xlsSemanticCompletionItem(second,0,&item) && !strcmp(item.selectionPath,dep));
    CHECK(xlsSemanticCompletionValidate(second));
    xlsSemanticCompletionDestroy(second);

    const char* unicode="\xef\xbb\xbf" "func main(...) as int\r\n string 结果 = \"😀\"\r\n return 结\r\nend\r\n";
    inputs[0]=(xls_semantic_input){path,unicode,strlen(unicode)};
    second=xlsSemanticCompletionCreate(path,path,(size_t)(strstr(unicode,"return 结")-unicode)+strlen("return 结")-3,inputs,1,NULL,NULL,NULL,0);
    CHECK(second && xlsSemanticCompletionInfo(second).available && has(second,"结果","string"));
    info=xlsSemanticCompletionInfo(second); CHECK(info.end-info.start==strlen("结"));
    xlsSemanticCompletionDestroy(second);
    unsigned calls=0; char error[256];
    CHECK(!xlsSemanticCompletionCreate(path,path,1,inputs,1,cancelled,&calls,error,sizeof(error)) && strstr(error,"cancelled"));
    CHECK(!xlsSemanticCompletionCreate("relative.xl",path,1,inputs,1,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCompletionCreate(path,path,strlen(unicode)+1,inputs,1,NULL,NULL,error,sizeof(error)));
    inputs[1]=inputs[0];
    CHECK(!xlsSemanticCompletionCreate(path,path,1,inputs,2,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCompletionItem(first,info.count+100,&item));
    xlsSemanticCompletionDestroy(first); xrtFree(path); xrtFree(dep);
    printf("analysis_semantic_test passed: %u checks\n",checks); return 0;
}
