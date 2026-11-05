/* CPU-only libtcc final-link tests. The fixture itself is built by GCC.
   Generated PE files are inspected, never executed. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static int checks, failures;
static char diagnostics[8192];
static const char *library_root;
static void check(int ok, const char *name) {
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL %s: %s\n", name, diagnostics); }
}
static void error_sink(void *u, const char *s) {
    size_t n = strlen(diagnostics);
    (void)u;
    if (n < sizeof diagnostics - 2)
        snprintf(diagnostics + n, sizeof diagnostics - n, "%s\n", s);
}
static TCCState *make_state(int output, const char *options) {
    TCCState *s = tcc_new();
    diagnostics[0] = 0;
    tcc_set_error_func(s, NULL, error_sink);
    tcc_set_lib_path(s, library_root);
    check(tcc_set_options(s, options) == 0, "options accepted");
    check(tcc_set_output_type(s, output) == 0, "output selected");
    return s;
}
static unsigned long allocated(TCCState *s) {
    int i; unsigned long n = 0;
    for(i = 1; i < s->nb_sections; ++i)
        if(s->sections[i]->sh_flags & SHF_ALLOC)n += s->sections[i]->data_offset;
    return n;
}
static const char *program =
    "extern int absent_dead_symbol(void);\n"
    "static int dead_function(void){return absent_dead_symbol();}\n"
    "static const unsigned char dead_blob[16384]={1,2,3};\n"
    "static int dead_bss[8192];\n"
    "static int(*dead_callback)(void)=absent_dead_symbol;\n"
    "static int count;\n"
    "static int callback(void){return count+31;}\n"
    "struct C {int(*call)(void); const char* label;};\n"
    "static const struct C callbacks={callback,\"retained callback\"};\n"
    "__attribute__((constructor)) static void setup(void){count=6;}\n"
    "__attribute__((destructor)) static void finish(void){count=0;}\n"
    "int entry(void){return callbacks.call()+callbacks.label[0]-'r';}\n"
    "int public_late_query(void){return 17;}\n"
    "__attribute__((visibility(\"hidden\"))) int hidden_late_query(void){return 18;}\n";
static void test_jit(int explicit_roots) {
    TCCState *s = make_state(TCC_OUTPUT_MEMORY, explicit_roots ?
        "-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=entry" :
        "-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections");
    int (*entry)(void); void (**a)(void),(**z)(void);
    unsigned long before;
    check(tcc_compile_string(s, program)==0,"JIT source compiled");
    before=allocated(s);
    check(tcc_relocate(s)==0,"JIT dead undefined discarded before lookup");
    entry=tcc_get_symbol(s,"entry");
    a=tcc_get_symbol(s,"__init_array_start");z=tcc_get_symbol(s,"__init_array_end");
    check(entry && a && z && z-a==1,"entry and constructor retained");
    if(a && z)while(a<z)(*a++)();
    check(entry && entry()==37,"callback record/data/constructor execute");
#ifdef _WIN64
    if(entry) {
        DWORD64 image=0; PRUNTIME_FUNCTION p=RtlLookupFunctionEntry((DWORD64)(uintptr_t)entry,&image,NULL);
        check(p && image+p->BeginAddress<=(DWORD64)(uintptr_t)entry && image+p->EndAddress>(DWORD64)(uintptr_t)entry,"JIT registered sorted per-function unwind");
    }
#endif
    check(!tcc_get_symbol(s,"dead_function"),"dead function is not exposed");
    check(explicit_roots ? !tcc_get_symbol(s,"public_late_query") : tcc_get_symbol(s,"public_late_query")!=NULL,"JIT public lookup root policy");
    check(explicit_roots ? !tcc_get_symbol(s,"hidden_late_query") : tcc_get_symbol(s,"hidden_late_query")!=NULL,"JIT late hidden global lookup preserved unless explicit roots");
    a=tcc_get_symbol(s,"__fini_array_start");z=tcc_get_symbol(s,"__fini_array_end");
    check(a && z && z-a==1,"destructor retained");
    if(a && z)while(a<z)(*a++)();
    check(entry && entry()==31,"destructor executes retained graph");
    check(before>16384+8192*4,"independent object storage emitted before GC");
    tcc_delete(s);
}
static void test_errors(void) {
    TCCState*s=make_state(TCC_OUTPUT_MEMORY,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=entry");
    check(tcc_compile_string(s,"extern int absent_live(void);int entry(void){return absent_live();}")==0,"live missing source compiled");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"absent_live"),"live undefined still fails");tcc_delete(s);
    s=make_state(TCC_OUTPUT_MEMORY,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--no-gc-sections");
    check(!s->gc_sections,"explicit no-gc accepted");
    check(tcc_compile_string(s,program)==0,"disabled GC source compiled");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"absent_dead_symbol"),"disabled GC does not ignore undefined");tcc_delete(s);
    s=make_state(TCC_OUTPUT_MEMORY,"-nostdlib");
    check(!s->function_sections && !s->data_sections && !s->gc_sections,"all flags default off");tcc_delete(s);
}
static void test_objects(const char *directory) {
    TCCState*s;char object[2048], image[2048];unsigned long before,after;
    snprintf(object,sizeof object,"%s/split.o",directory);snprintf(image,sizeof image,"%s/linked-native.bin",directory);
    s=make_state(TCC_OUTPUT_OBJ,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections");
    check(tcc_compile_string(s,program)==0,"split object source compiled");
    check(tcc_output_file(s,object)==0,"relocatable output preserves all source sections");
    check(!s->gc_sections_done,"object-only is not garbage collected");tcc_delete(s);
    s=make_state(TCC_OUTPUT_EXE,"-nostdlib -Wl,--gc-sections,--print-gc-sections,-e,entry");
    check(tcc_add_file(s,object)==0,"split object reloaded");before=allocated(s);
    check(tcc_output_file(s,image)==0,"AOT discarded dead unresolved graph before imports");after=allocated(s);
    check(before>after+16000,"AOT image loses dead large data");
    check(!strstr(diagnostics,"absent_dead_symbol"),"AOT dead undefined absent from diagnostics");
    printf("AOT_ALLOCATED_BEFORE=%lu AFTER=%lu\n",before,after);tcc_delete(s);
    s=make_state(TCC_OUTPUT_DLL,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,-e,entry");
    check(tcc_compile_string(s,"int entry(void){return 0;} "
#ifdef TCC_TARGET_PE
        "__attribute__((dllexport)) "
#endif
        "int exported_api(void){return 89;} int native_global(void){return 5;} static int hidden_dead(void){return 7;}")==0,"shared source compiled");
    snprintf(image,sizeof image,"%s/shared-native.bin",directory);
    check(tcc_output_file(s,image)==0,"shared output linked");
    {ElfW(Sym)*sym;int found=0;
     for_each_elem(s->symtab,1,sym,ElfW(Sym)) {
       if(!strcmp((char*)s->symtab->link->data+sym->st_name,"exported_api")
          && sym->st_shndx!=SHN_ABS && ELFW(ST_BIND)(sym->st_info)!=STB_LOCAL)found=1;
     }
     check(found,"shared visible API retained");}
#ifdef TCC_TARGET_PE
    {ElfW(Sym)*sym;int dead=0;
     for_each_elem(s->symtab,1,sym,ElfW(Sym))
       if(!strcmp((char*)s->symtab->link->data+sym->st_name,"native_global")
          && sym->st_shndx==SHN_ABS && !sym->st_value)dead=1;
     check(dead,"PE DLL unexported native global can be collected");}
#endif
#ifndef TCC_TARGET_PE
    {ElfW(Sym)*sym;addr_t begin=0,end=0;Section*eh=have_section(s,".eh_frame");
     for_each_elem(s->symtab,1,sym,ElfW(Sym)) {
       const char*n=(char*)s->symtab->link->data+sym->st_name;
       if(!strcmp(n,"__start_eh_frame"))begin=sym->st_value;
       if(!strcmp(n,"__stop_eh_frame"))end=sym->st_value;
     }
     check(eh && begin && end-begin==eh->data_offset,"compacted unwind section boundary symbols corrected");}
#endif
    tcc_delete(s);
}
static void test_export_dynamic(const char *directory) {
    TCCState*s=make_state(TCC_OUTPUT_DLL,"-nostdlib -ffunction-sections -fdata-sections -rdynamic -Wl,--gc-sections,-e,entry");
    char image[2048];ElfW(Sym)*sym;int found=0;
    check(tcc_compile_string(s,"int entry(void){return 0;} int native_global(void){return 5;}")==0,"export-dynamic source compiled");
    snprintf(image,sizeof image,"%s/export-all-native.bin",directory);
    check(tcc_output_file(s,image)==0,"export-dynamic shared image linked");
    for_each_elem(s->symtab,1,sym,ElfW(Sym))
      if(!strcmp((char*)s->symtab->link->data+sym->st_name,"native_global")
         && sym->st_shndx!=SHN_ABS && ELFW(ST_BIND)(sym->st_info)!=STB_LOCAL)found=1;
    check(found,"explicit export-dynamic retains native global");tcc_delete(s);
}

static void test_registry_roots(void) {
    TCCState*s=make_state(TCC_OUTPUT_MEMORY,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=entry");
    int(*entry)(void);
    check(tcc_compile_string(s,"static int callback(void){return 63;} __attribute__((section(\"hooks\"))) static int(*registry[])(void)={callback}; extern int(*__start_hooks[])(void); int entry(void){return __start_hooks[0]();}")==0,"custom registration table compiled");
    check(tcc_relocate(s)==0,"live start-of-section root traversed");
    entry=tcc_get_symbol(s,"entry");check(entry && entry()==63,"section registry retains callback");
    tcc_delete(s);
}

static void test_same_static_names(const char *directory) {
    TCCState *s;char a[2048],b[2048],image[2048];
    snprintf(a,sizeof a,"%s/unit-a.o",directory);snprintf(b,sizeof b,"%s/unit-b.o",directory);
    s=make_state(TCC_OUTPUT_OBJ,"-nostdlib -ffunction-sections -fdata-sections");
    check(tcc_compile_string(s,"extern int absent_in_other_unit(void); static int helper(void){return absent_in_other_unit();} int unused_public(void){return helper();}")==0,"first same-name static helper compiled");
    check(tcc_output_file(s,a)==0,"first independent object emitted");tcc_delete(s);
    s=make_state(TCC_OUTPUT_OBJ,"-nostdlib -ffunction-sections -fdata-sections");
    check(tcc_compile_string(s,"static int helper(void){return 37;} int entry(void){return helper();}")==0,"second same-name helper compiled");
    check(tcc_output_file(s,b)==0,"second independent object emitted");tcc_delete(s);
    s=make_state(TCC_OUTPUT_EXE,"-nostdlib -Wl,--gc-sections,-e,entry");
    check(tcc_add_file(s,a)==0 && tcc_add_file(s,b)==0,"independent object contributions loaded");
    snprintf(image,sizeof image,"%s/same-name-native.bin",directory);
    check(tcc_output_file(s,image)==0,"unrelated same-name static does not resurrect dead undefined");
    tcc_delete(s);
}

static void test_debug_objects(const char *directory) {
    const char *modes[]={"-g", "-gdwarf-2", "-gdwarf-4", "-gdwarf-5"};
    const char *names[]={"stabs", "dwarf2", "dwarf4", "dwarf5"};
    int mode;
    for(mode=0;mode<4;++mode) {
      char options[512],image[2048];TCCState*s;ElfW(Sym)*sym;addr_t aa=0,bb=0;int dead=0;
      snprintf(options,sizeof options,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,-e,entry %s",modes[mode]);
      s=make_state(TCC_OUTPUT_EXE,options);
      check(tcc_compile_string(s,
        "#line 100 \"gc_debug.c\"\nint alpha(void) {\n return 21;\n}\n"
        "#line 110 \"gc_debug.c\"\nint beta(void) {\n return 22;\n}\n"
        "#line 130 \"gc_debug.c\"\nstatic int dead_debug(void) {\n return 0;\n}\n"
        "#line 140 \"gc_debug.c\"\nint entry(void) {return alpha()+beta();}\n")==0,"split debug source compiled");
      snprintf(image,sizeof image,"%s/debug-%s-native.bin",directory,names[mode]);
      check(tcc_output_file(s,image)==0,"split debug image linked with GC");
      for_each_elem(s->symtab,1,sym,ElfW(Sym)) {
        const char*n=(char*)s->symtab->link->data+sym->st_name;
        if(!strcmp(n,"alpha"))aa=sym->st_value+sym->st_size-2;
        if(!strcmp(n,"beta"))bb=sym->st_value+sym->st_size-2;
        if(!strcmp(n,"dead_debug") && sym->st_shndx==SHN_ABS && !sym->st_value)dead=1;
      }
      check(aa && bb && aa!=bb,"debug functions have independent code addresses");
      check(dead,"debug metadata does not resurrect dead function");
      printf("DEBUG_QUERY %s 0x%llx 0x%llx\n",names[mode],(unsigned long long)aa,(unsigned long long)bb);
#ifdef CONFIG_TCC_BACKTRACE
      {rt_context rc;bt_info bi;Section*line=have_section(s,mode?".debug_line":".stab");
       Section*str=have_section(s,mode?".debug_line_str":".stabstr");
       memset(&rc,0,sizeof rc);rc.dwarf=mode!=0;
       if(line){rc.dwarf_line=line->data;rc.dwarf_line_end=line->data+line->data_offset;}
       if(str)rc.dwarf_line_str=str->data;
       rc.esym_start=(ElfW(Sym)*)s->symtab->data;rc.esym_end=(ElfW(Sym)*)(s->symtab->data+s->symtab->data_offset);rc.elf_str=(char*)s->symtab->link->data;
       memset(&bi,0,sizeof bi);if(mode)rt_printline_dwarf(&rc,aa,&bi);else rt_printline(&rc,aa,&bi);
       check(strstr(bi.file,"gc_debug.c") && bi.line==101,"native backtrace reader resolves first split function");
       memset(&bi,0,sizeof bi);if(mode)rt_printline_dwarf(&rc,bb,&bi);else rt_printline(&rc,bb,&bi);
       check(strstr(bi.file,"gc_debug.c") && bi.line==111,"native backtrace reader resets between split functions");
      }
#endif
      tcc_delete(s);
    }
}
static void test_file_size_pair(const char *directory) {
    int gc;
    for(gc=0;gc<2;++gc) {
      TCCState*s;char options[512],image[2048];
      snprintf(options,sizeof options,"-nostdlib -ffunction-sections -fdata-sections -Wl,-e,entry %s",gc?"-Wl,--gc-sections":"");
      s=make_state(TCC_OUTPUT_EXE,options);
      check(tcc_compile_string(s,program)==0,"size comparison source compiled");
      check(tcc_compile_string(s,"int absent_dead_symbol(void){return 99;}")==0,"size comparison dead unresolved is explicitly defined in both controls");
      snprintf(image,sizeof image,"%s/size-%s-native.bin",directory,gc?"gc":"plain");
      check(tcc_output_file(s,image)==0,"same source AOT size comparison linked");
      tcc_delete(s);
    }
}

static void test_limits(void) {
    TCCState*s=make_state(TCC_OUTPUT_MEMORY,"-nostdlib");
    int old=s->nb_sections;
    s->nb_sections=SHN_LORESERVE;
    check(!new_section(s,".over-limit",SHT_PROGBITS,SHF_ALLOC),"section index guard returns failure without abort");
    check(s->nb_sections==SHN_LORESERVE,"section index guard leaves state count unchanged");
    check(tcc_set_output_type(s,TCC_OUTPUT_EXE)<0,"output metadata rejects unrepresentable section indices");
    s->nb_sections=old;tcc_delete(s);
}

int main(int argc,char**argv) {
    if(argc!=3)return 2;library_root=argv[1];
    check(tcc_native_link_features()==15u,"feature query advertises all four implemented options");
    test_jit(0);test_jit(1);test_errors();test_objects(argv[2]);test_export_dynamic(argv[2]);test_same_static_names(argv[2]);test_registry_roots();test_debug_objects(argv[2]);test_limits();test_file_size_pair(argv[2]);
    printf("TCC_GC_CHECKS=%d FAILURES=%d\n",checks,failures);
    return failures?1:0;
}
