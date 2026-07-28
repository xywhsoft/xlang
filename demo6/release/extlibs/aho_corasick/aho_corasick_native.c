/*
 * aho_corasick_native.c - Aho-Corasick multi-pattern matching
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern void xrtSetError(const char *msg);

static char ac_buf[8192];

/* ------------------------------------------------------------------ */
/* Trie node with failure links                                        */
/* ------------------------------------------------------------------ */
#define AC_ALPHABET 256

typedef struct ACNode {
    struct ACNode *children[AC_ALPHABET];
    struct ACNode *fail;
    int *outputs;           /* pattern indices that end here */
    int output_count;
    int output_cap;
} ACNode;

typedef struct {
    ACNode *root;
    char **patterns;
    int pattern_count;
    int pattern_cap;
    int built;
} XAhoCorasick;

static ACNode *ac_new_node(void) {
    ACNode *n = (ACNode *)calloc(1, sizeof(ACNode));
    return n;
}

static void ac_free_node(ACNode *n) {
    int i;
    if (!n) return;
    for (i = 0; i < AC_ALPHABET; i++) {
        if (n->children[i]) ac_free_node(n->children[i]);
    }
    free(n->outputs);
    free(n);
}

void *xac_create(void) {
    XAhoCorasick *ac = (XAhoCorasick *)calloc(1, sizeof(XAhoCorasick));
    if (!ac) { xrtSetError("aho_corasick: out of memory"); return NULL; }
    ac->root = ac_new_node();
    ac->pattern_cap = 16;
    ac->patterns = (char **)malloc(sizeof(char *) * ac->pattern_cap);
    if (!ac->root || !ac->patterns) { xrtSetError("aho_corasick: out of memory"); return NULL; }
    return ac;
}

void xac_destroy(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    int i;
    if (!ac) return;
    ac_free_node(ac->root);
    for (i = 0; i < ac->pattern_count; i++) free(ac->patterns[i]);
    free(ac->patterns);
    free(ac);
}

void xac_add_pattern(void *ptr, const char *pattern) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode *cur;
    const unsigned char *p;
    int idx;
    if (!ac || !pattern || !pattern[0]) return;
    if (ac->built) { xrtSetError("aho_corasick: cannot add after build"); return; }

    /* grow patterns */
    if (ac->pattern_count >= ac->pattern_cap) {
        ac->pattern_cap *= 2;
        ac->patterns = (char **)realloc(ac->patterns, sizeof(char *) * ac->pattern_cap);
    }
    idx = ac->pattern_count;
    ac->patterns[idx] = strdup(pattern);
    ac->pattern_count++;

    /* insert into trie */
    cur = ac->root;
    p = (const unsigned char *)pattern;
    while (*p) {
        if (!cur->children[*p]) cur->children[*p] = ac_new_node();
        cur = cur->children[*p];
        p++;
    }
    /* mark output */
    if (cur->output_count >= cur->output_cap) {
        cur->output_cap = cur->output_cap ? cur->output_cap * 2 : 4;
        cur->outputs = (int *)realloc(cur->outputs, sizeof(int) * cur->output_cap);
    }
    cur->outputs[cur->output_count++] = idx;
}

/* BFS to build failure links */
void xac_build(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode **queue;
    int qhead = 0, qtail = 0;
    int i;
    if (!ac || ac->built) return;

    queue = (ACNode **)malloc(sizeof(ACNode *) * 4096);
    if (!queue) { xrtSetError("aho_corasick: out of memory"); return; }

    /* init depth-1 nodes */
    for (i = 0; i < AC_ALPHABET; i++) {
        if (ac->root->children[i]) {
            ac->root->children[i]->fail = ac->root;
            if (qtail < 4096) queue[qtail++] = ac->root->children[i];
        }
    }

    while (qhead < qtail) {
        ACNode *cur = queue[qhead++];
        for (i = 0; i < AC_ALPHABET; i++) {
            ACNode *child = cur->children[i];
            if (!child) continue;
            /* find fail */
            ACNode *f = cur->fail;
            while (f && !f->children[i]) f = f->fail;
            child->fail = f ? f->children[i] : ac->root;
            if (child->fail == child) child->fail = ac->root;
            /* merge outputs from fail */
            if (child->fail->output_count > 0) {
                int j;
                for (j = 0; j < child->fail->output_count; j++) {
                    if (child->output_count >= child->output_cap) {
                        child->output_cap = child->output_cap ? child->output_cap * 2 : 4;
                        child->outputs = (int *)realloc(child->outputs, sizeof(int) * child->output_cap);
                    }
                    child->outputs[child->output_count++] = child->fail->outputs[j];
                }
            }
            if (qtail < 4096) queue[qtail++] = child;
        }
    }
    free(queue);
    ac->built = 1;
}

/* search: returns "pattern@pos;pattern@pos;..." */
const char *xac_search(void *ptr, const char *text) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode *cur;
    const unsigned char *p;
    int pos = 0;
    int used = 0;
    if (!ac || !text) return "";
    if (!ac->built) { xrtSetError("aho_corasick: must build before search"); return ""; }

    cur = ac->root;
    p = (const unsigned char *)text;
    ac_buf[0] = '\0';

    while (*p) {
        while (cur && !cur->children[*p] && cur != ac->root) cur = cur->fail;
        if (cur->children[*p]) cur = cur->children[*p];
        else cur = ac->root;

        if (cur->output_count > 0) {
            int j;
            for (j = 0; j < cur->output_count; j++) {
                int pidx = cur->outputs[j];
                int start = pos - (int)strlen(ac->patterns[pidx]) + 1;
                int n = snprintf(ac_buf + used, sizeof(ac_buf) - used, "%s%s@%d",
                                 used > 0 ? ";" : "", ac->patterns[pidx], start);
                if (n > 0) used += n;
            }
        }
        p++;
        pos++;
    }
    return ac_buf;
}

int xac_pattern_count(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    return ac ? ac->pattern_count : 0;
}

int xac_is_built(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    return ac ? ac->built : 0;
}
