/*
 * aho_corasick_native.c - Aho-Corasick multi-pattern matching engine
 *
 * Redesigned for structured results, case-insensitive support,
 * and optimized contains/count paths.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
extern void xrtSetError(const void *msg, int bFree);
extern void *xrtTempMemory(size_t size);

/* ------------------------------------------------------------------ */
/* Trie node                                                           */
/* ------------------------------------------------------------------ */
#define AC_ALPHABET 256

typedef struct ACNode {
    struct ACNode *children[AC_ALPHABET];
    struct ACNode *fail;
    int *outputs;
    int output_count;
    int output_cap;
} ACNode;

/* ------------------------------------------------------------------ */
/* Match result storage                                                */
/* ------------------------------------------------------------------ */
typedef struct {
    int pattern_idx;
    int start;
    int end;
} ACMatchEntry;

typedef struct {
    ACMatchEntry *items;
    int count;
    int cap;
} ACMatchVec;

/* ------------------------------------------------------------------ */
/* Result set (opaque handle for xlang MatchList)                      */
/* ------------------------------------------------------------------ */
typedef struct {
    ACMatchEntry *items;
    int count;
    char **patterns;       /* borrowed pattern strings */
    int pattern_count;
} ACResultSet;

/* ------------------------------------------------------------------ */
/* Main automaton                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    ACNode *root;
    char **patterns;
    int *pattern_lens;
    int pattern_count;
    int pattern_cap;
    int built;
    int ignore_case;
} XAhoCorasick;

/* ------------------------------------------------------------------ */
/* Node helpers                                                        */
/* ------------------------------------------------------------------ */
static ACNode *ac_new_node(void) {
    return (ACNode *)calloc(1, sizeof(ACNode));
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

static unsigned char ac_tolower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + 32) : c;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */
void *xac_create(int ignore_case) {
    XAhoCorasick *ac = (XAhoCorasick *)calloc(1, sizeof(XAhoCorasick));
    if (!ac) { xrtSetError("aho_corasick: out of memory", 0); return NULL; }
    ac->root = ac_new_node();
    if (!ac->root) { free(ac); xrtSetError("aho_corasick: out of memory", 0); return NULL; }
    ac->pattern_cap = 16;
    ac->patterns = (char **)malloc(sizeof(char *) * ac->pattern_cap);
    ac->pattern_lens = (int *)malloc(sizeof(int) * ac->pattern_cap);
    if (!ac->patterns || !ac->pattern_lens) {
        free(ac->patterns); free(ac->pattern_lens);
        ac_free_node(ac->root); free(ac);
        xrtSetError("aho_corasick: out of memory", 0);
        return NULL;
    }
    ac->ignore_case = ignore_case;
    return ac;
}

void xac_destroy(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    int i;
    if (!ac) return;
    ac_free_node(ac->root);
    for (i = 0; i < ac->pattern_count; i++) free(ac->patterns[i]);
    free(ac->patterns);
    free(ac->pattern_lens);
    free(ac);
}

/* ------------------------------------------------------------------ */
/* Pattern management                                                  */
/* ------------------------------------------------------------------ */
void xac_add_pattern(void *ptr, const char *pattern) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode *cur;
    const unsigned char *p;
    int idx, len;

    if (!ac || !pattern || !pattern[0]) return;
    if (ac->built) { xrtSetError("aho_corasick: cannot add after build", 0); return; }

    if (ac->pattern_count >= ac->pattern_cap) {
        ac->pattern_cap *= 2;
        ac->patterns = (char **)realloc(ac->patterns, sizeof(char *) * ac->pattern_cap);
        ac->pattern_lens = (int *)realloc(ac->pattern_lens, sizeof(int) * ac->pattern_cap);
    }
    idx = ac->pattern_count;
    ac->patterns[idx] = strdup(pattern);
    ac->pattern_lens[idx] = (int)strlen(pattern);
    ac->pattern_count++;

    /* insert into trie */
    cur = ac->root;
    p = (const unsigned char *)pattern;
    while (*p) {
        unsigned char c = ac->ignore_case ? ac_tolower(*p) : *p;
        if (!cur->children[c]) cur->children[c] = ac_new_node();
        cur = cur->children[c];
        p++;
    }
    /* mark output */
    if (cur->output_count >= cur->output_cap) {
        cur->output_cap = cur->output_cap ? cur->output_cap * 2 : 4;
        cur->outputs = (int *)realloc(cur->outputs, sizeof(int) * cur->output_cap);
    }
    cur->outputs[cur->output_count++] = idx;
    (void)len;
}

/* ------------------------------------------------------------------ */
/* Build failure links (BFS)                                           */
/* ------------------------------------------------------------------ */
void xac_build(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode **queue;
    int qhead = 0, qtail = 0, qcap;
    int i;

    if (!ac || ac->built) return;

    qcap = 4096;
    queue = (ACNode **)malloc(sizeof(ACNode *) * qcap);
    if (!queue) { xrtSetError("aho_corasick: out of memory", 0); return; }

    for (i = 0; i < AC_ALPHABET; i++) {
        if (ac->root->children[i]) {
            ac->root->children[i]->fail = ac->root;
            if (qtail >= qcap) { qcap *= 2; queue = (ACNode **)realloc(queue, sizeof(ACNode *) * qcap); }
            queue[qtail++] = ac->root->children[i];
        }
    }

    while (qhead < qtail) {
        ACNode *cur = queue[qhead++];
        for (i = 0; i < AC_ALPHABET; i++) {
            ACNode *child = cur->children[i];
            ACNode *f;
            if (!child) continue;
            f = cur->fail;
            while (f && !f->children[i]) f = f->fail;
            child->fail = f ? f->children[i] : ac->root;
            if (child->fail == child) child->fail = ac->root;
            /* merge outputs from fail chain */
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
            if (qtail >= qcap) { qcap *= 2; queue = (ACNode **)realloc(queue, sizeof(ACNode *) * qcap); }
            queue[qtail++] = child;
        }
    }
    free(queue);
    ac->built = 1;
}

/* ------------------------------------------------------------------ */
/* Search: returns result set handle                                   */
/* ------------------------------------------------------------------ */
void *xac_search(void *ptr, const char *text) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACResultSet *rs;
    ACMatchVec vec;
    ACNode *cur;
    const unsigned char *p;
    int pos = 0;

    if (!ac || !text) return NULL;
    if (!ac->built) { xrtSetError("aho_corasick: must build before search", 0); return NULL; }

    vec.items = NULL;
    vec.count = 0;
    vec.cap = 0;

    cur = ac->root;
    p = (const unsigned char *)text;

    while (*p) {
        unsigned char c = ac->ignore_case ? ac_tolower(*p) : *p;
        while (cur && !cur->children[c] && cur != ac->root) cur = cur->fail;
        if (cur->children[c]) cur = cur->children[c];
        else cur = ac->root;

        if (cur->output_count > 0) {
            int j;
            for (j = 0; j < cur->output_count; j++) {
                int pidx = cur->outputs[j];
                int start = pos - ac->pattern_lens[pidx] + 1;
                if (vec.count >= vec.cap) {
                    vec.cap = vec.cap ? vec.cap * 2 : 32;
                    vec.items = (ACMatchEntry *)realloc(vec.items, sizeof(ACMatchEntry) * vec.cap);
                }
                vec.items[vec.count].pattern_idx = pidx;
                vec.items[vec.count].start = start;
                vec.items[vec.count].end = pos + 1;
                vec.count++;
            }
        }
        p++;
        pos++;
    }

    rs = (ACResultSet *)calloc(1, sizeof(ACResultSet));
    if (!rs) { free(vec.items); xrtSetError("aho_corasick: out of memory", 0); return NULL; }
    rs->items = vec.items;
    rs->count = vec.count;
    rs->patterns = ac->patterns;
    rs->pattern_count = ac->pattern_count;
    return rs;
}

/* ------------------------------------------------------------------ */
/* Result set accessors                                                */
/* ------------------------------------------------------------------ */
int xac_result_count(void *rptr) {
    ACResultSet *rs = (ACResultSet *)rptr;
    return rs ? rs->count : 0;
}

const char *xac_result_pattern(void *rptr, int index) {
    ACResultSet *rs = (ACResultSet *)rptr;
    if (!rs || index < 0 || index >= rs->count) return "";
    return rs->patterns[rs->items[index].pattern_idx];
}

int xac_result_start(void *rptr, int index) {
    ACResultSet *rs = (ACResultSet *)rptr;
    if (!rs || index < 0 || index >= rs->count) return -1;
    return rs->items[index].start;
}

int xac_result_end(void *rptr, int index) {
    ACResultSet *rs = (ACResultSet *)rptr;
    if (!rs || index < 0 || index >= rs->count) return -1;
    return rs->items[index].end;
}

void xac_result_destroy(void *rptr) {
    ACResultSet *rs = (ACResultSet *)rptr;
    if (!rs) return;
    free(rs->items);
    free(rs);
}

/* ------------------------------------------------------------------ */
/* Optimized contains: early exit on first match                       */
/* ------------------------------------------------------------------ */
int xac_contains(void *ptr, const char *text) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode *cur;
    const unsigned char *p;

    if (!ac || !text) return 0;
    if (!ac->built) { xrtSetError("aho_corasick: must build before contains", 0); return 0; }

    cur = ac->root;
    p = (const unsigned char *)text;

    while (*p) {
        unsigned char c = ac->ignore_case ? ac_tolower(*p) : *p;
        while (cur && !cur->children[c] && cur != ac->root) cur = cur->fail;
        if (cur->children[c]) cur = cur->children[c];
        else cur = ac->root;

        if (cur->output_count > 0) return 1;
        p++;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Optimized count: no result allocation                               */
/* ------------------------------------------------------------------ */
int xac_count(void *ptr, const char *text) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACNode *cur;
    const unsigned char *p;
    int total = 0;

    if (!ac || !text) return 0;
    if (!ac->built) { xrtSetError("aho_corasick: must build before count", 0); return 0; }

    cur = ac->root;
    p = (const unsigned char *)text;

    while (*p) {
        unsigned char c = ac->ignore_case ? ac_tolower(*p) : *p;
        while (cur && !cur->children[c] && cur != ac->root) cur = cur->fail;
        if (cur->children[c]) cur = cur->children[c];
        else cur = ac->root;

        total += cur->output_count;
        p++;
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* Streaming scan: one match at a time, no bulk allocation             */
/* ------------------------------------------------------------------ */
typedef struct {
    XAhoCorasick *ac;
    const unsigned char *text;
    const unsigned char *p;
    ACNode *cur;
    int pos;
    int out_idx;       /* current output index within current node */
    int finished;
    /* last match fields (accessed via getters) */
    int match_start;
    int match_end;
    int match_pidx;
} ACScanCtx;

void *xac_scan_begin(void *ptr, const char *text) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACScanCtx *ctx;

    if (!ac || !text) return NULL;
    if (!ac->built) { xrtSetError("aho_corasick: must build before scan", 0); return NULL; }

    ctx = (ACScanCtx *)calloc(1, sizeof(ACScanCtx));
    if (!ctx) { xrtSetError("aho_corasick: out of memory", 0); return NULL; }
    ctx->ac = ac;
    ctx->text = (const unsigned char *)text;
    ctx->p = ctx->text;
    ctx->cur = ac->root;
    ctx->pos = 0;
    ctx->out_idx = 0;
    ctx->finished = 0;
    return ctx;
}

/*
 * Advance to next match. Returns 1 if a match is found, 0 if exhausted.
 * Caller retrieves match data via xac_scan_start/end/pidx.
 * Caller can stop early by calling xac_scan_end.
 */
int xac_scan_next(void *sptr) {
    ACScanCtx *ctx = (ACScanCtx *)sptr;
    XAhoCorasick *ac;

    if (!ctx || ctx->finished) return 0;
    ac = ctx->ac;

    for (;;) {
        /* If current node has pending outputs, emit them first */
        if (ctx->cur && ctx->out_idx < ctx->cur->output_count) {
            int pidx = ctx->cur->outputs[ctx->out_idx];
            ctx->out_idx++;
            ctx->match_pidx = pidx;
            ctx->match_start = ctx->pos - ac->pattern_lens[pidx];
            ctx->match_end = ctx->pos;
            return 1;
        }

        /* Advance to next character */
        if (*ctx->p == '\0') {
            ctx->finished = 1;
            return 0;
        }

        {
            unsigned char c = ac->ignore_case ? ac_tolower(*ctx->p) : *ctx->p;
            ACNode *cur = ctx->cur;
            while (cur && !cur->children[c] && cur != ac->root) cur = cur->fail;
            if (cur->children[c]) cur = cur->children[c];
            else cur = ac->root;
            ctx->cur = cur;
        }
        ctx->p++;
        ctx->pos++;
        ctx->out_idx = 0;
    }
}

int xac_scan_start(void *sptr) {
    ACScanCtx *ctx = (ACScanCtx *)sptr;
    return ctx ? ctx->match_start : -1;
}

int xac_scan_end_pos(void *sptr) {
    ACScanCtx *ctx = (ACScanCtx *)sptr;
    return ctx ? ctx->match_end : -1;
}

const char *xac_scan_pattern(void *sptr) {
    ACScanCtx *ctx = (ACScanCtx *)sptr;
    if (!ctx) return "";
    return ctx->ac->patterns[ctx->match_pidx];
}

void xac_scan_end(void *sptr) {
    free(sptr);
}

/* ------------------------------------------------------------------ */
/* Metadata                                                            */
/* ------------------------------------------------------------------ */
int xac_pattern_count(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    return ac ? ac->pattern_count : 0;
}

int xac_is_built(void *ptr) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    return ac ? ac->built : 0;
}

/* ------------------------------------------------------------------ */
/* Interval collection + merge helper                                  */
/* ------------------------------------------------------------------ */
typedef struct { int start; int end; } ACInterval;

/*
 * Collect all matches, sort by start, merge overlapping.
 * Returns merged interval count; caller frees *out with free().
 */
static int ac_merged_intervals(XAhoCorasick *ac, const char *text,
                               ACInterval **out) {
    ACMatchVec vec = {NULL, 0, 0};
    ACNode *cur;
    const unsigned char *p;
    int pos = 0, i, n, mc, mi;
    ACInterval *merged;

    /* --- scan --- */
    cur = ac->root;
    p = (const unsigned char *)text;
    while (*p) {
        unsigned char c = ac->ignore_case ? ac_tolower(*p) : *p;
        while (cur && !cur->children[c] && cur != ac->root) cur = cur->fail;
        if (cur->children[c]) cur = cur->children[c];
        else cur = ac->root;
        if (cur->output_count > 0) {
            int j;
            for (j = 0; j < cur->output_count; j++) {
                int pidx = cur->outputs[j];
                if (vec.count >= vec.cap) {
                    vec.cap = vec.cap ? vec.cap * 2 : 32;
                    vec.items = (ACMatchEntry *)realloc(vec.items,
                                    sizeof(ACMatchEntry) * vec.cap);
                }
                vec.items[vec.count].pattern_idx = pidx;
                vec.items[vec.count].start = pos - ac->pattern_lens[pidx] + 1;
                vec.items[vec.count].end = pos + 1;
                vec.count++;
            }
        }
        p++; pos++;
    }

    if (vec.count == 0) { free(vec.items); *out = NULL; return 0; }

    /* --- sort by start (insertion) --- */
    n = vec.count;
    for (i = 1; i < n; i++) {
        ACMatchEntry key = vec.items[i];
        int j = i - 1;
        while (j >= 0 && vec.items[j].start > key.start) {
            vec.items[j + 1] = vec.items[j]; j--;
        }
        vec.items[j + 1] = key;
    }

    /* --- merge --- */
    merged = (ACInterval *)malloc(sizeof(ACInterval) * n);
    mc = 0;
    for (mi = 0; mi < n; ) {
        int s = vec.items[mi].start;
        int e = vec.items[mi].end;
        mi++;
        while (mi < n && vec.items[mi].start <= e) {
            if (vec.items[mi].end > e) e = vec.items[mi].end;
            mi++;
        }
        merged[mc].start = s;
        merged[mc].end = e;
        mc++;
    }
    free(vec.items);
    *out = merged;
    return mc;
}

/* ------------------------------------------------------------------ */
/* replace: pre-calculate + single allocation                          */
/* ------------------------------------------------------------------ */
const char *xac_replace(void *ptr, const char *text, const char *repl) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACInterval *iv;
    int mc, i, textLen, replLen;
    size_t outLen;
    char *out, *w;
    int pos;

    if (!ac || !text) return text ? text : "";
    if (!ac->built) { xrtSetError("aho_corasick: must build before replace", 0); return text; }

    mc = ac_merged_intervals(ac, text, &iv);
    if (mc == 0) return text;

    textLen = (int)strlen(text);
    replLen = (int)strlen(repl);

    /* compute output length */
    outLen = 0;
    pos = 0;
    for (i = 0; i < mc; i++) {
        outLen += (iv[i].start - pos);  /* gap */
        outLen += replLen;
        pos = iv[i].end;
    }
    outLen += (textLen - pos);  /* tail */

    out = (char *)xrtTempMemory(outLen + 1);
    if (!out) { free(iv); xrtSetError("aho_corasick: out of memory", 0); return text; }

    /* fill */
    w = out;
    pos = 0;
    for (i = 0; i < mc; i++) {
        int gap = iv[i].start - pos;
        if (gap > 0) { memcpy(w, text + pos, gap); w += gap; }
        if (replLen > 0) { memcpy(w, repl, replLen); w += replLen; }
        pos = iv[i].end;
    }
    if (textLen - pos > 0) memcpy(w, text + pos, textLen - pos);
    out[outLen] = '\0';

    free(iv);
    return out;
}

/* ------------------------------------------------------------------ */
/* mask: replace each matched char with mask char(s)                   */
/* ------------------------------------------------------------------ */
const char *xac_mask(void *ptr, const char *text, const char *ch) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACInterval *iv;
    int mc, i, textLen, chLen;
    size_t outLen;
    char *out, *w;
    int pos, j;

    if (!ac || !text) return text ? text : "";
    if (!ac->built) { xrtSetError("aho_corasick: must build before mask", 0); return text; }

    mc = ac_merged_intervals(ac, text, &iv);
    if (mc == 0) return text;

    textLen = (int)strlen(text);
    chLen = (int)strlen(ch);

    /* output = gaps + (span * chLen) for each interval */
    outLen = 0;
    pos = 0;
    for (i = 0; i < mc; i++) {
        outLen += (iv[i].start - pos);
        outLen += (size_t)(iv[i].end - iv[i].start) * chLen;
        pos = iv[i].end;
    }
    outLen += (textLen - pos);

    out = (char *)xrtTempMemory(outLen + 1);
    if (!out) { free(iv); xrtSetError("aho_corasick: out of memory", 0); return text; }

    w = out;
    pos = 0;
    for (i = 0; i < mc; i++) {
        int gap = iv[i].start - pos;
        int span = iv[i].end - iv[i].start;
        if (gap > 0) { memcpy(w, text + pos, gap); w += gap; }
        for (j = 0; j < span; j++) {
            memcpy(w, ch, chLen); w += chLen;
        }
        pos = iv[i].end;
    }
    if (textLen - pos > 0) memcpy(w, text + pos, textLen - pos);
    out[outLen] = '\0';

    free(iv);
    return out;
}

/* ------------------------------------------------------------------ */
/* highlight: wrap merged intervals with open/close tags               */
/* ------------------------------------------------------------------ */
const char *xac_highlight(void *ptr, const char *text,
                          const char *openTag, const char *closeTag) {
    XAhoCorasick *ac = (XAhoCorasick *)ptr;
    ACInterval *iv;
    int mc, i, textLen, openLen, closeLen;
    size_t outLen;
    char *out, *w;
    int pos;

    if (!ac || !text) return text ? text : "";
    if (!ac->built) { xrtSetError("aho_corasick: must build before highlight", 0); return text; }

    mc = ac_merged_intervals(ac, text, &iv);
    if (mc == 0) return text;

    textLen = (int)strlen(text);
    openLen = (int)strlen(openTag);
    closeLen = (int)strlen(closeTag);

    /* output = textLen + (openLen + closeLen) * mc */
    outLen = (size_t)textLen + (size_t)(openLen + closeLen) * mc;

    out = (char *)xrtTempMemory(outLen + 1);
    if (!out) { free(iv); xrtSetError("aho_corasick: out of memory", 0); return text; }

    w = out;
    pos = 0;
    for (i = 0; i < mc; i++) {
        int gap = iv[i].start - pos;
        int span = iv[i].end - iv[i].start;
        if (gap > 0) { memcpy(w, text + pos, gap); w += gap; }
        memcpy(w, openTag, openLen); w += openLen;
        memcpy(w, text + iv[i].start, span); w += span;
        memcpy(w, closeTag, closeLen); w += closeLen;
        pos = iv[i].end;
    }
    if (textLen - pos > 0) memcpy(w, text + pos, textLen - pos);
    out[outLen] = '\0';

    free(iv);
    return out;
}
