/* Standard DWARF line sequences and CU ranges for independent code sections.
   Only the opt-in function-section mode changes the CU representation. */
static void dwarf_sections_addr(Section *s, addr_t value)
{
#if PTR_SIZE == 4
    dwarf_data4(s, value);
#else
    dwarf_data8(s, value);
#endif
}

static void dwarf_sections_begin(TCCState *s1)
{
    Section *ranges = find_section(s1, s1->dwarf >= 5 ? ".debug_rnglists" : ".debug_ranges");
    dwarf_info.ranges_section = ranges;
    ranges->sh_flags |= dwarf_info_section->sh_flags & SHF_ALLOC;
    dwarf_info.range_header = ranges->data_offset;
    if (s1->dwarf >= 5) {
        dwarf_data4(ranges, 0);
        dwarf_data2(ranges, 5);
        dwarf_data1(ranges, PTR_SIZE);
        dwarf_data1(ranges, 0);
        dwarf_data4(ranges, 0); /* offset_entry_count */
    }
    dwarf_info.range_start = ranges->data_offset;
}

static void dwarf_sections_function_begin(TCCState *s1)
{
    struct dwarf_code_sequence *q;
    int i;
    if (dwarf_line.sequence_count == dwarf_line.sequence_capacity) {
        dwarf_line.sequence_capacity = dwarf_line.sequence_capacity ? dwarf_line.sequence_capacity * 2 : 32;
        dwarf_line.sequences = tcc_realloc(dwarf_line.sequences,
            dwarf_line.sequence_capacity * sizeof *dwarf_line.sequences);
    }
    q = dwarf_line.sequences + dwarf_line.sequence_count++;
    q->symbol = dwarf_get_section_sym(cur_text_section);
    q->begin = func_ind;
    q->size = 0;
    dwarf_line_op(s1, 0);
    dwarf_uleb128_op(s1, 1 + PTR_SIZE);
    dwarf_line_op(s1, DW_LNE_set_address);
    q->offset = dwarf_line.line_size;
    for (i = 0; i < PTR_SIZE; ++i)
        dwarf_line_op(s1, (addr_t)func_ind >> (i * 8));
    dwarf_line.last_pc = func_ind;
    dwarf_line.last_line = 1;
    dwarf_line.last_file = 0;
}

static void dwarf_sections_function_end(TCCState *s1, int size)
{
    if (dwarf_line.sequence_count) {
        struct dwarf_code_sequence *q = dwarf_line.sequences + dwarf_line.sequence_count - 1;
        int delta = q->begin + size - dwarf_line.last_pc;
        q->size = size;
        if (delta > 0) {
            dwarf_line_op(s1, DW_LNS_advance_pc);
            dwarf_uleb128_op(s1, delta / DWARF_MIN_INSTR_LEN);
        }
        dwarf_line_op(s1, 0);
        dwarf_uleb128_op(s1, 1);
        dwarf_line_op(s1, DW_LNE_end_sequence);
    }
}

static void dwarf_sections_end(TCCState *s1)
{
    Section *ranges = dwarf_info.ranges_section;
    int i;
    for (i = 0; i < dwarf_line.sequence_count; ++i) {
        struct dwarf_code_sequence *q = dwarf_line.sequences + i;
        if (s1->dwarf >= 5)
            dwarf_data1(ranges, 6); /* DW_RLE_start_end */
        dwarf_reloc(ranges, q->symbol, R_DATA_PTR);
        dwarf_sections_addr(ranges, q->begin);
        dwarf_reloc(ranges, q->symbol, R_DATA_PTR);
        dwarf_sections_addr(ranges, q->begin + q->size);
        dwarf_reloc(dwarf_aranges_section, q->symbol, R_DATA_PTR);
        dwarf_sections_addr(dwarf_aranges_section, q->begin);
        dwarf_sections_addr(dwarf_aranges_section, q->size);
    }
    if (s1->dwarf >= 5) {
        dwarf_data1(ranges, 0); /* DW_RLE_end_of_list */
        write32le(ranges->data + dwarf_info.range_header,
            ranges->data_offset - dwarf_info.range_header - 4);
    } else {
        dwarf_sections_addr(ranges, 0);
        dwarf_sections_addr(ranges, 0);
    }
    dwarf_sections_addr(dwarf_aranges_section, 0);
    dwarf_sections_addr(dwarf_aranges_section, 0);
}
