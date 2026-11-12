#define _WIN32_WINNT 0x0601
#include "workspace_edit_fixture.h"
#include "platform/workspace_edit_aliases.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <windows.h>
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-alias:%d:%s\n",__LINE__,#v);exit(1);}}while(0)
static int probing;static size_t calls,failAt,readCalls;static unsigned polls,stopAt;
static int refused(void){return probing&&++calls==failAt;}
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);
void* __wrap_malloc(size_t n){return refused()?NULL:__real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){return refused()?NULL:__real_calloc(n,z);}
extern void*(*__real___imp_xrtMalloc)(size_t);extern void*(*__real___imp_xrtCalloc)(size_t,size_t);
static void* xm(size_t n){return refused()?NULL:__real___imp_xrtMalloc(n);}
static void* xc(size_t n,size_t z){return refused()?NULL:__real___imp_xrtCalloc(n,z);}
void*(*__wrap___imp_xrtMalloc)(size_t)=xm;void*(*__wrap___imp_xrtCalloc)(size_t,size_t)=xc;
extern BOOL(WINAPI*__real___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI readProbe(HANDLE h,LPVOID p,DWORD n,LPDWORD got,LPOVERLAPPED at){if(probing)++readCalls;return __real___imp_ReadFile(h,p,n,got,at);}
BOOL(WINAPI*__wrap___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=readProbe;
static int cancel(void* unused){(void)unused;++polls;return stopAt&&polls>=stopAt;}
static int writeText(const char* p,const char* body){return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)body,strlen(body)});}
static int writeInPlace(const char* path,const char* body)
{wchar_t* p=xidePathNativeWide(path);HANDLE h=p?CreateFileW(p,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL):INVALID_HANDLE_VALUE;free(p);if(h==INVALID_HANDLE_VALUE)return 0;DWORD n=0;int okay=WriteFile(h,body,(DWORD)strlen(body),&n,NULL)&&n==strlen(body)&&SetEndOfFile(h);CloseHandle(h);return okay;}
static xide_workspace_edit* plan(const char* path,int open)
{
 char* uri=xideLspPathToUri(path,NULL);CHECK(uri);xls_text* text=xlsTextCreate("abc\n",4,1024,NULL);CHECK(text);xide_workspace_edit_input input={.uri=uri,.text=text,.identity=open?1:0,.revision=1,.hasVersion=open,.version=5};xvalue* value=oneEdit(uri,open,5,"z");enum xide_workspace_edit_error why;xide_workspace_edit* result=xideWorkspaceEditPrepare(value,&input,1,0,0,NULL,NULL,&why);CHECK(result&&!why);xrtValueRelease(value);xlsTextRelease(text);free(uri);return result;
}
static char* volumeName(const char* path)
{
 wchar_t* native=xidePathNativeWide(path);CHECK(native&&native[5]==L':');wchar_t root[]={native[4],L':',L'\\',0},volume[80];CHECK(GetVolumeNameForVolumeMountPointW(root,volume,80));size_t n=wcslen(volume)+wcslen(native+7)+1;wchar_t* alias=malloc(n*sizeof(wchar_t));CHECK(alias);swprintf(alias,n,L"%ls%ls",volume,native+7);int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,alias,-1,NULL,0,NULL,NULL);char* result=bytes?malloc((size_t)bytes):NULL;CHECK(result&&WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,alias,-1,result,bytes,NULL,NULL));free(native);free(alias);return result;
}
static void traceHandles(const char* step){DWORD n=0;GetProcessHandleCount(GetCurrentProcess(),&n);fprintf(stderr,"alias handle trace %s: %lu\n",step,(unsigned long)n);}
static void expectDenied(const xide_workspace_edit* p,const xide_edit_alias_model* models,size_t count,const xide_edit_alias_exclusion* roots,size_t n)
{xide_model_error e={0};DWORD before,after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&before));xide_edit_aliases* gate=xideEditAliasesPin(p,models,count,roots,n,NULL,NULL,&e);if(gate){xideEditAliasesDestroy(gate);}CHECK(!gate&&e.code&&GetProcessHandleCount(GetCurrentProcess(),&after)&&before==after);}
int main(int argc,char** argv)
{
 if(argc!=2||!xideIoCreateDirectories(argv[1]))return 2;
 char* directory=xidePathResolveLexical(argv[1],"sources");char* protected=xidePathResolveLexical(argv[1],"protected");CHECK(directory&&protected&&xideIoCreateDirectories(directory)&&xideIoCreateDirectories(protected));
 char *a=xidePathResolveLexical(directory,"main.xl"),*b=xidePathResolveLexical(directory,"other.c"),*c=xidePathResolveLexical(protected,"asset.xl"),*missing=xidePathResolveLexical(directory,"missing.xl"),*link=xidePathResolveLexical(directory,"hardlink.xl"),*newRoot=xidePathResolveLexical(argv[1],"future-root"),*moved=xidePathResolveLexical(argv[1],"moved-sources");CHECK(a&&b&&c&&missing&&link&&newRoot&&moved&&writeText(a,"abc\n")&&writeText(b,"int other;\n")&&writeText(c,"abc\n"));
 xide_workspace_edit* open=plan(a,1);xide_workspace_edit* closed=plan(a,0);xide_edit_alias_model models[]={{a,1},{b,2}};xide_edit_alias_exclusion roots[]={{protected,1}};xide_model_error e={0};
 xide_edit_aliases* gate=xideEditAliasesPin(open,models,2,roots,1,cancel,NULL,&e);if(!gate)fprintf(stderr,"initial pin: %d %s\n",e.code,e.message);CHECK(gate&&xideEditAliasesStorage(gate)<16u*1024u*1024u&&xideEditAliasesValidate(gate,NULL,NULL,&e));xideEditAliasesDestroy(gate);
 traceHandles("baseline");
 DWORD handles;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));
 calls=failAt=readCalls=0;polls=0;probing=1;gate=xideEditAliasesPin(open,models,2,roots,1,cancel,NULL,&e);probing=0;CHECK(gate&&calls&&!readCalls);size_t allocations=calls;unsigned cancellations=polls;CHECK(cancellations);xideEditAliasesDestroy(gate);
 for(size_t n=1;n<=allocations;++n){calls=0;failAt=n;probing=1;gate=xideEditAliasesPin(open,models,2,roots,1,NULL,NULL,&e);probing=0;CHECK(!gate&&e.code&&calls>=n);DWORD after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);}failAt=0;
 for(unsigned n=1;n<=cancellations;++n){polls=0;stopAt=n;gate=xideEditAliasesPin(open,models,2,roots,1,cancel,NULL,&e);CHECK(!gate&&e.code&&polls==n);DWORD after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);}stopAt=0;
 traceHandles("faults");
 /* Exact owner roster, including untouched named models. */
 expectDenied(open,NULL,0,NULL,0);xide_edit_alias_model wrong[]={{a,9},{b,2}};expectDenied(open,wrong,2,NULL,0);wrong[0].identity=1;wrong[1].identity=1;expectDenied(open,wrong,2,NULL,0);wrong[1]=(xide_edit_alias_model){a,2};expectDenied(open,wrong,2,NULL,0);expectDenied(closed,models,2,NULL,0);
 traceHandles("before-volume");
 /* A volume-GUID name is a real, non-hardlink alias (link count remains one).
  * This cannot be detected by a case/slash/lexical-only comparison. */
 char* av=volumeName(a);xide_edit_alias_model aliased[]={{a,1},{av,2}};expectDenied(open,aliased,2,NULL,0);xide_edit_alias_model hiddenAlias={av,2};expectDenied(closed,&hiddenAlias,1,NULL,0);
 xide_edit_alias_exclusion fileAlias={av,0};expectDenied(open,models,2,&fileAlias,1);
 xide_workspace_edit* protectedPlan=plan(c,0);expectDenied(protectedPlan,NULL,0,roots,1);char* pv=volumeName(protected);xide_edit_alias_exclusion physicalRoot={pv,1};expectDenied(protectedPlan,NULL,0,&physicalRoot,1);xideWorkspaceEditRelease(protectedPlan);
 traceHandles("after-volume");
 /* Single-link policy is independent of the model/physical collision check. */
 wchar_t *wa=xidePathNativeWide(a),*wl=xidePathNativeWide(link),*wd=xidePathNativeWide(directory),*wm=xidePathNativeWide(moved);CHECK(wa&&wl&&wd&&wm&&CreateHardLinkW(wl,wa,NULL));expectDenied(open,models,2,NULL,0);CHECK(DeleteFileW(wl));traceHandles("after-hardlink");
 CHECK(SetFileAttributesW(wa,FILE_ATTRIBUTE_READONLY));gate=xideEditAliasesPin(open,models,2,NULL,0,NULL,NULL,&e);CHECK(gate&&xideEditAliasesValidate(gate,NULL,NULL,&e));xideEditAliasesDestroy(gate);traceHandles("after-readonly-destroy");CHECK(SetFileAttributesW(wa,FILE_ATTRIBUTE_NORMAL));
 gate=xideEditAliasesPin(open,models,2,NULL,0,NULL,NULL,&e);traceHandles("after-name-pin");CHECK(gate&&!MoveFileW(wd,wm)&&!DeleteFileW(wa));traceHandles("after-name-refusals");/* This is identity policy, not a content proof. */CHECK(writeInPlace(a,"different\n")&&xideEditAliasesValidate(gate,NULL,NULL,&e));traceHandles("after-inplace");xideEditAliasesDestroy(gate);traceHandles("after-name-destroy");CHECK(writeInPlace(a,"abc\n"));traceHandles("after-inplace-restore");
 traceHandles("after-pins");
 /* Missing overlays and exclusions are explicit absence claims. */
 xide_workspace_edit* absentPlan=plan(missing,1);xide_edit_alias_model absentModel={missing,1};gate=xideEditAliasesPin(absentPlan,&absentModel,1,NULL,0,NULL,NULL,&e);CHECK(gate&&xideEditAliasesValidate(gate,NULL,NULL,&e)&&writeText(missing,"created\n")&&!xideEditAliasesValidate(gate,NULL,NULL,&e));xideEditAliasesDestroy(gate);wchar_t* wn=xidePathNativeWide(missing);CHECK(wn&&DeleteFileW(wn));free(wn);xide_edit_alias_exclusion absentExcluded={missing,0};expectDenied(absentPlan,&absentModel,1,&absentExcluded,1);xideWorkspaceEditRelease(absentPlan);
 xide_edit_alias_exclusion future={newRoot,1};gate=xideEditAliasesPin(open,models,2,&future,1,NULL,NULL,&e);CHECK(gate&&xideIoCreateDirectories(newRoot)&&!xideEditAliasesValidate(gate,NULL,NULL,&e));xideEditAliasesDestroy(gate);
 traceHandles("after-absence");
 /* The actual upper model bound; the next item rejects before dereference. */
 xide_edit_alias_model* many=calloc(4096,sizeof(*many));CHECK(many);char** names=calloc(4096,sizeof(*names));CHECK(names);many[0]=models[0];for(size_t i=1;i<4096;++i){char leaf[40];snprintf(leaf,sizeof(leaf),"missing-%zu.xl",i);names[i]=xidePathResolveLexical(directory,leaf);CHECK(names[i]);many[i]=(xide_edit_alias_model){names[i],i+1};}gate=xideEditAliasesPin(open,many,4096,NULL,0,NULL,NULL,&e);CHECK(gate&&xideEditAliasesValidate(gate,NULL,NULL,&e));xideEditAliasesDestroy(gate);expectDenied(open,(const xide_edit_alias_model*)(uintptr_t)1,4097,NULL,0);for(size_t i=1;i<4096;++i)free(names[i]);free(names);free(many);
 traceHandles("final");
 DWORD after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);xideWorkspaceEditRelease(open);xideWorkspaceEditRelease(closed);free(wa);free(wl);free(wd);free(wm);free(av);free(pv);free(directory);free(protected);free(a);free(b);free(c);free(missing);free(link);free(newRoot);free(moved);
 printf("PASS workspace edit aliases: %u checks; %zu observed direct allocation failures, %u cancellation boundaries, actual volume-GUID aliases/roots, complete models, readonly/missing overlays, hardlinks, ancestor pins, explicit absence, metadata-only and handle balance\n",checks,allocations,cancellations);return 0;
}
