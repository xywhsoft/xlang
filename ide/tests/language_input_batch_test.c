#define XIDE_INPUT_IMPORT_NO_MAIN
#include "language_input_import_test.c"
#include "services/language_input_owned.h"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"input-batch:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static void resetObservation(void)
{reads=bytesRead=maxRead=hashBytes=allocations=maxAllocation=metadataCalls=0;readFault=closeFault=allocationFault=namedFault=metadataFault=eofFault=0;}
static int batch(const char* root)
{
 char *entry=xidePathResolve(root,"batch-entry.xl"),*disk=xidePathResolve(root,"batch-disk.xl"),*missing=xidePathResolve(root,"batch-absent.xl");
 const char* raw="\xef\xbb\xbf" "func main(...) as int\r\n return 0\r\nend\r\n",*dep="\xef\xbb\xbf" "export const value=7\r\n";
 CHECK(entry && disk && missing && !xideIoExists(missing) && writeBytes(entry,"saved",5) && writeBytes(disk,dep,strlen(dep)));
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=root,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);
 xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);xide_language_proof_overlay overlay={entry,text};xvalue *files=xrtValueArray(),*a=proof(entry,raw+3,strlen(raw)-3,1),*b=proof(disk,dep,strlen(dep),2);
 CHECK(context && text && files && a && b && put(a,"origin",xrtValueString(xrtStrView("overlay"))) && xrtValueArrayAppend(files,a) && xrtValueArrayAppend(files,b));xrtValueRelease(a);xrtValueRelease(b);
 xvalue *wire=inputWire(context,files,disk,missing),*array=xrtValueArray();xrtValueRelease(files);CHECK(wire && array);
 const xls_context* expected[8];xide_language_input_guard* guards[8]={0};for(size_t i=0;i<8;++i){expected[i]=context;CHECK(xrtValueArrayAppend(array,wire));}
 xide_model_error error={0};resetObservation();observing=1;xide_language_input_guard* single=xideLanguageInputGuardImport(wire,context,&overlay,1,NULL,NULL,&error);observing=0;
 CHECK(single && bytesRead==strlen(dep));size_t firstReads=reads,firstHash=hashBytes;
 resetObservation();observing=1;int okay=xideLanguageInputGuardsImport(array,expected,8,&overlay,1,guards,NULL,NULL,&error);observing=0;
 CHECK(okay && reads==firstReads && bytesRead==strlen(dep) && hashBytes==firstHash);
 for(size_t i=0;i<8;++i)CHECK(guards[i] && xideLanguageInputGuardSameContext(single,guards[i]));
 resetObservation();observing=1;okay=xideLanguageInputGuardValidate(single,NULL,NULL,&error);observing=0;CHECK(okay);size_t validateReads=reads,validateHash=hashBytes;
 resetObservation();observing=1;okay=xideLanguageInputGuardsValidate(guards,8,NULL,NULL,&error);observing=0;
 CHECK(okay && reads==validateReads && bytesRead==strlen(dep) && hashBytes==validateHash);
 /* Every new validation operation reopens/re-hashes, never reuses prior hits. */
 resetObservation();observing=1;okay=xideLanguageInputGuardsValidate(guards,8,NULL,NULL,&error);observing=0;CHECK(okay && reads==validateReads && bytesRead==strlen(dep) && hashBytes==validateHash);
 importPolls=0;importStop=1;CHECK(!xideLanguageInputGuardsValidate(guards,8,importCancelled,NULL,&error));importStop=0;
 for(size_t i=0;i<8;++i)CHECK(xideLanguageInputGuardCurrent(guards[i]));
 xvalue* changed=xrtValueDeepClone(wire);CHECK(changed && put((xvalue*)xrtValueArrayGet(get(changed,"files"),1),"futureMetadata",xrtValueString(xrtStrView("different complete report"))));
 xvalue* pair=xrtValueArray();CHECK(pair && xrtValueArrayAppend(pair,wire) && xrtValueArrayAppend(pair,changed));xide_language_input_guard* pairGuards[2]={0};
 resetObservation();observing=1;okay=xideLanguageInputGuardsImport(pair,expected,2,&overlay,1,pairGuards,NULL,NULL,&error);observing=0;CHECK(okay && bytesRead==2*strlen(dep));for(size_t i=0;i<2;++i)xideLanguageInputGuardRelease(pairGuards[i]);
 CHECK(put((xvalue*)xrtValueArrayGet(get(changed,"files"),1),"rawSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));
 CHECK(!xideLanguageInputGuardsImport(pair,expected,2,&overlay,1,pairGuards,NULL,NULL,&error) && !pairGuards[0] && !pairGuards[1]);xrtValueRelease(pair);xrtValueRelease(changed);
 /* Trusted host substitutions are exact original reports, operation-local.
  * They do not create their own physical authority; the native transaction
  * fixture separately proves the actual pins/locks/owned-write contract. */
 const xvalue* known=xrtValueArrayGet(xideLanguageInputGuardFiles(guards[0]),1);CHECK(known);
 resetObservation();observing=1;okay=xideLanguageInputGuardsValidateOwned(guards,8,&known,1,NULL,NULL,&error);observing=0;CHECK(okay&&!reads&&!hashBytes);
 xvalue* spelling=xrtValueDeepClone(known);CHECK(spelling);char* path=malloc(strlen(disk)+1);CHECK(path);strcpy(path,disk);for(char* p=path;*p;++p)if(*p=='/')*p='\\';CHECK(put(spelling,"path",xrtValueString(xrtStrView(path))));free(path);const xvalue* alternate=spelling;
 resetObservation();observing=1;okay=xideLanguageInputGuardsValidateOwned(guards,8,&alternate,1,NULL,NULL,&error);observing=0;CHECK(okay&&!reads&&!hashBytes);xrtValueRelease(spelling);
 const xvalue* overlaySeed=xrtValueArrayGet(xideLanguageInputGuardFiles(guards[0]),0);CHECK(!xideLanguageInputGuardsValidateOwned(guards,8,&overlaySeed,1,NULL,NULL,&error)&&xideLanguageInputGuardCurrent(guards[0]));
 xfileinfo actual={0};CHECK(xideIoReceiptStat(disk,&actual)&&xideLanguageInputFileMatchesInfo(known,&actual));xfileinfo altered=actual;++altered.Identity;CHECK(!xideLanguageInputFileMatchesInfo(known,&altered));altered=actual;++altered.Size;CHECK(!xideLanguageInputFileMatchesInfo(known,&altered));altered=actual;altered.Modified+=1000;CHECK(!xideLanguageInputFileMatchesInfo(known,&altered));
 xide_language_input_guard* isolated=xideLanguageInputGuardImport(wire,context,&overlay,1,NULL,NULL,&error);xvalue* forged=xrtValueDeepClone(known);CHECK(isolated&&forged&&put(forged,"futureOwned",xrtValueBool(true)));const xvalue* fake=forged;
 resetObservation();readFault=1;observing=1;okay=xideLanguageInputGuardsValidateOwned(&isolated,1,&fake,1,NULL,NULL,&error);observing=0;readFault=0;CHECK(!okay&&reads&&!xideLanguageInputGuardCurrent(isolated));xideLanguageInputGuardRelease(isolated);xrtValueRelease(forged);
 isolated=xideLanguageInputGuardImport(wire,context,&overlay,1,NULL,NULL,&error);CHECK(isolated&&writeBytes(missing,"shadow",6)&&!xideLanguageInputGuardsValidateOwned(&isolated,1,&known,1,NULL,NULL,&error));wchar_t* remove=xidePathNativeWide(missing);CHECK(remove&&DeleteFileW(remove));free(remove);xideLanguageInputGuardRelease(isolated);
 resetObservation();observing=1;okay=xideLanguageInputGuardsValidate(guards,8,NULL,NULL,&error);observing=0;CHECK(okay&&bytesRead==strlen(dep)&&hashBytes==validateHash);
 /* Replacement with identical bytes still changes the captured physical ID. */
 CHECK(writeBytes(disk,dep,strlen(dep)) && !xideLanguageInputGuardsValidate(guards,8,NULL,NULL,&error) && !xideLanguageInputGuardCurrent(guards[0]));
 for(size_t i=0;i<8;++i)xideLanguageInputGuardRelease(guards[i]);
 xideLanguageInputGuardRelease(single);xrtValueRelease(array);xrtValueRelease(wire);xlsTextRelease(text);xlsContextRelease(context);free(entry);free(disk);free(missing);return 1;
}
int main(int argc,char** argv)
{char* root=xidePathResolve(argc==2?argv[1]:"build/language-input-batch","batch-源码😀");int okay=root && xideIoCreateDirectories(root) && run(root) && runImport(root) && batch(root);free(root);if(!okay)return 1;printf("PASS aggregate input proofs: %u combined checks; identical complete source proofs read/hashed once per operation, fresh revalidation, strict differing proof and same-byte identity rejection, cancellation without poisoning\n",checks);return 0;}
