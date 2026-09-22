/*
 * semver_native.c - Semantic Versioning 2.0.0 parsing, precedence
 * comparison, and node-semver style range matching.
 *
 * Conventions:
 *   - xsemver_major/minor/patch return -1 when the input is not valid SemVer.
 *   - xsemver_compare returns -1/0/1 for valid inputs and -2 otherwise.
 *   - xsemver_prerelease/build return "" when absent or the input is invalid.
 *   - xsemver_clean strips leading "v"/"="/whitespace and returns the
 *     canonical form, or "" when the remainder is not valid SemVer.
 *   - xsemver_satisfies returns false for an invalid version or range.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define XSEMVER_MAX_TEXT 512
#define XSEMVER_MAX_FIELD 256
#define XSEMVER_MAX_SET_COMPARATORS 32
#define XSEMVER_RING 8

typedef struct {
    long long major;
    long long minor;
    long long patch;
    char pre[XSEMVER_MAX_FIELD];
    char build[XSEMVER_MAX_FIELD];
} SemVer;

/* Result strings rotate through a small ring so several calls can coexist
   in one expression before the xlang layer copies them. */
static char semver_ring[XSEMVER_RING][XSEMVER_MAX_FIELD];
static int semver_ring_next = 0;

static char *semver_ring_take(void) {
    char *slot = semver_ring[semver_ring_next];
    semver_ring_next = (semver_ring_next + 1) % XSEMVER_RING;
    return slot;
}

static int semver_is_digit(int c) {
    return c >= '0' && c <= '9';
}

/* Non-empty, digits only, and no leading zero unless the value itself is 0. */
static int semver_is_numeric_id(const char *s, size_t len) {
    size_t i;
    if (len == 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (!semver_is_digit((unsigned char)s[i])) {
            return 0;
        }
    }
    if (len > 1 && s[0] == '0') {
        return 0;
    }
    return 1;
}

static int semver_is_identifier_id(const char *s, size_t len) {
    size_t i;
    if (len == 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        int c = (unsigned char)s[i];
        if (!(isalnum(c) || c == '-')) {
            return 0;
        }
    }
    return 1;
}

/* Validate a dot separated prerelease or build identifier list. */
static int semver_valid_identifiers(const char *s, int allow_leading_zero) {
    const char *p = s;
    if (*p == '\0') {
        return 0;
    }
    while (1) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (allow_leading_zero) {
            if (!semver_is_identifier_id(p, len)) {
                return 0;
            }
        } else {
            int all_digits = 1;
            size_t i;
            for (i = 0; i < len; i++) {
                if (!semver_is_digit((unsigned char)p[i])) {
                    all_digits = 0;
                    break;
                }
            }
            if (all_digits) {
                if (!semver_is_numeric_id(p, len)) {
                    return 0;
                }
            } else if (!semver_is_identifier_id(p, len)) {
                return 0;
            }
        }
        if (!dot) {
            return 1;
        }
        p = dot + 1;
        if (*p == '\0' || *p == '.') {
            return 0;
        }
    }
}

static int semver_copy_part(char *dst, const char *src, size_t len) {
    if (len >= XSEMVER_MAX_FIELD) {
        return 0;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    return 1;
}

/* Strict SemVer 2.0.0 parse: MAJOR.MINOR.PATCH[-pre][+build]. */
static int semver_parse(const char *v, SemVer *out) {
    char core[XSEMVER_MAX_TEXT];
    const char *p = v;
    const char *dash;
    const char *plus;
    size_t core_len;
    char *dot1;
    char *dot2;
    char *endp = NULL;

    memset(out, 0, sizeof(*out));
    if (!v) {
        return 0;
    }

    /* In valid SemVer the '-' separator always precedes '+', so the numeric
       core ends at the first '-' if present, else at the first '+'. */
    plus = strchr(p, '+');
    dash = strchr(p, '-');
    core_len = (size_t)(dash ? dash - p : (plus ? plus - p : strlen(p)));
    if (core_len == 0 || core_len >= sizeof(core)) {
        return 0;
    }
    memcpy(core, p, core_len);
    core[core_len] = '\0';

    dot1 = strchr(core, '.');
    if (!dot1) {
        return 0;
    }
    dot2 = strchr(dot1 + 1, '.');
    if (!dot2 || strchr(dot2 + 1, '.') != NULL) {
        return 0;
    }
    *dot1 = '\0';
    *dot2 = '\0';

    if (!semver_is_numeric_id(core, strlen(core)) ||
        !semver_is_numeric_id(dot1 + 1, strlen(dot1 + 1)) ||
        !semver_is_numeric_id(dot2 + 1, strlen(dot2 + 1))) {
        return 0;
    }
    out->major = strtoll(core, &endp, 10);
    out->minor = strtoll(dot1 + 1, &endp, 10);
    out->patch = strtoll(dot2 + 1, &endp, 10);

    if (dash) {
        size_t pre_len = (size_t)((plus ? plus : (p + strlen(p))) - (dash + 1));
        if (pre_len == 0) {
            return 0;
        }
        if (!semver_copy_part(out->pre, dash + 1, pre_len)) {
            return 0;
        }
        if (!semver_valid_identifiers(out->pre, 0)) {
            return 0;
        }
    }
    if (plus) {
        size_t build_len = strlen(plus + 1);
        if (build_len == 0) {
            return 0;
        }
        if (!semver_copy_part(out->build, plus + 1, build_len)) {
            return 0;
        }
        if (!semver_valid_identifiers(out->build, 1)) {
            return 0;
        }
    }
    return 1;
}

/* Prerelease precedence: identifiers compared pairwise, numeric before
   alphanumeric, and a shorter list sorts lower when it is a prefix. */
static int semver_cmp_prerelease(const char *a, const char *b) {
    const char *pa = a;
    const char *pb = b;
    if (pa[0] == '\0' && pb[0] == '\0') {
        return 0;
    }
    if (pa[0] == '\0') {
        return 1; /* release ranks above any prerelease */
    }
    if (pb[0] == '\0') {
        return -1;
    }
    while (1) {
        const char *ea = strchr(pa, '.');
        const char *eb = strchr(pb, '.');
        size_t la = ea ? (size_t)(ea - pa) : strlen(pa);
        size_t lb = eb ? (size_t)(eb - pb) : strlen(pb);
        int num_a = semver_is_numeric_id(pa, la);
        int num_b = semver_is_numeric_id(pb, lb);
        int r;
        if (num_a && num_b) {
            /* No leading zeros, so longer digit runs are larger numbers. */
            if (la != lb) {
                r = la < lb ? -1 : 1;
            } else {
                r = memcmp(pa, pb, la);
            }
        } else if (num_a != num_b) {
            r = num_a ? -1 : 1;
        } else {
            size_t m = la < lb ? la : lb;
            r = memcmp(pa, pb, m);
            if (r == 0 && la != lb) {
                r = la < lb ? -1 : 1;
            }
        }
        if (r != 0) {
            return r < 0 ? -1 : 1;
        }
        if (!ea && !eb) {
            return 0;
        }
        if (!ea) {
            return -1;
        }
        if (!eb) {
            return 1;
        }
        pa = ea + 1;
        pb = eb + 1;
    }
}

static int semver_cmp_core(const SemVer *a, const SemVer *b) {
    if (a->major != b->major) {
        return a->major < b->major ? -1 : 1;
    }
    if (a->minor != b->minor) {
        return a->minor < b->minor ? -1 : 1;
    }
    if (a->patch != b->patch) {
        return a->patch < b->patch ? -1 : 1;
    }
    return semver_cmp_prerelease(a->pre, b->pre);
}

long long xsemver_valid(const char *v) {
    SemVer parsed;
    return semver_parse(v, &parsed);
}

long long xsemver_major(const char *v) {
    SemVer parsed;
    if (!semver_parse(v, &parsed)) {
        return -1;
    }
    return parsed.major;
}

long long xsemver_minor(const char *v) {
    SemVer parsed;
    if (!semver_parse(v, &parsed)) {
        return -1;
    }
    return parsed.minor;
}

long long xsemver_patch(const char *v) {
    SemVer parsed;
    if (!semver_parse(v, &parsed)) {
        return -1;
    }
    return parsed.patch;
}

const char *xsemver_prerelease(const char *v) {
    SemVer parsed;
    char *slot = semver_ring_take();
    slot[0] = '\0';
    if (semver_parse(v, &parsed)) {
        strcpy(slot, parsed.pre);
    }
    return slot;
}

const char *xsemver_build(const char *v) {
    SemVer parsed;
    char *slot = semver_ring_take();
    slot[0] = '\0';
    if (semver_parse(v, &parsed)) {
        strcpy(slot, parsed.build);
    }
    return slot;
}

const char *xsemver_clean(const char *v) {
    SemVer parsed;
    char *slot = semver_ring_take();
    const char *p = v;
    size_t pos = 0;
    slot[0] = '\0';
    if (!v) {
        return slot;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == 'v' || *p == 'V' || *p == '=') {
        p++;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    {
        /* Trim trailing whitespace so padded input still cleans. */
        char trimmed[XSEMVER_MAX_TEXT];
        size_t len;
        strncpy(trimmed, p, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        len = strlen(trimmed);
        while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t')) {
            trimmed[--len] = '\0';
        }
        if (!semver_parse(trimmed, &parsed)) {
            return slot;
        }
    }
    pos = (size_t)snprintf(slot, XSEMVER_MAX_FIELD, "%lld.%lld.%lld",
        parsed.major, parsed.minor, parsed.patch);
    if (parsed.pre[0] != '\0' && pos < XSEMVER_MAX_FIELD) {
        pos += (size_t)snprintf(slot + pos, XSEMVER_MAX_FIELD - pos, "-%s", parsed.pre);
    }
    if (parsed.build[0] != '\0' && pos < XSEMVER_MAX_FIELD) {
        snprintf(slot + pos, XSEMVER_MAX_FIELD - pos, "+%s", parsed.build);
    }
    return slot;
}

long long xsemver_compare(const char *a, const char *b) {
    SemVer va;
    SemVer vb;
    if (!semver_parse(a, &va) || !semver_parse(b, &vb)) {
        return -2;
    }
    return semver_cmp_core(&va, &vb);
}

/*
 * Range matching, node-semver style.
 */

enum {
    XSEMVER_OP_GE = 0,
    XSEMVER_OP_GT,
    XSEMVER_OP_LE,
    XSEMVER_OP_LT,
    XSEMVER_OP_EQ
};

typedef struct {
    int op;
    SemVer bound;
} Comparator;

typedef struct {
    Comparator items[XSEMVER_MAX_SET_COMPARATORS];
    int count;
} ComparatorSet;

typedef struct {
    /* present[i]: part i was a concrete number; star[i]: part was x, X,
       star, or absent */
    int present[3];
    int star[3];
    long long num[3];
    char pre[XSEMVER_MAX_FIELD];
    char build[XSEMVER_MAX_FIELD];
    int ok;
} Partial;

static int semver_parse_partial(const char *s, Partial *out) {
    char buf[XSEMVER_MAX_TEXT];
    char *parts[4];
    int count = 0;
    char *tok;
    char *meta;
    int i;

    memset(out, 0, sizeof(*out));
    if (!s) {
        return 0;
    }
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Split off prerelease/build metadata; only meaningful when the numeric
       core is fully concrete. The metadata separator is the first '-' or '+'
       because numeric parts never contain either character. */
    meta = buf;
    while (*meta != '\0' && *meta != '-' && *meta != '+') {
        meta++;
    }
    if (*meta != '\0') {
        char pre_buf[XSEMVER_MAX_FIELD];
        if (*meta == '-') {
            const char *pre_start = meta + 1;
            const char *plus = strchr(pre_start, '+');
            size_t pre_len = plus ? (size_t)(plus - pre_start) : strlen(pre_start);
            if (pre_len == 0 || pre_len >= sizeof(pre_buf)) {
                return 0;
            }
            memcpy(pre_buf, pre_start, pre_len);
            pre_buf[pre_len] = '\0';
            if (!semver_valid_identifiers(pre_buf, 0)) {
                return 0;
            }
            strcpy(out->pre, pre_buf);
        }
        *meta = '\0';
    }

    tok = strtok(buf, ".");
    while (tok && count < 4) {
        parts[count++] = tok;
        tok = strtok(NULL, ".");
    }
    if (count < 1 || count > 3) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (i < count) {
            const char *t = parts[i];
            if (strcmp(t, "x") == 0 || strcmp(t, "X") == 0 || strcmp(t, "*") == 0 || t[0] == '\0') {
                out->star[i] = 1;
            } else if (semver_is_numeric_id(t, strlen(t))) {
                out->present[i] = 1;
                out->num[i] = strtoll(t, NULL, 10);
            } else {
                return 0;
            }
        } else {
            out->star[i] = 1;
        }
    }
    out->ok = 1;
    return 1;
}

static void semver_fill(SemVer *v, long long major, long long minor,
    long long patch, const char *pre) {
    memset(v, 0, sizeof(*v));
    v->major = major;
    v->minor = minor;
    v->patch = patch;
    if (pre) {
        strncpy(v->pre, pre, XSEMVER_MAX_FIELD - 1);
        v->pre[XSEMVER_MAX_FIELD - 1] = '\0';
    }
}

static int semver_add(ComparatorSet *set, int op, long long major,
    long long minor, long long patch, const char *pre) {
    if (set->count >= XSEMVER_MAX_SET_COMPARATORS) {
        return 0;
    }
    semver_fill(&set->items[set->count].bound, major, minor, patch, pre);
    set->items[set->count].op = op;
    set->count++;
    return 1;
}

/* Turn one token (with or without an operator prefix) into primitive
   comparators. Returns 0 when the token cannot be understood. */
static int semver_desugar(ComparatorSet *set, const char *token) {
    int op = -1;
    const char *rest = token;
    Partial p;
    long long M;
    long long m;
    long long pt;

    if (strncmp(rest, ">=", 2) == 0) { op = XSEMVER_OP_GE; rest += 2; }
    else if (strncmp(rest, "<=", 2) == 0) { op = XSEMVER_OP_LE; rest += 2; }
    else if (rest[0] == '>') { op = XSEMVER_OP_GT; rest += 1; }
    else if (rest[0] == '<') { op = XSEMVER_OP_LT; rest += 1; }
    else if (rest[0] == '=') { op = XSEMVER_OP_EQ; rest += 1; }
    else if (rest[0] == '~') { op = 100; rest += 1; }
    else if (rest[0] == '^') { op = 101; rest += 1; }
    else { op = XSEMVER_OP_EQ; }

    if (rest[0] == '\0' || strcmp(rest, "*") == 0 || strcmp(rest, "x") == 0 ||
        strcmp(rest, "X") == 0) {
        /* "*" matches every version, including none added here. */
        if (op == XSEMVER_OP_GT || op == XSEMVER_OP_LT) {
            return 0;
        }
        return semver_add(set, XSEMVER_OP_GE, 0, 0, 0, NULL);
    }
    if (!semver_parse_partial(rest, &p)) {
        return 0;
    }
    M = p.present[0] ? p.num[0] : 0;
    m = p.present[1] ? p.num[1] : 0;
    pt = p.present[2] ? p.num[2] : 0;

    if (op == XSEMVER_OP_GT || op == XSEMVER_OP_LT || op == XSEMVER_OP_GE) {
        if (!p.present[0]) {
            return op == XSEMVER_OP_GE && semver_add(set, XSEMVER_OP_GE, 0, 0, 0, NULL);
        }
        if (p.present[2]) {
            return semver_add(set, op, M, m, pt, p.pre[0] ? p.pre : NULL);
        }
        if (p.present[1]) {
            if (op == XSEMVER_OP_GT) {
                return semver_add(set, XSEMVER_OP_GE, M, m + 1, 0, NULL);
            }
            if (op == XSEMVER_OP_LE) {
                return semver_add(set, XSEMVER_OP_LT, M, m + 1, 0, NULL);
            }
            if (op == XSEMVER_OP_LT) {
                return semver_add(set, XSEMVER_OP_LT, M, m, 0, NULL);
            }
            return semver_add(set, XSEMVER_OP_GE, M, m, 0, NULL);
        }
        if (op == XSEMVER_OP_GT) {
            return semver_add(set, XSEMVER_OP_GE, M + 1, 0, 0, NULL);
        }
        if (op == XSEMVER_OP_LE) {
            return semver_add(set, XSEMVER_OP_LT, M + 1, 0, 0, NULL);
        }
        if (op == XSEMVER_OP_LT) {
            return semver_add(set, XSEMVER_OP_LT, M, 0, 0, NULL);
        }
        return semver_add(set, XSEMVER_OP_GE, M, 0, 0, NULL);
    }
    if (op == XSEMVER_OP_LE && !p.present[2] && p.present[1]) {
        return semver_add(set, XSEMVER_OP_LT, M, m + 1, 0, NULL);
    }
    if (op == XSEMVER_OP_LE && !p.present[1]) {
        return semver_add(set, XSEMVER_OP_LT, M + 1, 0, 0, NULL);
    }
    if (op == 100) { /* tilde */
        if (!p.present[0]) {
            return semver_add(set, XSEMVER_OP_GE, 0, 0, 0, NULL);
        }
        if (!semver_add(set, XSEMVER_OP_GE, M, m, pt, p.pre[0] ? p.pre : NULL)) {
            return 0;
        }
        if (p.present[1]) {
            return semver_add(set, XSEMVER_OP_LT, M, m + 1, 0, NULL);
        }
        return semver_add(set, XSEMVER_OP_LT, M + 1, 0, 0, NULL);
    }
    if (op == 101) { /* caret */
        if (!p.present[0]) {
            return semver_add(set, XSEMVER_OP_GE, 0, 0, 0, NULL);
        }
        if (!semver_add(set, XSEMVER_OP_GE, M, m, pt, p.pre[0] ? p.pre : NULL)) {
            return 0;
        }
        if (M > 0) {
            return semver_add(set, XSEMVER_OP_LT, M + 1, 0, 0, NULL);
        }
        if (p.present[1] && m > 0) {
            return semver_add(set, XSEMVER_OP_LT, 0, m + 1, 0, NULL);
        }
        if (p.present[1] && p.present[2] && m == 0) {
            return semver_add(set, XSEMVER_OP_LT, 0, 0, pt + 1, NULL);
        }
        if (p.present[1] && !p.present[2] && m == 0) {
            return semver_add(set, XSEMVER_OP_LT, 0, 1, 0, NULL);
        }
        return semver_add(set, XSEMVER_OP_LT, 1, 0, 0, NULL);
    }
    /* exact or partial equality */
    if (!p.present[0]) {
        return semver_add(set, XSEMVER_OP_GE, 0, 0, 0, NULL);
    }
    if (p.present[2]) {
        return semver_add(set, XSEMVER_OP_EQ, M, m, pt, p.pre[0] ? p.pre : NULL);
    }
    if (!semver_add(set, XSEMVER_OP_GE, M, m, 0, NULL)) {
        return 0;
    }
    if (p.present[1]) {
        return semver_add(set, XSEMVER_OP_LT, M, m + 1, 0, NULL);
    }
    return semver_add(set, XSEMVER_OP_LT, M + 1, 0, 0, NULL);
}

static int semver_parse_set(const char *text, ComparatorSet *set) {
    char buf[XSEMVER_MAX_TEXT];
    char *hyphen;
    char *tok;
    int i;

    set->count = 0;
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    hyphen = strstr(buf, " - ");
    if (hyphen) {
        Partial lo;
        Partial hi;
        char left[XSEMVER_MAX_TEXT];
        char right[XSEMVER_MAX_TEXT];
        size_t left_len = (size_t)(hyphen - buf);
        size_t right_len = strlen(hyphen + 3);
        if (left_len >= sizeof(left) || right_len >= sizeof(right)) {
            return 0;
        }
        memcpy(left, buf, left_len);
        left[left_len] = '\0';
        memcpy(right, hyphen + 3, right_len);
        right[right_len] = '\0';
        if (!semver_parse_partial(left, &lo) || !semver_parse_partial(right, &hi)) {
            return 0;
        }
        if (!lo.present[0] || !hi.present[0]) {
            return 0;
        }
        if (!semver_add(set, XSEMVER_OP_GE,
                lo.num[0], lo.present[1] ? lo.num[1] : 0,
                lo.present[2] ? lo.num[2] : 0, lo.pre[0] ? lo.pre : NULL)) {
            return 0;
        }
        if (!hi.present[2]) {
            if (hi.present[1]) {
                return semver_add(set, XSEMVER_OP_LT, hi.num[0], hi.num[1] + 1, 0, NULL);
            }
            return semver_add(set, XSEMVER_OP_LT, hi.num[0] + 1, 0, 0, NULL);
        }
        return semver_add(set, XSEMVER_OP_LE, hi.num[0], hi.num[1], hi.num[2],
            hi.pre[0] ? hi.pre : NULL);
    }

    /* Collect tokens first, then desugar: parse_partial uses strtok
       internally, so the outer scan must complete before it runs. */
    {
        char *tokens[32];
        int ntokens = 0;
        tok = strtok(buf, " \t");
        while (tok) {
            if (ntokens >= 32) {
                return 0;
            }
            tokens[ntokens++] = tok;
            tok = strtok(NULL, " \t");
        }
        for (i = 0; i < ntokens; i++) {
            if (!semver_desugar(set, tokens[i])) {
                return 0;
            }
        }
    }
    return set->count > 0;
}

static int semver_comparator_matches(const Comparator *c, const SemVer *v) {
    int r = semver_cmp_core(v, &c->bound);
    switch (c->op) {
        case XSEMVER_OP_GE: return r >= 0;
        case XSEMVER_OP_GT: return r > 0;
        case XSEMVER_OP_LE: return r <= 0;
        case XSEMVER_OP_LT: return r < 0;
        default: return r == 0;
    }
}

static int semver_set_matches(const ComparatorSet *set, const SemVer *v) {
    int i;
    for (i = 0; i < set->count; i++) {
        if (!semver_comparator_matches(&set->items[i], v)) {
            return 0;
        }
    }
    /* Prerelease versions only satisfy sets that opt in on the same
       major.minor.patch tuple. */
    if (v->pre[0] != '\0') {
        int allowed = 0;
        for (i = 0; i < set->count; i++) {
            const SemVer *b = &set->items[i].bound;
            if (b->pre[0] != '\0' && b->major == v->major &&
                b->minor == v->minor && b->patch == v->patch) {
                allowed = 1;
                break;
            }
        }
        if (!allowed) {
            return 0;
        }
    }
    return 1;
}

long long xsemver_satisfies(const char *version, const char *range) {
    SemVer v;
    char range_buf[XSEMVER_MAX_TEXT];
    char *alt;
    char *cursor;

    if (!semver_parse(version, &v)) {
        return 0;
    }
    if (!range) {
        return 0;
    }
    strncpy(range_buf, range, sizeof(range_buf) - 1);
    range_buf[sizeof(range_buf) - 1] = '\0';

    cursor = range_buf;
    while (1) {
        ComparatorSet set;
        alt = strstr(cursor, "||");
        if (alt) {
            *alt = '\0';
        }
        /* Trim whitespace around the alternative. */
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        {
            size_t len = strlen(cursor);
            while (len > 0 && (cursor[len - 1] == ' ' || cursor[len - 1] == '\t')) {
                cursor[--len] = '\0';
            }
        }
        if (cursor[0] == '\0') {
            /* Empty alternative matches everything. */
            if (v.pre[0] == '\0') {
                return 1;
            }
        } else if (semver_parse_set(cursor, &set) &&
                   semver_set_matches(&set, &v)) {
            return 1;
        }
        if (!alt) {
            break;
        }
        cursor = alt + 2;
    }
    return 0;
}
