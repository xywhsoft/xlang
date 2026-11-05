#include "src/api/xl_api_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char* names[]={"tcc_vfs_open","tcc_vfs_close","tcc_vfs_read","tcc_vfs_lseek","tcc_vfs_fopen","tcc_vfs_fclose","tcc_vfs_mount_memory","tcc_vfs_clear_dynamic"};
typedef struct Seen {unsigned count;unsigned mask;} Seen;
static int visit(void* user,const char* name,const void* address)
{
    Seen* seen=user;
    for(unsigned i=0;i<8;++i)if(!strcmp(name,names[i])){
        assert(address && !(seen->mask&(1u<<i)));seen->mask|=1u<<i;++seen->count;
    }
    return 1;
}
int main(int argc,char** argv)
{
    assert(argc==7);int expected=atoi(argv[6]);assert(expected==0 || expected==8);
    XL_ContextOptions options=xlDefaultContextOptions();options.tccDllPath=argv[1];options.tccLibPath=argv[2];options.workDir=argv[3];
    XL_Context* ctx=xlCreateContext(&options);assert(ctx);
    assert(xlAddIncludePath(ctx,argv[4]) && xlAddIncludePath(ctx,argv[5]));
    if(!xlRunScriptString(ctx,"assert 6 * 7 == 42\n")){fprintf(stderr,"%s\n",xlGetLastError(ctx));assert(0);}
    assert(xl__tcc.ready);Seen seen={0};assert(xlApiVisitDefaultRuntimeSymbols(visit,&seen));
    assert(seen.count==(unsigned)expected && seen.mask==(expected ? 255u:0u));
    if(expected){
        assert(xl__tcc.tcc_vfs_mount_memory("__xlvfs__/optional_vfs.txt","abc",3));
        FILE* file=xl__tcc.tcc_vfs_fopen("__xlvfs__/optional_vfs.txt","rb");assert(file);
        assert(fgetc(file)=='a');assert(xl__tcc.tcc_vfs_fclose(file)==0);
        xl__tcc.tcc_vfs_clear_dynamic();
    }
    XL_TccApi saved=xl__tcc;
    xl__tcc.tcc_vfs_open=NULL;xl__tcc.tcc_vfs_close=NULL;xl__tcc.tcc_vfs_read=NULL;xl__tcc.tcc_vfs_lseek=NULL;
    xl__tcc.tcc_vfs_fopen=NULL;xl__tcc.tcc_vfs_fclose=NULL;xl__tcc.tcc_vfs_mount_memory=NULL;xl__tcc.tcc_vfs_clear_dynamic=NULL;
    seen=(Seen){0};assert(xlApiVisitDefaultRuntimeSymbols(visit,&seen));assert(!seen.count && !seen.mask);
    assert(xlRunScriptString(ctx,"assert 40 + 2 == 42\n"));xl__tcc=saved;
    xlDestroyContext(ctx);puts("optional VFS exact provider slots, NULL omission and ordinary libtcc execution: PASS");return 0;
}
