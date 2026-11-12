/* CPU relocation identity/admission tests against the real provider. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static int checks, failures;
static const char *library_root;
static char diagnostics[4096];
static void check(int ok, const char *name)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr,"FAIL %s: %s\n",name,diagnostics); }
}
static void sink(void *unused,const char *message)
{
    size_t n=strlen(diagnostics);(void)unused;
    if(n<sizeof diagnostics-2)snprintf(diagnostics+n,sizeof diagnostics-n,"%s\n",message);
}
static TCCState *state(int output)
{
    TCCState *s=tcc_new();diagnostics[0]=0;
    tcc_set_error_func(s,NULL,sink);tcc_set_lib_path(s,library_root);
    check(!tcc_set_options(s,"-nostdlib"),"ordinary options accepted");
    check(!tcc_set_output_type(s,output),"output mode selected");return s;
}
static int public_relocs(TCCState *s)
{
    int i,n=0;
    for(i=1;i<s->nb_sections;++i)
        if(s->sections[i]->sh_type==SHT_RELX && !(s->sections[i]->sh_flags&SHF_ALLOC))++n;
    return n;
}
static int private_relocs(TCCState *s)
{
    int i,n=0;
    for(i=0;i<s->nb_priv_sections;++i)
        if(s->priv_sections[i]->sh_type==SHT_RELX && (s->priv_sections[i]->sh_flags&SHF_PRIVATE))++n;
    return n;
}
static int host_value(int value){return value*4;}
static void metadata_owners(TCCState *s)
{
    int i,ok=1;
    for(i=0;i<s->nb_priv_sections;++i){
        Section *r=s->priv_sections[i];
        /* Output sorting also parks discarded public records here for later
           teardown. They are not private target-attached link metadata. */
        if(r->sh_type!=SHT_RELX || !(r->sh_flags&SHF_PRIVATE))continue;
        if(!(r->sh_flags&SHF_PRIVATE)||r->sh_num||r->sh_info<=0||r->sh_info>=s->nb_sections||
           s->sections[r->sh_info]->reloc!=r||r->link!=s->symtab)ok=0;
    }
    check(ok,"private metadata keeps its exact target backlink and sole container ownership");
}
static void source_units(void)
{
    TCCState *s=state(TCC_OUTPUT_MEMORY);int rc,(*entry)(void);
    check(!tcc_compile_string(s,"static int x=6;int first(void){return x;}"),"first source compiled");
    check(!tcc_compile_string(s,"extern int first(void),host_value(int);int entry(void){return first()+host_value(9);}"),"second source compiled");
    check(!public_relocs(s) && private_relocs(s)>0,"ordinary source relocations do not consume numbered slots");
    check(!tcc_add_symbol(s,"host_value",host_value),"host function bound");
    metadata_owners(s);
    rc=tcc_relocate(s);check(!rc,"multi-unit source relocated");
    entry=rc?NULL:tcc_get_symbol(s,"entry");check(entry&&entry()==42,"multi-unit symbol rewrites preserve execution");
    tcc_delete(s);
}
static void object_units(const char *directory)
{
    char first[2048],second[2048];TCCState *s;int rc,(*entry)(void);
    snprintf(first,sizeof first,"%s/first.o",directory);snprintf(second,sizeof second,"%s/second.o",directory);
    s=state(TCC_OUTPUT_OBJ);
    check(!tcc_compile_string(s,"static int x=6;int first(void){return x;}"),"first object source compiled");
    check(public_relocs(s)>0 && !private_relocs(s),"OBJ producer keeps numbered relocation records");
    check(!tcc_output_file(s,first),"first object serialized");tcc_delete(s);
    s=state(TCC_OUTPUT_OBJ);
    check(!tcc_compile_string(s,"extern int first(void),host_value(int);int entry(void){return first()+host_value(9);}"),"second object source compiled");
    check(!tcc_output_file(s,second),"second object serialized");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);
    check(!tcc_add_file(s,first)&&!tcc_add_file(s,second),"common object sections merged");
    check(!public_relocs(s)&&private_relocs(s)>0,"unaddressed imported relocation records are private");
    check(!tcc_add_symbol(s,"host_value",host_value),"object host function bound");
    metadata_owners(s);
    rc=tcc_relocate(s);check(!rc,"common object metadata relocated");
    entry=rc?NULL:tcc_get_symbol(s,"entry");check(entry&&entry()==42,"merged common relocation offsets preserve execution");
    tcc_delete(s);
}
static int add(TCCState *s,const char *directory,const char *name)
{
    char path[2048];snprintf(path,sizeof path,"%s/%s",directory,name);return tcc_add_file(s,path);
}
static void metadata_identity(const char *directory)
{
    TCCState *s=state(TCC_OUTPUT_MEMORY);Section *r;ElfW(Sym)*sym;int index,rc;
    uintptr_t *a,*b;
    check(!add(s,directory,"meta-first.o"),"unaddressed metadata input loaded");
    r=have_section(s,".rela.data");
    check(r && (r->sh_flags&SHF_PRIVATE),"first ordinary metadata is private");
    check(!add(s,directory,"meta-second.o"),"metadata definition input loaded after private contribution");
    r=have_section(s,".rela.data");index=find_elf_sym(s->symtab,"relocation_anchor");
    sym=index?(ElfW(Sym)*)s->symtab->data+index:NULL;
    check(r && !(r->sh_flags&SHF_PRIVATE) && r->sh_num>0,"addressed metadata becomes numbered");
    check(sym && sym->st_shndx==r->sh_num && sym->st_value==sizeof(ElfW_Rel),"metadata alias keeps its exact second-contribution identity and offset");
    check(r && r->data_offset==2*sizeof(ElfW_Rel),"promotion retains earlier relocation records");
    metadata_owners(s);
    rc=tcc_relocate(s);check(!rc,"promoted metadata applies both data fixups");
    a=rc?NULL:tcc_get_symbol(s,"first");b=rc?NULL:tcc_get_symbol(s,"second");
    check(a&&b&&*a==0x1234&&*b==0x1234,"both common data contributions have correct relocated values");tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);check(!add(s,directory,"meta-allocated.o"),"allocated relocation input loaded");
    r=have_section(s,".rela.data");check(r && (r->sh_flags&SHF_ALLOC) && !(r->sh_flags&SHF_PRIVATE)&&r->sh_num>0,"allocated relocation records keep numbered identity");
    tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);check(!add(s,directory,"meta-write.o"),"nonordinary relocation flags input loaded");
    r=have_section(s,".rela.data");check(r && (r->sh_flags&SHF_WRITE) && !(r->sh_flags&SHF_PRIVATE)&&r->sh_num>0,"nonordinary relocation flags keep numbered identity");
    tcc_delete(s);
    s=state(TCC_OUTPUT_MEMORY);
    check(!add(s,directory,"meta-first.o"),"promotion-refusal first contribution loaded");
    r=have_section(s,".rela.data");
    {
        int before;unsigned long symbols,bytes;char name[80];
        while(s->nb_sections<SHN_LORESERVE-32-4){snprintf(name,sizeof name,".promotion.seed.%d",s->nb_sections);new_section(s,name,SHT_PROGBITS,0);}
        before=s->nb_sections;symbols=s->symtab->data_offset;bytes=r?r->data_offset:0;
        check(add(s,directory,"meta-second.o")<0 && strstr(diagnostics,"section index limit"),"addressed-metadata promotion refuses a truly insufficient index budget");
        check(s->nb_sections==before && s->symtab->data_offset==symbols && r && r->data_offset==bytes,"promotion budget refusal retains existing metadata and publishes no symbols");
        metadata_owners(s);
    }
    tcc_delete(s);
}
static void shared_output(const char *directory)
{
    TCCState *s=state(TCC_OUTPUT_DLL);char path[2048];
#ifdef TCC_TARGET_PE
    const char *source="int meta_dll_entry(void*a,unsigned b,void*c){return 1;}"
        "int meta_value=39;int*meta_pointer=&meta_value;__declspec(dllexport)int metadata_answer(void){return *meta_pointer+3;}";
    check(!tcc_set_options(s,"-Wl,-e,meta_dll_entry"),"DLL entry selected");
    snprintf(path,sizeof path,"%s/metadata.dll",directory);
#else
    const char *source="int meta_value=39;int*meta_pointer=&meta_value;int metadata_answer(void){return *meta_pointer+3;}";
    snprintf(path,sizeof path,"%s/metadata.so",directory);
#endif
    check(!tcc_compile_string(s,source),"shared data-relocation source compiled");
    check(!tcc_output_file(s,path),"shared output published with runtime relocations");
    check(!s->nb_errors,"shared output has no hidden linker errors");
    {
        int i,ok=1;
        for(i=0;i<s->nb_priv_sections;++i){Section *r=s->priv_sections[i];if(r->sh_type==SHT_RELX&&(r->sh_flags&(SHF_PRIVATE|SHF_ALLOC))==(SHF_PRIVATE|SHF_ALLOC))ok=0;}
        check(ok,"all runtime relocation records were published before final image layout");
    }
    metadata_owners(s);
    tcc_delete(s);
}
static void captured_objects(const char *list)
{
    FILE *f=fopen(list,"rb");char path[4096],name[80];int target,loaded=0,expected=0,ok=1,i,index;
    TCCState *s=state(TCC_OUTPUT_MEMORY);
    check(f!=NULL,"captured project object list opened");if(!f){tcc_delete(s);return;}
    check(fscanf(f,"%d\n",&target)==1,"captured starting directory read");
    check(!tcc_set_options(s,"-ffunction-sections -fdata-sections"),"captured input contribution options selected");
    while(s->nb_sections<target){snprintf(name,sizeof name,".capture.seed.%d",s->nb_sections);new_section(s,name,SHT_PROGBITS,0);}
    while(fgets(path,sizeof path,f)){
        size_t n=strcspn(path,"\r\n");path[n]=0;if(!n)continue;++expected;
        if(ok){
            int before=s->nb_sections,errors=s->nb_errors,rc=tcc_add_file(s,path);
            printf("CAPTURE_ROW=%d BEFORE=%d AFTER=%d ERRORS=%d RC=%d %s\n",expected,before,s->nb_sections,s->nb_errors-errors,rc,path);
            if(rc<0 || s->nb_errors!=errors)ok=0;else ++loaded;
        }
    }
    fclose(f);check(ok && loaded==expected,"entire unique captured object population admitted");
    metadata_owners(s);
    if(ok){
        static const char *keys[]={"xlcCreateContext","tcc_compile_string","xrtErrorFree"};
        for(i=0;i<3;++i){ElfW(Sym)*v;index=find_elf_sym(s->symtab,keys[i]);v=index?(ElfW(Sym)*)s->symtab->data+index:NULL;
            check(v&&v->st_shndx>0&&v->st_shndx<s->nb_sections,"core definition retains an addressable section index");}
    }
    printf("CAPTURED_OBJECTS=%d EXPECTED=%d INDEXED=%d PRIVATE_RELOCS=%d\n",loaded,expected,s->nb_sections,private_relocs(s));
    tcc_delete(s);
}
int main(int argc,char **argv)
{
    if(argc<3||argc>4)return 2;library_root=argv[1];
    printf("LAYOUT_TCCSTATE=%zu SECTION=%zu MERGE=%zu\n",sizeof(TCCState),sizeof(Section),sizeof(SectionMergeInfo));
    source_units();object_units(argv[2]);metadata_identity(argv[2]);shared_output(argv[2]);
    if(argc==4)captured_objects(argv[3]);
    printf("RELOCATION_METADATA_CHECKS=%d FAILURES=%d\n",checks,failures);return failures?1:0;
}
