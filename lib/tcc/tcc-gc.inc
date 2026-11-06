/* Opt-in final-link reachability. Sections/symbol indices stay stable until the
   normal output sorter. A discarded symbol becomes a local absolute tombstone;
   importantly, an undefined referenced only by discarded code is not resolved. */
#ifndef SHF_GNU_RETAIN
#define SHF_GNU_RETAIN 0x200000
#endif

ST_FUNC Section *tcc_split_section(TCCState *s1, Section *base, const char *symbol)
{
    Section *s;
    size_t n = strlen(base->name) + strlen(symbol) + 40;
    char *name = tcc_malloc(n);
    snprintf(name, n, "%s.%s.%u", base->name, symbol, ++s1->gc_section_serial);
    s = new_section(s1, name, base->sh_type, base->sh_flags);
    s->sh_addralign = base->sh_addralign;
    tcc_free(name);
    return s;
}

typedef struct TCCGCFrame {
    unsigned long old, size, dst;
    int cie, owner, active, reloc_head;
} TCCGCFrame;

/* PE exception tables share storage, not reachability. These are temporary
   graph records: archive queries must never modify the input table. */
typedef struct TCCGCPDataRecord {
    unsigned long dst;
    int owner, active, reloc_head, begin_relocs;
} TCCGCPDataRecord;

typedef struct TCCGCPData {
    Section *section;
    TCCGCPDataRecord *records;
    int *reloc_next;
    int count, width, keep_all, opaque, marked;
} TCCGCPData;

typedef struct TCCGC {
    TCCState *s;
    unsigned char *live, *used;
    int *queue, *heads, *next, read, count;
    int nsec, nsym;
    unsigned long *old_sizes;
    Section *eh;
    TCCGCFrame *frames;
    int nframes, frame_capacity;
    int *frame_reloc_next;
    TCCGCPData *pdata;
} TCCGC;

ST_FUNC int tcc_pdata_name(const char *name)
{
#if defined(TCC_TARGET_PE) && (defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64))
    return !strcmp(name, ".pdata") || !strncmp(name, ".pdata.", 7)
        || !strncmp(name, ".pdata$", 7);
#else
    (void)name;
    return 0;
#endif
}

static void gc_mark_section(TCCGC *g, int n)
{
    if (n > 0 && n < g->nsec && !g->live[n]) {
        g->live[n] = 1;
        g->queue[g->count++] = n;
    }
}

static void gc_mark_symbol(TCCGC *g, unsigned n)
{
    TCCState *s1 = g->s;
    ElfW(Sym) *sym;
    if (n >= g->nsym) {
        tcc_error_noabort("gc-sections: invalid relocation symbol %u", n);
        return;
    }
    g->used[n] = 1;
    sym = (ElfW(Sym) *)g->s->symtab->data + n;
    /* A real symbol/address root into metadata is distinct from the reverse
       owner->record edge. Preserve its offsets, including section+addend and
       boundary-symbol references, instead of attempting partial remapping. */
    if (g->pdata && sym->st_shndx > 0 && sym->st_shndx < g->nsec
        && g->pdata[sym->st_shndx].section)
        g->pdata[sym->st_shndx].keep_all = 1;
    gc_mark_section(g, sym->st_shndx);
}

static void gc_mark_name(TCCGC *g, const char *name)
{
    int n;
    if (name && (n = find_elf_sym(g->s->symtab, name)))
        gc_mark_symbol(g, n);
}

static void gc_reloc_range(TCCGC *g, Section *s, unsigned long begin,
                           unsigned long end)
{
    ElfW_Rel *r;
    if (!s->reloc || s->reloc->link != g->s->symtab)
        return;
    for_each_elem(s->reloc, 0, r, ElfW_Rel) {
        if (r->r_offset >= begin && r->r_offset < end)
            gc_mark_symbol(g, ELFW(R_SYM)(r->r_info));
    }
}

/* Binary lookup keeps large amalgamated objects out of an O(FDE * reloc)
   scan. Relocations need not be ordered in an input object. */
static int gc_frame_at(TCCGC *g, unsigned long off)
{
    int lo = 0, hi = g->nframes;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        TCCGCFrame *f = g->frames + mid;
        if (off < f->old) hi = mid;
        else if (off >= f->old + f->size) lo = mid + 1;
        else return mid;
    }
    return -1;
}

/* FDE initial-PC relocations identify code owners. Personality/LSDA and CIE
   edges are followed only for live FDEs. Unknown ownership is conservative. */
static int gc_read_frames(TCCGC *g)
{
    TCCState *s1 = g->s;
    Section *s = g->eh;
    unsigned long off = 0;
    int i;
    ElfW_Rel *r;
    if (!s) return 0;
    if (!s->data && s->data_offset) goto invalid;
    while (off < s->data_offset) {
        unsigned long size;
        unsigned id;
        TCCGCFrame f;
        if (s->data_offset - off < 4) goto invalid;
        size = read32le(s->data + off);
        if (size == 0xffffffff || size > s->data_offset - off - 4) goto invalid;
        memset(&f, 0, sizeof f);
        f.old = off; f.size = size + 4; f.cie = -1;
        if (size) {
            if (size < 4) goto invalid;
            id = read32le(s->data + off + 4);
            if (id) {
                if (id > off + 4) goto invalid;
                i = gc_frame_at(g, off + 4 - id);
                if (i < 0 || g->frames[i].old != off + 4 - id
                    || g->frames[i].cie >= 0) goto invalid;
                f.cie = i;
            }
        }
        if (g->nframes == g->frame_capacity) {
            g->frame_capacity = g->frame_capacity ? g->frame_capacity * 2 : 32;
            g->frames = tcc_realloc(g->frames, g->frame_capacity * sizeof f);
        }
        g->frames[g->nframes++] = f;
        off += f.size;
    }
    if (s->reloc) {
        int count = s->reloc->data_offset / sizeof(ElfW_Rel);
        g->frame_reloc_next = tcc_mallocz(count * sizeof(int));
        for_each_elem(s->reloc, 0, r, ElfW_Rel) {
            unsigned n = ELFW(R_SYM)(r->r_info);
            int index = r - (ElfW_Rel *)s->reloc->data;
            i = gc_frame_at(g, r->r_offset);
            if (i < 0 || n >= g->nsym) goto invalid;
            g->frame_reloc_next[index] = g->frames[i].reloc_head;
            g->frames[i].reloc_head = index + 1;
            if (g->frames[i].cie >= 0 && r->r_offset == g->frames[i].old + 8) {
                ElfW(Sym) *sym = (ElfW(Sym) *)g->s->symtab->data + n;
                if (sym->st_shndx > 0 && sym->st_shndx < g->nsec
                    && (g->s->sections[sym->st_shndx]->sh_flags & SHF_EXECINSTR))
                    g->frames[i].owner = sym->st_shndx;
            }
        }
    }
    return 0;
invalid:
    return tcc_error_noabort("gc-sections: malformed or unsupported .eh_frame record");
}

static void gc_frame_edges(TCCGC *g, TCCGCFrame *f)
{
    int n;
    for (n = f->reloc_head; n; n = g->frame_reloc_next[n-1]) {
        ElfW_Rel *r = (ElfW_Rel *)g->eh->reloc->data + n - 1;
        gc_mark_symbol(g, ELFW(R_SYM)(r->r_info));
    }
}

static int gc_mark_frames(TCCGC *g)
{
    int i, changed = 0;
    for (i = 0; i < g->nframes; ++i) {
        TCCGCFrame *f = g->frames + i, *cie;
        if (f->cie < 0 || f->active || (f->owner && !g->live[f->owner]))
            continue;
        f->active = 1;
        cie = g->frames + f->cie;
        gc_mark_section(g, g->eh->sh_num);
        gc_frame_edges(g, f);
        if (!cie->active) {
            cie->active = 1;
            gc_frame_edges(g, cie);
        }
        changed = 1;
    }
    return changed;
}

static void gc_compact_frames(TCCGC *g)
{
    Section *s = g->eh;
    unsigned long dst = 0;
    int i;
    ElfW_Rel *r, *out;
    if (!s)
        return;
    for (i = 0; i < g->nframes; ++i) {
        TCCGCFrame *f = g->frames + i;
        f->dst = dst;
        if (!f->active)
            continue;
        memmove(s->data + dst, s->data + f->old, f->size);
        if (f->cie >= 0)
            write32le(s->data + dst + 4, dst + 4 - g->frames[f->cie].dst);
        dst += f->size;
    }
    if (s->reloc) {
        out = (ElfW_Rel *)s->reloc->data;
        for_each_elem(s->reloc, 0, r, ElfW_Rel) {
            i = gc_frame_at(g, r->r_offset);
            if (i >= 0 && g->frames[i].active) {
                TCCGCFrame *f = g->frames + i;
                *out = *r;
                out->r_offset = f->dst + r->r_offset - f->old;
                ++out;
            }
        }
        s->reloc->data_offset = (unsigned char *)out - s->reloc->data;
    }
    s->data_offset = dst;
}

#include "tcc-gc-pdata.inc"

/* Reconstruct per-function exception ownership after loading a TCC ELF object
   (the in-memory gc_owner pointer itself is intentionally not serialized). */
static int gc_section_owner(TCCGC *g, Section *s)
{
    /* Packed .pdata may contain many unrelated code owners, even if an old
       input contribution supplied a whole-section gc_owner association. */
    if (tcc_pdata_name(s->name)) return 0;
    if (s->gc_owner) return s->gc_owner->sh_num;
#if defined(TCC_TARGET_PE) && (defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64))
    Section *owner = NULL;
    ElfW_Rel *r;
    unsigned width =
#ifdef TCC_TARGET_X86_64
        12;
#else
        8;
#endif
    if (strncmp(s->name, ".pdata", 6) || !s->reloc) return 0;
    for_each_elem(s->reloc, 0, r, ElfW_Rel) {
        unsigned n;
        ElfW(Sym) *sym;
        if (r->r_offset % width) continue;
        n = ELFW(R_SYM)(r->r_info);
        if (n >= g->nsym) return 0;
        sym = (ElfW(Sym) *)g->s->symtab->data + n;
        if (!sym->st_shndx || sym->st_shndx >= g->nsec
            || !(g->s->sections[sym->st_shndx]->sh_flags & SHF_EXECINSTR)) return 0;
        if (owner && owner != g->s->sections[sym->st_shndx]) return 0;
        owner = g->s->sections[sym->st_shndx];
    }
    return owner ? owner->sh_num : 0;
#else
    return 0;
#endif
}

/* This graph pass writes only temporary work arrays. Archive selection can
   query it repeatedly without publishing tombstones, shrinking sections,
   compacting unwind records, or even caching inferred gc_owner pointers. */
static int gc_collect(TCCGC *g, TCCState *s1, const char *entry, int keep_exports)
{
    int i, n;
    ElfW(Sym) *sym;
    memset(g, 0, sizeof *g);
    g->s = s1;
    g->nsec = s1->nb_sections;
    g->nsym = s1->symtab->data_offset / sizeof(ElfW(Sym));
    g->old_sizes = tcc_malloc(g->nsec * sizeof(unsigned long));
    for (i = 1; i < g->nsec; ++i)
        g->old_sizes[i] = s1->sections[i]->data_offset;
    g->live = tcc_mallocz(g->nsec);
    g->used = tcc_mallocz(g->nsym);
    g->queue = tcc_malloc(g->nsec * sizeof(int));
    g->heads = tcc_mallocz(g->nsec * sizeof(int));
    g->next = tcc_mallocz(g->nsec * sizeof(int));
    g->eh = have_section(s1, ".eh_frame");
    if (gc_read_frames(g) < 0 || gc_read_pdata(g) < 0)
        return -1;
    for (i = 1; i < g->nsec; ++i) {
        Section *s = s1->sections[i];
        n = gc_section_owner(g, s);
        if (n > 0 && n < g->nsec) {
            g->next[i] = g->heads[n];
            g->heads[n] = i;
        }
        if (!(s->sh_flags & SHF_ALLOC) || s == g->eh || n
            || (g->pdata && g->pdata[i].section))
            continue;
        if ((s->sh_flags & SHF_GNU_RETAIN) || s->sh_type == SHT_INIT_ARRAY
            || s->sh_type == SHT_FINI_ARRAY || s->sh_type == SHT_PREINIT_ARRAY
            || !strcmp(s->name, ".init") || !strcmp(s->name, ".fini")
            || !strncmp(s->name, ".ctors", 6) || !strncmp(s->name, ".dtors", 6)
            || !strncmp(s->name, ".debug_", 7) || !strncmp(s->name, ".stab", 5)
            || !strncmp(s->name, ".CRT$", 5) || !strcmp(s->name, ".rsrc")
            || !strcmp(s->name, ".bounds") || !strcmp(s->name, ".lbounds"))
            gc_mark_section(g, i);
    }
    gc_mark_name(g, entry);
    gc_mark_name(g, s1->init_symbol);
    gc_mark_name(g, s1->fini_symbol);
    for (i = 0; i < s1->nb_gc_roots; ++i)
        gc_mark_name(g, s1->gc_roots[i]);
    for (i = 1; i < g->nsym; ++i) {
        int nonlocal, visible;
        sym = (ElfW(Sym) *)s1->symtab->data + i;
        nonlocal = ELFW(ST_BIND)(sym->st_info) != STB_LOCAL;
        visible = ELFW(ST_VISIBILITY)(sym->st_other) != STV_HIDDEN
            && ELFW(ST_VISIBILITY)(sym->st_other) != STV_INTERNAL;
        if (sym->st_shndx != SHN_UNDEF && nonlocal
            && ((keep_exports && visible)
                || (s1->rdynamic
#ifndef TCC_TARGET_PE
                    && visible
#endif
                    )
                || (s1->output_type == TCC_OUTPUT_MEMORY && !s1->nb_gc_roots
                    && !gc_pdata_implicit_boundary(g, sym))))
            gc_mark_symbol(g, i);
#ifdef TCC_TARGET_PE
        if (sym->st_other & ST_PE_EXPORT)
            gc_mark_symbol(g, i);
#endif
    }
    do {
        while (g->read < g->count) {
            Section *s = s1->sections[g->queue[g->read++]];
            for (n = g->heads[s->sh_num]; n; n = g->next[n])
                gc_mark_section(g, n);
            if (s != g->eh && !(g->pdata && g->pdata[s->sh_num].section)
                && strncmp(s->name, ".debug_", 7) && strncmp(s->name, ".stab", 5))
                gc_reloc_range(g, s, 0, s->data_offset);
        }
        if (gc_mark_frames(g) | gc_mark_pdata(g))
            continue;
#ifdef TCC_TARGET_PE
        /* The PE loader, not a C call relocation, owns the TLS descriptor.
           Add that root only once a live template/callback requires it. */
        for (i = 1; i < g->nsec; ++i) {
            Section *s = s1->sections[i];
            if (g->live[i] && s->data_offset && ((s->sh_flags & SHF_TLS) || !strncmp(s->name, ".CRT$XL", 7))) {
                gc_mark_name(g, "_tls_used");
                break;
            }
        }
#endif
        if (g->read == g->count)
            break;
    } while (1);
    return s1->nb_errors ? -1 : 0;
}
static void gc_release(TCCGC *g)
{
    gc_release_pdata(g);
    tcc_free(g->live); tcc_free(g->used); tcc_free(g->queue);
    tcc_free(g->heads); tcc_free(g->next); tcc_free(g->frames);
    tcc_free(g->frame_reloc_next); tcc_free(g->old_sizes);
}
ST_FUNC int tcc_gc_live_undefined(TCCState *s1, unsigned char **used, int *length)
{
    TCCGC g;
    int result;
    ElfW(Sym) *sym;
    *used = NULL; *length = 0;
    if (!(tcc_native_link_features() & TCC_NATIVE_LINK_GC_SECTIONS))
        return tcc_error_noabort("archive reachability is not implemented for this target backend");
    result = gc_collect(&g, s1, NULL, 0);
    if (!result) {
        for (int i = 1; i < g.nsym; ++i) {
            sym = (ElfW(Sym) *)s1->symtab->data + i;
            if (sym->st_shndx != SHN_UNDEF) g.used[i] = 0;
        }
        *used = g.used; *length = g.nsym; g.used = NULL;
    }
    gc_release(&g);
    return result;
}
ST_FUNC void tcc_gc_sections(TCCState *s1, const char *entry, int keep_exports)
{
    TCCGC g;
    int i, n;
    ElfW(Sym) *sym;
    if (!s1->gc_sections || s1->gc_sections_done || s1->output_type == TCC_OUTPUT_OBJ)
        return;
    if (!(tcc_native_link_features() & TCC_NATIVE_LINK_GC_SECTIONS)) {
        tcc_error_noabort("gc-sections is not implemented for this target backend");
        return;
    }
    if (gc_collect(&g, s1, entry, keep_exports) < 0) {
        gc_release(&g);
        return;
    }
    gc_compact_frames(&g);
    gc_compact_pdata(&g);
    for (i = 1; i < g.nsec; ++i) {
        Section *s = s1->sections[i];
        if (!(s->sh_flags & SHF_ALLOC) || g.live[i])
            continue;
        if (s1->print_gc_sections && s->data_offset)
            fprintf(stderr, "tcc: removing unused section '%s' (%lu bytes)\n", s->name, s->data_offset);
        s->data_offset = s->sh_size = 0;
        s->sh_flags &= ~SHF_ALLOC;
        if (s->reloc)
            s->reloc->data_offset = s->reloc->sh_size = 0;
    }
    for (i = 1; i < g.nsym; ++i) {
        sym = (ElfW(Sym) *)s1->symtab->data + i;
        n = sym->st_shndx;
        if (n > 0 && n < g.nsec && g.live[n]
            && sym->st_value == g.old_sizes[n]) {
            const char *name = (char *)s1->symtab->link->data + sym->st_name;
            if (!strncmp(name, "__stop_", 7) || !strcmp(name, "_etext")
                || !strcmp(name, "_edata") || !strcmp(name, "_end"))
                sym->st_value = s1->sections[n]->data_offset;
        }
        if ((n > 0 && n < g.nsec && !g.live[n]
             && !(s1->sections[n]->sh_flags & SHF_ALLOC))
            || (n == SHN_UNDEF && !g.used[i])) {
            /* Non-alloc debug/file symbols are not code roots, but their
               relocations can safely refer to these zero-valued tombstones. */
            if (n && !(s1->sections[n]->sh_flags & SHF_ALLOC)
                && s1->sections[n]->data_offset)
                continue;
            sym->st_shndx = SHN_ABS;
            sym->st_value = sym->st_size = 0;
            sym->st_info = ELFW(ST_INFO)(STB_LOCAL, STT_NOTYPE);
            sym->st_other = 0;
        }
    }
    /* Removed definitions now have local tombstones. Keep global name lookup
       consistent before creating late linker symbols such as the GOT base. */
    rebuild_hash(s1->symtab, 0);
    s1->gc_sections_done = 1;
    gc_release(&g);
}
