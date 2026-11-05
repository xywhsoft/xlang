/* Merge live x64/ARM64 exception records only after section GC; preserve their
   exact symbol/relocation meaning, then sort RVAs after final relocation. */
static void pe_merge_pdata(TCCState *s1)
{
#if defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64)
    Section *dst = have_section(s1, ".pdata");
    int i, n = s1->nb_sections;
    unsigned entry_size =
#ifdef TCC_TARGET_X86_64
        12;
#else
        8;
#endif
    for (i = 1; i < n; ++i) {
        Section *s = s1->sections[i];
        unsigned long off;
        ElfW(Sym) *sym;
        ElfW_Rel *r;
        if (s == dst || !(s->sh_flags & SHF_ALLOC) || !s->data_offset
            || (strncmp(s->name, ".pdata.", 7) && strncmp(s->name, ".pdata$", 7)))
            continue;
        if (s->data_offset % entry_size) {
            tcc_error_noabort("invalid exception-table size in '%s'", s->name);
            return;
        }
        if (!dst) {
            dst = new_section(s1, ".pdata", SHT_PROGBITS, SHF_ALLOC);
            dst->sh_addralign = 4;
        }
        dst->sh_flags |= SHF_ALLOC;
        off = section_add(dst, s->data_offset, 4);
        memcpy(dst->data + off, s->data, s->data_offset);
        if (s->reloc) {
            for_each_elem(s->reloc, 0, r, ElfW_Rel) {
                put_elf_reloca(s1->symtab, dst, off + r->r_offset,
                    ELFW(R_TYPE)(r->r_info), ELFW(R_SYM)(r->r_info),
#if SHT_RELX == SHT_RELA
                    r->r_addend
#else
                    0
#endif
                    );
            }
            s->reloc->data_offset = 0;
        }
        for_each_elem(s1->symtab, 1, sym, ElfW(Sym)) {
            if (sym->st_shndx == s->sh_num) {
                sym->st_shndx = dst->sh_num;
                sym->st_value += off;
            }
        }
        s->data_offset = s->sh_size = 0;
        s->sh_flags &= ~SHF_ALLOC;
    }
    if (dst && dst->data_offset % entry_size)
        tcc_error_noabort("invalid exception-table size in '.pdata'");
    s1->uw_pdata = dst;
#endif
}

static int pe_compare_pdata(const void *a, const void *b)
{
    unsigned aa = read32le((unsigned char *)a), bb = read32le((unsigned char *)b);
    return aa < bb ? -1 : aa > bb;
}

ST_FUNC void pe_sort_pdata(TCCState *s1)
{
#if defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64)
    Section *s = s1->uw_pdata;
    unsigned entry_size =
#ifdef TCC_TARGET_X86_64
        12;
#else
        8;
#endif
    if (s && s->data_offset) {
        unsigned i, count = s->data_offset / entry_size;
        unsigned stride = entry_size + sizeof(unsigned);
        unsigned char *order;
        unsigned *mapping;
        int needs_sort = 0, n;
        ElfW(Sym) *sym;
        for (i = 1; i < count; ++i)
            if (read32le(s->data + (i-1)*entry_size) > read32le(s->data + i*entry_size))
                needs_sort = 1;
        if (!needs_sort) return;
        /* A direct external reference to a table entry needs full linker
           address-map rewriting. Reject it rather than leave a stale pointer. */
        for (n = 1; n < s1->nb_sections; ++n) {
            Section *from = s1->sections[n];
            ElfW_Rel *r;
            if (from == s || !from->reloc) continue;
            for_each_elem(from->reloc, 0, r, ElfW_Rel) {
                unsigned k = ELFW(R_SYM)(r->r_info);
                sym = (ElfW(Sym) *)s1->symtab->data + k;
                if (sym->st_shndx == s->sh_num) {
                    tcc_error_noabort("cannot reorder exception table with external entry references");
                    return;
                }
            }
        }
        order = tcc_malloc(count * stride);
        mapping = tcc_malloc(count * sizeof(unsigned));
        for (i = 0; i < count; ++i) {
            memcpy(order + i*stride, s->data + i*entry_size, entry_size);
            write32le(order + i*stride + entry_size, i);
        }
        qsort(order, count, stride, pe_compare_pdata);
        for (i = 0; i < count; ++i) {
            memcpy(s->data + i*entry_size, order + i*stride, entry_size);
            mapping[read32le(order + i*stride + entry_size)] = i;
        }
        for_each_elem(s1->symtab, 1, sym, ElfW(Sym)) {
            if (sym->st_shndx == s->sh_num && sym->st_value >= s->sh_addr) {
                addr_t off = sym->st_value - s->sh_addr;
                if (off < s->data_offset)
                    sym->st_value = s->sh_addr + mapping[off / entry_size] * entry_size + off % entry_size;
            }
        }
        tcc_free(mapping); tcc_free(order);
    }
#endif
}
