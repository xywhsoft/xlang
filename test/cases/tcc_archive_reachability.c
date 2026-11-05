/* GCC-built CPU libtcc archive-selection test; no emitted PE files. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"
static int checks, failures;
static const char *library_root;
static char diagnostics[8192];
static void sink(void*u,const char*message){size_t n=strlen(diagnostics);(void)u;if(n<sizeof diagnostics-2)snprintf(diagnostics+n,sizeof diagnostics-n,"%s\n",message);}
static void check(int ok,const char*message){checks++;if(!ok){failures++;fprintf(stderr,"FAIL %s: %s\n",message,diagnostics);}}
static TCCState* state(int output,int roots,int gc){TCCState*s=tcc_new();diagnostics[0]=0;tcc_set_error_func(s,NULL,sink);tcc_set_lib_path(s,library_root);char options[512];snprintf(options,sizeof options,"-nostdlib -ffunction-sections -fdata-sections %s %s",gc?"-Wl,--gc-sections":"",roots?"-Wl,--undefined=entry":"");check(tcc_set_options(s,options)==0,"options");check(tcc_set_output_type(s,output)==0,"output");return s;}
static ElfW(Sym)* symbol(TCCState*s,const char*name){unsigned n=find_elf_sym(s->symtab,name);return n?((ElfW(Sym)*)s->symtab->data)+n:NULL;}
static uint64_t hash_bytes(uint64_t h,const void*p,size_t n){const unsigned char*b=p;while(n--){h^=*b++;h*=UINT64_C(1099511628211);}return h;}
static uint64_t hash_state(TCCState*s){uint64_t h=UINT64_C(1469598103934665603);h=hash_bytes(h,&s->gc_sections_done,sizeof s->gc_sections_done);h=hash_bytes(h,&s->nb_sections,sizeof s->nb_sections);for(int i=1;i<s->nb_sections;i++){Section*sec=s->sections[i];h=hash_bytes(h,sec,sizeof *sec);if(sec->data)h=hash_bytes(h,sec->data,sec->data_offset);}if(s->symtab->hash){Section*sec=s->symtab->hash;h=hash_bytes(h,sec,sizeof *sec);h=hash_bytes(h,sec->data,sec->data_offset);}return h;}
static void add(TCCState*s,const char*base,const char*name){char p[2048];snprintf(p,sizeof p,"%s/%s",base,name);check(tcc_add_file(s,p)==0,name);}
static void test_query(const char*base){
 TCCState*s=state(TCC_OUTPUT_MEMORY,1,1);add(s,base,"primary.o");
 uint64_t before=hash_state(s);unsigned char*used=NULL;int count=0;
 check(tcc_gc_live_undefined(s,&used,&count)==0,"query succeeds");
 ElfW(Sym)*a=symbol(s,"live_arch"),*b=symbol(s,"dead_arch");
 check(a && a->st_shndx==SHN_UNDEF && used[a-(ElfW(Sym)*)s->symtab->data],"live undefined is selected");
 check(b && b->st_shndx==SHN_UNDEF && !used[b-(ElfW(Sym)*)s->symtab->data],"dead undefined is excluded without tombstoning");
 check(hash_state(s)==before && !s->gc_sections_done,"query preserves sections/bytes/relocs/symbols/hash/owner metadata");
 tcc_free(used);used=NULL;check(tcc_gc_live_undefined(s,&used,&count)==0 && hash_state(s)==before,"repeat query is non-mutating");tcc_free(used);tcc_delete(s);
}
static void test_filtered(const char*base,int archive_root){
 TCCState*s=state(TCC_OUTPUT_MEMORY,1,1);
 if(!archive_root)add(s,base,"primary.o");
 add(s,base,archive_root?"all.a":"dependencies.a");
 check(symbol(s,"live_arch") && symbol(s,"live_arch")->st_shndx!=SHN_UNDEF,"reachable archive member loaded");
 check(symbol(s,"next_arch") && symbol(s,"next_arch")->st_shndx!=SHN_UNDEF,"transitive callback data dependency loaded");
 check(symbol(s,"ctor_arch") && symbol(s,"ctor_arch")->st_shndx!=SHN_UNDEF,"new constructor root triggers another archive pass");
 check(symbol(s,"dead_arch") && symbol(s,"dead_arch")->st_shndx==SHN_UNDEF,"dead primary call does not load TLS/callback archive member");
 ElfW(Sym)*dead=symbol(s,"dead_primary");check(dead && dead->st_shndx<SHN_LORESERVE && s->sections[dead->st_shndx]->data_offset>0,"archive analysis has not pruned dead primary code prematurely");
 check(!s->gc_sections_done,"final GC remains pending until relocation");
 check(tcc_relocate(s)==0,"public JIT succeeds without dead member TLS constructor");
 int(*entry)(void)=tcc_get_symbol(s,"entry");void(**a)(void)=tcc_get_symbol(s,"__init_array_start"),(**z)(void)=tcc_get_symbol(s,"__init_array_end");
 check(a && z && z-a==1,"selected member constructor is retained");if(a&&z)while(a<z)(*a++)();
 check(entry && entry()==43,"selected constructor and callback data dependency execute");tcc_delete(s);
}
static void test_legacy(const char*base,int output,int roots,int gc){
 TCCState*s=state(output,roots,gc);add(s,base,"primary.o");add(s,base,"dependencies.a");
 ElfW(Sym) *b=symbol(s,"dead_arch");check(b && b->st_shndx!=SHN_UNDEF,"ordinary JIT/noGC/AOT archive selection preserves existing behavior");tcc_delete(s);
}
static void test_qualified_roots(const char*base){
 TCCState*s=state(TCC_OUTPUT_MEMORY,1,1);tcc_set_options(s,"-rdynamic");add(s,base,"primary.o");add(s,base,"dependencies.a");
 check(symbol(s,"dead_arch") && symbol(s,"dead_arch")->st_shndx!=SHN_UNDEF,"rdynamic public definition roots still load their archive dependencies");tcc_delete(s);
 s=state(TCC_OUTPUT_MEMORY,1,1);tcc_set_options(s,"-Wl,--whole-archive");add(s,base,"all.a");
 check(symbol(s,"dead_arch") && symbol(s,"dead_arch")->st_shndx!=SHN_UNDEF,"whole-archive bypass remains explicitly complete");tcc_delete(s);
 s=state(TCC_OUTPUT_MEMORY,1,1);tcc_set_options(s,"-Wl,--whole-archive");add(s,base,"short-metadata.a");tcc_delete(s);
#ifdef TCC_TARGET_PE
 s=state(TCC_OUTPUT_MEMORY,1,1);tcc_set_options(s,"-Wl,--whole-archive");char file[2048];snprintf(file,sizeof file,"%s/short-import.a",base);
 check(tcc_add_file(s,file)<0 && strstr(diagnostics,"short import"),"whole-archive short-import objects fail closed instead of being silently ignored");tcc_delete(s);
#endif
}
static void test_explicit(const char*base){TCCState*s=state(TCC_OUTPUT_MEMORY,1,1);tcc_set_options(s,"-Wl,--undefined=dead_arch");add(s,base,"all.a");check(symbol(s,"dead_arch") && symbol(s,"dead_arch")->st_shndx!=SHN_UNDEF,"explicitly rooted TLS member is loaded");
#ifdef TCC_TARGET_PE
 check(tcc_relocate(s)<0 && strstr(diagnostics,"requires"),"explicit live Windows static TLS remains checked unsupported in JIT");
#endif
 tcc_delete(s);}
static void test_partial(const char*base){
 TCCState*s=state(TCC_OUTPUT_MEMORY,1,1);add(s,base,"primary.o");check(symbol(s,"entry") && symbol(s,"entry")->st_shndx!=SHN_UNDEF,"same native input is admitted by MEMORY reader");tcc_delete(s);
#ifdef TCC_TARGET_PE
 s=state(TCC_OUTPUT_OBJ,0,0);int before=s->nb_sections;unsigned long symbols=s->symtab->data_offset;char file[2048];snprintf(file,sizeof file,"%s/primary.o",base);
 check(tcc_add_file(s,file)<0 && strstr(diagnostics,"COFF partial linking"),"native COFF OBJ/-r path fails before publishing lossy ELF metadata");
 check(s->nb_sections==before && s->symtab->data_offset==symbols,"partial-link refusal leaves native sections/symbols unmodified");tcc_delete(s);
#endif
 s=state(TCC_OUTPUT_OBJ,0,0);check(tcc_compile_string(s,"int source_only(void){return 11;}")==0,"ordinary C precompile remains accepted");char output[2048];snprintf(output,sizeof output,"%s/source-roundtrip-%s.o",base,
#ifdef __OPTIMIZE__
 "O2"
#else
 "O0"
#endif
 );check(tcc_output_file(s,output)==0,"ordinary C precompile still writes a TCC ELF object");unsigned char header[4]={0};FILE*f=fopen(output,"rb");if(f){fread(header,1,4,f);fclose(f);}check(!memcmp(header,ELFMAG,4),"precompile output is the original supported ELF format");tcc_delete(s);
}
int main(int argc,char**argv){if(argc!=3 && argc!=4)return 2;library_root=argv[2];if(argc==4 && !strcmp(argv[3],"--partial-only")){test_partial(argv[1]);}else{test_query(argv[1]);test_filtered(argv[1],0);test_filtered(argv[1],1);test_legacy(argv[1],TCC_OUTPUT_MEMORY,0,1);test_legacy(argv[1],TCC_OUTPUT_MEMORY,1,0);test_legacy(argv[1],TCC_OUTPUT_EXE,1,1);test_explicit(argv[1]);test_qualified_roots(argv[1]);test_partial(argv[1]);}printf("ARCHIVE_CHECKS=%d FAILURES=%d\n",checks,failures);return failures?1:0;}
