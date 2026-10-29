#include "src/compile/xl6_source_report.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define REQUIRE(c) do { if(!(c)) { fprintf(stderr,"resolution_report_host:%d: %s\n",__LINE__,#c); return 1; } } while(0)
static char* utf8(const wchar_t* w) { int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL); char* p=n ? malloc((size_t)n) : NULL; if(p) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,p,n,NULL,NULL); return p; }
static int writeFile(const wchar_t* path) { HANDLE h=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); DWORD n=0; int ok=h!=INVALID_HANDLE_VALUE && WriteFile(h,"x",1,&n,NULL) && n==1; if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); return ok; }
int wmain(int argc,wchar_t** argv)
{
 XL6_SourceReport report; wchar_t file[4096],dir[4096]; char *path,*directory,*old,*value,*alias; REQUIRE(argc==2);
 REQUIRE(swprintf(file,4096,L"%ls\\候选Ω.xl",argv[1])>0 && swprintf(dir,4096,L"%ls\\regular-predicate",argv[1])>0); path=utf8(file); directory=utf8(dir); REQUIRE(path && directory);
 xl6SourceReportInit(&report); REQUIRE(!xl6SourceReportExists(&report,path) && report.probeCount==1 && xl6SourceReportValidate(&report)); REQUIRE(writeFile(file)); REQUIRE(!xl6SourceReportExists(&report,path) && !xl6SourceReportValidate(&report) && !report.complete); xl6SourceReportFree(&report);
 xl6SourceReportInit(&report); REQUIRE(xl6SourceReportExists(&report,path)); REQUIRE(DeleteFileW(file)); REQUIRE(xl6SourceReportExists(&report,path) && !xl6SourceReportValidate(&report)); xl6SourceReportFree(&report);
 REQUIRE(CreateDirectoryW(dir,NULL)); xl6SourceReportInit(&report); REQUIRE(!xl6SourceReportExists(&report,directory) && xl6SourceReportValidate(&report)); REQUIRE(RemoveDirectoryW(dir) && writeFile(dir)); REQUIRE(!xl6SourceReportExists(&report,directory) && !xl6SourceReportValidate(&report)); xl6SourceReportFree(&report); REQUIRE(DeleteFileW(dir));
 REQUIRE(swprintf(file,4096,L"%ls\\候选ω.XL",argv[1])>0); alias=utf8(file); REQUIRE(alias); xl6SourceReportInit(&report); REQUIRE(!xl6SourceReportExists(&report,path) && !xl6SourceReportExists(&report,alias) && report.probeCount==1 && xl6SourceReportValidate(&report)); xl6SourceReportFree(&report); free(alias);
 xl6SourceReportInit(&report); for(int i=0;i<2048;++i) { char name[8192]; snprintf(name,sizeof(name),"%s/missing-%d.xl",directory,i); REQUIRE(!xl6SourceReportExists(&report,name) && report.complete); } REQUIRE(report.probeCount==2048); REQUIRE(!xl6SourceReportExists(&report,"overflow-resolution-probe.xl") && !report.complete); xl6SourceReportFree(&report);
 old=(char*)xrtEnvGet("XLANG_PACKAGE_PATH"); REQUIRE(xrtEnvRemove("XLANG_PACKAGE_PATH")); xl6SourceReportInit(&report); value=xl6SourceReportPackagePath(&report); REQUIRE(!value && report.packagePathObserved && xl6SourceReportValidate(&report)); REQUIRE(xrtEnvSet("XLANG_PACKAGE_PATH","first")); REQUIRE(!xl6SourceReportPackagePath(&report) && !xl6SourceReportValidate(&report)); xl6SourceReportFree(&report);
 xl6SourceReportInit(&report); value=xl6SourceReportPackagePath(&report); REQUIRE(value && !strcmp(value,"first") && xl6SourceReportValidate(&report)); xrtFree(value); REQUIRE(xrtEnvSet("XLANG_PACKAGE_PATH","second")); value=xl6SourceReportPackagePath(&report); REQUIRE(value && !strcmp(value,"first") && !xl6SourceReportValidate(&report)); xrtFree(value); xl6SourceReportFree(&report);
 REQUIRE(xrtEnvSet("XLANG_PACKAGE_PATH","")); xl6SourceReportInit(&report); value=xl6SourceReportPackagePath(&report); REQUIRE(value && !*value && report.packagePath && xl6SourceReportValidate(&report)); xrtFree(value); REQUIRE(xrtEnvRemove("XLANG_PACKAGE_PATH") && !xl6SourceReportValidate(&report)); xl6SourceReportFree(&report);
 REQUIRE(old ? xrtEnvSet("XLANG_PACKAGE_PATH",old) : xrtEnvRemove("XLANG_PACKAGE_PATH")); xrtFree(old); free(path); free(directory);
 puts("resolution_report_host passed: cached regular-file candidates, creation/deletion/type change, Unicode aliases, 2048 bound, immutable missing/empty/present environment and invalidation."); return 0;
}
