#include "analysis/semantic.h"
#include "analysis/completion_session.h"
#include "analysis/check.h"
#include "analysis/signature.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"completion-import-errors:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const char* entry="import dep\nfunc main(...) as int\n return dep.val\nend\n";
static const char* full="import dep\nfunc main(...) as int\n return dep.value()\nend\n";
static size_t cursor(const char* body,const char* marker)
{const char* at=strstr(body,marker);return at ? (size_t)(at-body)+strlen(marker) : SIZE_MAX;}
static int available(xls_semantic_completion* value,int expected)
{
 CHECK(value);xls_completion_info info=xlsSemanticCompletionInfo(value);
 if(info.available!=expected)fprintf(stderr,"available=%d expected=%d error=%s\n",info.available,expected,info.error?info.error:"");
 CHECK(info.available==expected && xlsSemanticCompletionValidate(value));
 if(expected){xls_completion_item item;CHECK(info.count==1 && xlsSemanticCompletionItem(value,0,&item));
  CHECK(!strcmp(item.label,"value") && !strcmp(item.detail,"func() as int") && item.symbol && item.hasSelection);
 }else CHECK(!info.count && info.error && *info.error);
 return 1;
}
static int run(void)
{
 char *path=xrtPathAbs("completion-import-main.xl"),*dep=xrtPathAbs("dep.xl");CHECK(path && dep);
 const struct {const char* body;int okay;} cases[]={
  {"export func value() as int\n return 7\nend\n",1},
  {"export func value() as int\n return \"bad\"\nend\n",1},
  {"export func value() as int\n return missingName\nend\n",1},
  {"export func value() as int\n int number = \"bad\"\n return number\nend\n",1},
  {"export func value() as int\n return \"bad\"\nend\nfunc another() as int\n return missingName\nend\n",1},
  {"export func value() as MissingType\n return 7\nend\n",0},
  {"export func value(MissingType arg) as int\n return 7\nend\n",0},
  {"export func value(int arg = \"bad\") as int\n return 7\nend\n",0},
  {"int globalValue = 7\nexport func value() as int\n return \"bad\"\nend\n",0},
  {"int globalValue = \"bad\"\nexport func value() as int\n return 7\nend\n",0},
  {"class Base\nend\nclass Box<T> extends Base\nend\nexport func value() as int\n return 7\nend\n",0},
 };
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);
 xls_analysis_document docs[2]={{"entry",path,NULL,11,0},{"dep",dep,NULL,12,0}};
 xls_semantic_completion* retained=NULL;char error[256];
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){
  xls_semantic_input inputs[2]={{path,entry,strlen(entry)},{dep,cases[i].body,strlen(cases[i].body)}};
  for(size_t j=0;j<2;++j){xlsTextRelease(docs[j].text);docs[j].text=xlsTextCreate(inputs[j].text,inputs[j].size,1024u*1024u,NULL);CHECK(docs[j].text);++docs[j].revision;}
  size_t at=cursor(entry,"dep.val");
  xls_semantic_completion* actual=xlsCompletionSessionQuery(session,docs,2,"entry","entry",at,NULL,NULL,error,sizeof(error));
  xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,at,inputs,2,NULL,NULL,error,sizeof(error));
  fprintf(stdout,"case %zu expected %d\n",i,cases[i].okay);
  CHECK(available(actual,cases[i].okay) && available(fresh,cases[i].okay));
  char *a=xlsSemanticCompletionJson(actual,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));
  xls_semantic_completion* hit=xlsCompletionSessionQuery(session,docs,2,"entry","entry",at,NULL,NULL,error,sizeof(error));CHECK(hit);
  char* c=xlsSemanticCompletionJson(hit,NULL);CHECK(c && !strcmp(a,c));
  xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticStringFree(c);xlsSemanticCompletionDestroy(hit);
  if(i==1)retained=xlsSemanticCompletionRetain(actual);
  inputs[0]=(xls_semantic_input){path,full,strlen(full)};
  xls_semantic_check* checked=xlsSemanticCheckCreate(path,inputs,2,0,NULL,NULL,error,sizeof(error));CHECK(checked);
  xls_check_info info=xlsSemanticCheckInfo(checked);
  CHECK((i==0)==info.accepted);
  if(i){CHECK(info.diagnostics>0);int found=0;for(size_t j=0;j<info.diagnostics;++j){xls_check_diagnostic diagnostic;CHECK(xlsSemanticCheckDiagnostic(checked,j,&diagnostic));if(!strcmp(diagnostic.path,dep))found=1;}CHECK(found);}
  xlsSemanticCheckDestroy(checked);xlsSemanticCompletionDestroy(actual);xlsSemanticCompletionDestroy(fresh);
 }
 /* The receiver is checked in the query's real local scope. Imported body
  * diagnostics must not look like newly produced receiver diagnostics. */
 const char* member="import dep\nclass Box\n int value\nend\nfunc query(Box box) as int\n return box.val\nend\n";
 xls_semantic_input receiverInputs[2]={{path,member,strlen(member)},{dep,cases[1].body,strlen(cases[1].body)}};
 xls_semantic_completion* receiver=xlsSemanticCompletionCreate(path,path,cursor(member,"box.val"),receiverInputs,2,NULL,NULL,error,sizeof(error));
 CHECK(receiver && xlsSemanticCompletionInfo(receiver).available && xlsSemanticCompletionInfo(receiver).count==1);
 xls_completion_item field;CHECK(xlsSemanticCompletionItem(receiver,0,&field) && !strcmp(field.label,"value") && !strcmp(field.detail,"int") && !strcmp(field.kind,"field"));
 xlsSemanticCompletionDestroy(receiver);
 /* Current-file body errors and errors before the actual query remain hard
  * prerequisites. Equal source text after a repair must not reuse rejection. */
 const char* local="import dep\nfunc broken() as int\n return \"bad\"\nend\nfunc main(...) as int\n return dep.val\nend\n";
 xls_semantic_input inputs[2]={{path,local,strlen(local)},{dep,cases[0].body,strlen(cases[0].body)}};
 xls_semantic_completion* value=xlsSemanticCompletionCreate(path,path,cursor(local,"dep.val"),inputs,2,NULL,NULL,error,sizeof(error));CHECK(available(value,0));xlsSemanticCompletionDestroy(value);
 local="import dep\nfunc main(...) as int\n int bad = \"wrong\"\n return dep.val\nend\n";inputs[0]=(xls_semantic_input){path,local,strlen(local)};
 value=xlsSemanticCompletionCreate(path,path,cursor(local,"dep.val"),inputs,2,NULL,NULL,error,sizeof(error));CHECK(available(value,0));xlsSemanticCompletionDestroy(value);
 /* Signature help shares the same native prerequisite boundary. */
 local="import dep\nfunc main(...) as int\n return dep.value(\nend\n";
 inputs[0]=(xls_semantic_input){path,local,strlen(local)};inputs[1]=(xls_semantic_input){dep,cases[1].body,strlen(cases[1].body)};
 xls_signature_help* signature=xlsSignatureCreateInContext(NULL,path,path,cursor(local,"dep.value("),inputs,2,NULL,NULL,error,sizeof(error));CHECK(signature);
 xls_signature_info si=xlsSignatureInfo(signature);if(!si.available)fprintf(stderr,"signature error=%s\n",si.error?si.error:"");
 CHECK(si.available && si.count==1 && xlsSignatureValidate(signature));xlsSignatureRelease(signature);
 xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);CHECK(stats.hits>=sizeof(cases)/sizeof(*cases));
 xlsCompletionSessionDestroy(session);for(size_t j=0;j<2;++j)xlsTextRelease(docs[j].text);
 CHECK(available(retained,1));xlsSemanticCompletionDestroy(retained);xrtFree(path);xrtFree(dep);return 1;
}
int main(void){if(!run())return 1;printf("PASS imported body isolation: %u checks\n",checks);return 0;}
