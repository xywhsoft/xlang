#include "analysis/check.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

/* Links the SAME shared serializer with a deliberately tiny resource budget.
 * This is not a fake compiler or an injected semantic/AST result. */
int main(void)
{
    char* path=xrtPathAbs("index-budget-memory.xl"); char error[256];
    const char* good="func main(...) as int\n int result = 7\n return result\nend\n";
    xls_semantic_input input={path,good,strlen(good)};
    xls_semantic_check* result=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    if(!path || result || !strstr(error,"index exceeds byte limit")) { fprintf(stderr,"index budget must fail as infrastructure: %s\n",error); return 1; }
    result=xlsSemanticCheckCreate(path,&input,1,0,NULL,NULL,error,sizeof(error));
    if(!result || !xlsSemanticCheckInfo(result).accepted || xlsSemanticCheckInfo(result).diagnostics) return 1;
    xlsSemanticCheckDestroy(result);
    const char* bad="func main(...) as int\n return \"bad\"\nend\n";
    input=(xls_semantic_input){path,bad,strlen(bad)};
    result=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    if(!result || xlsSemanticCheckInfo(result).accepted || !xlsSemanticCheckInfo(result).diagnostics || strcmp(xlsSemanticCheckInfo(result).phase,"sem")) return 1;
    xlsSemanticCheckDestroy(result); xrtFree(path);
    puts("analysis_index_limit_test passed: index resource errors are separate from source diagnostics; semantic errors stay diagnostics.");
    return 0;
}
