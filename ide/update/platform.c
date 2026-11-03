#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "update.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <io.h>
static HANDLE install_lock=INVALID_HANDLE_VALUE;
static wchar_t* wide(const char* s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);
    wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL;
    if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n); return w;
}
static int utf8(const wchar_t* w,char* s,size_t cap) {return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,s,(int)cap,NULL,NULL)>0;}
static wchar_t* filewide(const char* s){
    wchar_t* w=wide(s);if(!w)return NULL;for(wchar_t* p=w;*p;p++)if(*p==L'/')*p=L'\\';
    if(!wcsncmp(w,L"\\\\?\\",4))return w;
    DWORD n=GetFullPathNameW(w,0,NULL,NULL);wchar_t* full=n?calloc((size_t)n+8,sizeof(wchar_t)):NULL;
    if(!full||!GetFullPathNameW(w,n,full,NULL)){free(w);free(full);return NULL;}free(w);
    size_t len=wcslen(full);w=calloc(len+9,sizeof(wchar_t));if(!w){free(full);return NULL;}
    if(full[0]==L'\\'&&full[1]==L'\\'){wcscpy(w,L"\\\\?\\UNC\\");wcscat(w,full+2);}else{wcscpy(w,L"\\\\?\\");wcscat(w,full);}free(full);return w;
}
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
static int install_lock=-1;
#endif
volatile sig_atomic_t up_cancel;
char up_last_error[2048];
static FILE* update_log;
void up_note(const char* text){if(update_log){fprintf(update_log,"%s\n",text);fflush(update_log);}}
void up_log_stop(void){if(update_log){fclose(update_log);update_log=NULL;}}
void up_log_start(const char* root){up_log_stop();char p[UP_PATH],old[UP_PATH];if(!up_safe_target(root,".xlang-update/update.log")||!up_safe_target(root,".xlang-update/update.previous.log"))return;up_join(p,sizeof(p),root,".xlang-update/update.log");up_join(old,sizeof(old),root,".xlang-update/update.previous.log");if(up_exists(p)&&up_length(p)>1024*1024)up_move(p,old);update_log=up_open(p,"ab");up_note("--- xlang-update session ---");}
int up_error(const char* format,...) {va_list a;va_start(a,format);vsnprintf(up_last_error,sizeof(up_last_error),format,a);va_end(a);fprintf(stderr,"\nERROR: %s\n",up_last_error);up_note(up_last_error);return 0;}
uint64_t up_now(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
#endif
}
FILE* up_open(const char* path,const char* mode) {
    if(up_regular(path)<0)return NULL;
#ifdef _WIN32
    wchar_t* p=filewide(path),*m=wide(mode);FILE* f=p&&m ? _wfopen(p,m) : NULL;free(p);free(m);return f;
#else
    int flags=strchr(mode,'w')?O_WRONLY|O_CREAT|O_TRUNC:strchr(mode,'a')?O_WRONLY|O_CREAT|O_APPEND:O_RDONLY;
    int fd=open(path,flags|O_NOFOLLOW,0600);if(fd<0)return NULL;
    struct stat s;if(fstat(fd,&s)||!S_ISREG(s.st_mode)){close(fd);return NULL;}
    FILE* f=fdopen(fd,mode);if(!f)close(fd);return f;
#endif
}
int up_exists(const char* p) {
#ifdef _WIN32
    wchar_t* w=filewide(p);DWORD a=w?GetFileAttributesW(w):INVALID_FILE_ATTRIBUTES;free(w);return a!=INVALID_FILE_ATTRIBUTES;
#else
    struct stat s;return lstat(p,&s)==0;
#endif
}
/* 1 regular, 2 directory, 0 missing, -1 symlink/reparse/special. */
int up_regular(const char* p) {
#ifdef _WIN32
    wchar_t* w=filewide(p);DWORD a=w?GetFileAttributesW(w):INVALID_FILE_ATTRIBUTES;free(w);
    return a==INVALID_FILE_ATTRIBUTES?0:(a&FILE_ATTRIBUTE_REPARSE_POINT)?-1:(a&FILE_ATTRIBUTE_DIRECTORY)?2:1;
#else
    struct stat s;if(lstat(p,&s))return 0;return S_ISREG(s.st_mode)?1:S_ISDIR(s.st_mode)?2:-1;
#endif
}
int up_mkdir(const char* p) {
    int a=up_regular(p);if(a)return a==2;
#ifdef _WIN32
    wchar_t* w=filewide(p);int ok=w&&CreateDirectoryW(w,NULL);free(w);return ok;
#else
    return mkdir(p,0700)==0;
#endif
}
int up_join(char* d,size_t cap,const char* b,const char* r) {return snprintf(d,cap,"%s/%s",b,r)>0 && strlen(b)+strlen(r)+2<=cap;}
int up_parents(const char* p) {
    char* s=strdup(p);if(!s)return 0;char* slash=NULL;for(char* q=s;*q;q++)if(*q=='/'||*q=='\\')slash=q;
    if(!slash){free(s);return 1;}
    if(slash==s||(slash>s&&slash[-1]==':'))slash[1]=0;else *slash=0;
    int kind=up_regular(s);int ok=kind==2||(kind==0&&up_parents(s)&&up_mkdir(s));free(s);return ok;
}
int up_safe(const char* s) {
    if(!s||!*s||strlen(s)>2048||*s=='/'||*s=='\\')return 0;
    const char* segment=s;
    for(const char* p=s;;p++) {
        unsigned char c=(unsigned char)*p;
        if(c&&((c<32)||c==127||c=='\\'||c==':'||c=='"'||c=='<'||c=='>'||c=='|'||c=='?'||c=='*'))return 0;
        if(!c||c=='/') {
            size_t n=(size_t)(p-segment);if(!n||n>240||segment[n-1]=='.'||segment[n-1]==' ')return 0;
            if((n==1&&*segment=='.')||(n==2&&!memcmp(segment,"..",2)))return 0;
            char name[256];memcpy(name,segment,n);name[n]=0;
            for(size_t j=0;j<n;j++)name[j]=(char)tolower((unsigned char)name[j]);
            char* dot=strchr(name,'.');if(dot)*dot=0;
            if(!strcmp(name,"con")||!strcmp(name,"prn")||!strcmp(name,"aux")||!strcmp(name,"nul")||
               (strlen(name)==4&&(!memcmp(name,"com",3)||!memcmp(name,"lpt",3))&&name[3]>='1'&&name[3]<='9'))return 0;
            if(!c)break;segment=p+1;
        }
    }return 1;
}
int up_managed(const char* p) {
    if(!up_safe(p))return 0;
    /* Explicit ownership, identical on every platform and in the maker. */
    const char* prefixes[]={"data/","include/","notices/","option/defaults/"};
    for(size_t i=0;i<sizeof(prefixes)/sizeof(*prefixes);i++)if(!strncmp(p,prefixes[i],strlen(prefixes[i])))return 1;
    const char* files[]={"xlang_ide.exe","xlang-ls.exe","xge.dll","xl.exe","xl.dll","xlc.dll","xl.def","xlc.def","xlang-update.exe","xlang_ide","xlang-ls","xlang-update","xl","libxl.so","libxlc.so","libxge.so","README.md","QUICKSTART.md","mdo.exe","mdo"};
    for(size_t i=0;i<sizeof(files)/sizeof(*files);i++)if(!strcmp(p,files[i]))return 1;
    return 0;
}
int up_safe_target(const char* root,const char* rel) {
    if(!up_safe(rel))return 0;char p[UP_PATH];if(!up_join(p,sizeof(p),root,rel))return 0;
    /* Reject reparse points/symlinks in the entire in-root path, even if missing later. */
    size_t start=strlen(root)+1;
    for(size_t i=start;;i++)if(p[i]=='/'||!p[i]) {
        char c=p[i];p[i]=0;int kind=up_regular(p);p[i]=c;
        if(kind<0 || (c&&kind==1))return 0;if(!c)break;
    }return 1;
}
int up_remove(const char* p) {
#ifdef _WIN32
    wchar_t* w=filewide(p);int ok=w&&(DeleteFileW(w)||GetLastError()==ERROR_FILE_NOT_FOUND);free(w);return ok;
#else
    return unlink(p)==0||errno==ENOENT;
#endif
}
int up_move(const char* a,const char* b) {
#ifdef _WIN32
    wchar_t* wa=filewide(a),*wb=filewide(b);int ok=wa&&wb&&MoveFileExW(wa,wb,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);free(wa);free(wb);return ok;
#else
    if(rename(a,b))return 0;char* d=strdup(b);char* s=d?strrchr(d,'/'):NULL;if(s){*s=0;int fd=open(d,O_RDONLY|O_DIRECTORY);if(fd>=0){fsync(fd);close(fd);}}free(d);return 1;
#endif
}
int up_sync(FILE* f) {if(fflush(f))return 0;
#ifdef _WIN32
    return _commit(_fileno(f))==0;
#else
    return fsync(fileno(f))==0;
#endif
}
int up_copy(const char* a,const char* b) {
    FILE* in=up_open(a,"rb"),*out=NULL;int ok=0;unsigned char buf[65536];size_t n;
    if(in&&up_parents(b))out=up_open(b,"wb");
    if(out){ok=1;while((n=fread(buf,1,sizeof(buf),in))>0)if(fwrite(buf,1,n,out)!=n){ok=0;break;}if(ferror(in)||!up_sync(out))ok=0;if(fclose(out))ok=0;}
    if(in)fclose(in);return ok;
}
int up_write(const char* p,const void* b,size_t n) {
    char t[UP_PATH];if(snprintf(t,sizeof(t),"%s.new",p)>=(int)sizeof(t)||!up_parents(t))return 0;
    if(up_regular(t)<0)return 0;FILE* f=up_open(t,"wb");if(!f)return 0;int ok=fwrite(b,1,n,f)==n&&up_sync(f);if(fclose(f))ok=0;
    return ok&&up_move(t,p);
}
unsigned char* up_read(const char* p,size_t max,size_t* n) {
    FILE* f=up_open(p,"rb");*n=0;if(!f)return NULL;
    unsigned char* b=malloc(max+1);if(!b){fclose(f);return NULL;}
    *n=fread(b,1,max+1,f);int ok=!ferror(f)&&*n<=max;fclose(f);if(!ok){free(b);return NULL;}b[*n]=0;return b;
}
void up_tohex(const void* b,size_t n,char* out){const unsigned char* s=b;for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",s[i]);out[n*2]=0;}
int up_hex(const char* s,void* b,size_t n) {if(!s||strlen(s)!=2*n)return 0;unsigned char* out=b;for(size_t i=0;i<n;i++){unsigned v=0;for(int j=0;j<2;j++){char c=s[2*i+j];if(c>='0'&&c<='9')v=v*16+c-'0';else if(c>='a'&&c<='f')v=v*16+c-'a'+10;else return 0;}out[i]=(unsigned char)v;}return 1;}
int up_hash(const char* p,char out[65]) {
    FILE* f=up_open(p,"rb");if(!f)return 0;xsha256 ctx;unsigned char b[65536],digest[32];size_t n;xrtSha256Init(&ctx);int ok=1;
    while((n=fread(b,1,sizeof(b),f))>0)if(!xrtSha256Update(&ctx,b,n)){ok=0;break;}
    if(ferror(f)||!xrtSha256Final(&ctx,digest))ok=0;fclose(f);if(ok)up_tohex(digest,32,out);return ok;
}
uint64_t up_length(const char* p){FILE* f=up_open(p,"rb");if(!f)return UINT64_MAX;
#ifdef _WIN32
    int ok=_fseeki64(f,0,SEEK_END)==0;int64_t size=ok?_ftelli64(f):-1;
#else
    int ok=fseeko(f,0,SEEK_END)==0;int64_t size=ok?(int64_t)ftello(f):-1;
#endif
    fclose(f);return size<0?UINT64_MAX:(uint64_t)size;
}
int up_executable(const char* p){
#ifdef _WIN32
    (void)p;return 1;
#else
    struct stat s;return stat(p,&s)==0&&(s.st_mode&0111)!=0;
#endif
}
int up_remove_tree(const char* p) {
    int kind=up_regular(p);if(!kind)return 1;if(kind!=2)return kind==1&&up_remove(p);
#ifdef _WIN32
    char pattern[UP_PATH];up_join(pattern,sizeof(pattern),p,"*");wchar_t* w=filewide(pattern);WIN32_FIND_DATAW d;HANDLE h=w?FindFirstFileW(w,&d):INVALID_HANDLE_VALUE;free(w);
    if(h!=INVALID_HANDLE_VALUE){do{char name[1024],child[UP_PATH];if(!utf8(d.cFileName,name,sizeof(name))){FindClose(h);return 0;}if(!strcmp(name,".")||!strcmp(name,".."))continue;if(!up_join(child,sizeof(child),p,name)||!up_remove_tree(child)){FindClose(h);return 0;}}while(FindNextFileW(h,&d));FindClose(h);}
    w=filewide(p);int ok=w&&RemoveDirectoryW(w);free(w);return ok;
#else
    DIR* dir=opendir(p);if(!dir)return 0;struct dirent* ent;int ok=1;
    while((ent=readdir(dir))){if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,".."))continue;char child[UP_PATH];if(!up_join(child,sizeof(child),p,ent->d_name)||!up_remove_tree(child)){ok=0;break;}}
    closedir(dir);return ok&&rmdir(p)==0;
#endif
}
int up_root(char* out,size_t cap,const char* selected) {
#ifdef _WIN32
    wchar_t path[32768],full[32768];if(selected){wchar_t* w=wide(selected);if(!w)return 0;DWORD n=GetFullPathNameW(w,32768,full,NULL);free(w);if(!n||n>=32768)return 0;}
    else{DWORD n=GetModuleFileNameW(NULL,path,32768);if(!n||n>=32768)return 0;wchar_t* slash=wcsrchr(path,L'\\');if(!slash)return 0;*slash=0;wcscpy(full,path);}return utf8(full,out,cap);
#else
    char path[UP_PATH];if(selected){if(!realpath(selected,path))return 0;}
    else{ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);if(n<=0)return 0;path[n]=0;char* slash=strrchr(path,'/');if(!slash)return 0;*slash=0;}return snprintf(out,cap,"%s",path)<(int)cap;
#endif
}
int up_lock(const char* root) {
    char p[UP_PATH];up_join(p,sizeof(p),root,".xlang-update/lock");if(!up_safe_target(root,".xlang-update/lock")||!up_parents(p))return 0;
#ifdef _WIN32
    wchar_t* w=filewide(p);install_lock=w?CreateFileW(w,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_HIDDEN,NULL):INVALID_HANDLE_VALUE;free(w);return install_lock!=INVALID_HANDLE_VALUE;
#else
    install_lock=open(p,O_CREAT|O_RDWR|O_NOFOLLOW,0600);return install_lock>=0&&flock(install_lock,LOCK_EX|LOCK_NB)==0;
#endif
}
void up_unlock(void) {
#ifdef _WIN32
    if(install_lock!=INVALID_HANDLE_VALUE){CloseHandle(install_lock);install_lock=INVALID_HANDLE_VALUE;}
#else
    if(install_lock>=0){close(install_lock);install_lock=-1;}
#endif
}
int up_wait_parent(unsigned long pid,int seconds) {if(!pid)return 1;
#ifdef _WIN32
    HANDLE p=OpenProcess(SYNCHRONIZE,FALSE,(DWORD)pid);if(!p)return GetLastError()==ERROR_INVALID_PARAMETER;int ok=WaitForSingleObject(p,(DWORD)seconds*1000)==WAIT_OBJECT_0;CloseHandle(p);return ok;
#else
    for(int i=0;i<seconds*10;i++){if(kill((pid_t)pid,0)&&errno==ESRCH)return 1;xrtSleep(100);}return 0;
#endif
}
uint64_t up_space(const char* p) {
#ifdef _WIN32
    wchar_t* w=filewide(p);ULARGE_INTEGER b;int ok=w&&GetDiskFreeSpaceExW(w,&b,NULL,NULL);free(w);return ok?b.QuadPart:0;
#else
    struct statvfs s;return statvfs(p,&s)?0:(uint64_t)s.f_bavail*s.f_frsize;
#endif
}
int up_launch(const char* root) {
    char exe[UP_PATH];up_join(exe,sizeof(exe),root,
#ifdef _WIN32
    "xlang_ide.exe");wchar_t* w=wide(exe),*cwd=wide(root);STARTUPINFOW si={0};PROCESS_INFORMATION pi;si.cb=sizeof(si);int ok=w&&cwd&&CreateProcessW(w,NULL,NULL,NULL,FALSE,0,NULL,cwd,&si,&pi);free(w);free(cwd);if(ok){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}return ok;
#else
    "xlang_ide");pid_t pid=fork();if(pid<0)return 0;if(pid==0){chdir(root);execl(exe,exe,(char*)NULL);_exit(127);}return 1;
#endif
}
int up_self_stage(int argc,char** argv,const char* root) {
#ifdef _WIN32
    wchar_t source[32768],temp[32768],dir[32768],target[32768];char src[UP_PATH],dst[UP_PATH];
    if(!GetModuleFileNameW(NULL,source,32768)||!GetTempPathW(32768,temp)||!GetTempFileNameW(temp,L"xlu",0,dir))return 0;
    DeleteFileW(dir);if(!CreateDirectoryW(dir,NULL))return 0;swprintf(target,32768,L"%ls\\xlang-update.exe",dir);
    if(!utf8(source,src,sizeof(src))||!utf8(target,dst,sizeof(dst))||!up_copy(src,dst))return 0;
    wchar_t* cmd=calloc(65536,sizeof(wchar_t));if(!cmd)return 0;swprintf(cmd,65536,L"\"%ls\" --staged --bootstrap-pid %lu --cleanup-helper \"%ls\"",target,(unsigned long)GetCurrentProcessId(),source);
    int hasRoot=0;
    for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--root"))hasRoot=1;wchar_t* w=wide(argv[i]);if(!w||wcschr(w,L'"')||wcslen(cmd)+wcslen(w)*2+8>=65536){free(w);free(cmd);DeleteFileW(target);RemoveDirectoryW(dir);return 0;}wcscat(cmd,L" \"");wcscat(cmd,w);size_t len=wcslen(w);while(len&&w[len-1]==L'\\'){wcscat(cmd,L"\\");len--;}wcscat(cmd,L"\"");free(w);}
    if(!hasRoot){wchar_t* w=wide(root);if(!w||wcslen(cmd)+wcslen(w)*2+20>=65536){free(w);free(cmd);DeleteFileW(target);RemoveDirectoryW(dir);return 0;}wcscat(cmd,L" --root \"");wcscat(cmd,w);size_t len=wcslen(w);while(len&&w[len-1]==L'\\'){wcscat(cmd,L"\\");len--;}wcscat(cmd,L"\"");free(w);}
    STARTUPINFOW si={0};PROCESS_INFORMATION pi;si.cb=sizeof(si);
    HANDLE streams[3]={GetStdHandle(STD_INPUT_HANDLE),GetStdHandle(STD_OUTPUT_HANDLE),GetStdHandle(STD_ERROR_HANDLE)},nul=INVALID_HANDLE_VALUE;DWORD flags[3]={0};int inherit=1;
    if(!streams[0]||streams[0]==INVALID_HANDLE_VALUE){SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};nul=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);streams[0]=nul;}
    for(int i=0;i<3;i++)if(!streams[i]||streams[i]==INVALID_HANDLE_VALUE||!GetHandleInformation(streams[i],&flags[i]))inherit=0;
    if(inherit){si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=streams[0];si.hStdOutput=streams[1];si.hStdError=streams[2];for(int i=0;i<3;i++)SetHandleInformation(streams[i],HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);}
    int ok=CreateProcessW(target,cmd,NULL,NULL,inherit,0,NULL,NULL,&si,&pi);free(cmd);
    if(inherit)for(int i=0;i<3;i++)SetHandleInformation(streams[i],HANDLE_FLAG_INHERIT,flags[i]&HANDLE_FLAG_INHERIT);
    if(nul!=INVALID_HANDLE_VALUE)CloseHandle(nul);
    if(ok){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 1;}
    DeleteFileW(target);RemoveDirectoryW(dir);return 0;
#else
    (void)argc;(void)argv;(void)root;return 0; /* POSIX permits replacing a running executable by rename. */
#endif
}
int up_needs_stage(const char* root){
#ifdef _WIN32
    char self[UP_PATH];if(!up_root(self,sizeof(self),NULL))return 1;
    wchar_t* a=filewide(self),*b=filewide(root);HANDLE x=a?CreateFileW(a,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL):INVALID_HANDLE_VALUE;
    HANDLE y=b?CreateFileW(b,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL):INVALID_HANDLE_VALUE;free(a);free(b);
    BY_HANDLE_FILE_INFORMATION one={0},two={0};int same=1;
    if(x!=INVALID_HANDLE_VALUE&&y!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(x,&one)&&GetFileInformationByHandle(y,&two))same=one.dwVolumeSerialNumber==two.dwVolumeSerialNumber&&one.nFileIndexHigh==two.nFileIndexHigh&&one.nFileIndexLow==two.nFileIndexLow;
    if(x!=INVALID_HANDLE_VALUE)CloseHandle(x);if(y!=INVALID_HANDLE_VALUE)CloseHandle(y);return same;
#else
    (void)root;return 0;
#endif
}
/* Stable bootstrap protocol: cleanup deletes one known temporary executable
   and its empty directory only, never a recursively selected directory. */
int up_cleanup_stage(const char* executable,unsigned long pid){
#ifdef _WIN32
    wchar_t temp[32768],full[32768];wchar_t* input=wide(executable);
    if(!input||!GetTempPathW(32768,temp)||!GetFullPathNameW(input,32768,full,NULL)){free(input);return 0;}free(input);
    size_t n=wcslen(temp);if(_wcsnicmp(full,temp,n))return 0;wchar_t* parent=full+n,*slash=wcschr(parent,L'\\');
    if(!slash||slash-parent!=11||_wcsnicmp(parent,L"xlu",3)||_wcsnicmp(parent+7,L".tmp",4)||wcscmp(slash+1,L"xlang-update.exe"))return 0;
    for(int i=3;i<7;i++)if(!((parent[i]>=L'0'&&parent[i]<=L'9')||(parent[i]>=L'A'&&parent[i]<=L'F')||(parent[i]>=L'a'&&parent[i]<=L'f')))return 0;
    char file[UP_PATH],directory[UP_PATH];if(!utf8(full,file,sizeof(file)))return 0;*slash=0;if(!utf8(full,directory,sizeof(directory))||up_regular(directory)!=2||up_regular(file)!=1||!pid||pid==GetCurrentProcessId())return 0;
    if(!up_wait_parent(pid,120))return 0;
    for(int attempt=0;attempt<30;attempt++){if(up_remove(file)){wchar_t* dir=filewide(directory);int ok=dir&&RemoveDirectoryW(dir);free(dir);return ok;}Sleep(100);}return 0;
#else
    (void)executable;(void)pid;return 1;
#endif
}
void up_schedule_cleanup(const char* helper){
#ifdef _WIN32
    if(!helper||!*helper)return;wchar_t self[32768];if(!GetModuleFileNameW(NULL,self,32768))return;
    wchar_t* exe=wide(helper),*cmd=calloc(65536,sizeof(wchar_t));if(!exe||!cmd){free(exe);free(cmd);return;}
    swprintf(cmd,65536,L"\"%ls\" --cleanup-stage \"%ls\" --wait-pid %lu",exe,self,(unsigned long)GetCurrentProcessId());STARTUPINFOW si={0};PROCESS_INFORMATION pi;si.cb=sizeof(si);
    if(CreateProcessW(exe,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}free(exe);free(cmd);
#else
    (void)helper;
#endif
}
