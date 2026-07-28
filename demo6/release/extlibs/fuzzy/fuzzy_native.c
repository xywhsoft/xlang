/*
 * fuzzy_native.c - Fuzzy string matching (Levenshtein, subsequence match, ratio)
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern void xrtSetError(const char *msg);

/* Levenshtein edit distance */
long long xfuzzy_levenshtein(const char *a, const char *b) {
    int la, lb, i, j;
    int *prev, *curr, *tmp;
    if (!a) a = "";
    if (!b) b = "";
    la = (int)strlen(a);
    lb = (int)strlen(b);
    if (la == 0) return lb;
    if (lb == 0) return la;

    prev = (int *)malloc(sizeof(int) * (lb + 1));
    curr = (int *)malloc(sizeof(int) * (lb + 1));
    for (j = 0; j <= lb; j++) prev[j] = j;

    for (i = 1; i <= la; i++) {
        curr[0] = i;
        for (j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j-1] + 1;
            int sub = prev[j-1] + cost;
            int min = del < ins ? del : ins;
            curr[j] = min < sub ? min : sub;
        }
        tmp = prev; prev = curr; curr = tmp;
    }
    i = prev[lb];
    free(prev);
    free(curr);
    return i;
}

/* Subsequence fuzzy match: returns score >= 0 if pattern is subsequence of text, -1 otherwise */
long long xfuzzy_match(const char *pattern, const char *text) {
    int score = 0;
    int consecutive = 0;
    int last_match = -2;
    int pi = 0, ti = 0;
    int plen, tlen;
    if (!pattern || !text) return -1;
    plen = (int)strlen(pattern);
    tlen = (int)strlen(text);
    if (plen == 0) return 0;
    if (plen > tlen) return -1;

    while (pi < plen && ti < tlen) {
        if (pattern[pi] == text[ti] ||
            (pattern[pi] >= 'A' && pattern[pi] <= 'Z' && pattern[pi] + 32 == text[ti]) ||
            (text[ti] >= 'A' && text[ti] <= 'Z' && text[ti] + 32 == pattern[pi])) {
            /* bonus for consecutive matches */
            if (ti == last_match + 1) { consecutive++; score += consecutive * 2; }
            else { consecutive = 0; }
            /* bonus for match at start or after separator */
            if (ti == 0 || text[ti-1] == '_' || text[ti-1] == '-' || text[ti-1] == ' ' || text[ti-1] == '/') {
                score += 5;
            }
            score += 1;
            last_match = ti;
            pi++;
        }
        ti++;
    }
    if (pi < plen) return -1;  /* not all pattern chars matched */
    return score;
}

/* Similarity ratio 0-100 based on Levenshtein */
long long xfuzzy_ratio(const char *a, const char *b) {
    int la, lb, dist, max_len;
    if (!a) a = "";
    if (!b) b = "";
    la = (int)strlen(a);
    lb = (int)strlen(b);
    if (la == 0 && lb == 0) return 100;
    max_len = la > lb ? la : lb;
    dist = xfuzzy_levenshtein(a, b);
    return (int)((1.0 - (double)dist / max_len) * 100.0);
}

/* Best match from a list of candidates (newline-separated) */
static char fuzzy_buf[4096];

const char *xfuzzy_best(const char *pattern, const char *candidates) {
    char buf[8192];
    char *line, *save;
    int best_score = -1;
    const char *best_match = "";
    if (!pattern || !candidates) return "";
    strncpy(buf, candidates, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    fuzzy_buf[0] = '\0';
    line = strtok(buf, "\n");
    while (line) {
        int score = xfuzzy_match(pattern, line);
        if (score > best_score) {
            best_score = score;
            strncpy(fuzzy_buf, line, sizeof(fuzzy_buf) - 1);
            fuzzy_buf[sizeof(fuzzy_buf) - 1] = '\0';
        }
        line = strtok(NULL, "\n");
    }
    if (best_score < 0) return "";
    return fuzzy_buf;
}
