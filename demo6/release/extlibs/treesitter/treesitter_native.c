/*
 * treesitter_native.c - Incremental syntax parsing
 * Tier 4: text processing
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xtreesitter_language(long long id) {
    switch (id) {
        case 0: return "c";
        case 1: return "cpp";
        case 2: return "python";
        case 3: return "javascript";
        case 4: return "typescript";
        case 5: return "rust";
        case 6: return "go";
        case 7: return "java";
        case 8: return "ruby";
        case 9: return "php";
        case 10: return "html";
        case 11: return "css";
        case 12: return "json";
        case 13: return "yaml";
        case 14: return "toml";
        case 15: return "markdown";
        default: return "Unknown";
    }
}

const char* xtreesitter_node_type(long long id) {
    switch (id) {
        case 0: return "source_file";
        case 1: return "function_definition";
        case 2: return "class_definition";
        case 3: return "expression_statement";
        case 4: return "identifier";
        case 5: return "string";
        case 6: return "number";
        case 7: return "comment";
        case 8: return "error";
        case 9: return "block";
        default: return "Unknown";
    }
}

const char* xtreesitter_edit_type(long long id) {
    switch (id) {
        case 0: return "insert";
        case 1: return "delete";
        case 2: return "replace";
        default: return "Unknown";
    }
}

const char* xtreesitter_query_capture(long long id) {
    switch (id) {
        case 0: return "@function";
        case 1: return "@class";
        case 2: return "@variable";
        case 3: return "@string";
        case 4: return "@number";
        case 5: return "@comment";
        case 6: return "@keyword";
        case 7: return "@operator";
        case 8: return "@type";
        case 9: return "@constant";
        default: return "Unknown";
    }
}

/* Simple tree depth calculator for nested parentheses */
long long xtreesitter_max_depth(const char *expr) {
    long long depth = 0, max = 0;
    const char *p = expr;
    while (*p) {
        if (*p == '(' || *p == '[' || *p == '{') {
            depth++;
            if (depth > max) max = depth;
        } else if (*p == ')' || *p == ']' || *p == '}') {
            if (depth > 0) depth--;
        }
        p++;
    }
    return max;
}

const char* xtreesitter_input_encoding(long long id) {
    switch (id) {
        case 0: return "UTF-8";
        case 1: return "UTF-16";
        default: return "Unknown";
    }
}

const char* xtreesitter_log_type(long long id) {
    switch (id) {
        case 0: return "parse";
        case 1: return "lex";
        default: return "Unknown";
    }
}
