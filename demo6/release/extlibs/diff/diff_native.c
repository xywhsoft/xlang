/*
 * diff_native.c - Line-based diff (LCS algorithm)
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern void xrtSetError(const char *msg);

static char diff_buf[16384];

#define DIFF_MAX_LINES 512
#define DIFF_LINE_MAX 1024

/* Split text into lines, returns count */
static int split_lines(const char *text, char lines[][DIFF_LINE_MAX], int max_lines) {
    int count = 0;
    const char *p = text;
    if (!text) return 0;
    while (*p && count < max_lines) {
        int i = 0;
        while (*p && *p != '\n' && i < DIFF_LINE_MAX - 1) {
            lines[count][i++] = *p++;
        }
        lines[count][i] = '\0';
        /* strip \r */
        if (i > 0 && lines[count][i-1] == '\r') lines[count][i-1] = '\0';
        count++;
        if (*p == '\n') p++;
    }
    return count;
}

/* LCS-based diff: outputs unified-style diff */
const char *xdiff_lines(const char *old_text, const char *new_text) {
    static char old_lines[DIFF_MAX_LINES][DIFF_LINE_MAX];
    static char new_lines[DIFF_MAX_LINES][DIFF_LINE_MAX];
    int m, n, i, j;
    int *lcs;  /* (m+1)*(n+1) table */
    int pos = 0;
    int max = (int)sizeof(diff_buf);

    if (!old_text) old_text = "";
    if (!new_text) new_text = "";

    m = split_lines(old_text, old_lines, DIFF_MAX_LINES);
    n = split_lines(new_text, new_lines, DIFF_MAX_LINES);

    /* allocate LCS table */
    lcs = (int *)calloc((m + 1) * (n + 1), sizeof(int));
    if (!lcs) { xrtSetError("diff: out of memory"); return ""; }

    /* fill LCS table */
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= n; j++) {
            if (strcmp(old_lines[i-1], new_lines[j-1]) == 0)
                lcs[i*(n+1)+j] = lcs[(i-1)*(n+1)+(j-1)] + 1;
            else {
                int a = lcs[(i-1)*(n+1)+j];
                int b = lcs[i*(n+1)+(j-1)];
                lcs[i*(n+1)+j] = a > b ? a : b;
            }
        }
    }

    /* backtrack to produce diff */
    diff_buf[0] = '\0';
    i = m; j = n;
    /* we need to output in forward order, so collect then reverse */
    {
        char tmp[16384];
        int tpos = 0;
        while (i > 0 || j > 0) {
            if (i > 0 && j > 0 && strcmp(old_lines[i-1], new_lines[j-1]) == 0) {
                tpos += snprintf(tmp + tpos, sizeof(tmp) - tpos, " %s\n", old_lines[i-1]);
                i--; j--;
            } else if (j > 0 && (i == 0 || lcs[i*(n+1)+(j-1)] >= lcs[(i-1)*(n+1)+j])) {
                tpos += snprintf(tmp + tpos, sizeof(tmp) - tpos, "+%s\n", new_lines[j-1]);
                j--;
            } else {
                tpos += snprintf(tmp + tpos, sizeof(tmp) - tpos, "-%s\n", old_lines[i-1]);
                i--;
            }
        }
        /* reverse lines */
        {
            char *lines_out[DIFF_MAX_LINES * 2];
            int lcount = 0;
            char *p = tmp;
            while (*p && lcount < DIFF_MAX_LINES * 2) {
                lines_out[lcount++] = p;
                while (*p && *p != '\n') p++;
                if (*p == '\n') *p++ = '\0';
            }
            for (i = lcount - 1; i >= 0; i--) {
                pos += snprintf(diff_buf + pos, max - pos, "%s\n", lines_out[i]);
            }
        }
    }
    free(lcs);
    return diff_buf;
}

/* Simple stats: additions, deletions, unchanged */
const char *xdiff_stats(const char *old_text, const char *new_text) {
    static char old_lines[DIFF_MAX_LINES][DIFF_LINE_MAX];
    static char new_lines[DIFF_MAX_LINES][DIFF_LINE_MAX];
    int m, n, i, j;
    int *lcs;
    int adds = 0, dels = 0, same = 0;

    if (!old_text) old_text = "";
    if (!new_text) new_text = "";
    m = split_lines(old_text, old_lines, DIFF_MAX_LINES);
    n = split_lines(new_text, new_lines, DIFF_MAX_LINES);

    lcs = (int *)calloc((m + 1) * (n + 1), sizeof(int));
    if (!lcs) return "0,0,0";
    for (i = 1; i <= m; i++)
        for (j = 1; j <= n; j++) {
            if (strcmp(old_lines[i-1], new_lines[j-1]) == 0)
                lcs[i*(n+1)+j] = lcs[(i-1)*(n+1)+(j-1)] + 1;
            else {
                int a = lcs[(i-1)*(n+1)+j], b = lcs[i*(n+1)+(j-1)];
                lcs[i*(n+1)+j] = a > b ? a : b;
            }
        }
    i = m; j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && strcmp(old_lines[i-1], new_lines[j-1]) == 0) { same++; i--; j--; }
        else if (j > 0 && (i == 0 || lcs[i*(n+1)+(j-1)] >= lcs[(i-1)*(n+1)+j])) { adds++; j--; }
        else { dels++; i--; }
    }
    free(lcs);
    snprintf(diff_buf, sizeof(diff_buf), "%d,%d,%d", adds, dels, same);
    return diff_buf;
}
