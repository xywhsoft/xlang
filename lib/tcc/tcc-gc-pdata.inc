/* Record-aware PE exception reachability. Only final-link GC may compact;
   archive queries rebuild this graph after every new input without mutation. */
#include <limits.h>

/* Linker-created boundary aliases describe the final live table; their mere
   availability to default JIT lookup is not an address-taking use. Actual
   relocations, explicit roots and exported aliases still preserve every row. */
static int gc_pdata_implicit_boundary(TCCGC *g, ElfW(Sym) *sym)
{
    const char *name, *section;
    if (!g->pdata || !sym->st_shndx || sym->st_shndx >= g->nsec
        || !g->pdata[sym->st_shndx].section
        || ELFW(ST_TYPE)(sym->st_info) != STT_NOTYPE || sym->st_size)
        return 0;
    name = (char *)g->s->symtab->link->data + sym->st_name;
    section = g->s->sections[sym->st_shndx]->name;
    if (*section == '.') ++section;
    return (!strncmp(name, "__start_", 8) && !strcmp(name + 8, section) && !sym->st_value)
        || (!strncmp(name, "__stop_", 7) && !strcmp(name + 7, section)
            && sym->st_value == g->old_sizes[sym->st_shndx]);
}

static int gc_read_pdata(TCCGC *g)
{
    TCCState *s1 = g->s;
    int i;
    ElfW_Rel *r;
    if (!tcc_pdata_name(".pdata")) return 0;
    g->pdata = tcc_mallocz(g->nsec * sizeof(*g->pdata));
    for (i = 1; i < g->nsec; ++i) {
        Section *s = g->s->sections[i];
        TCCGCPData *p = g->pdata + i;
        unsigned long count;
        if (!(s->sh_flags & SHF_ALLOC) || !tcc_pdata_name(s->name)) continue;
        p->section = s;
#ifdef TCC_TARGET_X86_64
        p->width = 12;
#else
        p->width = 8;
#endif
        p->keep_all = !!(s->sh_flags & SHF_GNU_RETAIN);
        count = s->data_offset / p->width;
        if (s->sh_type != SHT_PROGBITS || (!s->data && s->data_offset)
            || s->data_offset % p->width || count > INT_MAX / sizeof(*p->records)) {
            p->opaque = p->keep_all = 1;
            continue;
        }
        p->count = count;
        p->records = tcc_mallocz(p->count * sizeof(*p->records));
        if (!s->reloc) {
            if (p->count) p->opaque = p->keep_all = 1;
            continue;
        }
        if (s->reloc->link != g->s->symtab
            || (!s->reloc->data && s->reloc->data_offset)
            || s->reloc->data_offset % sizeof(ElfW_Rel)) {
            p->opaque = p->keep_all = 1;
            continue;
        }
        count = s->reloc->data_offset / sizeof(ElfW_Rel);
        if (count > INT_MAX / sizeof(int))
            return tcc_error_noabort("gc-sections: exception relocation table too large");
        p->reloc_next = tcc_mallocz(count * sizeof(int));
        for_each_elem(s->reloc, 0, r, ElfW_Rel) {
            unsigned n = ELFW(R_SYM)(r->r_info);
            int at = r - (ElfW_Rel *)s->reloc->data;
            TCCGCPDataRecord *f;
            ElfW(Sym) *sym;
            if (n >= g->nsym || r->r_offset >= s->data_offset)
                return tcc_error_noabort("gc-sections: invalid exception relocation");
            f = p->records + r->r_offset / p->width;
            p->reloc_next[at] = f->reloc_head;
            f->reloc_head = at + 1;
            /* PE runtime-function fields are 32-bit image-relative values.
               An unfamiliar relocation can span records: keep the table. */
            if (r->r_offset % 4
#if defined(TCC_TARGET_PE) && defined(TCC_TARGET_X86_64)
                || ELFW(R_TYPE)(r->r_info) != R_X86_64_RELATIVE
#elif defined(TCC_TARGET_PE) && defined(TCC_TARGET_ARM64)
                || ELFW(R_TYPE)(r->r_info) != R_AARCH64_RELATIVE
#endif
                ) p->opaque = p->keep_all = 1;
            if (r->r_offset % p->width) continue;
            ++f->begin_relocs;
            sym = (ElfW(Sym) *)g->s->symtab->data + n;
            if (sym->st_shndx > 0 && sym->st_shndx < g->nsec
                && (g->s->sections[sym->st_shndx]->sh_flags & SHF_EXECINSTR))
                f->owner = sym->st_shndx;
        }
        for (int j = 0; j < p->count; ++j)
            if (p->records[j].begin_relocs != 1) p->opaque = p->keep_all = 1;
    }
    /* External metadata addresses are not safely rewritten by deleting rows.
       Be conservative even when the referencing input itself later dies.
       Also catches metadata chains and section-symbol + arbitrary addend. */
    for (i = 1; i < g->nsec; ++i) {
        Section *s = g->s->sections[i];
        if (!s->reloc || s->reloc->link != g->s->symtab) continue;
        for_each_elem(s->reloc, 0, r, ElfW_Rel) {
            unsigned n = ELFW(R_SYM)(r->r_info);
            ElfW(Sym) *sym;
            if (n >= g->nsym)
                return tcc_error_noabort("gc-sections: invalid relocation symbol %u", n);
            sym = (ElfW(Sym) *)g->s->symtab->data + n;
            if (sym->st_shndx > 0 && sym->st_shndx < g->nsec
                && g->pdata[sym->st_shndx].section)
                g->pdata[sym->st_shndx].keep_all = 1;
        }
    }
    return 0;
}

static int gc_mark_pdata(TCCGC *g)
{
    int i, j, changed = 0;
    if (!g->pdata) return 0;
    for (i = 1; i < g->nsec; ++i) {
        TCCGCPData *p = g->pdata + i;
        if (!p->section) continue;
        if (p->opaque) {
            if (!p->marked) {
                p->marked = 1;
                gc_mark_section(g, i);
                gc_reloc_range(g, p->section, 0, p->section->data_offset);
                /* Unknown raw record bytes may contain code addresses with
                   no interpretable relocation. Do not prune possible code. */
                for (j = 1; j < g->nsec; ++j)
                    if (g->s->sections[j]->sh_flags & SHF_EXECINSTR)
                        gc_mark_section(g, j);
                changed = 1;
            }
            continue;
        }
        if (p->keep_all) gc_mark_section(g, i);
        for (j = 0; j < p->count; ++j) {
            TCCGCPDataRecord *f = p->records + j;
            int n;
            if (f->active || (!p->keep_all && f->owner && !g->live[f->owner]))
                continue;
            f->active = 1;
            gc_mark_section(g, i);
            for (n = f->reloc_head; n; n = p->reloc_next[n-1]) {
                ElfW_Rel *r = (ElfW_Rel *)p->section->reloc->data + n - 1;
                gc_mark_symbol(g, ELFW(R_SYM)(r->r_info));
            }
            changed = 1;
        }
    }
    return changed;
}

static void gc_compact_pdata(TCCGC *g)
{
    int i, j;
    if (!g->pdata) return;
    for (i = 1; i < g->nsec; ++i) {
        TCCGCPData *p = g->pdata + i;
        Section *s = p->section;
        unsigned long dst = 0;
        ElfW_Rel *r, *out;
        ElfW(Sym) *sym;
        if (!s || p->keep_all || p->opaque) continue;
        for (j = 0; j < p->count; ++j) {
            TCCGCPDataRecord *f = p->records + j;
            f->dst = dst;
            if (!f->active) continue;
            memmove(s->data + dst, s->data + (unsigned long)j * p->width, p->width);
            dst += p->width;
        }
        if (s->reloc) {
            out = (ElfW_Rel *)s->reloc->data;
            for_each_elem(s->reloc, 0, r, ElfW_Rel) {
                TCCGCPDataRecord *f = p->records + r->r_offset / p->width;
                if (!f->active) continue;
                *out = *r;
                out->r_offset = f->dst + r->r_offset % p->width;
                ++out;
            }
            s->reloc->data_offset = s->reloc->sh_size = (unsigned char *)out - s->reloc->data;
        }
        /* There are no published/inter-section references into a compacted
           table. Keep unreferenced local entry symbols coherent as well. */
        for_each_elem(g->s->symtab, 1, sym, ElfW(Sym)) {
            unsigned long off;
            TCCGCPDataRecord *f;
            if (sym->st_shndx != i) continue;
            off = sym->st_value;
            if (off == g->old_sizes[i]) {
                sym->st_value = dst;
                continue;
            }
            if (ELFW(ST_TYPE)(sym->st_info) == STT_SECTION && !off) continue;
            if (off >= g->old_sizes[i]) continue;
            f = p->records + off / p->width;
            if (f->active) sym->st_value = f->dst + off % p->width;
            else {
                sym->st_shndx = SHN_ABS;
                sym->st_value = sym->st_size = 0;
                sym->st_info = ELFW(ST_INFO)(STB_LOCAL, STT_NOTYPE);
                sym->st_other = 0;
            }
        }
        s->data_offset = s->sh_size = dst;
    }
}

static void gc_release_pdata(TCCGC *g)
{
    if (!g->pdata) return;
    for (int i = 1; i < g->nsec; ++i) {
        tcc_free(g->pdata[i].records);
        tcc_free(g->pdata[i].reloc_next);
    }
    tcc_free(g->pdata);
}
