#include "services/diagnostic_notice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"diagnostic_notice_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const char* id="1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee";
static const char* hash="53862da9c31f6ca0ce1ef2c1e002b2a36b78a0c096a440e968d5c23dbd5554d3";
static const char* body="a😀z\nnext\n";
static int cancel(void* user);
static int put(xvalue* value,const char* name,xvalue* owned) {return owned && xrtValueObjectSetNew(value,xrtStrView(name),owned);}
static xvalue* get(const xvalue* value,const char* name) {return (xvalue*)xrtValueObjectGet(value,xrtStrView(name));}
static xvalue* valueString(const char* text) {return xrtValueString(xrtStrView(text));}
static xide_diagnostic_dependencies* registry(void)
{
 const char* json="{\"schema\":1,\"id\":\"1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee\",\"removed\":false,\"part\":0,\"parts\":1,\"paths\":[\"D:/dep.xl\",\"D:/工程😀/main.xl\"]}";
 xjsonreadconfig config;xrtJsonReadConfigInit(&config);xvalue* p=xrtJsonRead(xrtStrView(json),&config);xide_diagnostic_dependencies* d=xideDiagnosticDependenciesCreate(NULL);
 if(!p || !d || xideDiagnosticDependenciesMessage(d,p,NULL,NULL)!=XIDE_DEPENDENCY_COMPLETE) {xideDiagnosticDependenciesDestroy(d);d=NULL;}
 xrtValueRelease(p);return d;
}
static xvalue* notice(void)
{
 const char* json="{\"uri\":\"file:///D:/dep.xl\",\"version\":7,\"xlang\":{\"schema\":1,\"generation\":\"12\",\"cleared\":false,\"dependencies\":\"1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee\",\"document\":{\"path\":\"D:/dep.xl\",\"origin\":\"overlay\",\"version\":7,\"normalizedSize\":12,\"normalizedSha256\":\"53862da9c31f6ca0ce1ef2c1e002b2a36b78a0c096a440e968d5c23dbd5554d3\"}},\"diagnostics\":[{\"file\":\"D:/dep.xl\",\"range\":{\"start\":{\"line\":0,\"character\":1},\"end\":{\"line\":0,\"character\":3}},\"severity\":1,\"source\":\"xlang\",\"message\":\"😀 error\"},{\"file\":\"D:/dep.xl\",\"range\":{\"start\":{\"line\":1,\"character\":1},\"end\":{\"line\":1,\"character\":3}},\"severity\":2,\"message\":\"next warning\"}]}";
 xjsonreadconfig config;xrtJsonReadConfigInit(&config);return xrtJsonRead(xrtStrView(json),&config);
}
static xide_diagnostic_notice* decode(xide_diagnostic_dependencies* d,const xvalue* params)
{char error[256];xide_diagnostic_notice* n=xideDiagnosticNoticeDecode(d,params,NULL,NULL,error,sizeof(error));if(!n) fprintf(stderr,"decode: %s\n",error);return n;}
static int rejects(xide_diagnostic_dependencies* d,const xvalue* p)
{
 xide_dependency_stats a,b;xideDiagnosticDependenciesStats(d,&a);char error[256];
 CHECK(!xideDiagnosticNoticeDecode(d,p,NULL,NULL,error,sizeof(error)) && *error);xideDiagnosticDependenciesStats(d,&b);CHECK(!memcmp(&a,&b,sizeof(a)));return 1;
}
static int mapping(void)
{
 const char* variants[]={"a😀z\nnext\n","a😀z\r\nnext\r\n","a😀z\rnext\r","\xef\xbb\xbf" "a😀z\nnext\n","\xef\xbb\xbf" "a😀z\r\nnext\r\n"};
 int nextStart[]={8,9,8,11,12},bom[]={0,0,0,3,3};xide_diagnostic_dependencies* d=registry();CHECK(d);
 for(size_t i=0;i<5;++i) {
  xvalue* p=notice();CHECK(p);xide_diagnostic_notice* n=decode(d,p);CHECK(n && n->generation==12 && !n->cleared && n->overlay && n->versionPresent && n->version==7 && n->count==2 && !strcmp(n->normalizedSha256,hash) && !strcmp(xideDiagnosticDependencyId(n->dependency),id));xrtValueRelease(p);
  xls_text* source=xlsTextCreate(variants[i],strlen(variants[i]),8u*1024u*1024u,NULL);CHECK(source && xideDiagnosticNoticeMap(n,source,NULL,NULL));
  CHECK(n->mapped && n->items[0].start==1+bom[i] && n->items[0].end==5+bom[i] && n->items[1].start==nextStart[i] && n->items[1].end==nextStart[i]+2 && n->items[1].severity==2);
  CHECK(!xideDiagnosticNoticeMap(n,source,NULL,NULL) && strstr(n->items[0].title,"😀 error"));xlsTextRelease(source);xideDiagnosticNoticeRelease(n);
 }
 xvalue* p=notice();CHECK(p);xide_diagnostic_notice* n=decode(d,p);CHECK(n);xls_text* wrong=xlsTextCreate("a😀x\nnext\n",12,100,NULL);CHECK(wrong && !xideDiagnosticNoticeMap(n,wrong,NULL,NULL) && n->items[0].start==-1 && n->items[1].end==-1);xlsTextRelease(wrong);
 xls_text* source=xlsTextCreate(body,strlen(body),100,NULL);CHECK(source && xideDiagnosticNoticeMap(n,source,NULL,NULL));xlsTextRelease(source);xideDiagnosticNoticeRelease(n);
 xvalue* last=(xvalue*)xrtValueArrayGet(get(p,"diagnostics"),1);CHECK(put(get(get(last,"range"),"start"),"character",xrtValueInt(1000)) && put(get(get(last,"range"),"end"),"character",xrtValueInt(1001)));
 n=decode(d,p);CHECK(n);source=xlsTextCreate(body,strlen(body),100,NULL);CHECK(source && !xideDiagnosticNoticeMap(n,source,NULL,NULL) && !n->mapped && n->items[0].start==-1 && n->items[1].end==-1);xlsTextRelease(source);xideDiagnosticNoticeRelease(n);xrtValueRelease(p);
 p=notice();CHECK(p);xvalue* document=get(get(p,"xlang"),"document");CHECK(put(document,"normalizedSize",xrtValueInt(1048576)) && put(document,"normalizedSha256",valueString("8ac5de8512e9ac3ace52b49a23797dda95e78d43ba4c481de639557be46ab4a0")));
 char* large=xrtMalloc(1048577);CHECK(large);memset(large,'x',1048576);memcpy(large,body,strlen(body));large[1048576]=0;source=xlsTextCreate(large,1048576,1048576,NULL);xrtFree(large);n=decode(d,p);CHECK(n && source);
 unsigned stop[]={0,8};CHECK(!xideDiagnosticNoticeMap(n,source,cancel,stop) && stop[0]==8 && !n->mapped && n->items[0].start==-1);
 CHECK(xideDiagnosticNoticeMap(n,source,NULL,NULL) && n->items[1].end==10);xideDiagnosticNoticeRelease(n);xlsTextRelease(source);xrtValueRelease(p);xideDiagnosticDependenciesDestroy(d);return 1;
}
static int emptyAndAliases(void)
{
 xide_diagnostic_dependencies* d=registry();CHECK(d);
 for(size_t i=0;i<2;++i) {
  xvalue* p=notice();CHECK(p);xvalue* document=get(get(p,"xlang"),"document");
  CHECK(put(p,"uri",valueString("file:///d:/%64ep.xl")) && put(p,"diagnostics",xrtValueArray()) && put(document,"normalizedSize",xrtValueInt(0)) && put(document,"normalizedSha256",valueString("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
  const char* value=i ? "\xef\xbb\xbf" : "";xls_text* source=xlsTextCreate(value,strlen(value),100,NULL);xide_diagnostic_notice* n=decode(d,p);
  CHECK(source && n && !n->cleared && !n->count && n->dependency && xideDiagnosticNoticeMap(n,source,NULL,NULL));xideDiagnosticNoticeRelease(n);xlsTextRelease(source);xrtValueRelease(p);
 }
 xvalue* p=notice();CHECK(p);xvalue* item=(xvalue*)xrtValueArrayGet(get(p,"diagnostics"),0);CHECK(put(get(get(item,"range"),"start"),"character",xrtValueInt(2)));
 xide_diagnostic_notice* n=decode(d,p);xls_text* source=xlsTextCreate(body,strlen(body),100,NULL);CHECK(n && source && !xideDiagnosticNoticeMap(n,source,NULL,NULL) && n->items[0].start==-1 && n->items[1].end==-1);
 xideDiagnosticNoticeRelease(n);xlsTextRelease(source);xrtValueRelease(p);xideDiagnosticDependenciesDestroy(d);return 1;
}
static int signedAndDefaults(void)
{
 xide_diagnostic_dependencies* d=registry();CHECK(d);
 const int versions[]={INT_MIN,-1,0,1,INT_MAX};for(size_t i=0;i<5;++i) {
  int version=versions[i];
  xvalue* p=notice();CHECK(p);CHECK(put(p,"version",xrtValueInt(version)) && put(get(get(p,"xlang"),"document"),"version",xrtValueInt(version)));
  xide_diagnostic_notice* n=decode(d,p);CHECK(n && n->version==version && n->versionPresent);xideDiagnosticNoticeRelease(n);xrtValueRelease(p);
 }
 xvalue* p=notice();CHECK(p && xrtValueObjectRemove(p,xrtStrView("version")));xvalue* item=(xvalue*)xrtValueArrayGet(get(p,"diagnostics"),0);
 CHECK(xrtValueObjectRemove(item,xrtStrView("severity")) && xrtValueObjectRemove(item,xrtStrView("source")) && put(item,"message",valueString("")));
 CHECK(put(get(p,"xlang"),"generation",valueString("18446744073709551615")));xide_diagnostic_notice* n=decode(d,p);
 CHECK(n && n->generation==UINT64_MAX && !n->versionPresent && n->version==7 && n->items[0].severity==1 && !*n->items[0].message && !strcmp(n->items[0].source,"xlang"));
 xideDiagnosticNoticeRelease(n);xrtValueRelease(p);xideDiagnosticDependenciesDestroy(d);return 1;
}
static int malformed(void)
{
 xide_diagnostic_dependencies* d=registry();CHECK(d);const char* badGenerations[]={"","01","-1","+1","18446744073709551616","1x"};
 for(size_t i=0;i<6;++i) {xvalue* p=notice();CHECK(p && put(get(p,"xlang"),"generation",valueString(badGenerations[i])) && rejects(d,p));xrtValueRelease(p);}
 for(size_t i=0;i<13;++i) {
  xvalue* p=notice();CHECK(p);xvalue* proof=get(p,"xlang"),*doc=get(proof,"document"),*item=(xvalue*)xrtValueArrayGet(get(p,"diagnostics"),1);
  if(i==0) CHECK(put(proof,"schema",xrtValueFloat(1)));
  if(i==1) CHECK(put(proof,"dependencies",valueString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  if(i==2) CHECK(put(doc,"path",valueString("D:/other.xl")));
  if(i==3) CHECK(put(doc,"normalizedSha256",valueString("bad")));
  if(i==4) CHECK(put(doc,"normalizedSize",xrtValueInt(8388609)));
  if(i==5) CHECK(put(p,"version",xrtValueInt(8)));
  if(i==6) CHECK(put(p,"uri",valueString("https://example.invalid/dep.xl")));
  if(i==7) CHECK(put(item,"file",valueString("D:/other.xl")));
  if(i==8) CHECK(put(item,"severity",xrtValueInt(5)));
  if(i==9) CHECK(put(get(get(item,"range"),"end"),"line",xrtValueInt(0)));
  if(i==10) CHECK(put(get(get(item,"range"),"start"),"character",xrtValueFloat(1)));
  if(i==11) CHECK(put(item,"message",xrtValueString((xstrview){"bad\0message",11})));
  if(i==12) CHECK(put(doc,"input",xrtValueNull()));
  CHECK(rejects(d,p));xrtValueRelease(p);
 }
 CHECK(rejects(d,NULL));xideDiagnosticDependenciesDestroy(d);return 1;
}
static int diskAndWithdrawal(void)
{
 xide_diagnostic_dependencies* d=registry();CHECK(d);xvalue* p=notice();CHECK(p);xvalue* doc=get(get(p,"xlang"),"document");
 const char* json="{\"path\":\"D:/dep.xl\",\"origin\":\"disk\",\"kind\":1,\"textSize\":12,\"rawSize\":15,\"textSha256\":\"53862da9c31f6ca0ce1ef2c1e002b2a36b78a0c096a440e968d5c23dbd5554d3\",\"rawSha256\":\"0000000000000000000000000000000000000000000000000000000000000000\",\"metadata\":{\"available\":73,\"attributes\":32,\"device\":\"0000000000000001\",\"identity\":\"0000000000000002\",\"modifiedUs\":\"-9223372036854775808\",\"changedUs\":\"9223372036854775807\"}}";
 xjsonreadconfig config;xrtJsonReadConfigInit(&config);xvalue* input=xrtJsonRead(xrtStrView(json),&config);CHECK(input && xrtValueObjectRemove(p,xrtStrView("version")) && xrtValueObjectRemove(doc,xrtStrView("version")) && put(doc,"origin",valueString("disk")) && xrtValueObjectSet(doc,xrtStrView("input"),input));xrtValueRelease(input);
 xide_diagnostic_notice* n=decode(d,p);CHECK(n && !n->overlay && n->input && !n->versionPresent);xideDiagnosticNoticeRelease(n);
 const char* names[]={"path","rawSize","textSize","kind","rawSha256"};
 for(size_t i=0;i<5;++i) {xvalue* bad=xrtValueDeepClone(p);CHECK(bad);xvalue* field=get(get(get(bad,"xlang"),"document"),"input");CHECK(put(field,names[i],i==0 ? valueString("D:/other.xl") : i==4 ? valueString("bad") : xrtValueInt(i==1 ? 16 : i==2 ? 11 : 4)) && rejects(d,bad));xrtValueRelease(bad);}
 for(size_t i=0;i<5;++i) {xvalue* bad=xrtValueDeepClone(p);CHECK(bad);xvalue* m=get(get(get(get(bad,"xlang"),"document"),"input"),"metadata");CHECK(put(m,i<2 ? "available" : "changedUs",i<2 ? xrtValueInt(i ? 4294967296LL : 1) : valueString(i==2 ? "-0" : i==3 ? "0922" : "9223372036854775808")) && rejects(d,bad));xrtValueRelease(bad);}
 /* The shared source report also publishes clean package-manifest groups. */
 xvalue* clean=xrtValueDeepClone(p);CHECK(clean && put(get(get(get(clean,"xlang"),"document"),"input"),"kind",xrtValueInt(2)) && put(clean,"diagnostics",xrtValueArray()));
 n=decode(d,clean);CHECK(n && !n->cleared && n->dependency && !n->count);xideDiagnosticNoticeRelease(n);xrtValueRelease(clean);
 CHECK(put(get(p,"xlang"),"cleared",xrtValueBool(true)));CHECK(rejects(d,p));
 CHECK(put(p,"diagnostics",xrtValueArray()) && xrtValueObjectRemove(get(p,"xlang"),xrtStrView("document")) && xrtValueObjectRemove(get(p,"xlang"),xrtStrView("dependencies")));
 n=decode(d,p);CHECK(n && n->cleared && !n->dependency && !n->count);xide_dependency_stats stats;xideDiagnosticDependenciesStats(d,&stats);CHECK(!stats.bindings);xideDiagnosticNoticeRelease(n);xrtValueRelease(p);xideDiagnosticDependenciesDestroy(d);return 1;
}
static int cancel(void* user) {unsigned* counter=user;return ++counter[0]>=counter[1];}
static int cancellation(void)
{
 unsigned failures=0,success=0;
 for(unsigned at=1;at<=20;++at) {xide_diagnostic_dependencies* d=registry();xvalue* p=notice();CHECK(d && p);unsigned counter[]={0,at};char error[256];xide_diagnostic_notice* n=xideDiagnosticNoticeDecode(d,p,cancel,counter,error,sizeof(error));
  xide_dependency_stats stats;xideDiagnosticDependenciesStats(d,&stats);if(!n) {++failures;CHECK(!stats.bindings);}else {++success;CHECK(stats.bindings==1 && counter[0]<at);xideDiagnosticNoticeRelease(n);}xrtValueRelease(p);xideDiagnosticDependenciesDestroy(d);}
 CHECK(failures && success);
 xide_diagnostic_dependencies* d=registry();xvalue* p=notice();CHECK(d && p);xls_text* source=xlsTextCreate(body,strlen(body),100,NULL);CHECK(source);
 for(unsigned at=1;at<=24;++at) {xide_diagnostic_notice* n=decode(d,p);CHECK(n);unsigned counter[]={0,at};int mapped=xideDiagnosticNoticeMap(n,source,cancel,counter);CHECK(mapped ? n->mapped && counter[0]<at : !n->mapped && n->items[0].start==-1 && n->items[1].end==-1);xideDiagnosticNoticeRelease(n);}
 xide_diagnostic_notice* n=decode(d,p);CHECK(n && xideDiagnosticNoticeMap(n,source,NULL,NULL));xrtValueRelease(p);xideDiagnosticDependenciesDestroy(d);CHECK(!strcmp(n->items[0].message,"😀 error") && xideDiagnosticDependencyCount(n->dependency)==2 && n->items[1].start==8);xideDiagnosticNoticeRelease(n);xlsTextRelease(source);return 1;
}
int main(void)
{
 xideDiagnosticNoticeRelease(NULL);if(xideDiagnosticNoticeMap(NULL,NULL,NULL,NULL) || !mapping() || !emptyAndAliases() || !signedAndDefaults() || !malformed() || !diskAndWithdrawal() || !cancellation()) return 1;
 printf("diagnostic_notice_test passed: %u checks; real file-proof schema, native dependency binding, clean/withdrawal separation, signed versions/generation overflow, source hash and atomic UTF16 mapping, Unicode/BOM/LF/CRLF/CR/1MiB, metadata rejection, cancellation and detached ownership.\n",checks);return 0;
}
