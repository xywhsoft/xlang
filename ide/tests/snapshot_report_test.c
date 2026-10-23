/* Exercise the actual untrusted source-report boundary in both XRT owners.
   The test owns this implementation; omit language_snapshot.o when linking. */
#include "app_xrt.h"
static unsigned fixtureHashes;
static bool snapshotHash(const void* data,size_t size,void* digest)
{ ++fixtureHashes; return xrtSha256(data,size,digest); }
#define xrtSha256 snapshotHash
#include "../src/services/language_snapshot.c"
#undef xrtSha256

static unsigned fixtureChecks;
#define CHECK(v) do { ++fixtureChecks; if(!(v)) { fprintf(stderr,"snapshot_report_test:%d: %s\n",__LINE__,#v); return 0; } } while(0)
static xvalue* fixtureReport(const char* path,const char* text)
{
 size_t size=strlen(text),skip=size>=3 && !memcmp(text,"\xef\xbb\xbf",3) ? 3 : 0;
 unsigned char digest[32];char hex[65];
 if(!xrtSha256(text+skip,size-skip,digest)) return NULL;
 for(int i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
 xvalue *raw=xrtValueObject(),*report=xrtValueObject(),*files=xrtValueArray(),*file=xrtValueObject(),*resolution=xrtValueObject();
 int okay=raw && report && files && file && resolution &&
  put(file,"path",xrtValueString(sv(path))) && put(file,"origin",xrtValueString(sv("overlay"))) && put(file,"kind",xrtValueInt(1)) &&
  put(file,"textSize",xrtValueInt((int64)(size-skip))) && put(file,"rawSize",xrtValueInt((int64)(size-skip))) &&
  put(file,"textSha256",xrtValueString(sv(hex))) && put(file,"rawSha256",xrtValueString(sv(hex))) && xrtValueArrayAppend(files,file) &&
  put(report,"schema",xrtValueInt(1)) && put(report,"scope",xrtValueString(sv("loaded-sources-and-manifests"))) && put(report,"complete",xrtValueBool(true)) &&
  put(report,"error",xrtValueString(sv(""))) && put(report,"errorFile",xrtValueString(sv(""))) && xrtValueObjectSet(report,sv("files"),files) &&
  put(resolution,"schema",xrtValueInt(1)) && put(resolution,"scope",xrtValueString(sv("xlang-import-resolution"))) &&
  put(resolution,"probes",xrtValueArray()) && put(resolution,"packagePathObserved",xrtValueBool(false)) && put(resolution,"XLANG_PACKAGE_PATH",xrtValueNull()) &&
  xrtValueObjectSet(raw,sv("sourceReport"),report) && xrtValueObjectSet(raw,sv("resolutionReport"),resolution);
 xrtValueRelease(report);xrtValueRelease(files);xrtValueRelease(file);xrtValueRelease(resolution);
 if(!okay) {xrtValueRelease(raw);return NULL;}return raw;
}
static int fixtureSource(const char* text,const char* normalizedText)
{
 char* path=xrtPathAbs("snapshot-report-source.xl");CHECK(path);
 xvalue* valid=fixtureReport(path,text);CHECK(valid);
 xide_language_snapshot input={path,text,7};xide_language_check q={.path=path,.snapshots=&input,.snapshotCount=1};xide_model_error error={0};
 CHECK(xideLanguageCheckPackagePath(&q.packagePath,&error));
 const char* wrong="0000000000000000000000000000000000000000000000000000000000000000";
 for(int mode=0;mode<19;++mode) {
  xvalue* raw=xrtValueDeepClone(valid);CHECK(raw);xvalue* file=xrtValueArrayGet(get(get(raw,"sourceReport"),"files"),0);
  if(mode==1 || mode==3) CHECK(put(file,"textSha256",xrtValueString(sv(wrong))));
  if(mode==2 || mode==3) CHECK(put(file,"rawSha256",xrtValueString(sv(wrong))));
  if(mode==4) CHECK(xrtValueObjectRemove(file,sv("textSha256")));
  if(mode==5) CHECK(xrtValueObjectRemove(file,sv("rawSha256")));
  if(mode==6) CHECK(put(file,"textSha256",xrtValueNull()));
  if(mode==7) CHECK(put(file,"rawSha256",xrtValueInt(0)));
  if(mode==8) CHECK(put(file,"rawSha256",xrtValueString(sv(""))));
  if(mode==9) CHECK(put(file,"textSha256",xrtValueString((xstrview){"abc\0def",7})));
  if(mode==10) {char upper[65];memcpy(upper,string(get(file,"textSha256")),65);for(int i=0;i<64;++i) if(upper[i]>='a' && upper[i]<='f') upper[i]-='a'-'A';CHECK(put(file,"textSha256",xrtValueString(sv(upper))) && put(file,"rawSha256",xrtValueString(sv(upper))));}
  if(mode==11) CHECK(put(file,"rawSize",xrtValueInt(-1)));
  if(mode==12) CHECK(put(file,"textSize",xrtValueInt(8u*1024u*1024u+1u)));
  if(mode==13) CHECK(put(file,"kind",xrtValueInt(2)));
  if(mode==14) CHECK(put(file,"origin",xrtValueString(sv("disk"))));
  if(mode==15) CHECK(put(file,"rawSha256",xrtValueString(sv("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"))));
  if(mode==16) CHECK(put(file,"rawSha256",xrtValueArray()));
  if(mode==17) CHECK(put(file,"textSha256",xrtValueString(sv("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"))) && put(file,"rawSha256",xrtValueString(sv("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"))));
  if(mode==18) CHECK(put(file,"path",xrtValueString(sv("relative.xl"))));
  source_graph graph={0};memset(&error,0,sizeof(error));int okay=loadGraph(&q,raw,&graph,&error);
  CHECK(okay==(mode==0));
  if(okay) {size_t skip=strlen(text)>=3 && !memcmp(text,"\xef\xbb\xbf",3) ? 3 : 0;CHECK(graph.count==1 && graph.items[0].overlay==0 && !strcmp(graph.items[0].original,text+skip) && !strcmp(graph.items[0].text,normalizedText));}
  clearGraph(&graph);xrtValueRelease(raw);
 }
 free(q.packagePath);xrtValueRelease(valid);xrtFree(path);return 1;
}
#ifdef XLS_COMPILER_XRT
static int fixtureCaptured(int cursor)
{
 char root[160];snprintf(root,sizeof(root),"build/snapshot-captured-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
 char *directory=xrtPathAbs(root),*path=directory ? xrtPathJoin(directory,"entry.xl") : NULL,*dep=directory ? xrtPathJoin(directory,"snapshot_view_dep.xl") : NULL;
 CHECK(directory && path && dep && xrtDirCreateAll(directory));
 const char* dependency="\xef\xbb\xbf" "export func value() as int\r\n return 42\r\nend\r\n";
 const char* entry="\xef\xbb\xbf// 中文😀\r\nimport snapshot_view_dep\r\nfunc main(...) as int\r\n return snapshot_view_dep.value()\r\nend\r\n";
 CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
 xls_semantic_input input={path,entry,strlen(entry)};char message[256];
 size_t at=(size_t)(strstr(entry,".value")-entry)+3-3;
 xls_semantic_check* checked=cursor ? NULL : xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,message,sizeof(message));
 xls_semantic_completion* completed=cursor ? xlsSemanticCompletionCreate(path,path,at,&input,1,NULL,NULL,message,sizeof(message)) : NULL;
 CHECK(cursor ? completed && xlsSemanticCompletionInfo(completed).available : checked && xlsSemanticCheckInfo(checked).accepted);
 char* report=cursor ? xlsSemanticCompletionJson(completed,NULL) : xlsSemanticCheckReportJson(checked,NULL);CHECK(report);
 xvalue* valid=xrtJsonParse(sv(report));xlsSemanticStringFree(report);CHECK(valid);
 xls_inputs inputs;CHECK((cursor ? xlsSemanticCompletionInputs(completed,&inputs) : xlsSemanticCheckInputs(checked,&inputs)) && inputs.sources==2 && inputs.probes);
 xide_language_snapshot snapshot={path,entry,9};xide_language_check q={.path=path,.snapshots=&snapshot,.snapshotCount=1,.nativeSemantic=1};xide_model_error error={0};
 CHECK(xideLanguageCheckPackagePath(&q.packagePath,&error));
 source_graph plain={0};fixtureHashes=0;CHECK(loadGraph(&q,valid,&plain,&error) && fixtureHashes>0);
 const char* wrong="0000000000000000000000000000000000000000000000000000000000000000";
 for(int mode=0;mode<13;++mode) {
  xvalue* raw=xrtValueDeepClone(valid);CHECK(raw);
  xvalue* item=xrtValueArrayGet(get(get(raw,"sourceReport"),"files"),0);CHECK(item);
  if(mode==1) CHECK(put(item,"textSha256",xrtValueString(sv(wrong))));
  if(mode==2) CHECK(put(item,"rawSha256",xrtValueString(sv(wrong))));
  if(mode==3) CHECK(put(item,"kind",xrtValueInt(2)));
  if(mode==4) CHECK(put(item,"origin",xrtValueString(sv("disk"))));
  if(mode==5) CHECK(put(item,"path",xrtValueString(sv("relative.xl"))));
  if(mode==6) CHECK(put(get(raw,"sourceReport"),"files",xrtValueArray()));
  if(mode==7) CHECK(put(item,"path",xrtValueString(sv(dep))));
  if(mode==8) CHECK(put(get(raw,"resolutionReport"),"probes",xrtValueArray()));
  if(mode==9) CHECK(put(get(raw,"resolutionReport"),"packagePathObserved",xrtValueBool(!inputs.packagePathObserved)));
  if(mode==10) CHECK(put(item,"rawSize",xrtValueInt((int64)strlen(entry))));
  if(mode==11) snapshot.text="func main(...) as int\n return 0\nend\n";
  source_graph graph={.checked=xlsSemanticCheckRetain(checked),.completed=xlsSemanticCompletionRetain(completed)};
  if(mode==12) {
   if(!graph.checked) graph.checked=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,message,sizeof(message));
   if(!graph.completed) graph.completed=xlsSemanticCompletionCreate(path,path,at,&input,1,NULL,NULL,message,sizeof(message));
   CHECK(graph.checked && graph.completed);
  }
  memset(&error,0,sizeof(error));fixtureHashes=0;
  int okay=loadGraph(&q,raw,&graph,&error);snapshot.text=entry;CHECK(okay==(mode==0) && !fixtureHashes);
  if(okay) {
   CHECK(graph.count==plain.count && graph.bytes==plain.bytes && xrtValueEqual(graph.resolution,plain.resolution));
   for(size_t i=0;i<graph.count;++i) {
    xls_input_source source;CHECK(cursor ? xlsSemanticCompletionSource(completed,i,&source) : xlsSemanticCheckSource(checked,i,&source));
    CHECK(graph.items[i].borrowed && graph.items[i].original==source.text && !plain.items[i].borrowed);
    CHECK(!strcmp(graph.items[i].text,plain.items[i].text) && !strcmp(graph.items[i].original,plain.items[i].original));
   }
   CHECK(validateGraph(&q,&graph,&error) && (cursor ? xlsSemanticCompletionValidate(completed) : xlsSemanticCheckValidate(checked)));
  }
  clearGraph(&graph);xrtValueRelease(raw);
 }
 clearGraph(&plain);
 /* The typed rows are captured snapshots, not fresh filesystem reads. A disk
  * edit can be loaded from the old snapshot, but both publication validators
  * must reject it. The JSON-only path still rejects on its initial disk read. */
 const char* changed="export func value() as int\n return 43\nend\n";
 CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)changed,strlen(changed)}));
 source_graph graph={.checked=xlsSemanticCheckRetain(checked),.completed=xlsSemanticCompletionRetain(completed)};memset(&error,0,sizeof(error));fixtureHashes=0;
 CHECK(loadGraph(&q,valid,&graph,&error) && !fixtureHashes);
 source_graph legacy={0};memset(&error,0,sizeof(error));CHECK(!loadGraph(&q,valid,&legacy,&error));clearGraph(&legacy);
 memset(&error,0,sizeof(error));CHECK(!validateGraph(&q,&graph,&error) && !(cursor ? xlsSemanticCompletionValidate(completed) : xlsSemanticCheckValidate(checked)));
 xlsSemanticCheckDestroy(checked);checked=NULL;
 xlsSemanticCompletionDestroy(completed);completed=NULL;
 CHECK(graph.count==2 && strstr(graph.items[1].original,"42"));clearGraph(&graph);
 /* A transient missing candidate permanently invalidates an owner even when
    the filesystem predicate is restored. Publication must still reject it. */
 const char* missing="import snapshot_probe_only_missing\nfunc main(...) as int\n return missingValue\nend\n";
 input=(xls_semantic_input){path,missing,strlen(missing)};snapshot.text=missing;at=(size_t)(strstr(missing,"missingValue")-missing)+4;
 checked=cursor ? NULL : xlsSemanticCheckCreate(path,&input,1,0,NULL,NULL,message,sizeof(message));
 completed=cursor ? xlsSemanticCompletionCreate(path,path,at,&input,1,NULL,NULL,message,sizeof(message)) : NULL;
 CHECK(cursor ? completed!=NULL : checked!=NULL);
 report=cursor ? xlsSemanticCompletionJson(completed,NULL) : xlsSemanticCheckReportJson(checked,NULL);CHECK(report);
 xvalue* missingRaw=xrtJsonParse(sv(report));xlsSemanticStringFree(report);CHECK(missingRaw);
 graph=(source_graph){.checked=xlsSemanticCheckRetain(checked),.completed=xlsSemanticCompletionRetain(completed)};
 memset(&error,0,sizeof(error));CHECK(loadGraph(&q,missingRaw,&graph,&error) && capturedInputs(&graph,&inputs));
 char* candidate=NULL;
 for(size_t i=0;i<inputs.probes && !candidate;++i) {
  xls_input_probe p;CHECK(cursor ? xlsSemanticCompletionProbe(completed,i,&p) : xlsSemanticCheckProbe(checked,i,&p));
  size_t n=strlen(directory);if(!p.exists && !strncmp(p.path,directory,n) && (p.path[n]=='/' || p.path[n]=='\\')) candidate=xrtStrDup(p.path);
 }
 char* parent=candidate ? xrtPathParent(candidate) : NULL;
 CHECK(candidate && parent && xrtDirCreateAll(parent) && xrtFileWriteAtomic(candidate,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
 CHECK(!(cursor ? xlsSemanticCompletionValidate(completed) : xlsSemanticCheckValidate(checked)) && xrtFileDelete(candidate));
 memset(&error,0,sizeof(error));CHECK(resolution(&q,graph.resolution,&error));
 memset(&error,0,sizeof(error));CHECK(!validateGraph(&q,&graph,&error) && strstr(error.message,"永久失效"));
 xlsSemanticCheckDestroy(checked);xlsSemanticCompletionDestroy(completed);clearGraph(&graph);xrtValueRelease(missingRaw);xrtFree(parent);xrtFree(candidate);snapshot.text=entry;
 free(q.packagePath);xrtValueRelease(valid);xrtFree(directory);xrtFree(path);xrtFree(dep);return 1;
}
#endif
int main(void)
{
 if(!fixtureSource("","") || !fixtureSource("func main(...) as int\n return 0\nend\n","func main(...) as int\n return 0\nend\n") ||
    !fixtureSource("\xef\xbb\xbf// 中文😀\r\nfunc main(...) as int\r return 0\r\nend\n","\xef\xbb\xbf// 中文😀\nfunc main(...) as int\n return 0\nend\n")) return 1;
 xstrbuf large;xrtStrBufInit(&large);
 while(large.Size<1024u*1024u) if(!xrtStrBufAppend(&large,sv("// 中文😀 unchanged source bytes\n"))) return 1;
 if(!fixtureSource(large.Data,large.Data)) return 1;
 xrtStrBufFree(&large);
#ifdef XLS_COMPILER_XRT
 if(!fixtureCaptured(0) || !fixtureCaptured(1)) return 1;
#endif
 printf("snapshot_report_test passed: %u checks; real overlay graph boundary, both digest fields independently required, differing/equal forged digests, schema/path/size/origin rejection, empty/ASCII/BOM/CRLF/Unicode/1MiB and exact retained bytes.\n",fixtureChecks);return 0;
}
