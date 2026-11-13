#include "services/project_references.h"
static int importNativeResult(xide_reference_query*,const xvalue*);
#define XIDE_RENAME_INPUT_IMPORT_TEST importNativeResult
#define XIDE_RENAME_RESULT_TEST_NO_MAIN
#include "project_rename_result_test.c"
static xide_language_input_guard* retainedInput;
static int importNativeResult(xide_reference_query* query,const xvalue* wire)
{
 const reference_output* output=xrtFutureValue(query->future);CHECK(output&&output->guards.Count);
 const xls_context* first=xideLanguageInputGuardContext(((xide_language_input_guard**)output->guards.Data)[0]);CHECK(first);
 char* path=xidePathResolve(xlsContextSpec(first)->directory,"main.xl");const char* raw="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n";xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(path&&text);xide_language_proof_overlay overlay={path,text};const xvalue* proofs=testGet(wire,"inputGuards");
 for(size_t i=0;i<output->guards.Count;++i){xide_language_input_guard* original=((xide_language_input_guard**)output->guards.Data)[i];xide_model_error error={0};xide_language_input_guard* imported=xideLanguageInputGuardImport(xrtValueArrayGet(proofs,i),xideLanguageInputGuardContext(original),&overlay,1,NULL,NULL,&error);
  if(!imported)fprintf(stderr,"native proof import: %s\n",error.message);
  CHECK(imported&&xideLanguageInputGuardSameContext(original,imported)&&xrtValueEqual(xideLanguageInputGuardFiles(original),xideLanguageInputGuardFiles(imported))&&xideLanguageInputGuardValidate(imported,NULL,NULL,&error));
  if(!i){xideLanguageInputGuardRelease(retainedInput);retainedInput=xideLanguageInputGuardRetain(imported);}xideLanguageInputGuardRelease(imported);
 }
 free(path);xlsTextRelease(text);return 1;
}
int main(void)
{if(xideProjectReferencesRenameResult(NULL,0,NULL,NULL,NULL)||!runDraft()||!retainedInput||!detachedProof)return 1;xrtValueRelease(detachedProof);xide_model_error error={0};int okay=xideLanguageInputGuardValidate(retainedInput,NULL,NULL,&error);xideLanguageInputGuardRelease(retainedInput);if(!okay)return 1;printf("PASS actual native candidate proof import: %u combined checks; all four scopes, exact original provider contexts and request overlay, detached ownership after original producers exit\n",checks);return 0;}
