/* CPU-only PE unwind-record graph tests, built by the host GCC. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static int checks, failures;
static const char *library_root;
static char diagnostics[4096];
static void check(int ok, const char *name)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL %s: %s\n", name, diagnostics); }
}
static void sink(void *u, const char *text)
{
    size_t n = strlen(diagnostics);
    (void)u;
    if (n < sizeof diagnostics - 2)
        snprintf(diagnostics + n, sizeof diagnostics - n, "%s\n", text);
}
static TCCState *state(int output, int gc)
{
    TCCState *s = tcc_new();
    diagnostics[0] = 0;
    tcc_set_error_func(s, NULL, sink);
    tcc_set_lib_path(s, library_root);
    check(tcc_set_options(s, gc > 0 ? "-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=entry" :
        gc < 0 ? "-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections" :
        "-nostdlib -ffunction-sections -fdata-sections") == 0, "options accepted");
    check(tcc_set_output_type(s, output) == 0, "output accepted");
    return s;
}

#if defined(TCC_TARGET_PE) && defined(TCC_TARGET_X86_64)
typedef struct Fixture {
    TCCState *s;
    Section *live, *dead, *xlive, *xdead, *pd;
    int owner, deadowner, need, deadneed, metadata, deadmeta;
} Fixture;
static Section *bytes(TCCState *s, const char *name, int executable, unsigned n)
{
    Section *p = new_section(s, name, SHT_PROGBITS, SHF_ALLOC | (executable ? SHF_EXECINSTR : 0));
    memset(section_ptr_add(p, n), 0x90, n);
    return p;
}
static void row(Fixture *f, int owner, Section *xdata, unsigned marker)
{
    unsigned off = f->pd->data_offset;
    unsigned char *p = section_ptr_add(f->pd, 12);
    int x = put_elf_sym(f->s->symtab, 0, 0, ELFW(ST_INFO)(STB_LOCAL, STT_SECTION), 0, xdata->sh_num, NULL);
    write32le(p, marker); write32le(p + 4, marker + 4); write32le(p + 8, 0);
    put_elf_reloc(f->s->symtab, f->pd, off, R_X86_64_RELATIVE, owner);
    put_elf_reloc(f->s->symtab, f->pd, off + 4, R_X86_64_RELATIVE, owner);
    put_elf_reloc(f->s->symtab, f->pd, off + 8, R_X86_64_RELATIVE, x);
}
static Fixture fixture(int dead_first)
{
    Fixture f;
    memset(&f, 0, sizeof f);
    f.s = state(TCC_OUTPUT_MEMORY, 1);
    f.live = bytes(f.s, ".text.owner.live", 1, 4);
    f.dead = bytes(f.s, ".text.owner.dead", 1, 4);
    f.xlive = bytes(f.s, ".xdata.live", 0, 4);
    f.xdead = bytes(f.s, ".xdata.dead", 0, 4);
    f.pd = new_section(f.s, ".pdata", SHT_PROGBITS, SHF_ALLOC);
    f.pd->sh_addralign = 4;
    f.owner = put_elf_sym(f.s->symtab, 0, 4, ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, f.live->sh_num, "entry");
    f.deadowner = put_elf_sym(f.s->symtab, 0, 4, ELFW(ST_INFO)(STB_LOCAL, STT_FUNC), 0, f.dead->sh_num, "unused");
    f.need = put_elf_sym(f.s->symtab, 0, 0, ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, SHN_UNDEF, "live_handler");
    f.deadneed = put_elf_sym(f.s->symtab, 0, 0, ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, SHN_UNDEF, "dead_handler");
    put_elf_reloc(f.s->symtab, f.xlive, 0, R_X86_64_32, f.need);
    put_elf_reloc(f.s->symtab, f.xdead, 0, R_X86_64_32, f.deadneed);
    if (dead_first) row(&f, f.deadowner, f.xdead, 200);
    row(&f, f.owner, f.xlive, 100);
    if (!dead_first) row(&f, f.deadowner, f.xdead, 200);
    f.metadata = put_elf_sym(f.s->symtab, dead_first ? 12 : 0, 12,
        ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT), 0, f.pd->sh_num, "live_metadata");
    f.deadmeta = put_elf_sym(f.s->symtab, dead_first ? 0 : 12, 12,
        ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT), 0, f.pd->sh_num, "dead_metadata");
    /* An old input contribution may have this stale whole-table owner. */
    f.pd->gc_owner = f.dead;
    return f;
}
static void query(Fixture *f, int all, const char *name)
{
    unsigned char *used = NULL;
    int n = 0;
    size_t z = f->s->symtab->data_offset, rz = f->pd->reloc->data_offset;
    void *symbols = tcc_malloc(z), *relocs = tcc_malloc(rz);
    unsigned char saved[36];
    unsigned long size = f->pd->data_offset;
    memcpy(saved, f->pd->data, size); memcpy(symbols, f->s->symtab->data, z); memcpy(relocs, f->pd->reloc->data, rz);
    check(tcc_gc_live_undefined(f->s, &used, &n) == 0, name);
    check(used && n > f->deadneed && used[f->need] && !!used[f->deadneed] == all,
        "query follows live record handler graph only");
    check(f->pd->data_offset == size && !memcmp(saved, f->pd->data, size)
        && !memcmp(symbols, f->s->symtab->data, z) && !memcmp(relocs, f->pd->reloc->data, rz)
        && !f->s->gc_sections_done && f->pd->gc_owner == f->dead,
        "query does not mutate rows, symbols, relocations or inferred ownership");
    tcc_free(symbols); tcc_free(relocs); tcc_free(used);
}
static void basic(void)
{
    Fixture f = fixture(1);
    ElfW(Sym) *sym;
    query(&f, 0, "record archive query succeeds");
    query(&f, 0, "repeated record archive query succeeds");
    tcc_gc_sections(f.s, NULL, 0);
    check(f.pd->data_offset == 12 && read32le(f.pd->data) == 100, "only live record compacted from second row");
    check(f.pd->reloc->data_offset == 3 * sizeof(ElfW_Rel)
        && ((ElfW_Rel *)f.pd->reloc->data)[0].r_offset == 0
        && ((ElfW_Rel *)f.pd->reloc->data)[2].r_offset == 8, "record relocation offsets compacted");
    check(f.live->data_offset == 4 && !f.dead->data_offset
        && f.xlive->data_offset == 4 && !f.xdead->data_offset, "record graph preserves code/unwind and discards dead graph");
    sym = (ElfW(Sym) *)f.s->symtab->data;
    check(sym[f.metadata].st_value == 0 && sym[f.metadata].st_shndx == f.pd->sh_num
        && sym[f.deadmeta].st_shndx == SHN_ABS, "local metadata symbols remapped or tombstoned");
    check(sym[f.need].st_shndx == SHN_UNDEF && sym[f.deadneed].st_shndx == SHN_ABS,
        "only active handler remains undefined");
    tcc_delete(f.s);
}
static void protected_tables(void)
{
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f = fixture(1);
        if (!mode) f.pd->sh_flags |= SHF_GNU_RETAIN;
        if (mode == 1) tcc_set_options(f.s, "-Wl,--undefined=dead_metadata");
        if (mode == 2) ((ElfW(Sym) *)f.s->symtab->data)[f.deadmeta].st_other |= ST_PE_EXPORT;
        if (mode >= 3) {
            Section *ref = bytes(f.s, ".data.dead.metadata.pointer", 0, 8);
            int n = f.deadmeta;
            if (mode == 4) n = put_elf_sym(f.s->symtab, 0, 0, ELFW(ST_INFO)(STB_LOCAL, STT_SECTION), 0, f.pd->sh_num, NULL);
            put_elf_reloca(f.s->symtab, ref, 0, R_X86_64_64, n, mode == 4 ? 12 : 0);
        }
        query(&f, 1, "metadata root/external address conservatively preserves all rows");
        tcc_gc_sections(f.s, NULL, 0);
        check(f.pd->data_offset == 24 && read32le(f.pd->data) == 200
            && f.dead->data_offset == 4 && f.xdead->data_offset == 4,
            "protected metadata table offsets and whole referenced graph unchanged");
        tcc_delete(f.s);
    }
}
static void future_graph(void)
{
    Fixture f = fixture(0);
    int future = put_elf_sym(f.s->symtab, 0, 0, ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, SHN_UNDEF, "future_owner");
    row(&f, future, f.xlive, 300);
    query(&f, 0, "unknown record owner conservatively queried");
    {
        Section *late = bytes(f.s, ".text.late.archive.member", 1, 8);
        set_elf_sym(f.s->symtab, 0, 8, ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, late->sh_num, "future_owner");
        tcc_set_options(f.s, "-Wl,--undefined=future_owner");
        query(&f, 0, "future input definition and root incorporated without cached owner");
        tcc_gc_sections(f.s, NULL, 0);
        check(f.pd->data_offset == 24 && read32le(f.pd->data + 12) == 300
            && late->data_offset == 8, "future live input record retained at final link");
    }
    tcc_delete(f.s);
}
static void chains_and_opaque(void)
{
    Fixture f = fixture(0);
    Section *other = new_section(f.s, ".pdata$associated", SHT_PROGBITS, SHF_ALLOC);
    Section *save = f.pd;
    f.pd = other; row(&f, f.deadowner, f.xdead, 400); f.pd = save;
    int n = put_elf_sym(f.s->symtab, 0, 0, ELFW(ST_INFO)(STB_LOCAL, STT_SECTION), 0, other->sh_num, NULL);
    put_elf_reloca(f.s->symtab, f.xlive, 0, R_X86_64_32, n, 0);
    query(&f, 1, "unwind metadata chain reaches other table and handler");
    tcc_gc_sections(f.s, NULL, 0);
    check(other->data_offset == 12 && f.xdead->data_offset == 4 && f.dead->data_offset == 4,
        "metadata chain keeps complete target table and its code");
    tcc_delete(f.s);
    f = fixture(0);
    *(unsigned char *)section_ptr_add(f.pd, 1) = 0;
    query(&f, 1, "partial opaque record remains conservative");
    tcc_gc_sections(f.s, NULL, 0);
    check(f.pd->data_offset == 25 && f.dead->data_offset == 4,
        "opaque table is not compacted and unknown code is not pruned");
    tcc_delete(f.s);
    f = fixture(0);
    ((ElfW_Rel *)f.pd->reloc->data)[0].r_offset = 999;
    { unsigned char *used = NULL; int count;
      check(tcc_gc_live_undefined(f.s, &used, &count) < 0
            && strstr(diagnostics, "invalid exception relocation") && !used,
            "malformed relocation rejected without publishing a partial graph"); }
    check(f.pd->data_offset == 24 && !f.s->gc_sections_done, "bad query leaves table bytes/GC state intact");
    tcc_delete(f.s);
}
static void reject_unsafe_sort(void)
{
    Fixture f = fixture(1);
    Section *ref = bytes(f.s, ".data.published.metadata", 0, 8);
    unsigned char original[24];
    put_elf_sym(f.s->symtab, 0, 8, ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT), 0, ref->sh_num, "published_metadata");
    tcc_set_options(f.s, "-Wl,--undefined=published_metadata");
    put_elf_reloca(f.s->symtab, ref, 0, R_X86_64_64, f.deadmeta, 4);
    memcpy(original, f.pd->data, sizeof original);
    tcc_gc_sections(f.s, NULL, 0);
    f.s->uw_pdata = f.pd;
    pe_sort_pdata(f.s);
    check(f.s->nb_errors && strstr(diagnostics, "external entry references"),
        "existing PE sort rejects unsafe externally addressed metadata reorder");
    check(f.pd->data_offset == 24 && !memcmp(original, f.pd->data, sizeof original),
        "external pointer protection survives GC and rejected sorting");
    tcc_delete(f.s);
}
static void boundary_roots(void)
{
    for (int mode = 0; mode < 3; ++mode) {
        Fixture f = fixture(1);
        int n = set_global_sym(f.s, "__start_pdata", f.pd, 0);
        set_global_sym(f.s, "__stop_pdata", f.pd, -1);
        if (!mode) tcc_set_options(f.s, "-Wl,--undefined=__start_pdata");
        if (mode == 1) {
            Section *ref = bytes(f.s, ".data.metadata.registry", 0, 8);
            put_elf_reloca(f.s->symtab, ref, 0, R_X86_64_64, n, 12);
        }
        if (mode == 2) ((ElfW(Sym) *)f.s->symtab->data)[n].st_other |= ST_PE_EXPORT;
        query(&f, 1, "explicit or address-taken boundary symbol is a strong whole-table root");
        tcc_gc_sections(f.s, NULL, 0);
        check(f.pd->data_offset == 24 && read32le(f.pd->data) == 200,
            "published boundary symbol preserves original metadata offsets");
        tcc_delete(f.s);
    }
}
static const char *program =
    "extern int dead_missing(void);"
    "static int dead(void){return dead_missing();}"
    "static int cb(void){return 37;}"
    "static int(*slot)(void)=cb;"
    "int(*callback_address(void))(void){return slot;}"
    "int entry(void){return callback_address()();}";
static void actual(const char *directory, int explicit_roots)
{
    TCCState *s = state(TCC_OUTPUT_MEMORY, explicit_roots ? 1 : -1);
    Section *p;
    unsigned long old;
    int (*entry)(void), (*cb)(void);
    check(tcc_compile_string(s, program) == 0, "actual callback program compiled");
    p = have_section(s, ".pdata"); old = p ? p->data_offset : 0;
    check(p && old >= 36, "frontend appended records to one canonical table");
    check(tcc_relocate(s) == 0, "actual JIT ignores dead unresolved native graph");
    entry = tcc_get_symbol(s, "entry"); cb = NULL;
    { int (*(*get)(void))(void) = tcc_get_symbol(s, "callback_address");
      if (get) cb = get(); }
    check(entry && entry() == 37, "callback relocation graph executes after GC");
    check(p && p->data_offset < old && !tcc_get_symbol(s, "dead"), "dead function and unwind row removed together");
#ifdef _WIN64
    if (entry) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION a = RtlLookupFunctionEntry((DWORD64)(uintptr_t)entry, &base, NULL);
        check(a && base + a->BeginAddress <= (DWORD64)(uintptr_t)entry
            && base + a->EndAddress > (DWORD64)(uintptr_t)entry, "OS unwind lookup finds retained entry record");
        check(cb != NULL, "indirect callback symbol has final runtime address");
        if (cb) {
            a = RtlLookupFunctionEntry((DWORD64)(uintptr_t)cb, &base, NULL);
            check(a && base + a->BeginAddress <= (DWORD64)(uintptr_t)cb
                && base + a->EndAddress > (DWORD64)(uintptr_t)cb, "OS unwind lookup finds indirect callback record");
        }
    }
#endif
    tcc_delete(s);
    {
        char path[2048];
        snprintf(path, sizeof path, "%s/roundtrip-%s.o", directory, explicit_roots ? "explicit" : "implicit");
        s = state(TCC_OUTPUT_OBJ, 1);
        check(tcc_compile_string(s, program) == 0, "object source compiled with GC option");
        p = have_section(s, ".pdata"); old = p->data_offset;
        check(tcc_output_file(s, path) == 0 && !s->gc_sections_done && p->data_offset == old,
            "object output retains all exception rows");
        tcc_delete(s);
        s = state(TCC_OUTPUT_MEMORY, 1);
        check(tcc_add_file(s, path) == 0, "serialized canonical metadata reloaded");
        p = have_section(s, ".pdata");
        check(p && p->data_offset == old && tcc_relocate(s) == 0,
            "reloaded table ownership reconstructed from record relocations");
        entry = tcc_get_symbol(s, "entry");
        check(entry && entry() == 37 && p->data_offset < old, "object roundtrip callback and row GC succeed");
        tcc_delete(s);
        s = state(TCC_OUTPUT_MEMORY, 0);
        check(tcc_compile_string(s, program) == 0, "noGC source compiled");
        p = have_section(s, ".pdata"); old = p->data_offset;
        tcc_gc_sections(s, NULL, 0);
        check(!s->gc_sections_done && p->data_offset == old, "default noGC leaves every metadata row intact");
        tcc_delete(s);
    }
}
#endif

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    library_root = argv[1];
#if defined(TCC_TARGET_PE) && defined(TCC_TARGET_X86_64)
    check(tcc_pdata_name(".pdata") && tcc_pdata_name(".pdata.fn") && tcc_pdata_name(".pdata$fn")
        && !tcc_pdata_name(".pdatax"), "exact exception-table name grammar");
    basic(); protected_tables(); future_graph(); chains_and_opaque(); reject_unsafe_sort(); boundary_roots();
    actual(argv[2], 0); actual(argv[2], 1);
#else
    check(!tcc_pdata_name(".pdata"), "nonPE target does not reinterpret pdata");
#endif
    printf("TCC_PDATA_CHECKS=%d FAILURES=%d\n", checks, failures);
    return failures ? 1 : 0;
}
