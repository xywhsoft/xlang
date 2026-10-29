#include "src/compile/xl6_build_inputs.h"
#include "lib/tcc/tcc_builtin_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
static int checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"check %d failed at line %d: %s\n",checks,__LINE__,#value); return 1; } } while(0)
static int writeBytes(const char* path,const void* data,size_t size) { return xrtFileWriteAll(path,(xbytesview){(bytes)data,size}); }
int main(void)
{
    XL6_BuildInputs r; const unsigned char *a,*b; size_t n,m;
    XL6_BuildInputs* previous; char* text;
    CHECK(writeBytes("binary.dat","a\0b",3));
    xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsRead(&r,"binary.dat",&a,&n)==1 && n==3 && !memcmp(a,"a\0b",3));
    CHECK(writeBytes("binary.dat","new",3));
    CHECK(xl6BuildInputsRead(&r,"./binary.dat",&b,&m)==1 && a==b && m==3 && !memcmp(b,"a\0b",3));
    CHECK(!xl6BuildInputsValidate(&r));
    CHECK(!r.complete && strstr(r.error,"changed")); xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsRead(&r,"missing.h",&a,&n)==0 && r.missingCount==1);
    CHECK(writeBytes("missing.h","#define VALUE 7",15));
    CHECK(xl6BuildInputsRead(&r,"missing.h",&a,&n)==0 && r.missingCount==1);
    CHECK(!xl6BuildInputsValidate(&r)); xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsRead(&r,"binary.dat",&a,&n)==1);
    CHECK(!xl6BuildInputsOutputValid(&r,"binary.dat")); xl6BuildInputsFree(&r);
    CHECK(writeBytes("unicode-中文.h","\xef\xbb\xbfint answer;",14));
    xl6BuildInputsInit(&r); previous=xl6BuildInputsActivate(&r);
    text=xl6BuildInputReadText("unicode-中文.h",&n);
    CHECK(text && n==11 && !strcmp(text,"int answer;")); xrtFree(text);
    CHECK(r.count==1 && r.files[0].size==14);
    CHECK(xl6BuildInputsValidate(&r)); xl6BuildInputsActivate(previous); xl6BuildInputsFree(&r);
    CHECK(writeBytes("nul.txt","a\0b",3));
    xl6BuildInputsInit(&r); previous=xl6BuildInputsActivate(&r);
    CHECK(xl6BuildInputReadText("nul.txt",&n)==NULL && !r.complete);
    xl6BuildInputsActivate(previous); xl6BuildInputsFree(&r);
    CHECK(writeBytes("artifact.exe","private-artifact",16));
    xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsBindArtifact(&r,"artifact.exe"));
    CHECK(xl6BuildInputsWrite(&r,"artifact.exe","receipt.json"));
    CHECK(!xl6BuildInputsWrite(&r,"artifact.exe","artifact.exe")); xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r); CHECK(xl6BuildInputsBindArtifact(&r,"artifact.exe"));
    CHECK(writeBytes("artifact.exe","other-artifact!!",16));
    CHECK(!xl6BuildInputsWrite(&r,"artifact.exe","replaced.json")); xl6BuildInputsFree(&r);
#ifdef _WIN32
    CHECK(writeBytes("identity.dat","same",4));
    xl6BuildInputsInit(&r); CHECK(xl6BuildInputsRead(&r,"identity.dat",&a,&n)==1);
    CHECK(writeBytes("replacement.dat","same",4));
    CHECK(MoveFileExA("replacement.dat","identity.dat",MOVEFILE_REPLACE_EXISTING));
    CHECK(!xl6BuildInputsValidate(&r)); xl6BuildInputsFree(&r);
    CHECK(CreateHardLinkA("alias.dat","identity.dat",NULL));
    xl6BuildInputsInit(&r); CHECK(xl6BuildInputsRead(&r,"identity.dat",&a,&n)==1);
    CHECK(!xl6BuildInputsOutputValid(&r,"alias.dat")); xl6BuildInputsFree(&r);
#endif
    xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsRead(&r,".",&a,&n)==-1 && !r.complete); xl6BuildInputsFree(&r);
    CHECK(writeBytes("empty.dat","",0)); xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsRead(&r,"empty.dat",&a,&n)==1 && a && !n);
    CHECK(xl6BuildInputsValidate(&r)); xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r);
    CHECK(!xl6SourceReportExists(&r.sources,"future-module.xlp"));
    CHECK(!xl6BuildInputsOutputValid(&r,"future-module.xlp")); xl6BuildInputsFree(&r);
    CHECK(writeBytes("bad-encoding.txt","\xff\xfe\x00\x00",4));
    xl6BuildInputsInit(&r); previous=xl6BuildInputsActivate(&r);
    CHECK(xl6BuildInputReadText("bad-encoding.txt",&n)==NULL && !r.complete);
    xl6BuildInputsActivate(previous); xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r);
    CHECK(xl6BuildInputsRead(&r,"unicode-中文.h",&a,&n)==1);
    text=xl6SourceReportReadText(&r.sources,"unicode-中文.h",XL6_SOURCE_REPORT_SOURCE,&n);
    CHECK(text!=NULL); xrtFree(text);
    CHECK(xl6BuildInputsBindArtifact(&r,"artifact.exe"));
    CHECK(xl6BuildInputsWrite(&r,"artifact.exe","deduplicated.json"));
    text=xrtFileReadText("deduplicated.json",XENCODING_UTF8,XUTF_STRICT,&n);
    CHECK(text!=NULL);
    { xvalue* root=xrtJsonParse(xrtStrViewN(text,n));
      CHECK(root && xrtValueCount(xrtValueObjectGet(root,xrtStrView("files")))==1);
      xrtValueRelease(root); }
    xrtFree(text); xl6BuildInputsFree(&r);
#ifdef _WIN32
    { HANDLE file=CreateFileA("oversize.dat",GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
      LARGE_INTEGER length; length.QuadPart=(LONGLONG)XL6_BUILD_INPUT_FILE_BYTES+1;
      CHECK(file!=INVALID_HANDLE_VALUE);
      CHECK(SetFilePointerEx(file,length,NULL,FILE_BEGIN)&&SetEndOfFile(file)); CloseHandle(file);
      xl6BuildInputsInit(&r);
      CHECK(xl6BuildInputsRead(&r,"oversize.dat",&a,&n)==-1 && !r.complete && !r.count && !a && !n);
      xl6BuildInputsFree(&r); }
#endif
    CHECK(writeBytes("stdio.dat","old",3)); xl6BuildInputsInit(&r);
    tcc_vfs_set_disk_reader(xl6BuildInputsRead,&r);
    { FILE* file=tcc_vfs_fopen("stdio.dat","rb"); char body[4]={0};
      CHECK(file && fread(body,1,3,file)==3 && !strcmp(body,"old")); CHECK(tcc_vfs_fclose(file)==0);
      CHECK(writeBytes("stdio.dat","new",3)); file=tcc_vfs_fopen("stdio.dat","rb"); memset(body,0,sizeof(body));
      CHECK(file && fread(body,1,3,file)==3 && !strcmp(body,"old")); CHECK(tcc_vfs_fclose(file)==0);
      CHECK(!xl6BuildInputsValidate(&r)); }
    tcc_vfs_set_disk_reader(NULL,NULL); xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r); tcc_vfs_set_disk_reader(xl6BuildInputsRead,&r);
    { FILE* file=tcc_vfs_fopen("stdio-write.dat","wb");
      CHECK(file && fwrite("write",1,5,file)==5); CHECK(tcc_vfs_fclose(file)==0);
      CHECK(!r.count && !r.missingCount && r.complete); }
    tcc_vfs_set_disk_reader(NULL,NULL); xl6BuildInputsFree(&r);
    /* Exercise each independent resource ceiling without weakening rejection.
     * The byte-counter injection isolates arithmetic from filesystem limits. */
    xl6BuildInputsInit(&r); r.missingBytes=XL6_BUILD_INPUT_PROBE_BYTES;
    CHECK(xl6BuildInputsRead(&r,"byte-budget-missing.h",&a,&n)==-1 && !r.complete);
    CHECK(!r.missingCount && !a && !n && strstr(r.error,"limit"));
    xl6BuildInputsFree(&r);
    xl6BuildInputsInit(&r);
    r.missing=xrtCalloc(XL6_BUILD_INPUT_PROBES,sizeof(*r.missing)); CHECK(r.missing);
    r.missingCapacity=XL6_BUILD_INPUT_PROBES;
    { size_t index; char path[64];
      for(index=0;index<XL6_BUILD_INPUT_PROBES;++index) {
          snprintf(path,sizeof(path),
#ifdef _WIN32
                   "C:/__xl6_count_budget_missing_%u.h",
#else
                   "/__xl6_count_budget_missing_%u.h",
#endif
                   (unsigned)index);
          r.missing[index]=xrtPathAbs(path); CHECK(r.missing[index]);
          r.missingBytes+=strlen(r.missing[index])+1; ++r.missingCount;
      } }
    CHECK(r.missingBytes<XL6_BUILD_INPUT_PROBE_BYTES);
    CHECK(xl6BuildInputsRead(&r,"count-budget-next.h",&a,&n)==-1 && !r.complete);
    CHECK(r.missingCount==XL6_BUILD_INPUT_PROBES && !a && !n && strstr(r.error,"limit"));
    xl6BuildInputsFree(&r);
    printf("Build input snapshot checks: %d passed\n",checks); return 0;
}
