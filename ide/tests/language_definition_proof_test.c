#include "services/language_check.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"language_definition_proof_test:%d: %s\n",__LINE__,#v); return 0; } } while(0)
static const xvalue* get(const xvalue* value,const char* key)
{ return value && xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,xrtStrView(key)) : NULL; }
static const char* string(const xvalue* value)
{ xstrview text={0}; return value && xrtValueGetString(value,&text) ? text.Data : NULL; }
static int textIs(const xvalue* value,const char* text)
{ const char* actual=string(value); return actual && !strcmp(actual,text); }
static int numberIs(const xvalue* value,int64 number)
{ int64 actual=0; return value && xrtValueGetInt(value,&actual) && actual==number; }
static int writeBytes(const char* path,const char* bytes)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)bytes,strlen(bytes)}); }
static int hashIs(const char* bytes,const xvalue* value)
{
 unsigned char digest[32]; char hex[65]; if(!xrtSha256(bytes,strlen(bytes),digest)) return 0;
 for(size_t i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
 return textIs(value,hex);
}
static xide_language_check* start(xide_toolchains* cache,const char* compiler,const char* root,const char* entry,const xide_language_snapshot* inputs,size_t count)
{
 xide_model_error error={0}; xide_language_analysis_options options={.nativeBackend=1};
 xide_language_check* check=xideLanguageCheckStartSnapshotsEx(cache,compiler,root,entry,inputs,count,&options,&error);
 uint64_t began=xrtClock();
 while(check && xideLanguageCheckState(check,error.message,sizeof(error.message))==XIDE_CHECK_RUNNING && xrtClock()-began<15000000) xrtSleep(1);
 if(!check || xideLanguageCheckState(check,error.message,sizeof(error.message))!=XIDE_CHECK_READY) fprintf(stderr,"definition proof check: %s\n",error.message);
 return check;
}
static const xvalue* groupFor(const xvalue* result,const char* path)
{
 const xvalue* groups=get(result,"documents");
 for(size_t i=0;i<xrtValueCount(groups);++i) { const xvalue* group=xrtValueArrayGet(groups,i); const char* name=string(get(group,"path")); if(name && xidePathLexicalEqual(name,path)) return group; }
 return NULL;
}
static int sameMetadata(const xvalue* proof,const xvalue* group)
{
 const xvalue* metadata=get(get(proof,"xlang"),"document");
 const char* fields[]={"path","origin","version","normalizedSize","normalizedSha256","input"};
 if(!numberIs(get(get(proof,"xlang"),"schema"),1) || !metadata || xrtValueCount(metadata)!=5 || get(metadata,"diagnostics") || get(metadata,"text")) return 0;
 for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i) {
  const xvalue *a=get(metadata,fields[i]),*b=get(group,fields[i]); if((a==NULL)!=(b==NULL) || (a && !xrtValueEqual(a,b))) return 0;
 }
 return 1;
}
typedef struct consumer { xide_language_check* check; const char* entry; int line,character; const xvalue* expected; int failed; } consumer;
static int consume(void* user)
{
 consumer* value=user;
 for(unsigned i=0;i<16;++i) {
  xvalue* out=xideLanguageCheckDefinitionWithProof(value->check,value->entry,9,value->line,value->character);
  if(!out || !xrtValueEqual(out,value->expected)) value->failed=1;
  xrtValueRelease(out);
 }
 return 0;
}
static int run(void)
{
 CHECK(!xideLanguageCheckDefinitionWithProof(NULL,"x",1,0,0));
 char relative[160]; snprintf(relative,sizeof(relative),"build/definition-proof-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
 CHECK(xrtDirCreateAll(relative)); char* root=xrtPathAbs(relative); CHECK(root);
 char *entry=xrtPathJoin(root,"入口😀.xl"),*dep=xrtPathJoin(root,"target_dep.xl"),*unused=xrtPathJoin(root,"unused.xl"),*compiler=xrtPathJoin(root,"absent/xl.exe"),*alias=xrtPathJoin(root,"TARGET_DEP.XL");
 CHECK(entry && dep && unused && compiler && alias);
 const char* text="\nimport target_dep\nfunc main(...) as int\n string 文本 = \"😀\"; return target_dep.value()\nend\n";
 const char* body="export func value() as int\n string 文本 = \"😀\"\n return 7\nend\n";
 const char* raw="\xef\xbb\xbf" "export func value() as int\r\n string 文本 = \"😀\"\r\n return 7\r\nend\r\n";
 const char* overlay="\n\nexport func value() as int\n return 8\nend\n";
 CHECK(writeBytes(dep,raw));
 xide_toolchains* cache=xideToolchainsCreate(); CHECK(cache);
 xide_language_snapshot inputs[3]={{entry,text,9},{unused,"func unused() as int\n return 1\nend\n",17},{alias,overlay,7}};
 xide_language_check* check=start(cache,compiler,root,entry,inputs,2); CHECK(check && xideLanguageCheckState(check,NULL,0)==XIDE_CHECK_READY);
 int line=0,character=0; CHECK(xideLspOffsetToPosition(text,(int)(strstr(text,".value")+1-text),&line,&character));
 xvalue *plain=xideLanguageCheckDefinition(check,entry,9,line,character),*proof=xideLanguageCheckDefinitionWithProof(check,entry,9,line,character),*result=xideLanguageCheckResult(check);
 CHECK(plain && proof && result && xrtValueCount(plain)==2 && xrtValueCount(proof)==3 && xrtValueEqual(get(plain,"uri"),get(proof,"uri")) && xrtValueEqual(get(plain,"range"),get(proof,"range")));
 CHECK(sameMetadata(proof,groupFor(result,dep)));
 const xvalue* metadata=get(get(proof,"xlang"),"document");
 CHECK(textIs(get(metadata,"origin"),"disk") && !get(metadata,"version") && hashIs(body,get(metadata,"normalizedSha256")) && hashIs(raw,get(get(metadata,"input"),"rawSha256")));
 CHECK(numberIs(get(get(get(proof,"range"),"start"),"line"),0) && numberIs(get(get(get(proof,"range"),"start"),"character"),12));
 CHECK(!xideLanguageCheckDefinitionWithProof(check,NULL,9,line,character) && !xideLanguageCheckDefinitionWithProof(check,entry,10,line,character));
 xvalue* empty=xideLanguageCheckDefinitionWithProof(check,entry,9,0,0); CHECK(empty && xrtValueType(empty)==XVALUE_NULL); xrtValueRelease(empty);
 CHECK(!xideLanguageCheckDefinitionWithProof(check,unused,17,0,5));
 consumer readers[4]; xthread* threads[4];
 for(size_t i=0;i<4;++i) { readers[i]=(consumer){check,entry,line,character,proof,0}; threads[i]=xrtThreadCreate(consume,&readers[i],0); CHECK(threads[i]); }
 for(size_t i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !readers[i].failed); xrtThreadDestroy(threads[i]); }
 xvalue* changed=xideLanguageCheckDefinitionWithProof(check,entry,9,line,character); CHECK(changed);
 CHECK(xrtValueObjectSetNew((xvalue*)get(get(changed,"xlang"),"document"),xrtStrView("origin"),xrtValueString(xrtStrView("mutated-caller-copy"))));
 xrtValueRelease(changed); changed=xideLanguageCheckDefinitionWithProof(check,entry,9,line,character); CHECK(changed && xrtValueEqual(changed,proof)); xrtValueRelease(changed);
 CHECK(writeBytes(dep,raw)); /* Same bytes, replacement identity. */
 uint64_t until=xrtClock()+6000000;
 while(xideLanguageCheckGuardState(check,NULL,0)!=XIDE_GUARD_INVALID && xrtClock()<until) xrtSleep(1);
 CHECK(xideLanguageCheckGuardState(check,NULL,0)==XIDE_GUARD_INVALID && !xideLanguageCheckDefinitionWithProof(check,entry,9,line,character));
 xideLanguageCheckDestroy(check); xrtValueRelease(plain); xrtValueRelease(result);
 CHECK(textIs(get(get(get(proof,"xlang"),"document"),"origin"),"disk"));
 check=start(cache,compiler,root,entry,inputs,3); CHECK(check && xideLanguageCheckState(check,NULL,0)==XIDE_CHECK_READY);
 xvalue *newProof=xideLanguageCheckDefinitionWithProof(check,entry,9,line,character),*newResult=xideLanguageCheckResult(check); CHECK(newProof && newResult);
 CHECK(sameMetadata(newProof,groupFor(newResult,alias)));
 metadata=get(get(newProof,"xlang"),"document");
 CHECK(textIs(get(metadata,"origin"),"overlay") && numberIs(get(metadata,"version"),7) && !get(metadata,"input") && hashIs(overlay,get(metadata,"normalizedSha256")) && numberIs(get(get(get(newProof,"range"),"start"),"line"),2));
 xideLanguageCheckCancel(check); CHECK(!xideLanguageCheckDefinitionWithProof(check,entry,9,line,character));
 xideLanguageCheckDestroy(check); xideToolchainsDestroy(cache);
 CHECK(sameMetadata(newProof,groupFor(newResult,alias)) && textIs(get(get(get(proof,"xlang"),"document"),"origin"),"disk"));
 xrtValueRelease(newResult); xrtValueRelease(newProof); xrtValueRelease(proof);
 xrtFree(entry); xrtFree(dep); xrtFree(unused); xrtFree(compiler); xrtFree(alias); xrtFree(root); return 1;
}
int main(void)
{
 if(!run()) return 1;
 printf("language_definition_proof_test passed: %u checks and64concurrentlookups; exact target metadata, generic parity, BOM/CRLF/Unicode, alias/overlay/actual-input guards, independent lifetime and cancellation.\n",checks); return 0;
}
