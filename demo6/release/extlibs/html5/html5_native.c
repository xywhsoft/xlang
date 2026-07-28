/*
 * html5_native.c - HTML5 parser/tokenizer helpers
 * Tier 3: document format utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xhtml5_tag_category(long long id) {
    switch (id) {
        case 0: return "void";
        case 1: return "normal";
        case 2: return "foreign";
        case 3: return "raw text";
        case 4: return "escapable raw text";
        default: return "Unknown";
    }
}

long long xhtml5_is_void_element(const char *tag) {
    /* HTML5 void elements */
    if (strcmp(tag, "area") == 0) return 1;
    if (strcmp(tag, "base") == 0) return 1;
    if (strcmp(tag, "br") == 0) return 1;
    if (strcmp(tag, "col") == 0) return 1;
    if (strcmp(tag, "embed") == 0) return 1;
    if (strcmp(tag, "hr") == 0) return 1;
    if (strcmp(tag, "img") == 0) return 1;
    if (strcmp(tag, "input") == 0) return 1;
    if (strcmp(tag, "link") == 0) return 1;
    if (strcmp(tag, "meta") == 0) return 1;
    if (strcmp(tag, "param") == 0) return 1;
    if (strcmp(tag, "source") == 0) return 1;
    if (strcmp(tag, "track") == 0) return 1;
    if (strcmp(tag, "wbr") == 0) return 1;
    return 0;
}

const char* xhtml5_entity(long long id) {
    switch (id) {
        case 0: return "&amp;";
        case 1: return "&lt;";
        case 2: return "&gt;";
        case 3: return "&quot;";
        case 4: return "&apos;";
        case 5: return "&nbsp;";
        case 6: return "&copy;";
        case 7: return "&reg;";
        case 8: return "&trade;";
        case 9: return "&mdash;";
        default: return "Unknown";
    }
}

const char* xhtml5_input_type(long long id) {
    switch (id) {
        case 0: return "text";
        case 1: return "password";
        case 2: return "email";
        case 3: return "number";
        case 4: return "checkbox";
        case 5: return "radio";
        case 6: return "file";
        case 7: return "date";
        case 8: return "color";
        case 9: return "range";
        case 10: return "hidden";
        case 11: return "submit";
        default: return "Unknown";
    }
}

const char* xhtml5_aria_role(long long id) {
    switch (id) {
        case 0: return "alert";
        case 1: return "button";
        case 2: return "checkbox";
        case 3: return "dialog";
        case 4: return "grid";
        case 5: return "link";
        case 6: return "menu";
        case 7: return "navigation";
        case 8: return "progressbar";
        case 9: return "tab";
        case 10: return "tooltip";
        default: return "Unknown";
    }
}

long long xhtml5_escape(const char *src) {
    int pos = 0;
    int i;
    for (i = 0; src[i] && pos < 500; i++) {
        switch (src[i]) {
            case '&':
                buf[pos++] = '&'; buf[pos++] = 'a'; buf[pos++] = 'm'; buf[pos++] = 'p'; buf[pos++] = ';';
                break;
            case '<':
                buf[pos++] = '&'; buf[pos++] = 'l'; buf[pos++] = 't'; buf[pos++] = ';';
                break;
            case '>':
                buf[pos++] = '&'; buf[pos++] = 'g'; buf[pos++] = 't'; buf[pos++] = ';';
                break;
            case '"':
                buf[pos++] = '&'; buf[pos++] = 'q'; buf[pos++] = 'u'; buf[pos++] = 'o'; buf[pos++] = 't'; buf[pos++] = ';';
                break;
            default:
                buf[pos++] = src[i];
                break;
        }
    }
    buf[pos] = '\0';
    return pos;
}

const char* xhtml5_get_escaped(void) {
    return buf;
}

const char* xhtml5_doctype(void) {
    return "<!DOCTYPE html>";
}
