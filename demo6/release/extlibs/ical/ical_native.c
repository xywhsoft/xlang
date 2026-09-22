/*
 * ical_native.c - RFC 5545 iCalendar parsing and generation.
 *
 * parse unfolds continuation lines (leading space or tab), decodes the
   \\ \n \; \, escapes, and collects VEVENT and VTODO properties into
   {events: [...], todos: [...]}. Property parameters such as
   DTSTART;TZID=... are dropped; values stay as source strings, so
   dates keep their ISO 8601 or Z form.
 * build re-escapes values, emits VCALENDAR/VEVENT, and folds output
   lines at 74 octets with a single leading space continuation byte.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XI_DOMAIN "xlang.extlib.ical"

static void xi_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XI_DOMAIN, 0, message);
}

static xvalue *xi_null(void) {
    return xrtValueNull();
}

static xvalue *xi_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xi_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

static int xi_set_str(xvalue *obj, const char *key, const char *s) {
    return xi_set(obj, key, xi_text(s ? s : "", s ? strlen(s) : 0));
}

/* Map DTSTART to dtstart etc.; returns the mapped key or NULL. */
static const char *xi_prop_key(const char *name) {
    static const char *const known[] = {
        "UID", "SUMMARY", "DTSTART", "DTEND", "LOCATION", "DESCRIPTION",
        "ORGANIZER", "STATUS", "RRULE", "DUE", "PRIORITY", "CATEGORIES",
        "URL", "CREATED", "LAST-MODIFIED", "SEQUENCE", "TRANSP"
    };
    static const char *const mapped[] = {
        "uid", "summary", "dtstart", "dtend", "location", "description",
        "organizer", "status", "rrule", "due", "priority", "categories",
        "url", "created", "lastModified", "sequence", "transp"
    };
    size_t i;
    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (strcmp(name, known[i]) == 0) return mapped[i];
    }
    return NULL;
}

static void xi_unescape(const char *src, size_t len, char *dst) {
    size_t i;
    size_t o = 0;
    for (i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len) {
            char c = src[i + 1];
            if (c == 'n' || c == 'N') dst[o++] = '\n';
            else if (c == '\\') dst[o++] = '\\';
            else if (c == ';') dst[o++] = ';';
            else if (c == ',') dst[o++] = ',';
            else {
                dst[o++] = src[i];
                dst[o++] = c;
            }
            i++;
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
}

xvalue *xi_parse(const char *text) {
    xvalue *result;
    xvalue *events;
    xvalue *todos;
    xvalue *current;
    xvalue *currentList;
    const char *p;
    if (text == NULL) {
        xi_error("ical text is null");
        return xi_null();
    }
    result = xrtValueObject();
    events = xrtValueArray();
    todos = xrtValueArray();
    if (result == NULL || events == NULL || todos == NULL) {
        return xi_null();
    }
    xi_set(result, "events", events);
    xi_set(result, "todos", todos);
    current = NULL;
    currentList = NULL;
    p = text;
    while (p != NULL && *p != '\0') {
        const char *eol = strchr(p, '\n');
        const char *lineEnd = eol ? eol : p + strlen(p);
        char line[4096];
        size_t len = (size_t)(lineEnd - p);
        /* copy the first segment before appending continuations */
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        while (len > 0 && line[len - 1] == '\r') len--;
        line[len] = '\0';
        /* unfold: continuation lines start with space or tab */
        while (eol != NULL &&
               (eol[1] == ' ' || eol[1] == '\t') &&
               len < sizeof(line) - 1) {
            const char *next = eol + 2; /* skip the single folding marker */
            const char *nextEol = strchr(next, '\n');
            size_t nlen = nextEol ? (size_t)(nextEol - next)
                                  : strlen(next);
            /* each continuation segment may end with \r before its \n */
            while (nlen > 0 && next[nlen - 1] == '\r') nlen--;
            if (nlen > sizeof(line) - 1 - len) {
                nlen = sizeof(line) - 1 - len;
            }
            memcpy(line + len, next, nlen);
            len += nlen;
            eol = nextEol;
        }
        line[len] = '\0';
        while (len > 0 && (line[len - 1] == '\r')) len--;
        line[len] = '\0';
        p = eol ? eol + 1 : NULL;
        if (line[0] == '\0') continue;
        /* component begin/end */
        if (strncmp(line, "BEGIN:VEVENT", 12) == 0) {
            current = xrtValueObject();
            currentList = events;
        } else if (strncmp(line, "BEGIN:VTODO", 11) == 0) {
            current = xrtValueObject();
            currentList = todos;
        } else if (strncmp(line, "END:VEVENT", 10) == 0 ||
                   strncmp(line, "END:VTODO", 9) == 0) {
            if (current != NULL && currentList != NULL) {
                xrtValueArrayAppendNew(currentList, current);
                current = NULL;
                currentList = NULL;
            }
        } else if (current != NULL && strchr(line, ':') != NULL) {
            /* property NAME[;param]=... : value */
            char name[64];
            const char *colon = strchr(line, ':');
            const char *semi = (const char *)memchr(line, ';',
                (size_t)(colon - line));
            size_t nameLen = (semi ? semi : colon) - line;
            if (nameLen < sizeof(name)) {
                memcpy(name, line, nameLen);
                name[nameLen] = '\0';
                {
                    const char *mapped = xi_prop_key(name);
                    if (mapped != NULL) {
                        char value[2048];
                        xi_unescape(colon + 1, strlen(colon + 1), value);
                        xi_set_str(current, mapped, value);
                    }
                }
            }
        }
    }
    return result;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XiBuf;

static int xi_put(XiBuf *b, const char *text, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 1024;
        char *grown;
        while (cap < b->len + n + 1) cap *= 2;
        grown = (char *)xrtMalloc(cap);
        if (grown == NULL) return 0;
        memcpy(grown, b->data, b->len);
        grown[b->len] = '\0';
        xrtFree(b->data);
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 1;
}

static int xi_puts(XiBuf *b, const char *text) {
    return xi_put(b, text, strlen(text));
}

/* Emit "NAME:value" with escaping and 74-octet folding. */
static int xi_emit(XiBuf *b, const char *name, const char *value) {
    char escaped[2048];
    size_t o = 0;
    const char *p;
    int col = 0;
    if (!xi_puts(b, name) || !xi_puts(b, ":")) return 0;
    col = (int)strlen(name) + 1;
    for (p = value ? value : ""; *p != '\0' && o + 3 < sizeof(escaped);
         ++p) {
        switch (*p) {
            case '\\': escaped[o++] = '\\'; escaped[o++] = '\\'; break;
            case ';':  escaped[o++] = '\\'; escaped[o++] = ';';  break;
            case ',':  escaped[o++] = '\\'; escaped[o++] = ',';  break;
            case '\n': escaped[o++] = '\\'; escaped[o++] = 'n';  break;
            default:   escaped[o++] = *p;
        }
    }
    escaped[o] = '\0';
    {
        size_t i;
        for (i = 0; i < o; i++) {
            if (col >= 74) {
                if (!xi_puts(b, "\r\n ")) return 0;
                col = 1;
            }
            {
                char one[2] = {escaped[i], 0};
                if (!xi_puts(b, one)) return 0;
                col++;
            }
        }
    }
    return xi_puts(b, "\r\n");
}

static xvalue *xi_get(xvalue *obj, const char *key) {
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (obj == NULL) return NULL;
    n = xrtValueCount(obj);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            return v;
        }
    }
    return NULL;
}

static const char *xi_read(xvalue *obj, const char *key) {
    static char ring[6][256];
    static int next = 0;
    char *slot = ring[next];
    xvalue *value;
    xstrview text;
    next = (next + 1) % 6;
    slot[0] = '\0';
    value = xi_get(obj, key);
    if (value != NULL && xrtValueGetString(value, &text)) {
        if (text.Size >= 256) text.Size = 255;
        memcpy(slot, text.Data, text.Size);
        slot[text.Size] = '\0';
    }
    return slot;
}

static int xi_write_component(XiBuf *b, const char *kind, xvalue *item) {
    if (!xi_puts(b, "BEGIN:") || !xi_puts(b, kind) ||
        !xi_puts(b, "\r\n")) {
        return 0;
    }
    {
        static const char *const keys[] = {
            "uid", "dtstart", "dtend", "summary", "description",
            "location", "organizer", "status", "rrule", "due",
            "priority", "categories", "url", "created", "lastModified",
            "sequence", "transp"
        };
        static const char *const names[] = {
            "UID", "DTSTART", "DTEND", "SUMMARY", "DESCRIPTION",
            "LOCATION", "ORGANIZER", "STATUS", "RRULE", "DUE",
            "PRIORITY", "CATEGORIES", "URL", "CREATED", "LAST-MODIFIED",
            "SEQUENCE", "TRANSP"
        };
        size_t i;
        for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *value = xi_read(item, keys[i]);
            if (value[0] != '\0') {
                if (!xi_emit(b, names[i], value)) return 0;
            }
        }
    }
    return xi_puts(b, "END:") && xi_puts(b, kind) && xi_puts(b, "\r\n");
}

const char *xi_build(xvalue *calendar) {
    static char ring[2][65536];
    static int next;
    XiBuf buf = {NULL, 0, 0};
    xvalue *events;
    xvalue *todos;
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    ring[next][0] = '\0';
    if (!xi_puts(&buf, "BEGIN:VCALENDAR\r\n") ||
        !xi_puts(&buf, "VERSION:2.0\r\n") ||
        !xi_puts(&buf, "PRODID:-//xlang//ical extlib//EN\r\n")) {
        xrtFree(buf.data);
        return ring[next];
    }
    events = xi_get(calendar, "events");
    n = events != NULL ? xrtValueCount(events) : 0;
    for (i = 0; i < n; i++) {
        xvalue *item = xrtValueArrayAt(events, i);
        if (item != NULL && !xi_write_component(&buf, "VEVENT", item)) {
            xrtFree(buf.data);
            return ring[next];
        }
    }
    todos = xi_get(calendar, "todos");
    n = todos != NULL ? xrtValueCount(todos) : 0;
    for (i = 0; i < n; i++) {
        xvalue *item = xrtValueArrayAt(todos, i);
        if (item != NULL && !xi_write_component(&buf, "VTODO", item)) {
            xrtFree(buf.data);
            return ring[next];
        }
    }
    if (!xi_puts(&buf, "END:VCALENDAR\r\n")) {
        xrtFree(buf.data);
        return ring[next];
    }
    strncpy(ring[next], buf.data ? buf.data : "", sizeof(ring[next]) - 1);
    ring[next][sizeof(ring[next]) - 1] = '\0';
    xrtFree(buf.data);
    return ring[next];
}
