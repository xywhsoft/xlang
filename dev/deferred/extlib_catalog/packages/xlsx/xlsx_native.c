/*
 * xlsx_native.c - XLSX spreadsheet helpers
 * Tier 3: Office Open XML spreadsheet utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Cell type */
const char* xxlsx_cell_type(long long type) {
    switch (type) {
        case 0: return "number";
        case 1: return "shared string";
        case 2: return "string";
        case 3: return "inline string";
        case 4: return "boolean";
        case 5: return "error";
        case 6: return "date";
        default: return "unknown";
    }
}

/* Column index to letter (0-based: 0=A, 25=Z, 26=AA) */
const char* xxlsx_col_letter(long long col) {
    int pos = 0;
    col++; /* 1-based */
    while (col > 0) {
        int rem = (int)((col - 1) % 26);
        buf[pos++] = (char)('A' + rem);
        col = (col - 1) / 26;
    }
    /* Reverse */
    for (int i = 0; i < pos / 2; i++) {
        char tmp = buf[i];
        buf[i] = buf[pos - 1 - i];
        buf[pos - 1 - i] = tmp;
    }
    buf[pos] = '\0';
    return buf;
}

/* Column letter to index (A=0, Z=25, AA=26) */
long long xxlsx_col_index(const char *letter) {
    long long result = 0;
    for (int i = 0; letter[i]; i++) {
        result = result * 26 + (letter[i] - 'A' + 1);
    }
    return result - 1;
}

/* Cell reference (e.g., "A1", "B2") */
const char* xxlsx_cell_ref(long long col, long long row) {
    const char *col_str = xxlsx_col_letter(col);
    sprintf(buf, "%s%lld", col_str, row + 1);
    return buf;
}

/* Number format ID to name */
const char* xxlsx_num_format(long long id) {
    switch (id) {
        case 0: return "General";
        case 1: return "0";
        case 2: return "0.00";
        case 3: return "#,##0";
        case 4: return "#,##0.00";
        case 9: return "0%";
        case 10: return "0.00%";
        case 11: return "0.00E+00";
        case 12: return "# ?/?";
        case 13: return "# ??/??";
        case 14: return "mm-dd-yy";
        case 15: return "d-mmm-yy";
        case 16: return "d-mmm";
        case 17: return "mmm-yy";
        case 18: return "h:mm AM/PM";
        case 19: return "h:mm:ss AM/PM";
        case 20: return "h:mm";
        case 21: return "h:mm:ss";
        case 22: return "m/d/yy h:mm";
        case 37: return "#,##0 ;(#,##0)";
        case 38: return "#,##0 ;[Red](#,##0)";
        case 39: return "#,##0.00;(#,##0.00)";
        case 40: return "#,##0.00;[Red](#,##0.00)";
        case 45: return "mm:ss";
        case 46: return "[h]:mm:ss";
        case 47: return "mmss.0";
        case 48: return "##0.0E+0";
        case 49: return "@";
        default: return "Custom";
    }
}

/* Sheet state */
const char* xxlsx_sheet_state(long long state) {
    switch (state) {
        case 0: return "visible";
        case 1: return "hidden";
        case 2: return "veryHidden";
        default: return "unknown";
    }
}

/* Error code */
const char* xxlsx_error_name(long long code) {
    switch (code) {
        case 0: return "#NULL!";
        case 1: return "#DIV/0!";
        case 2: return "#VALUE!";
        case 3: return "#REF!";
        case 4: return "#NAME?";
        case 5: return "#NUM!";
        case 6: return "#N/A";
        case 7: return "#GETTING_DATA";
        default: return "Unknown";
    }
}
