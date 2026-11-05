/* Native AMD64 COFF objects are translated into the linker's existing symbol,
 * section and relocation model. File offsets are relative to the bounded
 * archive member, never to the containing archive. No object conversion tool
 * or alternate allocator/runtime is involved. */
#ifdef TCC_TARGET_X86_64
typedef struct TCCCoffSection {
    Section *output;
    uint32_t size, raw, reloc, flags;
    unsigned long output_offset;
    uint32_t association;
    uint16_t reloc_count;
    unsigned char selection, discarded, reverse_array;
    char *name, *key;
} TCCCoffSection;

static unsigned coff_u16(const unsigned char *p)
{ return p[0] | (unsigned)p[1] << 8; }
static uint32_t coff_u32(const unsigned char *p)
{ return (uint32_t)coff_u16(p) | (uint32_t)coff_u16(p + 2) << 16; }
static int coff_range(size_t size, uint64_t offset, uint64_t count)
{ return offset <= size && count <= size - offset; }
static const char *coff_string(const unsigned char *strings, size_t size, uint32_t at)
{
    if (at < 4 || at >= size || !memchr(strings + at, 0, size - at)) return NULL;
    return (const char *)strings + at;
}
static char *coff_name(const unsigned char *raw, const unsigned char *strings,
    size_t size, int section)
{
    const char *name = NULL;
    char short_name[9];
    if (section && raw[0] == '/') {
        uint32_t at = 0;
        unsigned i;
        for (i = 1; i < 8 && raw[i]; ++i) {
            if (raw[i] < '0' || raw[i] > '9' || at > (UINT32_MAX - 9) / 10) return NULL;
            at = at * 10 + raw[i] - '0';
        }
        if (i == 1) return NULL;
        name = coff_string(strings, size, at);
    } else if (!section && !coff_u32(raw)) {
        name = coff_string(strings, size, coff_u32(raw + 4));
    } else {
        memcpy(short_name, raw, 8); short_name[8] = 0;
        name = short_name;
    }
    return name && *name ? tcc_strdup(name) : NULL;
}
static int coff_ignored_section(const char *name, uint32_t flags)
{
    return !strncmp(name, ".debug", 6) || !strcmp(name, ".comment") ||
        !strcmp(name, ".llvm_addrsig") || (flags & 0x00000800u);
}
static int coff_directives(TCCState *s1, const unsigned char *data, size_t size)
{
    size_t at = 0;
    while (at < size) {
        size_t begin, end;
        while (at < size && (data[at] == 0 || data[at] == ' ' || data[at] == '\t' ||
            data[at] == '\n' || data[at] == '\r')) ++at;
        if (at == size) break;
        begin = at;
        while (at < size && data[at] && data[at] != ' ' && data[at] != '\t' &&
            data[at] != '\n' && data[at] != '\r') ++at;
        end = at;
        if (end - begin > 17 && !memcmp(data + begin, "-exclude-symbols:", 17)) continue;
        if (end - begin > 8 && (!memcmp(data + begin, "-export:", 8) ||
            !memcmp(data + begin, "/EXPORT:", 8))) {
            char *name = tcc_malloc(end - begin - 7);
            size_t length = end - begin - 8;
            memcpy(name, data + begin + 8, length); name[length] = 0;
            if (length > 1 && name[0] == '"' && name[length - 1] == '"') {
                memmove(name, name + 1, length - 2); name[length - 2] = 0;
            }
            int index = find_elf_sym(s1->symtab, name);
            tcc_free(name);
            if (!index) return -1;
            ((ElfW(Sym) *)s1->symtab->data)[index].st_other |= ST_PE_EXPORT;
        } else return -1;
    }
    return 0;
}

ST_FUNC int tcc_load_coff_object(TCCState *s1, int fd, unsigned long file_offset)
{
    unsigned char header[60], *data = NULL, *primary = NULL;
    TCCCoffSection *sections = NULL;
    int *symbols = NULL;
    char **names = NULL;
    size_t size = 0, strings_size, loaded = 0;
    uint32_t symbol_at = 0, symbol_count = 0;
    unsigned section_count = 0;
    const unsigned char *strings;
    const char *error = "invalid AMD64 COFF object";
    int result = -1;
    long end = lseek(fd, 0, SEEK_END);
    if (end < 0 || file_offset > (unsigned long)end) goto done;
    size = (size_t)((unsigned long)end - file_offset);
    if (file_offset) {
        /* ar members have a fixed-size header immediately before the payload. */
        uint64_t member_size = 0;
        unsigned i;
        if (file_offset < 60 || lseek(fd, file_offset - 60, SEEK_SET) < 0 ||
            full_read(fd, header, 60) != 60 || memcmp(header + 58, "`\n", 2)) goto done;
        for (i = 48; i < 58 && header[i] != ' '; ++i) {
            if (header[i] < '0' || header[i] > '9' || member_size > (SIZE_MAX - 9) / 10) goto done;
            member_size = member_size * 10 + header[i] - '0';
        }
        if (i == 48 || member_size > size) goto done;
        size = (size_t)member_size;
    }
    if (size < 20 || lseek(fd, file_offset, SEEK_SET) < 0 ||
        full_read(fd, header, 20) != 20 || coff_u16(header) != 0x8664) goto done;
    if (s1->output_type == TCC_OUTPUT_OBJ) {
        error = "COFF partial linking to TCC ELF objects is unsupported; link the original COFF input directly";
        goto done;
    }
    section_count = coff_u16(header + 2);
    symbol_at = coff_u32(header + 8); symbol_count = coff_u32(header + 12);
    if (!section_count || coff_u16(header + 16) || symbol_count > INT_MAX ||
        (uint64_t)s1->nb_sections + (uint64_t)section_count * 2 + 8 >= SHN_LORESERVE ||
        !coff_range(size, 20, (uint64_t)section_count * 40) ||
        !coff_range(size, symbol_at, (uint64_t)symbol_count * 18 + 4)) goto done;
    data = tcc_malloc(size);
    if (lseek(fd, file_offset, SEEK_SET) < 0 || full_read(fd, data, size) != size) goto done;
    strings = data + symbol_at + (size_t)symbol_count * 18;
    strings_size = coff_u32(strings);
    if (strings_size < 4 || !coff_range(size, (size_t)(strings - data), strings_size)) goto done;
    sections = tcc_mallocz((size_t)(section_count + 1) * sizeof(*sections));
    symbols = tcc_mallocz((size_t)symbol_count * sizeof(*symbols));
    primary = tcc_mallocz(symbol_count);
    names = tcc_mallocz((size_t)symbol_count * sizeof(*names));
    /* Validate the complete physical input before admitting linker sections. */
    for (unsigned i = 1; i <= section_count; ++i) {
        const unsigned char *raw = data + 20 + (size_t)(i - 1) * 40;
        TCCCoffSection *sec = &sections[i];
        sec->name = coff_name(raw, strings, strings_size, 1);
        sec->size = coff_u32(raw + 16); sec->raw = coff_u32(raw + 20);
        sec->reloc = coff_u32(raw + 24); sec->reloc_count = coff_u16(raw + 32);
        sec->flags = coff_u32(raw + 36);
        if (sec->name && (!strcmp(sec->name, ".idata") || !strncmp(sec->name, ".idata$", 7))) {
            error = "COFF import-table fragments require a DLL definition provider"; goto done;
        }
        if (sec->name && (!strncmp(sec->name, ".ctors.", 7) ||
            !strncmp(sec->name, ".dtors.", 7))) {
            error = "unsupported COFF initialization priority contract"; goto done;
        }
        if (!sec->name || ((sec->flags & 0x01000000u) != 0) ||
            ((sec->flags & 0x80u) && sec->reloc_count) ||
            (!(sec->flags & 0x80u) && sec->size && !coff_range(size, sec->raw, sec->size)) ||
            !coff_range(size, sec->reloc, (uint64_t)sec->reloc_count * 10)) goto done;
        if ((sec->flags & 0x00f00000u) == 0x00f00000u) goto done;
    }
    for (uint32_t i = 0; i < symbol_count;) {
        const unsigned char *raw = data + symbol_at + (size_t)i * 18;
        unsigned aux = raw[17], storage = raw[16];
        int section = (int16_t)coff_u16(raw + 12);
        if (aux > symbol_count - i - 1 || section > (int)section_count || section < -2) goto done;
        primary[i] = 1;
        names[i] = coff_name(raw, strings, strings_size, 0);
        if (!names[i]) goto done;
        if (storage == 3 && section > 0 && aux && !coff_u16(raw + 14)) {
            const unsigned char *info = raw + 18;
            sections[section].selection = info[14];
            sections[section].association = coff_u16(info + 12);
        }
        if (storage == 2 && section > 0 && !sections[section].key)
            sections[section].key = names[i];
        i += aux + 1;
    }
    for (unsigned i = 1; i <= section_count; ++i) {
        TCCCoffSection *sec = &sections[i];
        if (coff_ignored_section(sec->name, sec->flags) || !strcmp(sec->name, ".drectve")) continue;
        for (unsigned j = 0; j < sec->reloc_count; ++j) {
            const unsigned char *rel = data + sec->reloc + (size_t)j * 10;
            unsigned type = coff_u16(rel + 8), width;
            uint32_t symbol = coff_u32(rel + 4), at = coff_u32(rel);
            width = type == 1 ? 8 : type == 10 ? 2 :
                (type >= 2 && type <= 9) || type == 11 ? 4 : 0;
            if (type == 0) continue;
            if (!width) { error = "unsupported AMD64 COFF relocation"; goto done; }
            if (symbol >= symbol_count || !primary[symbol] || !coff_range(sec->size, at, width)) goto done;
            if ((!strcmp(sec->name, ".ctors") || !strcmp(sec->name, ".dtors") ||
                 !strcmp(sec->name, ".init_array") || !strcmp(sec->name, ".fini_array")) &&
                (type != 1 || at % 8)) goto done;
        }
    }
    /* Keep every distinct input section: this is the unit of optional GC. */
    for (unsigned pass = 0; pass < 2; ++pass) for (unsigned i = 1; i <= section_count; ++i) {
        TCCCoffSection *sec = &sections[i];
        unsigned associated = (sec->flags & 0x1000u) && sec->selection == 5;
        if (associated != pass || coff_ignored_section(sec->name, sec->flags) ||
            !strcmp(sec->name, ".drectve")) continue;
        int init_array = !strcmp(sec->name, ".ctors") || !strcmp(sec->name, ".init_array");
        int fini_array = !strcmp(sec->name, ".dtors") || !strcmp(sec->name, ".fini_array");
        /* Linkonce metadata may have only a static section-definition symbol
           (e.g. GCC .pdata$/ .xdata$), with no external data symbol. The exact
           input section name then supplies its COMDAT identity. */
        char *key = sec->key ? sec->key : sec->name;
        if (associated) {
            if (!sec->association || sec->association > section_count || sec->association == i ||
                !sections[sec->association].output) goto done;
            key = sections[sec->association].key ? sections[sec->association].key : sections[sec->association].name;
        }
        char *section_name;
        size_t name_size = strlen(sec->name) + (key ? strlen(key) : 0) + 64;
        section_name = tcc_malloc(name_size);
        if (sec->flags & 0x1000u) {
            if ((sec->selection != 1 && sec->selection != 2 && sec->selection != 3 && sec->selection != 5) || !key) {
                tcc_free(section_name); error = "unsupported COFF COMDAT selection"; goto done;
            }
            snprintf(section_name, name_size, "%s.coff.%s", sec->name, key);
            for (int q = 1; q < s1->nb_sections; ++q)
                if (!strcmp(s1->sections[q]->name, section_name)) { sec->output = s1->sections[q]; break; }
            if (sec->output) {
                tcc_free(section_name);
                if (sec->selection == 1) { error = "duplicate COFF NODUPLICATES section"; goto done; }
                if (sec->selection == 3 && sec->output->data_offset != sec->size) {
                    error = "COFF SAME_SIZE section has incompatible size"; goto done;
                }
                sec->discarded = 1; continue;
            }
        } else snprintf(section_name, name_size, "%s.coff.%d", sec->name, s1->nb_sections);
        if (init_array || fini_array) {
            const char *name = init_array ? ".init_array" : ".fini_array";
            if (sec->size % 8 || (sec->flags & 0x1080u) || sec->reloc_count != sec->size / 8) {
                tcc_free(section_name); goto done;
            }
            sec->reverse_array = !strcmp(sec->name, ".ctors") || !strcmp(sec->name, ".dtors");
            sec->output = find_section(s1, name);
            sec->output->sh_type = init_array ? SHT_INIT_ARRAY : SHT_FINI_ARRAY;
            sec->output->sh_flags |= SHF_ALLOC | SHF_WRITE;
            sec->output->sh_addralign = 8;
            sec->output->sh_entsize = 8;
            sec->output_offset = sec->output->data_offset;
            unsigned char *target = section_ptr_add(sec->output, sec->size);
            for (uint32_t at = 0; at < sec->size; at += 8)
                memcpy(target + (sec->reverse_array ? sec->size - 8 - at : at), data + sec->raw + at, 8);
            tcc_free(section_name); ++loaded; continue;
        }
        if (s1->nb_sections >= SHN_LORESERVE - 2) { tcc_free(section_name); goto done; }
        unsigned long flags = SHF_ALLOC;
        if (sec->flags & 0x80000000u) flags |= SHF_WRITE;
        if (sec->flags & 0x20000000u) flags |= SHF_EXECINSTR;
        if (!strcmp(sec->name, ".tls") || !strncmp(sec->name, ".tls$", 5)) flags |= SHF_TLS;
        sec->output = new_section(s1, section_name, sec->flags & 0x80u ? SHT_NOBITS : SHT_PROGBITS, flags);
        sec->output->coff_name_len = strlen(sec->name);
        tcc_free(section_name); ++loaded;
        unsigned alignment = (sec->flags >> 20) & 15;
        sec->output->sh_addralign = alignment ? 1u << (alignment - 1) : 1;
        if (sec->flags & 0x80u) sec->output->data_offset = sec->size;
        else if (sec->size) memcpy(section_ptr_add(sec->output, sec->size), data + sec->raw, sec->size);
        if (associated) sec->output->gc_owner = sections[sec->association].output;
    }
    for (uint32_t i = 0; i < symbol_count;) {
        const unsigned char *raw = data + symbol_at + (size_t)i * 18;
        unsigned storage = raw[16], aux = raw[17];
        int section = (int16_t)coff_u16(raw + 12), binding = storage == 2 ? STB_GLOBAL : storage == 105 ? STB_WEAK : STB_LOCAL;
        uint32_t value = coff_u32(raw + 8);
        addr_t symbol_value = value, symbol_size = 0;
        int type = (coff_u16(raw + 14) & 0x20) ? STT_FUNC : STT_OBJECT, index = SHN_UNDEF;
        if (storage == 103 || section == -2) { i += aux + 1; continue; }
        if (storage != 2 && storage != 3 && storage != 6 && storage != 105) {
            error = "unsupported COFF symbol storage class"; goto done;
        }
        if (storage == 105) { i += aux + 1; continue; }
        if (section > 0) {
            TCCCoffSection *sec = &sections[section];
            if (!sec->output) { i += aux + 1; continue; }
            if (value > sec->size) goto done;
            index = sec->output->sh_num;
            if (type == STT_OBJECT && (sec->output->sh_flags & SHF_EXECINSTR)) type = STT_NOTYPE;
            symbol_value += sec->output_offset;
            if (storage == 3 && aux && !coff_u16(raw + 14)) type = STT_SECTION;
            if (type == STT_FUNC && aux) symbol_size = coff_u32(raw + 22);
        } else if (section == -1) index = SHN_ABS;
        else if (value) {
            index = SHN_COMMON; symbol_size = value; symbol_value = 1;
            while (symbol_value < 16 && symbol_value < value) symbol_value <<= 1;
        }
        if (section > 0 && sections[section].discarded && binding != STB_LOCAL) {
            symbols[i] = find_elf_sym(s1->symtab, names[i]);
            if (!symbols[i]) goto done;
        } else symbols[i] = set_elf_sym(s1->symtab, symbol_value, symbol_size,
            ELFW(ST_INFO)(binding, type), 0, index, names[i]);
        i += aux + 1;
    }
    for (uint32_t i = 0; i < symbol_count;) {
        const unsigned char *raw = data + symbol_at + (size_t)i * 18;
        unsigned aux = raw[17];
        if (raw[16] == 105) {
            if (!aux) goto done;
            uint32_t fallback = coff_u32(raw + 18);
            if (fallback >= symbol_count || !primary[fallback] || !symbols[fallback]) goto done;
            ElfW(Sym) copy = ((ElfW(Sym) *)s1->symtab->data)[symbols[fallback]];
            if (copy.st_shndx == SHN_UNDEF) { error = "unresolved COFF weak alias fallback"; goto done; }
            symbols[i] = set_elf_sym(s1->symtab, copy.st_value, copy.st_size,
                ELFW(ST_INFO)(STB_WEAK, ELFW(ST_TYPE)(copy.st_info)), copy.st_other, copy.st_shndx, names[i]);
        }
        i += aux + 1;
    }
    for (unsigned i = 1; i <= section_count; ++i) {
        TCCCoffSection *sec = &sections[i];
        if (!strcmp(sec->name, ".drectve")) {
            if (coff_directives(s1, data + sec->raw, sec->size)) { error = "unsupported COFF linker directive"; goto done; }
            continue;
        }
        if (!sec->output || sec->discarded) continue;
        Section *unwind_owner = NULL;
        int multiple_owners = 0;
        for (unsigned j = 0; j < sec->reloc_count; ++j) {
            const unsigned char *raw = data + sec->reloc + (size_t)j * 10;
            unsigned type = coff_u16(raw + 8), relocation;
            uint32_t index = coff_u32(raw + 4), at = coff_u32(raw);
            if (!type) continue;
            if (!symbols[index]) goto done;
            if (sec->reverse_array) at = sec->size - 8 - at;
            at += sec->output_offset;
            unsigned char *place = sec->output->data + at;
            addr_t addend;
            if (type == 1) { relocation = R_X86_64_64; addend = read64le(place); write64le(place, 0); }
            else if (type == 10) { relocation = R_TCC_PE_SECTION; addend = coff_u16(place); write16le(place, 0); }
            else if (type == 11) { relocation = R_TCC_PE_SECREL; addend = (int32_t)coff_u32(place); write32le(place, 0); }
            else {
                addend = (int32_t)coff_u32(place); write32le(place, 0);
                if (type == 2) relocation = R_X86_64_32;
                else if (type == 3) relocation = R_X86_64_RELATIVE;
                else { relocation = R_X86_64_PC32; addend -= 4 + type - 4; }
            }
            put_elf_reloca(s1->symtab, sec->output, at, relocation, symbols[index], addend);
            if (!strncmp(sec->name, ".pdata", 6) && at % 12 == 0) {
                ElfW(Sym) symbol = ((ElfW(Sym) *)s1->symtab->data)[symbols[index]];
                if (symbol.st_shndx > 0 && symbol.st_shndx < s1->nb_sections) {
                    Section *code = s1->sections[symbol.st_shndx];
                    if (unwind_owner && code != unwind_owner) multiple_owners = 1;
                    unwind_owner = code;
                }
            }
        }
        if (unwind_owner && !multiple_owners) sec->output->gc_owner = unwind_owner;
    }
    result = 0;
done:
    if (names) for (uint32_t i = 0; i < symbol_count; ++i) tcc_free(names[i]);
    if (sections) for (unsigned i = 1; i <= section_count; ++i) tcc_free(sections[i].name);
    tcc_free(names); tcc_free(primary); tcc_free(symbols); tcc_free(sections); tcc_free(data);
    if (result) tcc_error_noabort("%s%s", error, loaded ? " (discard failed link state)" : "");
    return result;
}
#else
ST_FUNC int tcc_load_coff_object(TCCState *s1, int fd, unsigned long file_offset)
{
    (void)s1; (void)fd; (void)file_offset;
    return tcc_error_noabort("native COFF objects require the AMD64 target");
}
#endif
