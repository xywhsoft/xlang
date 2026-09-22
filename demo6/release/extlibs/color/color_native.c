/*
 * color_native.c - Color parsing, conversion, and WCAG contrast helpers.
 *
 * Conventions:
 *   - Hex forms accepted: #RGB, #RGBA, #RRGGBB, #RRGGBBAA (case-insensitive).
 *   - Component readers return -1 for invalid input.
 *   - Invalid color names return "" from xcolor_from_name.
 *   - float results use double on the C side, matching xlang float.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define XCOLOR_RING 8
#define XCOLOR_TEXT 64

static char xcolor_ring[XCOLOR_RING][XCOLOR_TEXT];
static int xcolor_ring_next = 0;

static char *xcolor_slot(void) {
    char *slot = xcolor_ring[xcolor_ring_next];
    xcolor_ring_next = (xcolor_ring_next + 1) % XCOLOR_RING;
    return slot;
}

static int xcolor_hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

typedef struct {
    int r;
    int g;
    int b;
    int a;
} Rgba;

static int xcolor_parse_hex(const char *s, Rgba *out) {
    size_t len;
    int d[8];
    size_t i;
    if (!s) return 0;
    if (s[0] == '#') s++;
    len = strlen(s);
    if (len != 3 && len != 4 && len != 6 && len != 8) return 0;
    for (i = 0; i < len; i++) {
        d[i] = xcolor_hex_digit((unsigned char)s[i]);
        if (d[i] < 0) return 0;
    }
    if (len == 3) {
        out->r = d[0] * 17;
        out->g = d[1] * 17;
        out->b = d[2] * 17;
        out->a = 255;
    } else if (len == 4) {
        out->r = d[0] * 17;
        out->g = d[1] * 17;
        out->b = d[2] * 17;
        out->a = d[3] * 17;
    } else if (len == 6) {
        out->r = d[0] * 16 + d[1];
        out->g = d[2] * 16 + d[3];
        out->b = d[4] * 16 + d[5];
        out->a = 255;
    } else {
        out->r = d[0] * 16 + d[1];
        out->g = d[2] * 16 + d[3];
        out->b = d[4] * 16 + d[5];
        out->a = d[6] * 16 + d[7];
    }
    return 1;
}

long long xcolor_valid(const char *s) {
    Rgba c;
    return xcolor_parse_hex(s, &c);
}

long long xcolor_red(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return -1;
    return c.r;
}

long long xcolor_green(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return -1;
    return c.g;
}

long long xcolor_blue(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return -1;
    return c.b;
}

long long xcolor_alpha(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return -1;
    return c.a;
}

const char *xcolor_hex(long long r, long long g, long long b) {
    char *slot = xcolor_slot();
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XCOLOR_TEXT, "#%02X%02X%02X", (int)r, (int)g, (int)b);
    return slot;
}

const char *xcolor_hex_a(long long r, long long g, long long b, long long a) {
    char *slot = xcolor_slot();
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 ||
        a < 0 || a > 255) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XCOLOR_TEXT, "#%02X%02X%02X%02X", (int)r, (int)g, (int)b, (int)a);
    return slot;
}

const char *xcolor_normalize(const char *s) {
    Rgba c;
    char *slot = xcolor_slot();
    slot[0] = '\0';
    if (!xcolor_parse_hex(s, &c)) return slot;
    snprintf(slot, XCOLOR_TEXT, "#%02X%02X%02X", c.r, c.g, c.b);
    return slot;
}

static double xcolor_max3(double a, double b, double c) {
    double m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

static double xcolor_min3(double a, double b, double c) {
    double m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

/* hue in [0,360), saturation/value in [0,1] */
static void xcolor_rgb_to_hsv(const Rgba *c, double *h, double *s, double *v) {
    double r = c->r / 255.0;
    double g = c->g / 255.0;
    double b = c->b / 255.0;
    double max = xcolor_max3(r, g, b);
    double min = xcolor_min3(r, g, b);
    double delta = max - min;
    *v = max;
    *s = max > 0 ? delta / max : 0;
    if (delta <= 0) {
        *h = 0;
    } else if (max == r) {
        *h = 60.0 * fmod((g - b) / delta, 6.0);
    } else if (max == g) {
        *h = 60.0 * ((b - r) / delta + 2.0);
    } else {
        *h = 60.0 * ((r - g) / delta + 4.0);
    }
    if (*h < 0) *h += 360.0;
}

static void xcolor_rgb_to_hsl(const Rgba *c, double *h, double *s, double *l) {
    double r = c->r / 255.0;
    double g = c->g / 255.0;
    double b = c->b / 255.0;
    double max = xcolor_max3(r, g, b);
    double min = xcolor_min3(r, g, b);
    double delta = max - min;
    *l = (max + min) / 2.0;
    if (delta <= 0) {
        *h = 0;
        *s = 0;
    } else {
        double d = *l > 0.5 ? 2.0 - max - min : max + min;
        *s = delta / (d > 0 ? d : 1.0);
        if (max == r) {
            *h = 60.0 * fmod((g - b) / delta, 6.0);
        } else if (max == g) {
            *h = 60.0 * ((b - r) / delta + 2.0);
        } else {
            *h = 60.0 * ((r - g) / delta + 4.0);
        }
        if (*h < 0) *h += 360.0;
    }
}

static double xcolor_hue_to_rgb(double p, double q, double t) {
    if (t < 0) t += 1.0;
    if (t > 1) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

static int xcolor_hsv_to_rgb(double h, double s, double v, Rgba *out) {
    double c = v * s;
    double hp = fmod(h, 360.0);
    if (hp < 0) hp += 360.0;
    hp /= 60.0;
    {
        double x = c * (1.0 - fabs(fmod(hp, 2.0) - 1.0));
        double r = 0, g = 0, b = 0;
        int sector = (int)hp;
        if (s < 0) s = 0;
        if (s > 1) s = 1;
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        switch (sector) {
            case 0: r = c; g = x; break;
            case 1: r = x; g = c; break;
            case 2: g = c; b = x; break;
            case 3: g = x; b = c; break;
            case 4: r = x; b = c; break;
            default: r = c; b = x; break;
        }
        {
            double m = v - c;
            out->r = (int)lround((r + m) * 255.0);
            out->g = (int)lround((g + m) * 255.0);
            out->b = (int)lround((b + m) * 255.0);
        }
    }
    out->a = 255;
    if (out->r < 0) out->r = 0;
    if (out->r > 255) out->r = 255;
    if (out->g < 0) out->g = 0;
    if (out->g > 255) out->g = 255;
    if (out->b < 0) out->b = 0;
    if (out->b > 255) out->b = 255;
    return 1;
}

static int xcolor_hsl_to_rgb(double h, double s, double l, Rgba *out) {
    double q;
    double p;
    if (s < 0) s = 0;
    if (s > 1) s = 1;
    if (l < 0) l = 0;
    if (l > 1) l = 1;
    q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    p = 2.0 * l - q;
    {
        double hk = fmod(h, 360.0);
        if (hk < 0) hk += 360.0;
        out->r = (int)lround(xcolor_hue_to_rgb(p, q, hk / 360.0 + 1.0 / 3.0) * 255.0);
        out->g = (int)lround(xcolor_hue_to_rgb(p, q, hk / 360.0) * 255.0);
        out->b = (int)lround(xcolor_hue_to_rgb(p, q, hk / 360.0 - 1.0 / 3.0) * 255.0);
    }
    out->a = 255;
    if (out->r < 0) out->r = 0;
    if (out->r > 255) out->r = 255;
    if (out->g < 0) out->g = 0;
    if (out->g > 255) out->g = 255;
    if (out->b < 0) out->b = 0;
    if (out->b > 255) out->b = 255;
    return 1;
}

double xcolor_hue(const char *s) {
    Rgba c;
    double h;
    double sv;
    double v;
    if (!xcolor_parse_hex(s, &c)) return -1;
    xcolor_rgb_to_hsv(&c, &h, &sv, &v);
    return h;
}

double xcolor_saturation(const char *s) {
    Rgba c;
    double h;
    double sv;
    double v;
    if (!xcolor_parse_hex(s, &c)) return -1;
    xcolor_rgb_to_hsv(&c, &h, &sv, &v);
    return sv;
}

double xcolor_value(const char *s) {
    Rgba c;
    double h;
    double sv;
    double v;
    if (!xcolor_parse_hex(s, &c)) return -1;
    xcolor_rgb_to_hsv(&c, &h, &sv, &v);
    return v;
}

double xcolor_lightness(const char *s) {
    Rgba c;
    double h;
    double sv;
    double l;
    if (!xcolor_parse_hex(s, &c)) return -1;
    xcolor_rgb_to_hsl(&c, &h, &sv, &l);
    return l;
}

const char *xcolor_from_hsv(double h, double s, double v) {
    Rgba c;
    char *slot = xcolor_slot();
    xcolor_hsv_to_rgb(h, s, v, &c);
    snprintf(slot, XCOLOR_TEXT, "#%02X%02X%02X", c.r, c.g, c.b);
    return slot;
}

const char *xcolor_from_hsl(double h, double s, double l) {
    Rgba c;
    char *slot = xcolor_slot();
    xcolor_hsl_to_rgb(h, s, l, &c);
    snprintf(slot, XCOLOR_TEXT, "#%02X%02X%02X", c.r, c.g, c.b);
    return slot;
}

/* WCAG 2.x relative luminance */
static double xcolor_channel_luminance(int v8) {
    double c = v8 / 255.0;
    return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double xcolor_luminance_rgba(const Rgba *c) {
    return 0.2126 * xcolor_channel_luminance(c->r) +
           0.7152 * xcolor_channel_luminance(c->g) +
           0.0722 * xcolor_channel_luminance(c->b);
}

double xcolor_luminance(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return -1;
    return xcolor_luminance_rgba(&c);
}

double xcolor_contrast(const char *a, const char *b) {
    Rgba ca;
    Rgba cb;
    double la;
    double lb;
    if (!xcolor_parse_hex(a, &ca) || !xcolor_parse_hex(b, &cb)) return -1;
    la = xcolor_luminance_rgba(&ca);
    lb = xcolor_luminance_rgba(&cb);
    if (la > lb) {
        return (la + 0.05) / (lb + 0.05);
    }
    return (lb + 0.05) / (la + 0.05);
}

long long xcolor_is_dark(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return 0;
    return xcolor_luminance_rgba(&c) < 0.5;
}

long long xcolor_is_light(const char *s) {
    Rgba c;
    if (!xcolor_parse_hex(s, &c)) return 0;
    return xcolor_luminance_rgba(&c) >= 0.5;
}

const char *xcolor_mix(const char *a, const char *b, double t) {
    Rgba ca;
    Rgba cb;
    char *slot = xcolor_slot();
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    slot[0] = '\0';
    if (!xcolor_parse_hex(a, &ca) || !xcolor_parse_hex(b, &cb)) return slot;
    snprintf(slot, XCOLOR_TEXT, "#%02X%02X%02X",
        (int)lround(ca.r + (cb.r - ca.r) * t),
        (int)lround(ca.g + (cb.g - ca.g) * t),
        (int)lround(ca.b + (cb.b - ca.b) * t));
    return slot;
}

/* CSS named colors (the stable CSS Color Module Level 4 set). */
typedef struct {
    const char *name;
    const char *hex;
} XColorName;

static const XColorName xcolor_names[] = {
    { "aliceblue", "#F0F8FF" }, { "antiquewhite", "#FAEBD7" },
    { "aqua", "#00FFFF" }, { "aquamarine", "#7FFFD4" },
    { "azure", "#F0FFFF" }, { "beige", "#F5F5DC" },
    { "bisque", "#FFE4C4" }, { "black", "#000000" },
    { "blanchedalmond", "#FFEBCD" }, { "blue", "#0000FF" },
    { "blueviolet", "#8A2BE2" }, { "brown", "#A52A2A" },
    { "burlywood", "#DEB887" }, { "cadetblue", "#5F9EA0" },
    { "chartreuse", "#7FFF00" }, { "chocolate", "#D2691E" },
    { "coral", "#FF7F50" }, { "cornflowerblue", "#6495ED" },
    { "cornsilk", "#FFF8DC" }, { "crimson", "#DC143C" },
    { "cyan", "#00FFFF" }, { "darkblue", "#00008B" },
    { "darkcyan", "#008B8B" }, { "darkgoldenrod", "#B8860B" },
    { "darkgray", "#A9A9A9" }, { "darkgreen", "#006400" },
    { "darkgrey", "#A9A9A9" }, { "darkkhaki", "#BDB76B" },
    { "darkmagenta", "#8B008B" }, { "darkolivegreen", "#556B2F" },
    { "darkorange", "#FF8C00" }, { "darkorchid", "#9932CC" },
    { "darkred", "#8B0000" }, { "darksalmon", "#E9967A" },
    { "darkseagreen", "#8FBC8F" }, { "darkslateblue", "#483D8B" },
    { "darkslategray", "#2F4F4F" }, { "darkslategrey", "#2F4F4F" },
    { "darkturquoise", "#00CED1" }, { "darkviolet", "#9400D3" },
    { "deeppink", "#FF1493" }, { "deepskyblue", "#00BFFF" },
    { "dimgray", "#696969" }, { "dimgrey", "#696969" },
    { "dodgerblue", "#1E90FF" }, { "firebrick", "#B22222" },
    { "floralwhite", "#FFFAF0" }, { "forestgreen", "#228B22" },
    { "fuchsia", "#FF00FF" }, { "gainsboro", "#DCDCDC" },
    { "ghostwhite", "#F8F8FF" }, { "gold", "#FFD700" },
    { "goldenrod", "#DAA520" }, { "gray", "#808080" },
    { "green", "#008000" }, { "greenyellow", "#ADFF2F" },
    { "grey", "#808080" }, { "honeydew", "#F0FFF0" },
    { "hotpink", "#FF69B4" }, { "indianred", "#CD5C5C" },
    { "indigo", "#4B0082" }, { "ivory", "#FFFFF0" },
    { "khaki", "#F0E68C" }, { "lavender", "#E6E6FA" },
    { "lavenderblush", "#FFF0F5" }, { "lawngreen", "#7CFC00" },
    { "lemonchiffon", "#FFFACD" }, { "lightblue", "#ADD8E6" },
    { "lightcoral", "#F08080" }, { "lightcyan", "#E0FFFF" },
    { "lightgoldenrodyellow", "#FAFAD2" }, { "lightgray", "#D3D3D3" },
    { "lightgreen", "#90EE90" }, { "lightgrey", "#D3D3D3" },
    { "lightpink", "#FFB6C1" }, { "lightsalmon", "#FFA07A" },
    { "lightseagreen", "#20B2AA" }, { "lightskyblue", "#87CEFA" },
    { "lightslategray", "#778899" }, { "lightslategrey", "#778899" },
    { "lightsteelblue", "#B0C4DE" }, { "lightyellow", "#FFFFE0" },
    { "lime", "#00FF00" }, { "limegreen", "#32CD32" },
    { "linen", "#FAF0E6" }, { "magenta", "#FF00FF" },
    { "maroon", "#800000" }, { "mediumaquamarine", "#66CDAA" },
    { "mediumblue", "#0000CD" }, { "mediumorchid", "#BA55D3" },
    { "mediumpurple", "#9370DB" }, { "mediumseagreen", "#3CB371" },
    { "mediumslateblue", "#7B68EE" }, { "mediumspringgreen", "#00FA9A" },
    { "mediumturquoise", "#48D1CC" }, { "mediumvioletred", "#C71585" },
    { "midnightblue", "#191970" }, { "mintcream", "#F5FFFA" },
    { "mistyrose", "#FFE4E1" }, { "moccasin", "#FFE4B5" },
    { "navajowhite", "#FFDEAD" }, { "navy", "#000080" },
    { "oldlace", "#FDF5E6" }, { "olive", "#808000" },
    { "olivedrab", "#6B8E23" }, { "orange", "#FFA500" },
    { "orangered", "#FF4500" }, { "orchid", "#DA70D6" },
    { "palegoldenrod", "#EEE8AA" }, { "palegreen", "#98FB98" },
    { "paleturquoise", "#AFEEEE" }, { "palevioletred", "#DB7093" },
    { "papayawhip", "#FFEFD5" }, { "peachpuff", "#FFDAB9" },
    { "peru", "#CD853F" }, { "pink", "#FFC0CB" },
    { "plum", "#DDA0DD" }, { "powderblue", "#B0E0E6" },
    { "purple", "#800080" }, { "rebeccapurple", "#663399" },
    { "red", "#FF0000" }, { "rosybrown", "#BC8F8F" },
    { "royalblue", "#4169E1" }, { "saddlebrown", "#8B4513" },
    { "salmon", "#FA8072" }, { "sandybrown", "#F4A460" },
    { "seagreen", "#2E8B57" }, { "seashell", "#FFF5EE" },
    { "sienna", "#A0522D" }, { "silver", "#C0C0C0" },
    { "skyblue", "#87CEEB" }, { "slateblue", "#6A5ACD" },
    { "slategray", "#708090" }, { "slategrey", "#708090" },
    { "snow", "#FFFAFA" }, { "springgreen", "#00FF7F" },
    { "steelblue", "#4682B4" }, { "tan", "#D2B48C" },
    { "teal", "#008080" }, { "thistle", "#D8BFD8" },
    { "tomato", "#FF6347" }, { "turquoise", "#40E0D0" },
    { "violet", "#EE82EE" }, { "wheat", "#F5DEB3" },
    { "white", "#FFFFFF" }, { "whitesmoke", "#F5F5F5" },
    { "yellow", "#FFFF00" }, { "yellowgreen", "#9ACD32" }
};

const char *xcolor_from_name(const char *name) {
    size_t i;
    char lower[64];
    char *slot = xcolor_slot();
    slot[0] = '\0';
    if (!name) return slot;
    for (i = 0; name[i] != '\0' && i < sizeof(lower) - 1; i++) {
        lower[i] = (char)tolower((unsigned char)name[i]);
    }
    lower[i] = '\0';
    for (i = 0; i < sizeof(xcolor_names) / sizeof(xcolor_names[0]); i++) {
        if (strcmp(lower, xcolor_names[i].name) == 0) {
            strncpy(slot, xcolor_names[i].hex, XCOLOR_TEXT - 1);
            slot[XCOLOR_TEXT - 1] = '\0';
            return slot;
        }
    }
    return slot;
}

const char *xcolor_to_name(const char *s) {
    Rgba c;
    char hex[8];
    size_t i;
    char *slot = xcolor_slot();
    slot[0] = '\0';
    if (!xcolor_parse_hex(s, &c)) return slot;
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", c.r, c.g, c.b);
    /* gray/grey and other aliases resolve to the first table entry. */
    for (i = 0; i < sizeof(xcolor_names) / sizeof(xcolor_names[0]); i++) {
        if (strcmp(hex, xcolor_names[i].hex) == 0 &&
            strcmp(xcolor_names[i].name, "cyan") != 0 &&
            strcmp(xcolor_names[i].name, "magenta") != 0 &&
            strcmp(xcolor_names[i].name, "grey") != 0) {
            strncpy(slot, xcolor_names[i].name, XCOLOR_TEXT - 1);
            slot[XCOLOR_TEXT - 1] = '\0';
            return slot;
        }
    }
    return slot;
}
