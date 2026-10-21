#include "analysis/check.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_check_test:%d: %s\n",__LINE__,#value); return 1; } } while(0)
static int cancelled(void* user) { unsigned* calls=user; return ++*calls>=3; }
static xls_semantic_check* check(const char* path,const char* text,int index)
{
    char error[256]; xls_semantic_input input={path,text,strlen(text)};
    xls_semantic_check* result=xlsSemanticCheckCreate(path,&input,1,index,NULL,NULL,error,sizeof(error));
    if(!result) fprintf(stderr,"native check failed: %s\n",error);
    return result;
}
int main(void)
{
    char *path=xrtPathAbs("native-check-memory.xl"),*dep=xrtPathAbs("native_check_dep.xl");
    CHECK(path && dep);
    const char* good="func main(...) as int\n int result = 7\n return result\nend\n";
    const char* bad="func main(...) as int\n return \"bad\"\nend\n";
    xls_semantic_check* first=check(path,good,1);
    CHECK(first);
    xls_check_info info=xlsSemanticCheckInfo(first);
    CHECK(info.accepted && info.hasIndex && !info.diagnostics && info.irTypes && info.irNodes && !strcmp(info.phase,"checked"));
    CHECK(xlsSemanticCheckIndex(first) && strstr(xlsSemanticCheckIndex(first),"result"));
    size_t size=0; char* json=xlsSemanticCheckJson(first,&size);
    CHECK(json && size==strlen(json) && strstr(json,"\"semanticIndex\"") && strstr(json,"\"sourceReport\""));
    xlsSemanticStringFree(json);
    xls_semantic_check* second=check(path,bad,1);
    CHECK(second && !xlsSemanticCheckInfo(second).accepted && !strcmp(xlsSemanticCheckInfo(second).phase,"sem"));
    xls_check_diagnostic d;
    CHECK(xlsSemanticCheckDiagnostic(second,0,&d) && !strcmp(d.path,path) && d.line==2 && d.column>=1 && d.message && *d.message);
    CHECK(!xlsSemanticCheckIndex(second) && !xlsSemanticCheckInfo(second).hasIndex);
    json=xlsSemanticCheckJson(second,NULL);
    CHECK(json && strstr(json,"\"complete\":false,\"nodes\":[]")); xlsSemanticStringFree(json);
    CHECK(xlsSemanticCheckInfo(first).accepted && strstr(xlsSemanticCheckIndex(first),"result"));
    xlsSemanticCheckDestroy(second);

    second=check(path,"func main(\n",1);
    CHECK(second && !xlsSemanticCheckInfo(second).accepted && !strcmp(xlsSemanticCheckInfo(second).phase,"parse") && xlsSemanticCheckInfo(second).diagnostics);
    xlsSemanticCheckDestroy(second);
    second=check(path,"import native_check_missing_module\n",1);
    CHECK(second && !xlsSemanticCheckInfo(second).accepted && !strcmp(xlsSemanticCheckInfo(second).phase,"import"));
    xlsSemanticCheckDestroy(second);
    second=check(path,good,0);
    CHECK(second && xlsSemanticCheckInfo(second).accepted && !xlsSemanticCheckInfo(second).hasIndex && !xlsSemanticCheckIndex(second));
    json=xlsSemanticCheckJson(second,&size); CHECK(json && !strstr(json,"\"semanticIndex\"")); xlsSemanticStringFree(json);
    xlsSemanticCheckDestroy(second);

    const char* entry="import native_check_dep\nfunc main(...) as int\n return native_check_dep.value()\nend\n";
    const char* dependency="export func value() as int\n return \"bad\"\nend\n";
    xls_semantic_input inputs[2]={{path,entry,strlen(entry)},{dep,dependency,strlen(dependency)}};
    second=xlsSemanticCheckCreate(path,inputs,2,1,NULL,NULL,NULL,0);
    CHECK(second && !xlsSemanticCheckInfo(second).accepted && xlsSemanticCheckInfo(second).diagnostics==1);
    CHECK(xlsSemanticCheckDiagnostic(second,0,&d) && !strcmp(d.path,dep) && d.line==2);
    xlsSemanticCheckDestroy(second);
    const char* unicode="\xef\xbb\xbf" "func main(...) as int\r\n string 中文 = \"😀\"; int x = missingName\r\n return 0\r\nend\r\n";
    second=check(path,unicode,1);
    CHECK(second && !xlsSemanticCheckInfo(second).accepted && xlsSemanticCheckDiagnostic(second,0,&d) && d.line==2 && strstr(d.message,"missingName"));
    json=xlsSemanticCheckJson(second,&size); CHECK(json && strstr(json,"\"origin\":\"overlay\"")); xlsSemanticStringFree(json);
    xlsSemanticCheckDestroy(second);

    unsigned calls=0; char error[256];
    CHECK(!xlsSemanticCheckCreate(path,inputs,2,1,cancelled,&calls,error,sizeof(error)) && strstr(error,"cancelled"));
    CHECK(!xlsSemanticCheckCreate("relative.xl",inputs,2,1,NULL,NULL,error,sizeof(error)));
    inputs[1]=inputs[0]; CHECK(!xlsSemanticCheckCreate(path,inputs,2,1,NULL,NULL,error,sizeof(error)));
    inputs[0].size=8u*1024u*1024u+1; CHECK(!xlsSemanticCheckCreate(path,inputs,1,1,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCheckCreate(path,NULL,1,1,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCheckCreate(path,NULL,0,2,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCheckDiagnostic(first,1000,&d));

    /* A normal 3004-line file with a long path exceeded the CLI's 8 MiB limit.
       Native analysis must retain its complete index instead of reporting a
       false source error. Use 1500 functions to cross 8 MiB on short paths too. */
    size_t capacity=128u*1024u,used=0; char* large=malloc(capacity); CHECK(large);
    for(unsigned i=0;i<1500;++i) {
        int n=snprintf(large+used,capacity-used,"func helper_%u(int value) as int\n return value + 1\nend\n",i);
        CHECK(n>0 && (size_t)n<capacity-used); used+=(size_t)n;
    }
    CHECK(strlen(good)<capacity-used); memcpy(large+used,good,strlen(good)+1); used+=strlen(good);
    inputs[0]=(xls_semantic_input){path,large,used};
    second=xlsSemanticCheckCreate(path,inputs,1,1,NULL,NULL,error,sizeof(error)); free(large);
    CHECK(second && xlsSemanticCheckInfo(second).accepted && xlsSemanticCheckInfo(second).hasIndex);
    json=xlsSemanticCheckJson(second,&size); printf("native large index bytes: %zu\n",size);
    CHECK(json && size>8u*1024u*1024u && size<=XLS_CHECK_REPORT_BYTES);
    xlsSemanticStringFree(json); xlsSemanticCheckDestroy(second);

    char root[160]; snprintf(root,sizeof(root),"build/native-check-core-%llu-%llu",
        (unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
    CHECK(xrtDirCreateAll(root));
    char* disk=xrtPathJoin(root,"disk.xl"); char* absolute=disk ? xrtPathAbs(disk) : NULL;
    CHECK(absolute && xrtFileWriteAtomic(absolute,(xbytesview){(const unsigned char*)good,strlen(good)}));
    second=xlsSemanticCheckCreate(absolute,NULL,0,1,NULL,NULL,error,sizeof(error));
    CHECK(second && xlsSemanticCheckInfo(second).accepted && xlsSemanticCheckValidate(second));
    CHECK(xrtFileWriteAtomic(absolute,(xbytesview){(const unsigned char*)good,strlen(good)}));
    CHECK(!xlsSemanticCheckValidate(second) && !xlsSemanticCheckInfo(second).accepted && !xlsSemanticCheckTypedIndex(second) && !xlsSemanticCheckIndex(second) && !xlsSemanticCheckJson(second,NULL) && !xlsSemanticCheckReportJson(second,NULL));
    CHECK(!xlsSemanticCheckValidate(second));
    xlsSemanticCheckDestroy(second); xlsSemanticCheckDestroy(first);
    xrtFree(absolute); xrtFree(disk); xrtFree(path); xrtFree(dep);
    printf("analysis_check_test passed: %u checks; full semantic/IR/index, error phases, exact sources, immutable ownership and permanent disk guard.\n",checks);
    return 0;
}
