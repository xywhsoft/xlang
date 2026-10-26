/* Private path implementation allows deterministic failed/stale observations.
   The original pairwise implementation is never intercepted. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static HANDLE batchOpen(const wchar_t* path);
static BOOL batchInfo(HANDLE handle,LPBY_HANDLE_FILE_INFORMATION info);
#define XIDE_PATH_BATCH_OPEN batchOpen
#define XIDE_PATH_BATCH_INFO batchInfo
#include "../src/platform/path.c"
static unsigned checks,opens,infos,failInfo,replaceOpen,stopAt,stops;
static const char *replaceTarget,*replaceSource;
static int hookFailed;
static HANDLE batchOpen(const wchar_t* path)
{
 ++opens;if(replaceOpen && opens==replaceOpen){wchar_t *target=xidePathNativeWide(replaceTarget),*source=xidePathNativeWide(replaceSource);
  if(!target || !source || !DeleteFileW(target) || !CreateHardLinkW(target,source,NULL))hookFailed=1;
  free(target);free(source);}
 return openIdentity(path);
}
static BOOL batchInfo(HANDLE handle,LPBY_HANDLE_FILE_INFORMATION info)
{return ++infos==failInfo ? FALSE : GetFileInformationByHandle(handle,info);}
static int stopped(void* user){(void)user;return ++stops>=stopAt;}
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"path_identity_batch_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int oracle(const char* const* paths,size_t count)
{for(size_t i=0;i<count;++i)for(size_t j=0;j<i;++j)if(xidePathEqual(paths[i],paths[j]))return 0;return 1;}
static int writeFile(const char* path)
{wchar_t* native=xidePathNativeWide(path);HANDLE file=native ? CreateFileW(native,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,0,NULL) : INVALID_HANDLE_VALUE;free(native);if(file==INVALID_HANDLE_VALUE)return 0;return CloseHandle(file)!=0;}
static void reset(void){opens=infos=failInfo=replaceOpen=stops=hookFailed=0;}
static int run(void)
{
 char temp[MAX_PATH],directory[MAX_PATH];CHECK(GetTempPathA(sizeof(temp),temp) && GetTempFileNameA(temp,"xpi",0,directory));CHECK(DeleteFileA(directory) && CreateDirectoryA(directory,NULL));
 char names[65][MAX_PATH+32];const char* paths[65];for(unsigned i=0;i<65;++i){snprintf(names[i],sizeof(names[i]),"%s/file-%u.xl",directory,i);paths[i]=names[i];CHECK(writeFile(paths[i]));}
 DWORD before,after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&before));reset();CHECK(xidePathsDistinct(paths,64,NULL,NULL) && oracle(paths,64));CHECK(opens==128 && infos==128);
 CHECK(xidePathsDistinct(NULL,0,NULL,NULL) && !xidePathsDistinct(NULL,1,NULL,NULL));const char* invalid[]={NULL};CHECK(!xidePathsDistinct(invalid,1,NULL,NULL));
 const char* pair[]={paths[0],paths[0]};reset();CHECK(!xidePathsDistinct(pair,2,NULL,NULL) && !oracle(pair,2));
 char alias[MAX_PATH+32];snprintf(alias,sizeof(alias),"%s/alias.xl",directory);CHECK(CreateHardLinkA(alias,paths[0],NULL));pair[1]=alias;CHECK(!xidePathsDistinct(pair,2,NULL,NULL) && !oracle(pair,2));CHECK(DeleteFileA(alias));
 char upper[MAX_PATH+32];strcpy(upper,paths[0]);CharUpperBuffA(upper,(DWORD)strlen(upper));pair[1]=upper;CHECK(xidePathsDistinct(pair,2,NULL,NULL)==oracle(pair,2));
 const char* missing[]={"X:/absent-xlang-case/A.xl","X:/absent-xlang-case/a.xl"};CHECK(xidePathsDistinct(missing,2,NULL,NULL)==oracle(missing,2));missing[1]=missing[0];CHECK(!xidePathsDistinct(missing,2,NULL,NULL));
 const char* dirs[]={directory,temp};CHECK(xidePathsDistinct(dirs,2,NULL,NULL)==oracle(dirs,2));
 reset();failInfo=2;CHECK(xidePathsDistinct(paths,64,NULL,NULL) && oracle(paths,64));CHECK(opens==2 && infos==2);
 reset();replaceOpen=65;replaceTarget=paths[1];replaceSource=paths[0];CHECK(!xidePathsDistinct(paths,64,NULL,NULL) && !hookFailed && !oracle(paths,64));CHECK(opens==66);CHECK(DeleteFileA(paths[1]) && writeFile(paths[1]));
 reset();CHECK(xidePathsDistinct(paths,65,NULL,NULL) && !opens && oracle(paths,65));
 reset();stopAt=1;CHECK(!xidePathsDistinct(paths,64,stopped,NULL) && !opens);reset();stopAt=90;CHECK(!xidePathsDistinct(paths,64,stopped,NULL) && opens==64);reset();stopAt=2085;CHECK(!xidePathsDistinct(paths,64,stopped,NULL));
 for(unsigned i=0;i<30;++i){reset();failInfo=i%3;CHECK(xidePathsDistinct(paths,64,NULL,NULL));}
 CHECK(GetProcessHandleCount(GetCurrentProcess(),&after) && after==before);
 for(unsigned i=0;i<65;++i)CHECK(DeleteFileA(paths[i]));
 CHECK(RemoveDirectoryA(directory));return 1;
}
int main(void){if(!run())return 1;printf("path_identity_batch_test passed: %u checks;original pairwise oracle,64/65 paths,hardlinks/case/missing/directories,failed metadata,replaced names,cancel and handle balance\n",checks);return 0;}
