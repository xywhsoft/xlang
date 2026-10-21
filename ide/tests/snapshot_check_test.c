#include "services/language_check.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"snapshot_check_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* key) { return xrtValueObjectGet(o,sv(key)); }
static const char* string(const xvalue* v) { xstrview text={0}; return xrtValueGetString(v,&text) ? text.Data : NULL; }
static int number(const xvalue* v) { int64 n=-1; return xrtValueGetInt(v,&n) ? (int)n : -1; }
static int waitCheck(xide_language_check* q,xide_model_error* e)
{ uint64_t deadline=GetTickCount64()+20000; do { int state=xideLanguageCheckState(q,e->message,sizeof(e->message)); if(state!=XIDE_CHECK_RUNNING) return state; Sleep(5); } while(GetTickCount64()<deadline); return -1; }
static int waitInvalid(xide_language_check* q,xide_model_error* e)
{ uint64_t until=GetTickCount64()+5000; do { if(xideLanguageCheckGuardState(q,e->message,sizeof(e->message))==XIDE_GUARD_INVALID) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
int main(int argc,char** argv)
{
 int failed=0; xide_model_error error={0}; xide_toolchains* cache=NULL; xide_language_check* q=NULL; xvalue* result=NULL;
 char *root=NULL,*entry=NULL,*dep=NULL,*virtualFile=NULL,*alias=NULL,*body=NULL,*large=NULL; xide_file_snapshot before[2],after[2];
 CHECK(argc==3); char unique[90]; snprintf(unique,sizeof(unique),"snapshots-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(argv[1],unique); entry=xidePathResolve(root,"入口😀.xl"); dep=xidePathResolve(root,"dep.xl"); virtualFile=xidePathResolve(root,"virtual.xl"); alias=xidePathResolveLexical(root,"./DEP.XL");
 CHECK(root && entry && dep && virtualFile && alias && xrtDirCreateAll(root));
 const char *diskEntry="func main(...) as int\n    return 0\nend\n",*entryText="import dep\nfunc main(...) as int\n    return dep.value()\nend\n",*good="export func value() as int\n    return 7\nend\n",*bad="export func value() as int\n    return \"bad\"\nend\n";
 CHECK(writeText(entry,diskEntry) && writeText(dep,good) && xideFileSnapshotInspect(entry,&before[0],&error) && xideFileSnapshotInspect(dep,&before[1],&error)); cache=xideToolchainsCreate(); CHECK(cache);
 body=malloc(strlen(bad)+1); CHECK(body); strcpy(body,bad); xide_language_snapshot inputs[3]={{entry,entryText,13},{dep,body,21}};
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,2,&error); CHECK(q); memset(body,' ',strlen(body)); free(body); body=NULL; inputs[1].text=good; inputs[1].version=99;
 CHECK(waitCheck(q,&error)==XIDE_CHECK_READY && (result=xideLanguageCheckResult(q)) && number(get(result,"version"))==13 && xrtValueCount(get(result,"diagnostics"))==1);
 CHECK(xrtValueCount(get(result,"documents"))==2 && number(get(xrtValueArrayGet(get(result,"documents"),1),"version"))==21 && xidePathLexicalEqual(string(get(xrtValueArrayGet(get(result,"diagnostics"),0),"file")),dep)); xrtValueRelease(result); result=NULL; xideLanguageCheckDestroy(q); q=NULL;
 inputs[0]=(xide_language_snapshot){entry,entryText,14}; inputs[1]=(xide_language_snapshot){dep,"import virtual\nexport func value() as int\n    return virtual.value()\nend\n",22}; inputs[2]=(xide_language_snapshot){virtualFile,good,1};
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,3,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY && (result=xideLanguageCheckResult(q)) && !xrtValueCount(get(result,"diagnostics")) && !xidePathExists(virtualFile)); xrtValueRelease(result); result=NULL; xideLanguageCheckDestroy(q); q=NULL;
 xide_language_snapshot invalid[]={{entry,entryText,1},{alias,bad,2},{dep,good,3}}; CHECK(!xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,invalid,3,&error));
 invalid[0].text="\xed\xa0\x80"; CHECK(!xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,invalid,1,&error)); invalid[0].text=entryText; CHECK(!xideLanguageCheckStartSnapshots(cache,argv[2],root,virtualFile,invalid,1,&error)); CHECK(!xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,invalid,65,&error));
 large=malloc(8u*1024u*1024u+2); CHECK(large); memset(large,' ',8u*1024u*1024u+1); large[8u*1024u*1024u+1]=0; invalid[0].text=large; CHECK(!xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,invalid,1,&error));
 large[8u*1024u*1024u]=0; const char* batchPaths[]={"a.xl","b.xl","c.xl","d.xl"}; xide_language_snapshot batch[5]={{entry,entryText,1}}; char* names[4]={0};
 for(int i=0;i<4;++i) { names[i]=xidePathResolve(root,batchPaths[i]); batch[i+1]=(xide_language_snapshot){names[i],large,1}; }
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,batch,5,&error); for(int i=0;i<4;++i) free(names[i]); CHECK(!q); free(large); large=NULL;
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,3,&error); CHECK(q); xideLanguageCheckCancel(q); uint64_t began=GetTickCount64(); CHECK(waitCheck(q,&error)==XIDE_CHECK_CANCELLED && GetTickCount64()-began<2000); xideLanguageCheckDestroy(q); q=NULL;
 CHECK(xideFileSnapshotInspect(entry,&after[0],&error) && xideFileSnapshotInspect(dep,&after[1],&error) && xideFileSnapshotEqual(&before[0],&after[0]) && xideFileSnapshotEqual(&before[1],&after[1]) && !xidePathExists(virtualFile));
 /* Publication remains guarded without another compiler process. */
 inputs[0]=(xide_language_snapshot){entry,entryText,40}; inputs[1]=(xide_language_snapshot){dep,bad,41};
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,2,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY);
 Sleep(1200); CHECK(xideLanguageCheckGuardState(q,NULL,0)==XIDE_GUARD_RUNNING && !xideLanguageCheckWorkerFinished(q));
 /* Disk writes behind an overlay do not replace its submitted immutable text. */
 CHECK(writeText(dep,bad)); Sleep(1200); CHECK(xideLanguageCheckGuardState(q,NULL,0)==XIDE_GUARD_RUNNING);
 xideLanguageCheckCancel(q); uint64_t stopped=GetTickCount64(); while(!xideLanguageCheckWorkerFinished(q) && GetTickCount64()-stopped<2000) Sleep(5); CHECK(xideLanguageCheckWorkerFinished(q) && !xideLanguageCheckResult(q)); xideLanguageCheckDestroy(q); q=NULL; CHECK(writeText(dep,good));
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,1,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY && xideLanguageCheckGuardState(q,NULL,0)==XIDE_GUARD_RUNNING);
 CHECK(writeText(dep,bad) && waitInvalid(q,&error) && !xideLanguageCheckResult(q)); xideLanguageCheckDestroy(q); q=NULL; CHECK(writeText(dep,good));
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,1,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY);
 CHECK(writeText(dep,good) && waitInvalid(q,&error)); xideLanguageCheckDestroy(q); q=NULL;
 /* Environment is process-local; unset and empty remain different states. */
 char* originalPackage=NULL; CHECK(xideLanguageCheckPackagePath(&originalPackage,&error));
 q=xideLanguageCheckStartSnapshots(cache,argv[2],root,entry,inputs,1,&error); CHECK(q && waitCheck(q,&error)==XIDE_CHECK_READY);
 CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",originalPackage && !*originalPackage ? NULL : L"") && waitInvalid(q,&error));
 xideLanguageCheckDestroy(q); q=NULL;
 if(originalPackage) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,originalPackage,-1,NULL,0); wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; CHECK(w && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,originalPackage,-1,w,n) && SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",w)); free(w); }
 else { CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL)); }
 free(originalPackage);
 puts("snapshot_check_test passed: immutable copied batches, dirty recursive and virtual imports, per-file versions, aliases, UTF-8/entry/64-file/8-MiB/32-MiB bounds, cancellation, permanent post-publication disk/identity/environment invalidation, overlay authority and signaled worker retirement.");
cleanup:
 xideLanguageCheckDestroy(q); xrtValueRelease(result); xideToolchainsDestroy(cache); free(root); free(entry); free(dep); free(virtualFile); free(alias); free(body); free(large); return failed;
}
