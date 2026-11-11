#include "workspace_edit_fixture.h"
#include "platform/workspace_edit_files.h"
#include "platform/path.h"
#include <windows.h>
#include <winioctl.h>
static size_t allocationCalls,failAllocation;static int tracking;static unsigned polls,stopAt;
#ifdef XLS_COMPILER_XRT
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void* __real_xrtCalloc(size_t,size_t);bool __real_xrtArrayPush(xarray*,const void*);
static int refused(void){return tracking && ++allocationCalls==failAllocation;}
void* __wrap_malloc(size_t n){return refused()?NULL:__real_malloc(n);}
void* __wrap_calloc(size_t n,size_t s){return refused()?NULL:__real_calloc(n,s);}
void* __wrap_xrtCalloc(size_t n,size_t s){return refused()?NULL:__real_xrtCalloc(n,s);}
bool __wrap_xrtArrayPush(xarray* a,const void* p){return refused()?false:__real_xrtArrayPush(a,p);}
#endif
static int stopFiles(void* unused){(void)unused;++polls;return stopAt && polls>=stopAt;}
static void hashBytes(const char* body,size_t size,char hash[65])
{unsigned char bytes[32];xsha256 s;xrtSha256Init(&s);xrtSha256Update(&s,body,size);xrtSha256Final(&s,bytes);const char* hex="0123456789abcdef";for(size_t i=0;i<32;++i){hash[2*i]=hex[bytes[i]>>4];hash[2*i+1]=hex[bytes[i]&15];}hash[64]=0;}
static xide_workspace_edit* captured(const char* const* paths,const char* const* raw,const char* const* normal,size_t count)
{
 xvalue *value=xrtValueObject(),*docs=xrtValueArray();xide_workspace_edit_input inputs[4]={0};char* uris[4]={0};int okay=count<=4 && value && docs;
 for(size_t i=0;okay && i<count;++i){
  uris[i]=xideLspPathToUri(paths[i],NULL);inputs[i]=(xide_workspace_edit_input){.uri=uris[i],.text=xlsTextCreate(normal[i],strlen(normal[i]),1024,NULL),.revision=i+1};
  xvalue *edits=xrtValueArray(),*row=documentEdit(uris[i],0,0,edits),*extension=xrtValueObject(),*document=xrtValueObject(),*input=xrtValueObject();char normHash[65],rawHash[65];hashBytes(normal[i],strlen(normal[i]),normHash);hashBytes(raw[i],strlen(raw[i]),rawHash);
  okay=uris[i] && inputs[i].text && row && extension && document && input && putEdit(document,"path",xrtValueString(xrtStrView(paths[i]))) && putEdit(document,"origin",xrtValueString(xrtStrView("disk"))) && putEdit(document,"normalizedSize",xrtValueInt((int64)strlen(normal[i]))) && putEdit(document,"normalizedSha256",xrtValueString(xrtStrView(normHash))) && putEdit(input,"path",xrtValueString(xrtStrView(paths[i]))) && putEdit(input,"origin",xrtValueString(xrtStrView("disk"))) && putEdit(input,"rawSize",xrtValueInt((int64)strlen(raw[i]))) && putEdit(input,"rawSha256",xrtValueString(xrtStrView(rawHash)));
  if(okay){okay=putEdit(document,"input",input);input=NULL;}if(okay){okay=putEdit(extension,"document",document);document=NULL;}if(okay)okay=putEdit(extension,"schema",xrtValueInt(1));if(okay){okay=putEdit(row,"xlang",extension);extension=NULL;}if(okay){okay=xrtValueArrayAppendNew(docs,row);row=NULL;}
  xrtValueRelease(row);xrtValueRelease(extension);xrtValueRelease(document);xrtValueRelease(input);
 }
 if(okay){okay=putEdit(value,"documentChanges",docs);docs=NULL;}enum xide_workspace_edit_error error;xide_workspace_edit* plan=okay?xideWorkspaceEditPrepare(value,inputs,count,1,0,NULL,NULL,&error):NULL;
 xrtValueRelease(docs);xrtValueRelease(value);for(size_t i=0;i<count && i<4;++i){xlsTextRelease(inputs[i].text);free(uris[i]);}return plan;
}
static int writeSource(const char* path,const char* data)
{wchar_t* w=xidePathNativeWide(path);HANDLE f=w?CreateFileW(w,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;free(w);if(f==INVALID_HANDLE_VALUE)return 0;DWORD size=(DWORD)strlen(data),written=0;int okay=WriteFile(f,data,size,&written,NULL) && written==size;CloseHandle(f);return okay;}
static int removeSource(const char* path){wchar_t* w=xidePathNativeWide(path);int okay=w && DeleteFileW(w);free(w);return okay;}
static int junction(const char* path,const char* target)
{
 wchar_t *w=xidePathNativeWide(path),*to=xidePathNativeWide(target);int okay=w && to && CreateDirectoryW(w,NULL);HANDLE h=okay?CreateFileW(w,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL):INVALID_HANDLE_VALUE;
 struct {DWORD tag;WORD length,reserved,subOffset,subLength,printOffset,printLength;wchar_t text[1024];} data={.tag=IO_REPARSE_TAG_MOUNT_POINT};size_t n=to?wcslen(to):0;
 if(h!=INVALID_HANDLE_VALUE && n<1023){memcpy(data.text,to,(n+1)*sizeof(wchar_t));memcpy(data.text,L"\\??\\",4*sizeof(wchar_t));data.subLength=(WORD)(n*sizeof(wchar_t));data.printOffset=(WORD)((n+1)*sizeof(wchar_t));data.length=(WORD)(8+(n+2)*sizeof(wchar_t));DWORD bytes=0;okay=DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&data,data.length+8,NULL,0,&bytes,NULL)!=0;}else okay=0;
 if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);}free(w);free(to);return okay;
}
static int run(const char* directory,size_t singleFault)
{
 wchar_t* wd=xidePathNativeWide(directory);CHECK(wd && CreateDirectoryW(wd,NULL));char a[4096],b[4096],alias[4096],link[4096],inside[4096],moved[4096];snprintf(a,sizeof(a),"%s/a.xl",directory);snprintf(b,sizeof(b),"%s/中文.xl",directory);snprintf(alias,sizeof(alias),"%s/hardlink.xl",directory);snprintf(link,sizeof(link),"%s/junction",directory);snprintf(inside,sizeof(inside),"%s/junction/a.xl",directory);snprintf(moved,sizeof(moved),"%s-moved",directory);
 const char *paths[]={a,b},*raw[]={"\xef\xbb\xbf" "a\r\nb\r\n","中文\r"},*normal[]={"a\nb\n","中文\n"};CHECK(writeSource(a,raw[0]) && writeSource(b,raw[1]));xide_workspace_edit* plan=captured(paths,raw,normal,2);CHECK(plan);xide_model_error error={0};DWORD handles;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));uint64_t blocks=editBlocks();
 allocationCalls=0;failAllocation=singleFault;tracking=1;polls=0;xide_workspace_edit_files* files=xideWorkspaceEditFilesPin(plan,stopFiles,NULL,&error);tracking=0;
 if(singleFault){CHECK(allocationCalls>=singleFault);if(files){CHECK(xideWorkspaceEditFilesValidate(files,NULL,NULL,&error));xideWorkspaceEditFilesDestroy(files);}else CHECK(error.code);CHECK(editBlocks()==blocks);}
 else {
  CHECK(files && !error.code && xideWorkspaceEditFilesCount(files)==2);size_t observed=allocationCalls;unsigned observedPolls=polls;printf("file gate coverage: %zu observed allocation gates,%u cancellation polls\n",observed,observedPolls);
  size_t document; xide_file_snapshot snapshot;CHECK(xideWorkspaceEditFilesAt(files,1,&document,&snapshot) && document==1 && snapshot.length==strlen(raw[1]));CHECK(!xideWorkspaceEditFilesAt(files,2,&document,&snapshot) && document==SIZE_MAX && !snapshot.length);
  CHECK(!writeSource(a,"changed") && !removeSource(b));wchar_t* next=xidePathNativeWide(moved);CHECK(next && !MoveFileW(wd,next));free(next);
  xideWorkspaceEditRelease(plan);plan=NULL;CHECK(xideWorkspaceEditFilesValidate(files,NULL,NULL,&error));xideWorkspaceEditFilesDestroy(files);files=NULL;
#ifdef XLS_COMPILER_XRT
  CHECK(editBlocks()<blocks);
#endif
  plan=captured(paths,raw,normal,2);CHECK(plan);blocks=editBlocks();
  for(unsigned i=1;i<=observedPolls;++i){polls=0;stopAt=i;CHECK(!xideWorkspaceEditFilesPin(plan,stopFiles,NULL,&error) && error.code && polls==i && editBlocks()==blocks);}stopAt=0;
  CHECK(writeSource(a,"\xef\xbb\xbf" "A\r\nb\r\n"));CHECK(!xideWorkspaceEditFilesPin(plan,NULL,NULL,&error) && error.code);CHECK(writeSource(a,raw[0]));
  /* Same raw length and decoded text; only the physical byte proof differs. */
  CHECK(writeSource(b,"中文\n"));CHECK(!xideWorkspaceEditFilesPin(plan,NULL,NULL,&error));CHECK(writeSource(b,raw[1]));
  const char* wrong[]={"wrong\n","中文\n"};xide_workspace_edit* contradictory=captured(paths,raw,wrong,2);CHECK(contradictory && !xideWorkspaceEditFilesPin(contradictory,NULL,NULL,&error));xideWorkspaceEditRelease(contradictory);
  wchar_t *wa=xidePathNativeWide(a),*wb=xidePathNativeWide(b),*wl=xidePathNativeWide(alias);CHECK(wa && wb && wl && CreateHardLinkW(wl,wa,NULL));CHECK(!xideWorkspaceEditFilesPin(plan,NULL,NULL,&error));CHECK(DeleteFileW(wl));
  CHECK(SetFileAttributesW(wb,FILE_ATTRIBUTE_READONLY));CHECK(!xideWorkspaceEditFilesPin(plan,NULL,NULL,&error));CHECK(SetFileAttributesW(wb,FILE_ATTRIBUTE_NORMAL));
  HANDLE writer=CreateFileW(wa,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);CHECK(writer!=INVALID_HANDLE_VALUE && !xideWorkspaceEditFilesPin(plan,NULL,NULL,&error));CHECK(CloseHandle(writer));
  CHECK(junction(link,directory));const char* nested[]={inside};xide_workspace_edit* reparse=captured(nested,raw,normal,1);CHECK(reparse && !xideWorkspaceEditFilesPin(reparse,NULL,NULL,&error));xideWorkspaceEditRelease(reparse);wchar_t* wj=xidePathNativeWide(link);CHECK(wj && RemoveDirectoryW(wj));free(wj);
  files=xideWorkspaceEditFilesPin(plan,NULL,NULL,&error);CHECK(files);HANDLE attributes=CreateFileW(wb,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);FILETIME changed={12345,6789};CHECK(attributes!=INVALID_HANDLE_VALUE && SetFileTime(attributes,NULL,NULL,&changed));CloseHandle(attributes);CHECK(!xideWorkspaceEditFilesValidate(files,NULL,NULL,&error));xideWorkspaceEditFilesDestroy(files);
  free(wa);free(wb);free(wl);CHECK(editBlocks()==blocks);
 }
 xideWorkspaceEditRelease(plan);DWORD after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after) && after==handles);CHECK(removeSource(a) && removeSource(b) && RemoveDirectoryW(wd));free(wd);return 1;
}
int main(int argc,char** argv)
{
#ifdef XLS_COMPILER_XRT
 xrtMemStatsEnable(true);
#endif
 char temp[MAX_PATH],directory[2048];if(argc>1)snprintf(directory,sizeof(directory),"%s",argv[1]);else{if(!GetTempPathA(sizeof(temp),temp))return 2;snprintf(directory,sizeof(directory),"%sxlang-workspace-files-%lu",temp,GetCurrentProcessId());}
 if(!run(directory,argc>2?(size_t)strtoull(argv[2],NULL,10):0)){return 1;}printf("workspace_edit_files_test passed: %u checks; pinned raw/decoded proof, BOM/EOL/Unicode, writer/delete/ancestor locks, hardlinks, readonly, junctions, changes, cancellation, fault cleanup and handle balance\n",checks);return 0;
}
