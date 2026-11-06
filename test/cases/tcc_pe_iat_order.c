/* GCC-built host includes production libtcc. Emitted PE files run in the tool. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static unsigned checks, failures;
static char diagnostics[8192];
static const char *distribution;
static void check(int ok, const char *what)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr,"FAIL %s: %s\n",what,diagnostics); }
}
static void error_sink(void *user, const char *text)
{
    size_t n=strlen(diagnostics); (void)user;
    if(n<sizeof(diagnostics)-2)snprintf(diagnostics+n,sizeof(diagnostics)-n,"%s\n",text);
}
static TCCState *state(int output, int gc)
{
    TCCState *s=tcc_new();diagnostics[0]=0;
    tcc_set_lib_path(s,distribution);tcc_set_error_func(s,NULL,error_sink);
    check(tcc_set_options(s,gc ? "-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--entry=main,--undefined=main" : "-nostdlib -ffunction-sections -fdata-sections -Wl,--entry=main")==0,"options");
    check(tcc_set_output_type(s,output)==0,"output kind");return s;
}
static const char *client=
    "void *TlsGetValue(unsigned long);void *TlsQuery(unsigned long);"
    "unsigned long TlsAlloc(void);int TlsSetValue(unsigned long,void*);int TlsFree(unsigned long);"
    "void *tcc_iat_native(unsigned long);"
    "int main(void){unsigned long n=TlsAlloc();int result;void*p=(void*)0x12345;"
    "if(n==0xffffffffUL)return 11;if(!TlsSetValue(n,p))return 12;"
    "result=TlsGetValue(n)==p && tcc_iat_native(n)==p && TlsQuery(n)==p;"
    "return TlsFree(n) && result ? 0:13;}";
static void mixed(const char *object,const char *definition,const char *out)
{
    int gc,order,output;
    for(gc=0;gc<2;++gc)for(order=0;order<2;++order)for(output=0;output<2;++output){
        TCCState *s=state(output?TCC_OUTPUT_EXE:TCC_OUTPUT_MEMORY,gc);int result;
        if(order)check(tcc_compile_string(s,client)==0,"plain reference first");
        check(tcc_add_file(s,object)==0,"real GCC COFF IAT input");
        if(!order)check(tcc_compile_string(s,client)==0,"IAT reference first");
        check(tcc_add_file(s,definition)==0,"typed actual kernel32 imports/alias");
        if(output){char path[2048];snprintf(path,sizeof(path),"%s/gc%d-order%d.exe",out,gc,order);
            check(tcc_output_file(s,path)==0,"PE link both orders");
        }else{
            result=tcc_relocate(s);check(result==0,"PE_RUN JIT both orders");
            if(result==0){int(*entry)(void)=tcc_get_symbol(s,"main");
                check(entry && entry()==0,"real IAT/plain/alias calls and OS TLS slot agree");}
        }
        tcc_delete(s);
    }
}
static void zero_thunk(const char *definition)
{
    TCCState *s1=state(TCC_OUTPUT_EXE,0);struct pe_info pe={0};int a,b;
    check(tcc_compile_string(s1,"int stricmp(const char*,const char*);int _stricmp(const char*,const char*);void* refs[]={stricmp,_stricmp};")==0,"zero-offset declarations");
    check(tcc_add_file(s1,definition)==0,"shared export definition");
    check(text_section->data_offset==0,"canonical text initially empty");
    pe.s1=s1;pe.type=PE_EXE;check(pe_check_symbols(&pe)==0,"zero-offset imports checked");
    a=find_elf_sym(s1->symtab,"stricmp");b=find_elf_sym(s1->symtab,"_stricmp");
    {ElfW(Sym)*syms=(ElfW(Sym)*)s1->symtab->data;
        check(a && b && syms[a].st_value==0 && syms[b].st_value==0,"two spellings share valid thunk at offset zero");}
    check(text_section->data_offset==8,"one AMD64 thunk rather than duplicate zero sentinel");
    check(pe.imp_count==1 && pe.imp_info[0]->sym_count==1,"single actual import");
    pe_free_imports(&pe);tcc_delete(s1);
}
static void data_and_gc(const char *definition)
{
    TCCState*s=state(TCC_OUTPUT_MEMORY,1);int result;
    check(tcc_add_file(s,definition)==0,"DATA alias definition");
    check(tcc_compile_string(s,"__declspec(dllimport) double huge_alias;int main(void){return huge_alias>1e307?0:1;}")==0,"DATA alias client");
    result=tcc_relocate(s);check(result==0,"DATA alias relocation");
    if(!result){int(*entry)(void)=tcc_get_symbol(s,"main");check(entry && entry()==0,"actual MSVCRT DATA address preserved");}tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY,1);check(tcc_add_file(s,definition)==0,"DATA negative definition");
    check(tcc_compile_string(s,"double huge_alias(void);int main(void){return huge_alias()==0;}")==0,"function versus DATA negative client");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"DATA import"),"function cannot resolve to DATA");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY,1);
    check(tcc_compile_string(s,"extern int unavailable_iat_dead(void);static int dead(void){return unavailable_iat_dead();}int main(void){return 0;}")==0,"dead missing import client");
    check(tcc_relocate(s)==0,"GC still discards dead missing import");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY,0);
    check(tcc_compile_string(s,"extern int unavailable_iat_dead(void);static int dead(void){return unavailable_iat_dead();}int main(void){return 0;}")==0,"noGC missing import client");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"unavailable_iat_dead"),"noGC still rejects unavailable import");tcc_delete(s);
}
int main(int argc,char**argv)
{
    if(argc!=7)return 2;distribution=argv[1];mixed(argv[2],argv[3],argv[6]);zero_thunk(argv[4]);data_and_gc(argv[5]);
    printf("TCC_PE_IAT_CHECKS=%u FAILURES=%u\n",checks,failures);return failures?1:0;
}
