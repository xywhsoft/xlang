/* GCC-built CPU fixture for the real COFF reader and PE layout pipeline.
   No TinyCC-produced image is written or executed. Unrelated CRT imports are
   resolved to inert fixture storage only in the real-tlssup structural case. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static int checks, failures;
static char diagnostics[8192];
static const char *library_root;
static void error_sink(void *opaque, const char *message)
{
    size_t n = strlen(diagnostics); (void)opaque;
    if (n < sizeof diagnostics - 2)
        snprintf(diagnostics + n, sizeof diagnostics - n, "%s\n", message);
}
static void check(int pass, const char *message)
{
    ++checks;
    if (!pass) { ++failures; fprintf(stderr,"FAIL %s: %s\n",message,diagnostics); }
}
static TCCState *state(int jit, const char *root)
{
    TCCState *s = tcc_new(); char options[512]; diagnostics[0]=0;
    tcc_set_error_func(s,NULL,error_sink);tcc_set_lib_path(s,library_root);
    snprintf(options,sizeof options,"-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,-e,%s,--undefined=%s",root,root);
    check(tcc_set_options(s,options)==0,"options accepted");
    check(tcc_set_output_type(s,jit ? TCC_OUTPUT_MEMORY : TCC_OUTPUT_EXE)==0,"output selected");
    return s;
}
static void add(TCCState *s,const char *base,const char *name)
{
    char file[2048];snprintf(file,sizeof file,"%s/%s",base,name);
    check(tcc_add_file(s,file)==0,name);
}
static ElfW(Sym) *symbol(TCCState *s,const char *name)
{
    unsigned i=find_elf_sym(s->symtab,name);
    ElfW(Sym) *sym;
    if(i)return ((ElfW(Sym)*)s->symtab->data)+i;
    /* Final GC removes local tombstones from the public lookup hash. Inspect
       them by raw index only for the internal metadata assertion below. */
    for_each_elem(s->symtab,1,sym,ElfW(Sym))
        if(!strcmp((const char*)s->symtab->link->data+sym->st_name,name))return sym;
    return NULL;
}
static unsigned char *bytes(TCCState *s,const char *name)
{
    ElfW(Sym) *sym=symbol(s,name);Section *sec;
    if(!sym || !sym->st_shndx || sym->st_shndx>=s->nb_sections)return NULL;
    sec=s->sections[sym->st_shndx];
    return sec->data ? sec->data + sym->st_value-sec->sh_addr : NULL;
}
static int prepare(TCCState *s,struct pe_info *pe,int jit)
{
    memset(pe,0,sizeof *pe);pe->s1=s;
    pe->filename=jit ? NULL : "<memory-only-PE-layout>";
    pe_add_runtime(s,pe);resolve_common_syms(s);
    tcc_gc_sections(s,pe->start_symbol,0);
    return pe_prepare_tls(pe);
}
static void stub_imports(TCCState *s)
{
    Section *code=new_section(s,".text.fixture-imports",SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR);
    Section *data=new_section(s,".data.fixture-imports",SHT_PROGBITS,SHF_ALLOC|SHF_WRITE);
    unsigned end=s->symtab->data_offset/sizeof(ElfW(Sym));
    *(unsigned char*)section_ptr_add(code,1)=0xc3;
    for(unsigned i=1;i<end;i++) {
        ElfW(Sym)*sym=((ElfW(Sym)*)s->symtab->data)+i;
        if(sym->st_shndx==SHN_UNDEF) {
            const char *name=(const char*)s->symtab->link->data+sym->st_name;
            int pointer=!strncmp(name,"__imp_",6);
            sym->st_shndx=pointer?data->sh_num:code->sh_num;
            sym->st_value=pointer?section_add(data,8,8):0;
        }
    }
}
static int layout(TCCState *s,struct pe_info *pe)
{
    pe_merge_pdata(s);pe_set_options(s,pe);pe_check_symbols(pe);
    if(s->nb_errors)return -1;
    pe_assign_addresses(pe);relocate_syms(s,s->symtab,0);
    s->pe_imagebase=pe->imagebase;
    if(s->nb_errors || pe_apply_section_relocs(pe))return -1;
    relocate_sections(s);
    if(s->nb_errors || pe_validate_tls(pe))return -1;
    pe_sort_pdata(s);
    return s->nb_errors?-1:0;
}
static void finish(TCCState *s,struct pe_info *pe)
{
    dynarray_reset(&pe->sec_info,&pe->sec_count);pe_free_imports(pe);tcc_delete(s);
}
static int base_reloc(struct pe_info *pe,addr_t address)
{
    TCCState *s=pe->s1;
    for(int i=1;i<s->nb_sections;i++) {
        Section *sec=s->sections[i];
        if(strcmp(sec->name,".reloc"))continue;
        for(unsigned at=0;at+8<=sec->data_offset;) {
            unsigned page=read32le(sec->data+at),size=read32le(sec->data+at+4);
            if(size<8 || size>sec->data_offset-at)return 0;
            for(unsigned k=at+8;k<at+size;k+=2) {
                unsigned value=read16le(sec->data+k);
                if((value>>12)==10 && pe->imagebase+page+(value&4095)==address)return 1;
            }
            at+=size;
        }
    }
    return 0;
}
static void directory_checks(TCCState *s,struct pe_info *pe,int synthetic)
{
    ElfW(Sym)*used=symbol(s,"_tls_used");unsigned char *p=bytes(s,"_tls_used");
    check(p && used && pe->tls_directory!=0,"TLS directory resolves to image data");
    if(!p)return;
    addr_t first=read64le(p),last=read64le(p+8),index=read64le(p+16),callbacks=read64le(p+24);
    check(pe->tls_synthetic==synthetic,"real CRT descriptor preferred over synthetic record");
    check(first>=pe->tls_template->sh_addr && last>=first && last<=pe->tls_template->sh_addr+pe->tls_template->data_offset,"template start/end are valid VAs");
    check(index==symbol(s,"_tls_index")->st_value,"index points to actual ordinary storage");
    check(callbacks && !(callbacks&7),"callback VA is pointer aligned");
    check(((read32le(p+36)>>20)&15)>=7,"TLS alignment covers 64-byte input");
    for(int i=0;i<4;i++) check(base_reloc(pe,used->st_value+8*i),"TLS directory VA has DIR64 base relocation");
    check(pe_validate_tls(pe)==0,"template/index/callback validation passes");
    printf("TLS_SYNTHETIC=%d TEMPLATE_BYTES=%lu ALIGN=%u DIRECTORY_RVA=%llx INDEX_VA=%llx CALLBACKS_VA=%llx\n",synthetic,pe->tls_template->data_offset,pe->tls_alignment,(unsigned long long)(used->st_value-pe->imagebase),(unsigned long long)index,(unsigned long long)callbacks);
}
static void test_aot(const char *base,int crt)
{
    TCCState*s=state(0,"entry");struct pe_info pe;
    if(crt){s->has_text_addr=1;s->text_addr=0x140000000ULL;}
    add(s,base,"tls.o");add(s,base,"relative.o");
    if(crt) {
        add(s,base,"lib64_libmingw32_a-tlssup.o");
        add(s,base,"lib64_libmingw32_a-tlsthrd.o");
        add(s,base,"lib64_libmingw32_a-tlsmcrt.o");
        add(s,base,"lib64_libmingw32_a-tls_atexit.o");
    } else add(s,base,"callbacks.o");
    check(prepare(s,&pe,0)==0,"post-GC static TLS preparation succeeds");
    if(crt)stub_imports(s);
    if(!s->nb_errors)check(layout(s,&pe)==0,"real PE address/relocation pipeline succeeds without emitting a file");
    if(!s->nb_errors) {
        directory_checks(s,&pe,!crt);
        unsigned char*p=bytes(s,"tls_value");
        check(p && read32le(p)==0x12345678,"initialized TLS bytes survive sorted merge");
        p=bytes(s,"tls_zero");check(p && !read32le(p),"zero initialized TLS template survives");
        ElfW(Sym)*v=symbol(s,"tls_align");check(v && !(v->st_value&63),"aligned TLS object address remains 64-byte aligned");
        p=bytes(s,"relative_values");v=symbol(s,"relative_anchor");
        Section*sec=v?s->sections[v->st_shndx]:NULL;
        check(p && sec && read16le(p)==sec->sh_info,"SECTION relocation stores final one-based output section index");
        check(p && sec && read32le(p+2)==v->st_value-pe.sec_info[sec->sh_info-1]->sh_addr+3,"SECREL relocation uses final output section base and addend");
        check(p && sec && read32le(p+2)!=3,"SECREL includes earlier input contribution, not only input-local offset");
    }
    finish(s,&pe);
}
static void test_jit(const char*base,int live)
{
    TCCState*s=state(1,live?"entry":"plain_entry");struct pe_info pe;
    add(s,base,"tls.o");add(s,base,"relative.o");
    int result=prepare(s,&pe,1);
    check(live ? result<0 && strstr(diagnostics,"requires") : result==0,"JIT live TLS rejected after GC; dead TLS does not block");
    if(!live) {
        check(!pe.tls_directory,"dead TLS does not synthesize a directory");
        check(symbol(s,"tls_value")->st_shndx==SHN_ABS,"dead TLS symbol is GC tombstone");
    }
    finish(s,&pe);
}
static void test_public_jit(const char*base)
{
    TCCState*s=state(1,"plain_entry");add(s,base,"tls.o");add(s,base,"relative.o");
    check(tcc_relocate(s)==0,"public JIT API discards dead native TLS before layout checks");
    int(*entry)(void)=tcc_get_symbol(s,"plain_entry");
    check(entry && entry()==29,"non-TLS retained COFF function executes through real public JIT");tcc_delete(s);
    s=state(1,"entry");add(s,base,"tls.o");add(s,base,"relative.o");
    check(tcc_relocate(s)<0 && strstr(diagnostics,"requires"),"public JIT API rejects retained native static TLS");tcc_delete(s);
}
static void test_extra(const char*base)
{
    TCCState*s=state(0,"plain_entry");struct pe_info pe;
    tcc_set_options(s,"-Wl,--undefined=tls_value");add(s,base,"tls.o");
    check(prepare(s,&pe,0)==0,"explicit TLS data root synthesizes index even when code-index use was discarded");
    check(layout(s,&pe)==0,"TLS-only root has a complete relocated image layout");finish(s,&pe);
    s=state(0,"plain_entry");add(s,base,"tls.o");add(s,base,"callbacks.o");
    check(prepare(s,&pe,0)==0,"callback-only image gets minimal OS template");
    check(pe.tls_template && pe.tls_template->data_offset==1,"callback-only synthetic template contains only required placeholder");
    check(layout(s,&pe)==0,"callback-only layout succeeds");
    if(!s->nb_errors) {
        unsigned char*p=bytes(s,"_tls_used");addr_t callbacks=read64le(p+24);
        Section*a=pe_va_section(s,callbacks,8,0);
        check(a && bytes(s,"cb_a") && read64le(a->data+callbacks-a->sh_addr)==read64le(bytes(s,"cb_a")),"sorted XLB callback precedes XLD and leading XLA sentinel is skipped");
    }
    finish(s,&pe);
    s=state(0,"plain_entry");add(s,base,"tls.o");add(s,base,"dynamic.o");
    check(prepare(s,&pe,0)<0 && strstr(diagnostics,"real CRT descriptor"),"dynamic initializer without live TLS still requires its actual CRT");finish(s,&pe);
}
static void test_unsupported(const char*base)
{
    TCCState*s=state(0,"plain_entry");struct pe_info pe;add(s,base,"tls.o");add(s,base,"unsupported.o");
    check(prepare(s,&pe,0)<0 && strstr(diagnostics,"CRT initialization family"),"live unsupported CRT family fails closed after GC");finish(s,&pe);
    s=state(0,"entry");add(s,base,"tls.o");add(s,base,"dynamic.o");
    check(prepare(s,&pe,0)<0 && strstr(diagnostics,"real CRT descriptor"),"dynamic initializer cannot use synthetic fake CRT");finish(s,&pe);
}
static void test_malformed(const char*base)
{
    TCCState*s=state(0,"plain_entry");struct pe_info pe;add(s,base,"tls.o");add(s,base,"bad-array.o");
    check(prepare(s,&pe,0)<0 && strstr(diagnostics,"callback-array contribution"),"invalid TLS callback record size fails after GC");finish(s,&pe);
    s=state(0,"entry");add(s,base,"tls.o");add(s,base,"relative.o");add(s,base,"lib64_libmingw32_a-tlssup.o");
    ElfW(Sym)*used=symbol(s,"_tls_used");Section*sec=used?s->sections[used->st_shndx]:NULL;
    check(sec && sec->data_offset>=used->st_value+40,"real descriptor record available before layout");
    if(sec)write32le(sec->data+used->st_value+36,0x00f00000);
    check(prepare(s,&pe,0)<0 && strstr(diagnostics,"invalid TLS directory alignment"),"reserved alignment encoding is checked");finish(s,&pe);
}
static void test_native(const char*base)
{
    TCCState*s=state(0,"doc_md4c_parse");struct pe_info pe;unsigned tls=0,relocations=0;
    add(s,base,"native-md4c.o");
    for(int i=1;i<s->nb_sections;i++) {
        Section*sec=s->sections[i];ElfW_Rel*r;
        if((sec->sh_flags&SHF_TLS) && sec->data_offset)tls++;
        if(sec->reloc)for_each_elem(sec->reloc,0,r,ElfW_Rel)
            if(ELFW(R_TYPE)(r->r_info)==R_TCC_PE_SECREL)relocations++;
    }
    check(tls==25 && relocations>=400,"actual native MD4C input contains 25 TLS contributions and 400 SECREL relocations");
    check(prepare(s,&pe,0)==0,"actual native static-TLS graph prepares after GC");stub_imports(s);
    check(layout(s,&pe)==0,"actual native TLS and unwind layout relocates without an emitted PE file");
    check(pe.tls_template && pe.tls_template->data_offset>=4,"actual native graph has a retained TLS template");
    printf("NATIVE_TLS_INPUTS=%u SECREL_INPUTS=%u TEMPLATE_BYTES=%lu SECTIONS=%d\n",tls,relocations,pe.tls_template?pe.tls_template->data_offset:0,s->nb_sections);
    finish(s,&pe);
}
static int listed_zero, listed_undefined;
static void symbol_sink(void*ctx,const char*name,const void*value)
{
    (void)ctx;if(!strcmp(name,"zero_entry"))listed_zero=!value;
    if(!strcmp(name,"undefined_external"))listed_undefined=1;
}
static void test_symbols(void)
{
    TCCState*s=state(1,"zero_entry");
    check(tcc_compile_string(s,"extern int undefined_external(void);int zero_entry(void){return undefined_external();}")==0,"split symbol fixture compiled");
    tcc_list_symbols(s,NULL,symbol_sink);
    check(listed_zero && !listed_undefined,"list_symbols enumerates defined offset-zero but excludes undefined");tcc_delete(s);
}
int main(int argc,char**argv)
{
    if(argc!=3)return 2;library_root=argv[2];
    test_aot(argv[1],0);test_aot(argv[1],1);test_jit(argv[1],0);test_jit(argv[1],1);test_unsupported(argv[1]);test_extra(argv[1]);test_public_jit(argv[1]);test_native(argv[1]);test_malformed(argv[1]);test_symbols();
    printf("CHECKS=%d FAILURES=%d SECTION_SIZE=%zu NAME_OFFSET=%zu\n",checks,failures,sizeof(Section),offsetof(Section,name));
    return failures?1:0;
}
