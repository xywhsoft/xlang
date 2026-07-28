/* ncurses_native.c - Virtual terminal screen buffer */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ROWS 64
#define MAX_COLS 128

static char screen[MAX_ROWS][MAX_COLS + 1];
static int attr_screen[MAX_ROWS][MAX_COLS];
static int cur_row = 0, cur_col = 0;
static int nrows = 24, ncols = 80;
static int cur_attr = 0;

/* Initialize screen */
long long xncurses_init(long long rows, long long cols) {
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (cols > MAX_COLS) cols = MAX_COLS;
    nrows = (int)rows; ncols = (int)cols;
    int r, c;
    for (r = 0; r < nrows; r++) {
        for (c = 0; c < ncols; c++) { screen[r][c] = ' '; attr_screen[r][c] = 0; }
        screen[r][ncols] = '\0';
    }
    cur_row = 0; cur_col = 0; cur_attr = 0;
    return 1;
}

/* Clear screen */
void xncurses_clear(void) {
    int r, c;
    for (r = 0; r < nrows; r++)
        for (c = 0; c < ncols; c++) { screen[r][c] = ' '; attr_screen[r][c] = 0; }
}

/* Move cursor */
void xncurses_move(long long row, long long col) {
    cur_row = (int)row; cur_col = (int)col;
    if (cur_row >= nrows) cur_row = nrows - 1;
    if (cur_col >= ncols) cur_col = ncols - 1;
}

/* Add string at cursor */
void xncurses_addstr(const char *s) {
    int len = (int)strlen(s);
    int i;
    for (i = 0; i < len && cur_col < ncols; i++) {
        screen[cur_row][cur_col] = s[i];
        attr_screen[cur_row][cur_col] = cur_attr;
        cur_col++;
    }
}

/* Add string at position */
void xncurses_mvaddstr(long long row, long long col, const char *s) {
    xncurses_move(row, col);
    xncurses_addstr(s);
}

/* Add character at cursor */
void xncurses_addch(long long ch) {
    if (cur_col < ncols) {
        screen[cur_row][cur_col] = (char)ch;
        attr_screen[cur_row][cur_col] = cur_attr;
        cur_col++;
    }
}

/* Get character at position */
long long xncurses_inch(long long row, long long col) {
    if (row < 0 || row >= nrows || col < 0 || col >= ncols) return 0;
    return (long long)(unsigned char)screen[row][col];
}

/* Set attribute (1=bold, 2=underline, 4=reverse, 8=blink) */
void xncurses_attrset(long long attr) { cur_attr = (int)attr; }

/* Get attribute at position */
long long xncurses_attrat(long long row, long long col) {
    if (row < 0 || row >= nrows || col < 0 || col >= ncols) return 0;
    return (long long)attr_screen[row][col];
}

/* Draw horizontal line */
void xncurses_hline(long long row, long long col, long long ch, long long len) {
    int i;
    for (i = 0; i < (int)len && col + i < ncols; i++) {
        screen[row][col + i] = (char)ch;
        attr_screen[row][col + i] = cur_attr;
    }
}

/* Draw vertical line */
void xncurses_vline(long long row, long long col, long long ch, long long len) {
    int i;
    for (i = 0; i < (int)len && row + i < nrows; i++) {
        screen[row + i][col] = (char)ch;
        attr_screen[row + i][col] = cur_attr;
    }
}

/* Draw box (border) */
void xncurses_box(long long r1, long long c1, long long r2, long long c2) {
    xncurses_hline(r1, c1, '-', c2 - c1 + 1);
    xncurses_hline(r2, c1, '-', c2 - c1 + 1);
    xncurses_vline(r1, c1, '|', r2 - r1 + 1);
    xncurses_vline(r1, c2, '|', r2 - r1 + 1);
    screen[r1][c1] = '+'; screen[r1][c2] = '+';
    screen[r2][c1] = '+'; screen[r2][c2] = '+';
}

/* Get row content as string */
const char *xncurses_getrow(long long row) {
    static char row_buf[MAX_COLS + 1];
    if (row < 0 || row >= nrows) return "";
    memcpy(row_buf, screen[row], ncols);
    row_buf[ncols] = '\0';
    /* Trim trailing spaces */
    int len = ncols;
    while (len > 0 && row_buf[len-1] == ' ') len--;
    row_buf[len] = '\0';
    return row_buf;
}

/* Render full screen to string (rows separated by \n) */
const char *xncurses_render(void) {
    static char render_buf[MAX_ROWS * (MAX_COLS + 2)];
    int pos = 0, r;
    for (r = 0; r < nrows; r++) {
        int len = ncols;
        while (len > 0 && screen[r][len-1] == ' ') len--;
        memcpy(render_buf + pos, screen[r], len);
        pos += len;
        render_buf[pos++] = '\n';
    }
    render_buf[pos] = '\0';
    return render_buf;
}

/* Get cursor position (encoded as row*1000+col) */
long long xncurses_getyx(void) { return (long long)(cur_row * 1000 + cur_col); }

/* Get screen dimensions (encoded as rows*1000+cols) */
long long xncurses_getmax(void) { return (long long)(nrows * 1000 + ncols); }

/* Fill region with character */
void xncurses_fill(long long r1, long long c1, long long r2, long long c2, long long ch) {
    int r, c;
    for (r = (int)r1; r <= (int)r2 && r < nrows; r++)
        for (c = (int)c1; c <= (int)c2 && c < ncols; c++)
            screen[r][c] = (char)ch;
}
