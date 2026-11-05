/* Import libraries describe two names: the local linker symbol and the DLL
 * export. Keep that mapping as data, not a generated forwarding function. */
static int pe_define_import(TCCState *s1, int dll, const char *local,
    const char *actual, unsigned ordinal, int data)
{
    int index = pe_putimport(s1, dll, local, ordinal);
    ElfW(Sym) *sym = (ElfW(Sym) *)s1->dynsymtab_section->data + index;
    Section *names;
    unsigned name_offset;
    if (sym->st_size != dll) return 0; /* normal first DLL / host symbol wins */
    if (strcmp(local, actual)) {
        names = s1->pe_import_names;
        if (!names)
            s1->pe_import_names = names = new_section(s1,
                ".pe.import.names", SHT_PROGBITS, SHF_PRIVATE);
        if ((unsigned long)(index + 1) * sizeof(unsigned) > names->data_offset)
            section_ptr_add(names, (index + 1) * sizeof(unsigned) - names->data_offset);
        name_offset = ((unsigned *)names->data)[index];
        if (name_offset && strcmp((char *)s1->dynsymtab_section->link->data + name_offset, actual))
            return tcc_error_noabort("conflicting DLL import alias for '%s'", local);
        if (!name_offset)
            ((unsigned *)names->data)[index] = put_elf_str(s1->dynsymtab_section->link, actual);
    }
    sym->st_info = ELFW(ST_INFO)(STB_GLOBAL, data ? STT_OBJECT : STT_FUNC);
    return 0;
}

static char *pe_def_space(char *p)
{
    while (*p && (unsigned char)*p <= ' ') ++p;
    return p;
}

/* '=' is a delimiter for aliases, '@' remains part of decorated names. */
static char *pe_def_token(char **cursor)
{
    char *p = pe_def_space(*cursor), *start = p;
    if (*p == '"') {
        start = ++p;
        while (*p && *p != '"') ++p;
        if (!*p) return NULL;
    } else {
        while (*p && (unsigned char)*p > ' ' && *p != '=') ++p;
    }
    if (p == start) return NULL;
    if (*p == '=') { *cursor = p; return start; }
    if (*p) *p++ = 0;
    *cursor = p;
    return start;
}

static int pe_load_def(TCCState *s1, int fd)
{
    char *buf = tcc_load_text(fd), *line, *next, *p, *local, *actual, *token;
    char *dllname = NULL;
    int state = 0, dll = 0, ret = -1;
    unsigned line_number = 0;
    if (!buf) return -1;
    for (line = buf; line && *line; line = next) {
        unsigned ordinal = 0;
        int data = 0, noname = 0;
        char *equal;
        ++line_number;
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        /* ';' starts a comment outside a quoted DLL name. */
        {
            int quoted = 0;
            for (p = line; *p; ++p) {
                if (*p == '"') quoted = !quoted;
                if (*p == ';' && !quoted) { *p = 0; break; }
            }
        }
        p = pe_def_space(line);
        if (!*p) continue;
        if (state == 0) {
            token = pe_def_token(&p);
            if (!token || stricmp(token, "LIBRARY")) goto invalid;
            dllname = pe_def_token(&p);
            if (!dllname || *pe_def_space(p)) goto invalid;
            state = 1;
            continue;
        }
        if (state == 1) {
            token = pe_def_token(&p);
            if (!token || stricmp(token, "EXPORTS") || *pe_def_space(p)) goto invalid;
            dll = tcc_add_dllref(s1, dllname, 0)->index;
            state = 2;
            continue;
        }
        /* Split the alias before tokenizing, so both x=y and x = y work. */
        equal = strchr(p, '=');
        if (equal) {
            *equal = 0;
            local = pe_def_token(&p);
            if (!local || *pe_def_space(p)) goto invalid;
            p = equal + 1;
            actual = pe_def_token(&p);
            if (!actual || strchr(actual, '=')) goto invalid;
        } else {
            actual = local = pe_def_token(&p);
            if (!local) goto invalid;
        }
        while (*(p = pe_def_space(p))) {
            token = pe_def_token(&p);
            if (!token) goto invalid;
            if (token[0] == '@') {
                char *end;
                unsigned long value;
                if (ordinal || token[1] < '0' || token[1] > '9') goto invalid;
                value = strtoul(token + 1, &end, 10);
                if (*end || !value || value > 65535) goto invalid;
                ordinal = value;
            } else if (!stricmp(token, "DATA") || !stricmp(token, "CONSTANT")) {
                data = 1;
            } else if (!stricmp(token, "NONAME")) {
                noname = 1;
            } else if (stricmp(token, "PRIVATE")) goto invalid;
        }
        if (noname && !ordinal) goto invalid;
        if (pe_define_import(s1, dll, local, actual, noname ? ordinal : 0, data) < 0)
            goto quit;
    }
    if (state != 2) goto invalid;
    ret = 0;
    goto quit;
invalid:
    tcc_error_noabort("invalid DLL import definition at line %u", line_number);
quit:
    tcc_free(buf);
    return ret;
}
