/* Exercise the actual untrusted source-report boundary in both XRT owners.
   The test owns this implementation; omit language_snapshot.o when linking. */
#include "../src/services/language_snapshot.c"

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
int main(void)
{
 if(!fixtureSource("","") || !fixtureSource("func main(...) as int\n return 0\nend\n","func main(...) as int\n return 0\nend\n") ||
    !fixtureSource("\xef\xbb\xbf// 中文😀\r\nfunc main(...) as int\r return 0\r\nend\n","\xef\xbb\xbf// 中文😀\nfunc main(...) as int\n return 0\nend\n")) return 1;
 xstrbuf large;xrtStrBufInit(&large);
 while(large.Size<1024u*1024u) if(!xrtStrBufAppend(&large,sv("// 中文😀 unchanged source bytes\n"))) return 1;
 if(!fixtureSource(large.Data,large.Data)) return 1;
 xrtStrBufFree(&large);
 printf("snapshot_report_test passed: %u checks; real overlay graph boundary, both digest fields independently required, differing/equal forged digests, schema/path/size/origin rejection, empty/ASCII/BOM/CRLF/Unicode/1MiB and exact retained bytes.\n",fixtureChecks);return 0;
}
