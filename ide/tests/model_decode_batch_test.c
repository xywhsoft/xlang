#include "model/workspace.h"
#include "services/analysis_context.h"
#include "protocol/analysis_context_wire.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;static size_t equalCalls,statCalls,attributeCalls;static int opaque;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"model-decode-batch:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static int observedEqual(const char* a,const char* b){++equalCalls;return xidePathEqual(a,b);}
static int observedStat(const char* path,int follow,xfileinfo* info){++statCalls;return !opaque&&xideIoStat(path,follow,info);}
static DWORD WINAPI observedAttributes(LPCWSTR path){++attributeCalls;return GetFileAttributesW(path);}
#ifndef XIDE_MODEL_REFERENCE
static size_t faultAt,faultCalls,raceAt;static const char* racePath;
static int tGate(void){return ++faultCalls!=faultAt;}
static bool tMapInit(xmap* m,size_t n){return tGate()&&xrtMapInit(m,n);}
static ptr tMapAdd(xmap* m,xbytesview k,bool* added){return tGate()?xrtMapGetOrAdd(m,k,added):NULL;}
static bool tArrayInit(xarray* a,size_t n){return tGate()&&xrtArrayInit(a,n);}
static bool tArrayPush(xarray* a,const void* p){return tGate()&&xrtArrayPush(a,p);}
static int racingStat(const char* path,int follow,xfileinfo* info)
{
 if(raceAt&&statCalls+1==raceAt){const char body[]="func replacement(...) as int\n return 1\nend\n";CHECK(xideIoWriteAtomic(racePath,(xbytesview){(const unsigned char*)body,sizeof(body)-1}));}
 return observedStat(path,follow,info);
}
#define xrtMapInit tMapInit
#define xrtMapGetOrAdd tMapAdd
#define xrtArrayInit tArrayInit
#define xrtArrayPush tArrayPush
#define xideIoStat racingStat
#else
#define xideIoStat observedStat
#endif
#define xidePathEqual observedEqual
#define GetFileAttributesW observedAttributes
#ifdef XIDE_MODEL_REFERENCE
#include "model_reference.c"
#else
#include "../src/model/workspace.c"
#endif
#undef xidePathEqual
#undef xideIoStat
#undef GetFileAttributesW
#ifndef XIDE_MODEL_REFERENCE
#undef xrtMapInit
#undef xrtMapGetOrAdd
#undef xrtArrayInit
#undef xrtArrayPush
#endif
static void tPut(xvalue* value,const char* name,xvalue* owned){CHECK(owned&&xrtValueObjectSetNew(value,xrtStrView(name),owned));}
static xvalue* tObject(void){xvalue* p=xrtValueObject();CHECK(p);return p;}
static char* tJoin(const char* base,const char* leaf){char* p=xidePathResolveLexical(base,leaf);CHECK(p);return p;}
static void tWrite(const char* path,const char* body){CHECK(xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}));}
static xvalue* tProject(size_t count)
{
 xvalue *root=tObject(),*ide=tObject(),*files=xrtValueArray();CHECK(files);tPut(root,"format",xrtValueString(xrtStrView("xlang-project-1")));tPut(root,"entry",xrtValueString(xrtStrView("f000.xl")));tPut(ide,"name",xrtValueString(xrtStrView("bulk")));
 for(size_t i=0;i<count;++i){char id[40],name[40];snprintf(id,sizeof(id),"given-%zu",i);snprintf(name,sizeof(name),"f%03zu.xl",i);xvalue* row=tObject();tPut(row,"id",xrtValueString(xrtStrView(id)));tPut(row,"path",xrtValueString(xrtStrView(name)));CHECK(xrtValueArrayAppend(files,row));xrtValueRelease(row);}
 tPut(ide,"files",files);tPut(root,"ide",ide);return root;
}
static xvalue* tRecord(const char* name,const char* manifest,const xvalue* value,int expected)
{
 xide_project project={0};xide_model_error e={0};equalCalls=statCalls=attributeCalls=0;int okay=xideProjectDecode(&project,manifest,value,&e);CHECK(okay==expected);
 xvalue* row=tObject();tPut(row,"name",xrtValueString(xrtStrView(name)));tPut(row,"okay",xrtValueBool(okay!=0));tPut(row,"code",xrtValueInt(e.code));tPut(row,"pairwise",xrtValueInt((int64)equalCalls));tPut(row,"stats",xrtValueInt((int64)statCalls));tPut(row,"attributes",xrtValueInt((int64)attributeCalls));
 if(okay){xvalue* files=xrtValueArray();CHECK(files);for(size_t i=0;i<project.files.count;++i){xide_file* f=project.files.items+i;xvalue* item=tObject();tPut(item,"path",xrtValueString(xrtStrView(f->path)));tPut(item,"kind",xrtValueString(xrtStrView(f->kind)));tPut(item,"id",xrtValueString(xrtStrView(!strncmp(f->id,"given-",6)?f->id:"generated")));CHECK(xrtValueArrayAppend(files,item));xrtValueRelease(item);}
  xls_context* context=xideAnalysisProjectContext(&project,NULL,XIDE_ANALYSIS_PROFILE,NULL,&e);xvalue* wire=context?xideAnalysisContextWrite(context):NULL;CHECK(wire);tPut(row,"files",files);tPut(row,"context",wire);xlsContextRelease(context);
 }
 xideProjectClear(&project);return row;
}
static void tAppend(xvalue* cases,xvalue* row){CHECK(xrtValueArrayAppend(cases,row));xrtValueRelease(row);}
static int tCancel(void* p){return ++*(unsigned*)p>=3;}
int main(int argc,char** argv)
{
 CHECK(argc==3);(void)observedStat;(void)observedAttributes;(void)tCancel;char* data=tJoin(argv[1],"model-空间😀");char* manifest=tJoin(data,"bulk.xlp");CHECK(xideIoCreateDirectories(data));for(size_t i=0;i<256;++i){char name[40];snprintf(name,sizeof(name),"f%03zu.xl",i);char* path=tJoin(data,name);if(!xideIoExists(path))tWrite(path,"func main(...) as int\n return 0\nend\n");free(path);}
 xvalue *cases=xrtValueArray(),*root=tProject(256);CHECK(cases);xvalue* row=tRecord("stable-256",manifest,root,1);
#ifdef XIDE_MODEL_REFERENCE
 int64 pairs=0;CHECK(xrtValueGetInt(xrtValueObjectGet(row,xrtStrView("pairwise")),&pairs)&&pairs>=256*255/2);
#else
 int64 pairs=0,stats=0;CHECK(xrtValueGetInt(xrtValueObjectGet(row,xrtStrView("pairwise")),&pairs)&&pairs==0);CHECK(xrtValueGetInt(xrtValueObjectGet(row,xrtStrView("stats")),&stats)&&stats<=256*2+2);
#endif
 tAppend(cases,row);xrtValueRelease(root);
 root=tProject(2);xvalue* files=(xvalue*)xrtValueObjectGet(xrtValueObjectGet(root,xrtStrView("ide")),xrtStrView("files"));tPut((xvalue*)xrtValueArrayGet(files,1),"id",xrtValueString(xrtStrView("given-0")));tAppend(cases,tRecord("duplicate-id",manifest,root,0));xrtValueRelease(root);
 root=tProject(2);files=(xvalue*)xrtValueObjectGet(xrtValueObjectGet(root,xrtStrView("ide")),xrtStrView("files"));tPut((xvalue*)xrtValueArrayGet(files,1),"path",xrtValueString(xrtStrView("f000.xl")));tAppend(cases,tRecord("duplicate-path",manifest,root,0));xrtValueRelease(root);
 char *target=tJoin(data,"f000.xl"),*alias=tJoin(data,"alias.xl");wchar_t *wt=xidePathNativeWide(target),*wa=xidePathNativeWide(alias);CHECK(wt&&wa&&(xideIoExists(alias)||CreateHardLinkW(wa,wt,NULL)));free(wt);free(wa);free(target);free(alias);
 root=tProject(2);files=(xvalue*)xrtValueObjectGet(xrtValueObjectGet(root,xrtStrView("ide")),xrtStrView("files"));tPut((xvalue*)xrtValueArrayGet(files,1),"path",xrtValueString(xrtStrView("alias.xl")));tAppend(cases,tRecord("physical-alias",manifest,root,0));opaque=1;tAppend(cases,tRecord("opaque-physical-alias",manifest,root,0));opaque=0;xrtValueRelease(root);
 root=tProject(2);files=(xvalue*)xrtValueObjectGet(xrtValueObjectGet(root,xrtStrView("ide")),xrtStrView("files"));tPut(root,"entry",xrtValueString(xrtStrView("missing.xl")));tPut((xvalue*)xrtValueArrayGet(files,0),"path",xrtValueString(xrtStrView("missing.xl")));tPut((xvalue*)xrtValueArrayGet(files,1),"path",xrtValueString(xrtStrView("MISSING.xl")));tAppend(cases,tRecord("missing-case-distinct",manifest,root,1));xrtValueRelease(root);
 root=tProject(16);opaque=1;tAppend(cases,tRecord("opaque-fallback",manifest,root,1));opaque=0;xrtValueRelease(root);
 root=tProject(1);xvalue *c=xrtValueArray(),*resources=xrtValueArray();CHECK(c&&resources);for(size_t i=0;i<2;++i){char name[40];snprintf(name,sizeof(name),"c%zu.c",i);char* path=tJoin(data,name);if(!xideIoExists(path))tWrite(path,"int helper(void){return 0;}\n");free(path);CHECK(xrtValueArrayAppendNew(c,xrtValueString(xrtStrView(name))));}
 char* resource=tJoin(data,"view.json");if(!xideIoExists(resource))tWrite(resource,"{}\n");free(resource);CHECK(xrtValueArrayAppendNew(resources,xrtValueString(xrtStrView("view.json"))));tPut(root,"cSources",c);tPut(root,"resources",resources);tAppend(cases,tRecord("implicit-c-resource-members",manifest,root,1));xrtValueRelease(root);
#ifndef XIDE_MODEL_REFERENCE
 root=tProject(8);xide_project project={0};xide_model_error e={0};xvalue* original=tProject(1);CHECK(xideProjectDecode(&project,manifest,original,&e));xrtValueRelease(original);xide_model_decode_options options={.paths=2};CHECK(!xideProjectDecodeWithOptions(&project,manifest,root,&options,&e)&&project.files.count==1);options=(xide_model_decode_options){.labels=8};CHECK(!xideProjectDecodeWithOptions(&project,manifest,root,&options,&e)&&project.files.count==1);unsigned callbacks=0;options=(xide_model_decode_options){.cancelled=tCancel,.user=&callbacks};CHECK(!xideProjectDecodeWithOptions(&project,manifest,root,&options,&e)&&project.files.count==1&&callbacks>=3);
 char* raced=tJoin(data,"f000.xl");racePath=raced;raceAt=9;statCalls=0;CHECK(!xideProjectDecode(&project,manifest,root,&e)&&project.files.count==1&&e.code==XIDE_MODEL_IO);raceAt=0;free(raced);
 faultCalls=0;CHECK(xideProjectDecode(&project,manifest,root,&e));size_t gates=faultCalls;CHECK(gates>20);xideProjectClear(&project);
 for(size_t i=1;i<=gates;++i){original=tProject(1);CHECK(xideProjectDecode(&project,manifest,original,&e));xrtValueRelease(original);faultAt=i;faultCalls=0;CHECK(!xideProjectDecode(&project,manifest,root,&e)&&project.files.count==1);faultAt=0;xideProjectClear(&project);}
 printf("model indexed API fault gates:%zu;identity race/cancel/limits retain prior model.\n",gates);xrtValueRelease(root);
#endif
 xvalue* report=tObject();tPut(report,"status",xrtValueString(xrtStrView("passed")));tPut(report,"cases",cases);size_t size=0;char* encoded=xrtJsonStringify(report,true,&size);CHECK(encoded&&xideIoWriteAtomic(argv[2],(xbytesview){(const unsigned char*)encoded,size}));xrtFree(encoded);xrtValueRelease(report);free(manifest);free(data);printf("model_decode_batch_test passed: %u checks;stable/missing/opaque/alias parity and linear ordinary-file probes.\n",checks);return 0;
}
