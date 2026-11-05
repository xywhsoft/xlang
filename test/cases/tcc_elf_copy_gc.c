/* Linux ELF COPY-relocation regression, GCC-built libtcc host. No PE files. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"
static int checks,failures;
static char diagnostic[4096];
static void sink(void*u,const char*m){size_t n=strlen(diagnostic);(void)u;if(n<sizeof diagnostic-2)snprintf(diagnostic+n,sizeof diagnostic-n,"%s\n",m);}
static void check(int ok,const char*m){checks++;if(!ok){failures++;fprintf(stderr,"FAIL %s: %s\n",m,diagnostic);}}
int main(int argc,char**argv){
 if(argc!=5)return 2;TCCState*s=tcc_new();tcc_set_error_func(s,NULL,sink);tcc_set_lib_path(s,argv[1]);
 check(!tcc_set_options(s,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,-e,_start"),"GC options accepted");
 check(!tcc_set_output_type(s,TCC_OUTPUT_EXE),"ELF executable output selected");
 check(!tcc_add_file(s,argv[2]),"minimal ABI-correct assembly entry admitted");
 check(!tcc_compile_string(s,"extern void *stdout,*stderr;extern int fputs(const char*,void*);extern int fflush(void*);int main(void){if(!stdout||!stderr)return 11;if(fputs(\"COPY stdout\\n\",stdout)<0||fputs(\"COPY stderr\\n\",stderr)<0)return 12;return fflush(stdout)||fflush(stderr);}"),"program with only imported data and no own BSS compiled");
 check(!tcc_add_file(s,argv[3]),"actual shared libc exports admitted");
 check(have_section(s,".bss")->data_offset==0,"canonical BSS has no program allocation before GC");
 tcc_gc_sections(s,"_start",0);
 check(!s->nb_errors && !(have_section(s,".bss")->sh_flags&SHF_ALLOC),"GC actually discards empty canonical BSS before dynamic binding");
 check(!tcc_output_file(s,argv[4]),"final dynamic executable emitted");
 Section*b=have_section(s,".bss");check((b->sh_flags&(SHF_ALLOC|SHF_WRITE))==(SHF_ALLOC|SHF_WRITE) && b->sh_addr>4096 && b->data_offset>=24,"late COPY allocations restore writable allocated BSS with nonzero image address");
 int copies=0;ElfW_Rel*r;if(b->reloc)for_each_elem(b->reloc,0,r,ElfW_Rel)if(ELFW(R_TYPE)(r->r_info)==R_COPY){copies++;check(r->r_offset>=b->sh_addr && r->r_offset+8<=b->sh_addr+b->data_offset,"COPY target lies inside re-admitted BSS");}
 check(copies==2,"both imported stream objects use real COPY relocations");
 printf("BSS_ADDRESS=%llx BSS_BYTES=%lu COPY_RELOCS=%d\n",(unsigned long long)b->sh_addr,b->data_offset,copies);tcc_delete(s);printf("ELF_COPY_CHECKS=%d FAILURES=%d\n",checks,failures);return failures?1:0;
}
