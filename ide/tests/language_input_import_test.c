#define main diskGuardBaseMain
#include "language_disk_guard_test.c"
#undef main
#include "protocol/analysis_context_wire.h"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"input-import:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static size_t importPolls,importStop;
static int importCancelled(void* unused){(void)unused;return ++importPolls>=importStop&&importStop;}
static xvalue* inputWire(xls_context* context,xvalue* files,const char* exists,const char* missing)
{
 xvalue *out=xrtValueObject(),*resolution=xrtValueObject(),*probes=xrtValueArray();int okay=out&&resolution&&probes;
 for(int i=0;okay&&i<2;++i){xvalue* probe=xrtValueObject();okay=probe&&put(probe,"path",xrtValueString(xrtStrView(i?missing:exists)))&&put(probe,"exists",xrtValueBool(!i))&&xrtValueArrayAppend(probes,probe);xrtValueRelease(probe);}
 const char* package=xlsContextSpec(context)->packagePath;
 okay=okay&&put(resolution,"schema",xrtValueInt(1))&&put(resolution,"scope",xrtValueString(xrtStrView("xlang-import-resolution")))&&put(resolution,"packagePathObserved",xrtValueBool(true))&&put(resolution,"XLANG_PACKAGE_PATH",package?xrtValueString(xrtStrView(package)):xrtValueNull());
 if(okay){okay=put(resolution,"probes",probes);probes=NULL;}
 okay=okay&&put(out,"schema",xrtValueInt(1))&&put(out,"context",xideAnalysisContextWrite(context))&&xrtValueObjectSet(out,xrtStrView("files"),files);
 if(okay){okay=put(out,"resolution",resolution);resolution=NULL;}xrtValueRelease(probes);xrtValueRelease(resolution);if(!okay){xrtValueRelease(out);out=NULL;}return out;
}
static int runImport(const char* root)
{
 char *entry=xidePathResolve(root,"request.xl"),*disk=xidePathResolve(root,"dependency.xl"),*missing=xidePathResolve(root,"absent.xl");CHECK(entry&&disk&&missing&&!xrtFileExists(missing));
 const char* raw="\xef\xbb\xbf" "func 主程序(...) as int\r\n return 1\r\nend\r\n";const char* dep="\xef\xbb\xbf" "export const 值=7\r\n";CHECK(writeBytes(entry,"saved",5)&&writeBytes(disk,dep,strlen(dep)));
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.project="",.configuration="",.entry=entry,.directory=root,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath="explicit-unused-package-path"};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);CHECK(context);
 xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(text);xide_language_proof_overlay overlay={entry,text};xvalue *files=xrtValueArray(),*a=proof(entry,raw+3,strlen(raw)-3,1),*b=proof(disk,dep,strlen(dep),2);CHECK(files&&a&&b&&put(a,"origin",xrtValueString(xrtStrView("overlay")))&&put(b,"futureMetadata",xrtValueString(xrtStrView("preserve me")))&&xrtValueArrayAppend(files,a)&&xrtValueArrayAppend(files,b));xrtValueRelease(a);xrtValueRelease(b);xvalue* wire=inputWire(context,files,disk,missing);xrtValueRelease(files);CHECK(wire);
 xide_model_error error={0};importPolls=importStop=0;xide_language_input_guard* keep=xideLanguageInputGuardImport(wire,context,&overlay,1,importCancelled,NULL,&error);CHECK(keep&&xlsContextEqual(context,xideLanguageInputGuardContext(keep))&&xrtValueEqual(get(wire,"files"),xideLanguageInputGuardFiles(keep)));size_t polls=importPolls;CHECK(polls>10&&xideLanguageInputGuardReportBytes(keep)>0);
 for(size_t i=1;i<=polls;++i){importPolls=0;importStop=i;CHECK(!xideLanguageInputGuardImport(wire,context,&overlay,1,importCancelled,NULL,&error));}importStop=0;
 CHECK(!xideLanguageInputGuardImport(wire,NULL,&overlay,1,NULL,NULL,&error)&&!xideLanguageInputGuardImport(wire,context,NULL,1,NULL,NULL,&error)&&!xideLanguageInputGuardImport(wire,context,&overlay,65,NULL,NULL,&error));
 xls_text* wrong=xlsTextCreate("different",9,9,NULL);CHECK(wrong);xide_language_proof_overlay badOverlay={entry,wrong};CHECK(!xideLanguageInputGuardImport(wire,context,&badOverlay,1,NULL,NULL,&error)&&!xideLanguageInputGuardImport(wire,context,NULL,0,NULL,NULL,&error));xlsTextRelease(wrong);
 xide_language_proof_overlay duplicates[]={overlay,overlay};CHECK(!xideLanguageInputGuardImport(wire,context,duplicates,2,NULL,NULL,&error));
 /* Independently expected context cannot be learned from a forged response. */
 spec.packagePath="different";xls_context* other=xlsContextCreate(&spec,16u*1024u*1024u,NULL);CHECK(other&&!xideLanguageInputGuardImport(wire,other,&overlay,1,NULL,NULL,&error));xlsContextRelease(other);spec.packagePath="explicit-unused-package-path";
 spec.entry=missing;other=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xvalue* noEntry=xrtValueDeepClone(wire);CHECK(other&&noEntry&&put(noEntry,"context",xideAnalysisContextWrite(other))&&!xideLanguageInputGuardImport(noEntry,other,&overlay,1,NULL,NULL,&error));xrtValueRelease(noEntry);xlsContextRelease(other);spec.entry=entry;
 /* A Windows lexical key is bounded in wide characters, not UTF8 bytes.
  * This is an overlay-only path check, not compiler/deep-project qualification. */
 char* nativeRoot=xidePathNativeUtf8(root);CHECK(nativeRoot);const char unit[]="汉字汉字汉字汉字汉字汉字汉字汉字汉字汉字";size_t capacity=strlen(nativeRoot)+600*sizeof(unit)+32;char* longPath=malloc(capacity);CHECK(longPath);strcpy(longPath,nativeRoot);size_t at=strlen(longPath);for(size_t i=0;i<600;++i){longPath[at++]='/';memcpy(longPath+at,unit,sizeof(unit)-1);at+=sizeof(unit)-1;}strcpy(longPath+at,"/unsaved.xl");CHECK(strlen(longPath)>32767);
 spec.entry=longPath;other=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xvalue *longWire=xrtValueDeepClone(wire),*longFiles=xrtValueArray(),*longRow=xrtValueDeepClone(xrtValueArrayGet(get(wire,"files"),0));CHECK(other&&longWire&&longFiles&&longRow&&put(longRow,"path",xrtValueString(xrtStrView(longPath)))&&xrtValueArrayAppendNew(longFiles,longRow)&&put(longWire,"files",longFiles)&&put(longWire,"context",xideAnalysisContextWrite(other)));xide_language_proof_overlay longOverlay={longPath,text};xide_language_input_guard* longGuard=xideLanguageInputGuardImport(longWire,other,&longOverlay,1,NULL,NULL,&error);CHECK(longGuard);xideLanguageInputGuardRelease(longGuard);xrtValueRelease(longWire);xlsContextRelease(other);spec.entry=entry;free(longPath);free(nativeRoot);
 for(int mode=0;mode<23;++mode){xvalue* bad=xrtValueDeepClone(wire);CHECK(bad);xvalue* list=get(bad,"files"),*first=xrtValueArrayGet(list,0),*second=xrtValueArrayGet(list,1),*resolution=get(bad,"resolution");
  if(mode==0)CHECK(put(bad,"schema",xrtValueInt(2)));
  if(mode==1)CHECK(put(bad,"extra",xrtValueNull()));
  if(mode==2)CHECK(put(bad,"files",xrtValueArray()));
  if(mode==3)CHECK(xrtValueArrayAppend(list,first));
  if(mode==4)CHECK(put(first,"path",xrtValueString(xrtStrView("relative.xl"))));
  if(mode==5)CHECK(put(first,"origin",xrtValueString(xrtStrView("other"))));
  if(mode==6)CHECK(put(first,"kind",xrtValueInt(2)));
  if(mode==7)CHECK(put(first,"textSize",xrtValueInt(8u*1024u*1024u+1)));
  if(mode==8)CHECK(put(first,"rawSize",xrtValueInt(1)));
  if(mode==9)CHECK(put(first,"textSha256",xrtValueString(xrtStrView("bad"))));
  if(mode==10)CHECK(put(first,"rawSha256",digest("forged",6)));
  if(mode==11)CHECK(put(second,"origin",xrtValueString(xrtStrView("overlay"))));
  if(mode==12)CHECK(put(second,"rawSha256",digest("forged",6)));
  if(mode==13)CHECK(put(second,"path",xrtValueString(xrtStrView(entry)))&&put(first,"path",xrtValueString(xrtStrView(disk))));
  if(mode==14)CHECK(put(resolution,"schema",xrtValueInt(2)));
  if(mode==15)CHECK(put(resolution,"XLANG_PACKAGE_PATH",xrtValueNull()));
  if(mode==16)CHECK(put(xrtValueArrayGet(get(resolution,"probes"),1),"exists",xrtValueBool(true)));
  if(mode==17)CHECK(xrtValueArrayAppend(get(resolution,"probes"),xrtValueArrayGet(get(resolution,"probes"),0)));
  if(mode==18)CHECK(put(get(second,"metadata"),"identity",xrtValueString(xrtStrView("0000000000000000"))));
  if(mode==19)CHECK(put(first,"path",xrtValueString(xrtStrView(missing))));
  if(mode==20)CHECK(put(second,"kind",xrtValueInt(4)));
  if(mode==21)CHECK(put(get(bad,"context"),"analysisProfile",xrtValueString(xrtStrView("other-profile"))));
  if(mode==22)CHECK(put(first,"origin",xrtValueString(xrtStrView("disk")))&&put(first,"rawSize",xrtValueInt(5))&&put(first,"textSize",xrtValueInt(5))&&put(first,"rawSha256",digest("saved",5))&&put(first,"textSha256",digest("saved",5)));
  CHECK(!xideLanguageInputGuardImport(bad,context,&overlay,1,NULL,NULL,&error));xrtValueRelease(bad);
 }
 /* Exact encoded admission includes the wrapper and unknown future fields. */
 xvalue* large=xrtValueDeepClone(wire);CHECK(large);xvalue* future=xrtValueArrayGet(get(large,"files"),1);CHECK(put(future,"padding",xrtValueString(xrtStrView(""))));size_t encodedBytes=0;char* encoded=xrtJsonStringify(large,false,&encodedBytes);CHECK(encoded&&encodedBytes<7u*1024u*1024u);xrtFree(encoded);
 size_t padding=7u*1024u*1024u-encodedBytes;char* pad=malloc(padding+2);CHECK(pad);memset(pad,'z',padding+1);pad[padding+1]=0;CHECK(put(future,"padding",xrtValueString((xstrview){pad,padding})));xide_language_input_guard* exact=xideLanguageInputGuardImport(large,context,&overlay,1,NULL,NULL,&error);CHECK(exact);xideLanguageInputGuardRelease(exact);CHECK(put(future,"padding",xrtValueString((xstrview){pad,padding+1}))&&!xideLanguageInputGuardImport(large,context,&overlay,1,NULL,NULL,&error));free(pad);xrtValueRelease(large);
 importPolls=0;importStop=1;CHECK(!xideLanguageInputGuardValidate(keep,importCancelled,NULL,&error)&&xideLanguageInputGuardCurrent(keep));importStop=0;CHECK(xideLanguageInputGuardValidate(keep,NULL,NULL,&error));
 /* Retained proof owns context and unknown report fields; it does not retain
  * caller GUI models, source text or the mutable response DOM. */
 CHECK(put(wire,"files",xrtValueArray()));xrtValueRelease(wire);xlsContextRelease(context);xlsTextRelease(text);CHECK(xideLanguageInputGuardValidate(keep,NULL,NULL,&error)&&xrtValueCount(xideLanguageInputGuardFiles(keep))==2&&formatted(get(xrtValueArrayGet(xideLanguageInputGuardFiles(keep),1),"futureMetadata"),"preserve me"));
 CHECK(writeBytes(missing,"created",7)&&!xideLanguageInputGuardValidate(keep,NULL,NULL,&error)&&!xideLanguageInputGuardCurrent(keep));xideLanguageInputGuardRelease(keep);
 free(entry);free(disk);free(missing);printf("PASS strict native input proof import: %u combined checks; actual disk SHA/identity, request BOM/CRLF/Unicode overlays, context equality, unknown metadata ownership, every observed cancellation and negative probes\n",checks);return 1;
}
int main(int argc,char** argv)
{char* root=xidePathResolve(NULL,argc==2?argv[1]:"build/language-input-import");int okay=root&&xideIoCreateDirectories(root)&&run(root)&&runImport(root);free(root);return okay?0:1;}
