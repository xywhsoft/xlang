#include "services/project_references.h"
static xide_rename_scope* capturedScope;
static int captureScope(xide_rename_scope* s){capturedScope=xideRenameScopeRetain(s);return capturedScope!=NULL;}
static int importBoundProvider(xide_reference_query*,const xvalue*);
#define XIDE_RENAME_SCOPE_CAPTURE_TEST captureScope
#define XIDE_RENAME_INPUT_IMPORT_TEST importBoundProvider
#define XIDE_RENAME_RESULT_TEST_NO_MAIN
#include "project_rename_result_test.c"
static xide_language_input_guard* retainedProvider;
static int importBoundProvider(xide_reference_query* query,const xvalue* wire)
{
 const reference_output* output=xrtFutureValue(query->future);CHECK(output&&capturedScope&&xideRenameScopeCount(capturedScope)==4);
 const xls_context* first=xideRenameScopeAt(capturedScope,0);char* path=xidePathResolve(xlsContextSpec(first)->directory,"main.xl");const char* raw="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n";xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(path&&text);xide_language_proof_overlay overlay={path,text};const xvalue* proofs=testGet(wire,"inputGuards");uint64_t all=0;
 for(size_t i=0;i<output->guards.Count;++i){
  const xvalue* row=xrtValueArrayGet(proofs,i);xls_context* claim=xideAnalysisContextRead(testGet(row,"context"),NULL,0);CHECK(claim);uint64_t owners=0;enum xide_rename_scope_error why;xls_context* expected=xideRenameScopeProvider(capturedScope,claim,&owners,NULL,NULL,&why);CHECK(expected&&!why&&owners&&xlsContextEqual(expected,xideLanguageInputGuardContext(((xide_language_input_guard**)output->guards.Data)[i])));all|=owners;
  xide_model_error error={0};xide_language_input_guard* guard=xideLanguageInputGuardImport(row,expected,&overlay,1,NULL,NULL,&error);CHECK(guard&&xideLanguageInputGuardValidate(guard,NULL,NULL,&error));xlsContextRelease(expected);xlsContextRelease(claim);if(!i){xideLanguageInputGuardRelease(retainedProvider);retainedProvider=xideLanguageInputGuardRetain(guard);}xideLanguageInputGuardRelease(guard);
 }
 CHECK(all==15);free(path);xlsTextRelease(text);return 1;
}
int main(void)
{if(!runDraft()||!retainedProvider||!capturedScope||!detachedProof)return 1;xrtValueRelease(detachedProof);xideRenameScopeRelease(capturedScope);xide_model_error error={0};int okay=xideLanguageInputGuardValidate(retainedProvider,NULL,NULL,&error);xideLanguageInputGuardRelease(retainedProvider);if(!okay)return 1;printf("PASS native candidate provider scope binding: %u combined checks; derive independent contexts from captured workspace, all four scope bits including unrelated providers, strict import and detached ownership\n",checks);return 0;}
