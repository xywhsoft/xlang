#define main fileGateFixtureMain
#include "workspace_edit_files_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"file-export:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static int corruptRead,refuseWrite;static unsigned exportPoll,exportStop;
extern BOOL (WINAPI* __real___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
extern BOOL (WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI probeRead(HANDLE f,LPVOID p,DWORD n,LPDWORD got,LPOVERLAPPED ov)
{BOOL ok=__real___imp_ReadFile(f,p,n,got,ov);if(ok && corruptRead && *got){((unsigned char*)p)[0]^=1;corruptRead=0;}return ok;}
static BOOL WINAPI probeWrite(HANDLE f,LPCVOID p,DWORD n,LPDWORD got,LPOVERLAPPED ov)
{if(refuseWrite){*got=0;SetLastError(ERROR_DISK_FULL);return FALSE;}return __real___imp_WriteFile(f,p,n,got,ov);}
BOOL (WINAPI* __wrap___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=probeRead;
BOOL (WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=probeWrite;
static int cancelExport(void* unused){(void)unused;return ++exportPoll==exportStop;}
static HANDLE emptyTarget(const char* path)
{wchar_t* wide=xidePathNativeWide(path);CHECK(wide);HANDLE h=CreateFileW(wide,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);free(wide);CHECK(h!=INVALID_HANDLE_VALUE);return h;}
static void equalsHandle(HANDLE h,const unsigned char* bytes,size_t size)
{
 LARGE_INTEGER actual={0},zero={0};CHECK(GetFileSizeEx(h,&actual) && actual.QuadPart==(LONGLONG)size && SetFilePointerEx(h,zero,NULL,FILE_BEGIN));unsigned char block[65536];size_t at=0;
 while(at<size){DWORD got=0,want=size-at>sizeof(block)?sizeof(block):(DWORD)(size-at);CHECK(ReadFile(h,block,want,&got,NULL) && got==want && !memcmp(block,bytes+at,want));at+=got;}
}
int main(int argc,char** argv)
{
 CHECK(argc==2 && xrtDirCreateAll(argv[1]));char a[4096],b[4096],output[4096];snprintf(a,sizeof(a),"%s/export-source.xl",argv[1]);snprintf(b,sizeof(b),"%s/export-empty.xl",argv[1]);snprintf(output,sizeof(output),"%s/export.body",argv[1]);
 size_t size=196611;char* raw=malloc(size+1);CHECK(raw);memcpy(raw,"\xef\xbb\xbf",3);memset(raw+3,'a',size-3);raw[size]=0;const char *paths[]={a,b},*bytes[]={raw,""},*texts[]={raw+3,""};CHECK(writeSource(a,raw) && writeSource(b,""));xide_workspace_edit* plan=captured(paths,bytes,texts,2);CHECK(plan);xide_model_error error={0};xide_workspace_edit_files* files=xideWorkspaceEditFilesPin(plan,NULL,NULL,&error);CHECK(files);xideWorkspaceEditRelease(plan);int bom=0,eol=0;CHECK(xideWorkspaceEditFilesCodecAt(files,0,&bom,&eol) && bom==1 && eol==XIDE_TEXT_EOL_LF);CHECK(xideWorkspaceEditFilesCodecAt(files,1,&bom,&eol) && !bom && eol==XIDE_TEXT_EOL_LF);CHECK(!xideWorkspaceEditFilesCodecAt(files,2,&bom,&eol) && !bom && !eol);
 DWORD before;CHECK(GetProcessHandleCount(GetCurrentProcess(),&before));HANDLE target=emptyTarget(output);CHECK(xideWorkspaceEditFilesCopyBefore(files,0,target,NULL,NULL,&error) && !error.code);equalsHandle(target,(unsigned char*)raw,size);CHECK(!xideWorkspaceEditFilesCopyBefore(files,0,target,NULL,NULL,&error) && error.code==XIDE_MODEL_INVALID);equalsHandle(target,(unsigned char*)raw,size);CHECK(CloseHandle(target));
 target=emptyTarget(output);CHECK(xideWorkspaceEditFilesCopyBefore(files,1,target,NULL,NULL,&error));equalsHandle(target,(unsigned char*)"",0);CHECK(CloseHandle(target));
 for(unsigned at=1;at<=6;++at){target=emptyTarget(output);exportPoll=0;exportStop=at;CHECK(!xideWorkspaceEditFilesCopyBefore(files,0,target,cancelExport,NULL,&error) && error.code==XIDE_MODEL_CONFLICT && exportPoll==at);LARGE_INTEGER length={0};CHECK(GetFileSizeEx(target,&length) && length.QuadPart<=(LONGLONG)size);CHECK(CloseHandle(target));}exportStop=0;
 target=emptyTarget(output);corruptRead=1;CHECK(!xideWorkspaceEditFilesCopyBefore(files,0,target,NULL,NULL,&error) && error.code==XIDE_MODEL_CONFLICT && !corruptRead);CHECK(CloseHandle(target) && xideWorkspaceEditFilesValidate(files,NULL,NULL,&error));
 target=emptyTarget(output);refuseWrite=1;CHECK(!xideWorkspaceEditFilesCopyBefore(files,0,target,NULL,NULL,&error) && error.code==XIDE_MODEL_IO);refuseWrite=0;equalsHandle(target,(unsigned char*)"",0);CHECK(CloseHandle(target));
 wchar_t* empty=xidePathNativeWide(b);HANDLE alias=CreateFileW(empty,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);free(empty);CHECK(alias!=INVALID_HANDLE_VALUE && !xideWorkspaceEditFilesCopyBefore(files,1,alias,NULL,NULL,&error) && error.code==XIDE_MODEL_CONFLICT && CloseHandle(alias));
 target=emptyTarget(output);wchar_t* source=xidePathNativeWide(a);HANDLE attr=CreateFileW(source,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);free(source);FILETIME changed={123,456};CHECK(attr!=INVALID_HANDLE_VALUE && SetFileTime(attr,NULL,NULL,&changed) && CloseHandle(attr));CHECK(!xideWorkspaceEditFilesCopyBefore(files,0,target,NULL,NULL,&error) && error.code==XIDE_MODEL_CONFLICT);equalsHandle(target,(unsigned char*)"",0);CHECK(CloseHandle(target));
 DWORD after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after) && after==before);xideWorkspaceEditFilesDestroy(files);CHECK(removeSource(a) && removeSource(b) && removeSource(output));free(raw);printf("workspace_edit_file_export_test passed: %u checks; verified codec, exact streaming bytes, empty sources, no clobber, all cancellation polls, injected read corruption/write failure, source-alias refusal, metadata drift and handle balance\n",checks);return 0;
}
