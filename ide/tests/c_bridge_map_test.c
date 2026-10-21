#include "services/c_bridge_map.h"
#include "services/language_check.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(k)) : NULL; }
static int put(xvalue* v,const char* k,xvalue* t) { return t && xrtValueObjectSetNew(v,sv(k),t); }
static int record(const char* root,const char* name,const xvalue* v) { char* p=xidePathResolve(root,name);size_t n=0;char* text=xrtJsonStringify((xvalue*)v,true,&n);int ok=p && text && xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)text,n});free(p);xrtFree(text);return ok; }
static const char* string(const xvalue* v) { xstrview s={0};return v && xrtValueGetString(v,&s) ? s.Data : NULL; }
#define CHECK(t) do { if(!(t)) { fprintf(stderr,"c_bridge_map_test:%d: %s; %s\n",__LINE__,#t,error.message);failed=1;goto cleanup; } ++checks; } while(0)
static int publicTypes(const char* root,const char* compiler,xide_toolchains* cache)
{
 xide_model_error error={0};int failed=0,checks=0;xide_language_check* query=NULL;xvalue *result=NULL,*bindings=NULL,*mutation=NULL;char* path=xidePathResolve(root,"类型😀.xl");
 const char* text="\xef\xbb\xbf// 中文 😀 exact nominal type\r\nunion 数字\r\n c_int integer\r\n c_double real\r\nend\r\nunion Unused\r\n c_int unused\r\nend\r\ntypedef Alias = 数字\r\nexport \"type_read\" func read(Alias value) as c_int\r\n return value.integer\r\nend\r\n";
 CHECK(path && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}));xide_language_bridge_options options={0};query=xideLanguageBridgeRun(cache,compiler,root,path,NULL,0,&options,&error);CHECK(query);result=xideLanguageBridgeTake(query);CHECK(result);
 const xvalue* bridge=get(result,"cBridge"),*map=get(bridge,"declarationMap"),*rows=get(map,"records"),*files=get(result,"sourceFiles");const char* header=string(get(bridge,"declarations"));
 CHECK(header && xrtValueCount(rows)==3 && xrtValueCount(get(result,"sourceBindings"))==3 && record(root,"actual-public-type-map.json",map) && record(root,"public-type-bindings.json",get(result,"sourceBindings")));
 const xvalue *tag=xrtValueArrayGet(rows,0),*alias=xrtValueArrayGet(rows,1);int64 symbol=0,type=0;CHECK(!strcmp(string(get(tag,"kind")),"type") && !strcmp(string(get(alias,"kind")),"type") && xrtValueEqual(get(tag,"target"),get(alias,"target")) && xrtValueGetInt(get(tag,"symbolId"),&symbol) && xrtValueGetInt(get(tag,"typeId"),&type) && symbol>0 && type>0 && symbol!=type && xrtValueEqual(get(tag,"typeId"),get(alias,"typeId")) && !strstr(header,"Unused"));
 xide_semantic_source sources[]={{path,text+3,1,0}};int versions[]={0};bindings=xideCBridgeMapBindings(map,header,sources,files,versions,1,&error);CHECK(bindings && xrtValueEqual(bindings,get(result,"sourceBindings")));xrtValueRelease(bindings);bindings=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(xrtValueArrayGet(get(mutation,"records"),0),"typeId",xrtValueInt(0)));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(xrtValueArrayGet(get(mutation,"records"),0),"typeAbiId",xrtValueString(sv("not-an-abi-id"))));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(xrtValueArrayGet(get(mutation,"records"),1),"typeId",xrtValueInt(type+1)));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(xrtValueArrayGet(get(mutation,"records"),1),"typeAbiId",xrtValueString(sv("0000000000000001"))));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(mutation,"complete",xrtValueBool(false)) && put(xrtValueArrayGet(get(mutation,"records"),0),"target",xrtValueNull()) && put(xrtValueArrayGet(get(mutation,"records"),1),"target",xrtValueNull()));bindings=xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error);CHECK(bindings && xrtValueCount(bindings)==1);xrtValueRelease(bindings);bindings=NULL;xrtValueRelease(mutation);mutation=NULL;
 CHECK(xideLanguageBridgeGuard(query,&error));printf("Public type IDE map checks: %d passed; actual nominal symbol/TypeId domains, tag/typedef, BOM/CRLF/non-BMP, alias canonicalization, public reachability, malformed and conflicting identities, optional partial type withdrawal.\n",checks);
cleanup:
 xrtValueRelease(bindings);xrtValueRelease(mutation);xrtValueRelease(result);xideLanguageCheckDestroy(query);free(path);return !failed;
}
int main(int argc,char** argv)
{
 if((argc!=3 && argc!=4) || (argc==4 && strcmp(argv[3],"public-types")) || !xrtDirCreateAll(argv[1])) return 2;
 int failed=0,checks=0;xide_model_error error={0};char *root=NULL,*path=NULL,*text=NULL;xvalue *result=NULL,*mutation=NULL,*bindings=NULL;xide_language_check* query=NULL;xide_toolchains* cache=NULL;unsigned char* saved=NULL;size_t savedSize=0;
 char unique[80];snprintf(unique,sizeof(unique),"map-capacity-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());root=xidePathResolve(argv[1],unique);path=xidePathResolve(root,"来源😀.xl");
 CHECK(root && path && xrtDirCreateAll(root));text=malloc(4097u*64u+128);CHECK(text);size_t at=(size_t)sprintf(text,"\xef\xbb\xbf// 中文 😀 before exact source ranges\r\nexport \"map_g0\" c_int 值 = 0\r\n");
 for(unsigned i=1;i<4097;++i) at+=(size_t)sprintf(text+at,"export \"map_g%x\" c_int v%x = 0\r\n",i,i);
 CHECK(xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,at}));cache=xideToolchainsCreate();CHECK(cache);
 xide_language_bridge_options options={0};query=xideLanguageBridgeRun(cache,argv[2],root,path,NULL,0,&options,&error);CHECK(query);result=xideLanguageBridgeTake(query);CHECK(result);
 const xvalue* bridge=get(result,"cBridge"),*map=get(bridge,"declarationMap"),*files=get(result,"sourceFiles"),*prepared=get(result,"sourceBindings");const char* header=string(get(bridge,"declarations"));
 CHECK(header && xrtValueCount(get(map,"records"))==4097 && xrtValueCount(files)==1 && xrtValueCount(prepared)==4097);
 CHECK(record(root,"actual-compiler-map.json",map) && record(root,"same-source-files.json",files) && record(root,"validated-source-bindings.json",prepared));
 xide_semantic_source sources[]={{path,text+3,1,0}};int versions[]={0};bindings=xideCBridgeMapBindings(map,header,sources,files,versions,1,&error);CHECK(bindings && xrtValueEqual(bindings,prepared));xrtValueRelease(bindings);bindings=NULL;
 int64 line=-1,start=-1,end=-1;const xvalue* first=xrtValueArrayGet(prepared,0),*selection=get(first,"selectionRange");
 CHECK(xrtValueGetInt(get(get(selection,"start"),"line"),&line) && line==1 && xrtValueGetInt(get(get(selection,"start"),"character"),&start) && xrtValueGetInt(get(get(selection,"end"),"character"),&end) && end-start==1);
 /* Mutate actual compiler metadata, never fabricate a replacement declaration. */
 mutation=xrtValueDeepClone(map);CHECK(mutation);xvalue* row=xrtValueArrayGet(get(mutation,"records"),0);int64 value=0;CHECK(xrtValueGetInt(get(get(row,"headerRange"),"endOffset"),&value) && put(get(row,"headerRange"),"endOffset",xrtValueInt(value+1)));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(get(xrtValueArrayGet(get(mutation,"records"),0),"target"),"sourceId",xrtValueInt(2)));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation);row=get(get(xrtValueArrayGet(get(mutation,"records"),0),"target"),"selectionRange");CHECK(xrtValueGetInt(get(row,"startColumn"),&value) && put(row,"startColumn",xrtValueInt(value+1)));CHECK(!xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error));xrtValueRelease(mutation);mutation=NULL;
 mutation=xrtValueDeepClone(map);CHECK(mutation && put(mutation,"complete",xrtValueBool(false)) && put(xrtValueArrayGet(get(mutation,"records"),0),"target",xrtValueNull()));bindings=xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error);CHECK(bindings && xrtValueCount(bindings)==4096);xrtValueRelease(bindings);bindings=NULL;xrtValueRelease(mutation);mutation=NULL;
 bindings=xideCBridgeMapBindings(NULL,header,sources,files,versions,1,&error);CHECK(bindings && !xrtValueCount(bindings));xrtValueRelease(bindings);bindings=NULL;
 mutation=xrtValueObject();CHECK(mutation && put(mutation,"schema",xrtValueInt(1)) && put(mutation,"scope",xrtValueString(sv("lowered-public-symbol-source-bindings"))) && put(mutation,"complete",xrtValueBool(false)) && put(mutation,"reason",xrtValueString(sv("source-map-unavailable"))) && put(mutation,"records",xrtValueArray()));bindings=xideCBridgeMapBindings(mutation,header,sources,files,versions,1,&error);CHECK(bindings && !xrtValueCount(bindings));xrtValueRelease(bindings);bindings=NULL;xrtValueRelease(mutation);mutation=NULL;
 CHECK(xideLanguageBridgeGuard(query,&error));saved=xrtFileReadAllLimit(path,1024u*1024u,&savedSize);CHECK(saved && savedSize==at && !memcmp(saved,text,at));
 if(argc==4) CHECK(publicTypes(root,argv[2],cache));
 printf("c_bridge_map_test passed: %d assertions; actual 4097-export compiler response, enlarged bounded parser, exact Unicode/BOM/CRLF source conversion, malformed header/source/scalar rejection, partial unknown mapping and older/unavailable metadata; evidence %s.\n",checks,root);
cleanup:
 xrtFree(saved);xrtValueRelease(bindings);xrtValueRelease(mutation);xrtValueRelease(result);xideLanguageCheckDestroy(query);xideToolchainsDestroy(cache);free(root);free(path);free(text);return failed;
}
