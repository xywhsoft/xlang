/* GCC-built CPU host; executes libtcc JIT memory, never emitted PE files. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static unsigned checks, failures;
static char diagnostics[8192];
static const char *library_root;
static void check(int ok,const char *name)
{
    ++checks;
    if(!ok){++failures;fprintf(stderr,"FAIL %s: %s\n",name,diagnostics);}
}
static void error_sink(void *unused,const char *message)
{
    size_t n=strlen(diagnostics);(void)unused;
    if(n<sizeof(diagnostics)-2)snprintf(diagnostics+n,sizeof(diagnostics)-n,"%s\n",message);
}
static TCCState *state(int gc)
{
    TCCState *s=tcc_new();diagnostics[0]=0;
    tcc_set_error_func(s,NULL,error_sink);tcc_set_lib_path(s,library_root);
    check(tcc_set_options(s,gc?"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=main":"-nostdlib -ffunction-sections -fdata-sections") == 0,"options");
    check(tcc_set_output_type(s,TCC_OUTPUT_MEMORY)==0,"memory output");
    check(tcc_add_symbol(s,"puts",puts)==0,"resident puts only");
    return s;
}
static void relocation_diagnostics(TCCState *s)
{
    int i;unsigned printed=0;
    for(i=1;i<s->nb_sections && printed<12;++i){
        Section *sec=s->sections[i],*relocs;ElfW(Rela)*r,*end;
        if(!sec)continue;relocs=sec->reloc;
        if(!relocs)continue;
        end=(ElfW(Rela)*)(relocs->data+relocs->data_offset);
        for(r=(ElfW(Rela)*)relocs->data;r<end && printed<12;++r){
            unsigned type=ELFW(R_TYPE)(r->r_info),index=ELFW(R_SYM)(r->r_info);ElfW(Sym)*symbol;
            long long difference;
            if(type!=R_X86_64_PC32 && type!=R_X86_64_PLT32)continue;
            symbol=(ElfW(Sym)*)s->symtab->data+index;
            difference=(long long)(symbol->st_value+r->r_addend)-(long long)(sec->sh_addr+r->r_offset);
            if(difference < -2147483648LL || difference > 2147483647LL){
                fprintf(stderr,"RANGE_TARGET section=%s symbol=%s type=%u shndx=%u S=%llx P=%llx delta=%lld\n",sec->name,(char*)s->symtab->link->data+symbol->st_name,type,symbol->st_shndx,(unsigned long long)symbol->st_value,(unsigned long long)(sec->sh_addr+r->r_offset),difference);++printed;
            }
        }
    }
}
static int load_coff_inputs(TCCState *s,const char *client,const char *archive)
{
    int result=tcc_add_file(s,client);check(result==0,"compile client separately before archive");
    if(result<0)return 0;
    result=tcc_add_file(s,archive);check(result==0,"add true GCC COFF archive");
    return result==0;
}
static unsigned arrays(TCCState *s,const char *begin,const char *end,int reverse)
{
    void (**first)(void)=tcc_get_symbol(s,begin),(**last)(void)=tcc_get_symbol(s,end);
    unsigned count=0;
    check(first && last && last>=first,"native init/fini array bounds");
    if(!first || !last || last<first)return 0;
    count=(unsigned)(last-first);
    if(reverse)while(last>first){void(*fn)(void)=*--last;if(fn && (uintptr_t)fn!=(uintptr_t)-1)fn();}
    else while(first<last){void(*fn)(void)=*first++;if(fn && (uintptr_t)fn!=(uintptr_t)-1)fn();}
    return count;
}
static void native_jit(const char *client,const char *archive)
{
    TCCState *s=state(1);int(*entry)(void);int *constructed;
    if(!load_coff_inputs(s,client,archive)){tcc_delete(s);return;}
    {
        int i,text=0,xdata=0,pdata=0;
        for(i=1;i<s->nb_sections;++i){Section *sec=s->sections[i];
            if(!sec || !(sec->sh_flags&SHF_ALLOC)||!sec->data_offset)continue;
            if(strstr(sec->name,".text$_ZNK10CoffInline3getEv.coff."))++text;
            if(strstr(sec->name,".xdata$_ZNK10CoffInline3getEv.coff."))++xdata;
            if(strstr(sec->name,".pdata$_ZNK10CoffInline3getEv.coff."))++pdata;
        }
        check(text==1,"duplicate GNU inline method input coalesces to one body");
        check(xdata==1 && pdata==1,"duplicate GNU inline method input coalesces one unwind pair");
    }
    if(tcc_relocate(s)<0){relocation_diagnostics(s);check(0,"GC JIT final relocation");tcc_delete(s);return;}
    check(1,"GC JIT final relocation");
    check(!tcc_get_symbol(s,"coff_unreachable"),"dead function not exposed");
    check(!tcc_get_symbol(s,"coff_unused_payload"),"unused initialized data collected");
    check(!tcc_get_symbol(s,"coff_unused_bss"),"unused bss collected");
    check(!tcc_get_symbol(s,"coff_inactive"),"unused archive member not extracted");
    entry=tcc_get_symbol(s,"main");constructed=tcc_get_symbol(s,"coff_constructor_value");
    check(entry && constructed && *constructed==0,"native globals before constructors");
    check(arrays(s,"__init_array_start","__init_array_end",0)==1,"one native constructor");
    check(constructed && *constructed==29,"native constructor ran");
    check(entry && entry()==0,"callbacks/double/aggregate/COMDAT execute");
    check(arrays(s,"__fini_array_start","__fini_array_end",1)==1,"one native destructor");
    check(constructed && *constructed==0,"native destructor ran before libtcc delete");
    {
        void *method=tcc_get_symbol(s,"_ZNK10CoffInline3getEv");
#ifdef _WIN64
        DWORD64 image=0;PRUNTIME_FUNCTION fn=method?RtlLookupFunctionEntry((DWORD64)(uintptr_t)method,&image,NULL):NULL;
        check(method && fn && image+fn->BeginAddress<=(DWORD64)(uintptr_t)method && image+fn->EndAddress>(DWORD64)(uintptr_t)method,"GNU inline function live unwind registered after GC");
#else
        check(method!=NULL,"GNU inline method remains live after GC");
#endif
    }
    tcc_delete(s);
    s=state(0);if(load_coff_inputs(s,client,archive))check(tcc_relocate(s)<0 && strstr(diagnostics,"coff_unavailable_dead_dependency"),"without GC dead undefined is rejected");tcc_delete(s);
    s=state(1);
    check(tcc_compile_string(s,"extern int coff_unreachable(void);int main(void){return coff_unreachable();}")==0,"compile live missing client");
    check(tcc_add_file(s,archive)==0,"load live missing archive");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"coff_unavailable_dead_dependency"),"live undefined remains rejected");tcc_delete(s);
}
static TCCState *imports(const char *definition,const char *source)
{
    TCCState *s=state(1);
    check(tcc_add_file(s,definition)==0,"import alias/DATA definition loaded");
    check(tcc_compile_string(s,source)==0,"typed alias client compiled");return s;
}
static void import_jit(const char *definition)
{
    TCCState *s;int(*entry)(void);
    s=imports(definition,"__declspec(dllimport) long coff_abs(long);__declspec(dllimport) double coff_huge;int main(void){return coff_abs(-57)==57 && coff_huge>1e307?0:1;}");
    if(tcc_relocate(s)<0){relocation_diagnostics(s);check(0,"real MSVCRT function alias and DATA relocate");}
    else{check(1,"real MSVCRT function alias and DATA relocate");entry=tcc_get_symbol(s,"main");check(entry && entry()==0,"real MSVCRT alias and DATA values");}
    tcc_delete(s);
    s=imports(definition,"extern double coff_huge;int main(void){return coff_huge>1e307?0:1;}");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"dllimport"),"DATA requires dllimport (no guessed direct data address)");tcc_delete(s);
    s=imports(definition,"__declspec(dllimport) double coff_huge(void);int main(void){return coff_huge()>0;}");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"DATA import"),"DATA cannot be called as a function");tcc_delete(s);
}
static void invalid_definitions(const char *directory)
{
    static const char *invalid[]={"EXPORTS\ncoff_abs\n","LIBRARY msvcrt.dll\nEXPORTS\ncoff_abs=\n","LIBRARY msvcrt.dll\nEXPORTS\ncoff_abs=labs @0\n","LIBRARY msvcrt.dll\nEXPORTS\ncoff_abs=labs NONAME\n","LIBRARY msvcrt.dll\nEXPORTS\ncoff_abs=labs BOGUS\n"};
    unsigned i;char path[2048];FILE *f;TCCState *s;
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){
        snprintf(path,sizeof path,"%s/invalid-%u.def",directory,i);f=fopen(path,"wb");
        check(f!=NULL,"create invalid definition fixture");if(!f)continue;fputs(invalid[i],f);fclose(f);
        s=state(1);check(tcc_add_file(s,path)<0 && strstr(diagnostics,"invalid DLL import definition"),"invalid import definition rejected");tcc_delete(s);
    }
}
int main(int argc,char **argv)
{
    if(argc!=6)return 2;
    library_root=argv[1];native_jit(argv[2],argv[3]);import_jit(argv[4]);invalid_definitions(argv[5]);
    printf("TCC_COFF_API_CHECKS=%u FAILURES=%u\n",checks,failures);return failures?1:0;
}
