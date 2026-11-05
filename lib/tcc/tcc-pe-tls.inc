/* Windows AMD64 image static TLS and COFF section-relative relocation.
   No dynamic-TLS emulation is used by the in-memory linker. */
#ifdef TCC_TARGET_X86_64
static unsigned pe_input_key_length(Section *s)
{ return s->coff_name_len ? s->coff_name_len : strlen(s->name); }
static int pe_key_prefix(Section *s, const char *prefix)
{
    unsigned n = strlen(prefix);
    return pe_input_key_length(s) >= n && !memcmp(s->name, prefix, n);
}
static int pe_tls_section(Section *s)
{ return (s->sh_flags & SHF_TLS) != 0; }
static int pe_key_compare(const void *aa, const void *bb)
{
    Section *a = *(Section * const *)aa, *b = *(Section * const *)bb;
    unsigned an = pe_input_key_length(a), bn = pe_input_key_length(b), n = an < bn ? an : bn;
    int c = memcmp(a->name, b->name, n);
    if (c) return c;
    if (an != bn) return an < bn ? -1 : 1;
    return a->sh_num < b->sh_num ? -1 : a->sh_num > b->sh_num;
}
static void pe_move_contribution(TCCState *s1, Section *dst, Section *src)
{
    ElfW(Sym) *sym;
    ElfW_Rel *r;
    unsigned long off = section_add(dst, src->data_offset, src->sh_addralign);
    if (dst->sh_addralign < src->sh_addralign) dst->sh_addralign = src->sh_addralign;
    if (src->data && src->sh_type != SHT_NOBITS)
        memcpy(dst->data + off, src->data, src->data_offset);
    if (src->reloc) {
        for_each_elem(src->reloc, 0, r, ElfW_Rel) {
            put_elf_reloca(s1->symtab, dst, off + r->r_offset,
                ELFW(R_TYPE)(r->r_info), ELFW(R_SYM)(r->r_info), r->r_addend);
        }
        src->reloc->data_offset = 0;
    }
    for_each_elem(s1->symtab, 1, sym, ElfW(Sym)) {
        if (sym->st_shndx == src->sh_num) {
            sym->st_shndx = dst->sh_num;
            sym->st_value += off;
        }
    }
    src->data_offset = src->sh_size = 0;
    src->sh_flags &= ~SHF_ALLOC;
}
static Section *pe_merge_ordered(TCCState *s1, const char *prefix, int tls)
{
    Section **sections = tcc_malloc(s1->nb_sections * sizeof *sections), *dst = NULL;
    int i, count = 0;
    for (i = 1; i < s1->nb_sections; ++i) {
        Section *s = s1->sections[i];
        if ((s->sh_flags & SHF_ALLOC) &&
            (tls ? pe_tls_section(s) : pe_key_prefix(s, prefix))) sections[count++] = s;
    }
    qsort(sections, count, sizeof *sections, pe_key_compare);
    if (count) {
        dst = new_section(s1, tls ? ".tls" : prefix, SHT_PROGBITS,
            SHF_ALLOC | SHF_WRITE | (tls ? SHF_TLS : 0));
        dst->sh_addralign = tls ? 1 : 8;
        for (i = 0; i < count; ++i) pe_move_contribution(s1, dst, sections[i]);
    }
    tcc_free(sections);
    return dst;
}
static ElfW(Sym) *pe_defined_symbol(TCCState *s1, const char *name, unsigned *index)
{
    unsigned n = find_elf_sym(s1->symtab, name);
    ElfW(Sym) *sym;
    if (!n) return NULL;
    sym = (ElfW(Sym) *)s1->symtab->data + n;
    if (!sym->st_shndx || sym->st_shndx >= s1->nb_sections ||
        !(s1->sections[sym->st_shndx]->sh_flags & SHF_ALLOC)) return NULL;
    if (index) *index = n;
    return sym;
}
static int pe_null_slot(Section *s, unsigned long off)
{
    ElfW_Rel *r;
    if (off > s->data_offset || s->data_offset - off < 8 || read64le(s->data + off)) return 0;
    if (s->reloc) for_each_elem(s->reloc, 0, r, ElfW_Rel)
        if (r->r_offset >= off && r->r_offset < off + 8) return 0;
    return 1;
}
static int pe_tls_has_va_reloc(Section *s, unsigned long off)
{
    ElfW_Rel *r;
    if (s->reloc) for_each_elem(s->reloc, 0, r, ElfW_Rel)
        if (r->r_offset == off && ELFW(R_TYPE)(r->r_info) == R_X86_64_64) return 1;
    return 0;
}
static int pe_prepare_tls(struct pe_info *pe)
{
    TCCState *s1 = pe->s1;
    Section *callbacks, *dynamic;
    unsigned i, index = 0;
    int has_tls = 0, has_callbacks = 0, has_dynamic = 0;
    ElfW(Sym) *sym;
    for (i = 1; i < s1->nb_sections; ++i) {
        Section *s = s1->sections[i]; ElfW_Rel *r;
        if (!(s->sh_flags & SHF_ALLOC) || !s->data_offset) continue;
        has_tls |= pe_tls_section(s);
        has_callbacks |= pe_key_prefix(s, ".CRT$XL");
        has_dynamic |= pe_key_prefix(s, ".CRT$XD");
        if ((pe_key_prefix(s, ".CRT$XL") || pe_key_prefix(s, ".CRT$XD"))
            && ((s->data_offset & 7) || s->sh_addralign > 8))
            return tcc_error_noabort("invalid COFF TLS callback-array contribution");
        if (pe_key_prefix(s, ".CRT$") && !pe_key_prefix(s, ".CRT$XL") && !pe_key_prefix(s, ".CRT$XD"))
            return tcc_error_noabort("unsupported live COFF CRT initialization family '%s'", s->name);
        if (s->reloc) for_each_elem(s->reloc, 0, r, ElfW_Rel) {
            unsigned t = ELFW(R_TYPE)(r->r_info);
            if (t == R_X86_64_TPOFF32 || t == R_X86_64_TPOFF64
                || t == R_X86_64_TLSGD || t == R_X86_64_TLSLD
                || t == R_X86_64_DTPMOD64 || t == R_X86_64_DTPOFF64
                || t == R_X86_64_DTPOFF32 || t == R_X86_64_GOTTPOFF
                || t == R_X86_64_GOTPC32_TLSDESC || t == R_X86_64_TLSDESC_CALL || t == R_X86_64_TLSDESC)
                return tcc_error_noabort("ELF TLS access is incompatible with Windows static TLS");
            if (!pe->filename && (t == R_TCC_PE_SECTION || t == R_TCC_PE_SECREL))
                return tcc_error_noabort("live COFF section-relative access requires a PE image, not JIT memory");
        }
    }
    if (!has_tls && !has_callbacks && !has_dynamic) return 0;
    if (!pe->filename)
        return tcc_error_noabort("live Windows static TLS requires OS image-loader support; JIT is unsupported");
    callbacks = pe_merge_ordered(s1, ".CRT$XL", 0);
    dynamic = pe_merge_ordered(s1, ".CRT$XD", 0);
    pe->tls_template = pe_merge_ordered(s1, ".tls", 1);
    if (!pe->tls_template) {
        pe->tls_template = new_section(s1, ".tls", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE | SHF_TLS);
        }
    if (!pe->tls_template->data_offset) section_ptr_add(pe->tls_template, 1);
    pe->tls_alignment = pe->tls_template->sh_addralign;
    sym = pe_defined_symbol(s1, "_tls_used", &index);
    if (sym) {
        Section *s = s1->sections[sym->st_shndx];
        if (!s->data || sym->st_value > s->data_offset || s->data_offset - sym->st_value < 40)
            return tcc_error_noabort("invalid _tls_used directory record");
        pe->tls_directory = index;
        for (i = 0; i < 4; ++i)
            if (!pe_tls_has_va_reloc(s, sym->st_value + i * 8) && read64le(s->data + sym->st_value + i * 8))
                return tcc_error_noabort("_tls_used VA field lacks an absolute base relocation");
    } else {
        Section *data, *dir;
        unsigned tls_sym, callbacks_sym, index_sym;
        unsigned long callback_at = 0;
        if (dynamic && dynamic->reloc && dynamic->reloc->data_offset)
            return tcc_error_noabort("dynamic TLS initialization requires its real CRT descriptor and callbacks");
        if (!callbacks) {
            callbacks = new_section(s1, ".tls_callbacks", SHT_PROGBITS, SHF_ALLOC);
            callbacks->sh_addralign = 8; section_ptr_add(callbacks, 8);
        } else {
            unsigned leading;
            if (pe_defined_symbol(s1, "__xl_a", &leading)) {
                ElfW(Sym) start = ((ElfW(Sym) *)s1->symtab->data)[leading];
                if (start.st_shndx != callbacks->sh_num || !pe_null_slot(callbacks, start.st_value))
                    return tcc_error_noabort("invalid TLS callback leading sentinel");
                callback_at = start.st_value + 8;
            }
            /* Sorted XLA contributions can contain a leading NULL sentinel
               even when no CRT publishes the conventional __xl_a symbol. */
            while (callback_at + 8 < callbacks->data_offset && pe_null_slot(callbacks, callback_at))
                callback_at += 8;
            if (callbacks->data_offset % 8)
                return tcc_error_noabort("TLS callback table is not pointer-aligned");
            if (!callbacks->data_offset || !pe_null_slot(callbacks, callbacks->data_offset - 8))
                section_ptr_add(callbacks, 8);
        }
        if (!(sym = pe_defined_symbol(s1, "_tls_index", &index_sym))) {
            data = new_section(s1, ".data.tls_index", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
            data->sh_addralign = 4; section_ptr_add(data, 4);
            index_sym = set_elf_sym(s1->symtab, 0, 4, ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT), 0, data->sh_num, "_tls_index");
        }
        tls_sym = put_elf_sym(s1->symtab, 0, 0, ELFW(ST_INFO)(STB_LOCAL, STT_SECTION), 0, pe->tls_template->sh_num, NULL);
        callbacks_sym = put_elf_sym(s1->symtab, callback_at, 0, ELFW(ST_INFO)(STB_LOCAL, STT_NOTYPE), 0, callbacks->sh_num, NULL);
        dir = new_section(s1, ".rdata.tls_directory", SHT_PROGBITS, SHF_ALLOC);
        dir->sh_addralign = 8; section_ptr_add(dir, 40);
        pe->tls_directory = set_elf_sym(s1->symtab, 0, 40, ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT), 0, dir->sh_num, "_tls_used");
        put_elf_reloca(s1->symtab, dir, 0, R_X86_64_64, tls_sym, 0);
        put_elf_reloca(s1->symtab, dir, 8, R_X86_64_64, tls_sym, pe->tls_template->data_offset);
        put_elf_reloca(s1->symtab, dir, 16, R_X86_64_64, index_sym, 0);
        put_elf_reloca(s1->symtab, dir, 24, R_X86_64_64, callbacks_sym, 0);
        pe->tls_synthetic = 1;
    }
    sym = (ElfW(Sym) *)s1->symtab->data + pe->tls_directory;
    {
        Section *s = s1->sections[sym->st_shndx];
        unsigned code = 1, align = 1, old = read32le(s->data + sym->st_value + 36);
        if (old & ~0x00f00000u) return tcc_error_noabort("unsupported TLS directory characteristics");
        while (align < pe->tls_alignment && code < 14) { align <<= 1; ++code; }
        if (align < pe->tls_alignment) return tcc_error_noabort("TLS alignment exceeds PE directory encoding");
        if (((old >> 20) & 15) == 15) return tcc_error_noabort("invalid TLS directory alignment");
        if (((old >> 20) & 15) > code) code = (old >> 20) & 15;
        write32le(s->data + sym->st_value + 36, code << 20);
    }
    return s1->nb_errors ? -1 : 0;
}
static int pe_apply_section_relocs(struct pe_info *pe)
{
    TCCState *s1 = pe->s1;
    int i;
    for (i = 1; i < s1->nb_sections; ++i) {
        Section *s = s1->sections[i]; ElfW_Rel *r;
        if (!(s->sh_flags & SHF_ALLOC) || !s->reloc) continue;
        for_each_elem(s->reloc, 0, r, ElfW_Rel) {
            unsigned t = ELFW(R_TYPE)(r->r_info), n = ELFW(R_SYM)(r->r_info);
            ElfW(Sym) *sym;
            Section *target;
            int64_t value;
            if (t != R_TCC_PE_SECTION && t != R_TCC_PE_SECREL) continue;
            sym = (ElfW(Sym) *)s1->symtab->data + n;
            if (!sym->st_shndx || sym->st_shndx >= s1->nb_sections)
                return tcc_error_noabort("COFF section-relative relocation has no image section");
            target = s1->sections[sym->st_shndx];
            if (target->sh_info <= 0 || target->sh_info > pe->sec_count)
                return tcc_error_noabort("COFF section-relative relocation targets an unallocated section");
            value = t == R_TCC_PE_SECTION ? target->sh_info :
                sym->st_value - pe->sec_info[target->sh_info-1]->sh_addr;
            value += r->r_addend;
            if (value < 0 || (uint64_t)value > (t == R_TCC_PE_SECTION ? 0xffffu : UINT32_MAX))
                return tcc_error_noabort("COFF section-relative relocation overflow");
            if (t == R_TCC_PE_SECTION) write16le(s->data + r->r_offset, value);
            else write32le(s->data + r->r_offset, value);
            r->r_info = ELFW(R_INFO)(n, R_X86_64_NONE); r->r_addend = 0;
        }
    }
    return 0;
}
static Section *pe_va_section(TCCState *s1, addr_t at, unsigned size, unsigned flags)
{
    int i;
    for (i = 1; i < s1->nb_sections; ++i) {
        Section *s = s1->sections[i];
        if ((s->sh_flags & (SHF_ALLOC | flags)) == (SHF_ALLOC | flags)
            && at >= s->sh_addr && at - s->sh_addr <= s->data_offset
            && size <= s->data_offset - (at - s->sh_addr)) return s;
    }
    return NULL;
}
static int pe_validate_tls(struct pe_info *pe)
{
    TCCState *s1 = pe->s1;
    ElfW(Sym) *sym;
    Section *s, *array;
    unsigned char *dir;
    addr_t start, end, index, callbacks;
    if (!pe->tls_directory) return 0;
    sym = (ElfW(Sym) *)s1->symtab->data + pe->tls_directory;
    s = s1->sections[sym->st_shndx]; dir = s->data + sym->st_value - s->sh_addr;
    start = read64le(dir); end = read64le(dir + 8); index = read64le(dir + 16); callbacks = read64le(dir + 24);
    if (start > end || start != pe->tls_template->sh_addr || end > pe->tls_template->sh_addr + pe->tls_template->data_offset)
        return tcc_error_noabort("TLS template must start at its output section base and remain inside that section");
    s = pe_va_section(s1, index, 4, SHF_WRITE);
    if (!s || (index & 3) || (s->sh_flags & SHF_TLS)) return tcc_error_noabort("TLS index is not ordinary writable image storage");
    if (!callbacks) return 0;
    array = pe_va_section(s1, callbacks, 8, 0);
    if (!array || !array->data || (callbacks & 7)) return tcc_error_noabort("TLS callback table is outside image data");
    for (addr_t off = callbacks - array->sh_addr; off <= array->data_offset && array->data_offset - off >= 8; off += 8) {
        addr_t callback = read64le(array->data + off);
        if (!callback) {
            if (pe_key_prefix(array, ".CRT$XL"))
                for (addr_t tail = off + 8; tail + 8 <= array->data_offset; tail += 8)
                    if (read64le(array->data + tail))
                        return tcc_error_noabort("TLS callback contribution follows an early NULL terminator");
            return 0;
        }
        if (!pe_va_section(s1, callback, 1, SHF_EXECINSTR)) return tcc_error_noabort("TLS callback is outside executable image code");
    }
    return tcc_error_noabort("TLS callback table is not null-terminated");
}

#else
static int pe_prepare_tls(struct pe_info *pe) { (void)pe; return 0; }
static int pe_apply_section_relocs(struct pe_info *pe) { (void)pe; return 0; }
static int pe_validate_tls(struct pe_info *pe) { (void)pe; return 0; }
#endif
