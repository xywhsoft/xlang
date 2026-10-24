/* URL slug：XRT 负责 UTF-8 解码与动态结果，表只承担有限拉丁转写。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/slugify_native.h"
#include <string.h>

#define XSL_DOMAIN "xlang.extlib.slugify"
#define XSL_MAX_INPUT (1024u * 1024u)

static void xsl_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSL_DOMAIN, 0, message);
}

/* 按码点升序维护；只映射明确列出的拉丁变体和少量符号。 */
static const char *xsl_latin1(unsigned int cp) {
    static const struct { unsigned int cp; const char *rep; } table[] = {
        {0xA9,"c"},{0xAE,"r"},{0xB0,"deg"},{0xB5,"u"},{0xC0,"a"},
        {0xC1,"a"},{0xC2,"a"},{0xC3,"a"},{0xC4,"a"},{0xC5,"a"},
        {0xC6,"ae"},{0xC7,"c"},{0xC8,"e"},{0xC9,"e"},{0xCA,"e"},
        {0xCB,"e"},{0xCC,"i"},{0xCD,"i"},{0xCE,"i"},{0xCF,"i"},
        {0xD1,"n"},{0xD2,"o"},{0xD3,"o"},{0xD4,"o"},{0xD5,"o"},
        {0xD6,"o"},{0xD7,"x"},{0xD8,"o"},{0xD9,"u"},{0xDA,"u"},
        {0xDB,"u"},{0xDC,"u"},{0xDD,"y"},{0xE0,"a"},{0xE1,"a"},
        {0xE2,"a"},{0xE3,"a"},{0xE4,"a"},{0xE5,"a"},{0xE6,"ae"},
        {0xE7,"c"},{0xE8,"e"},{0xE9,"e"},{0xEA,"e"},{0xEB,"e"},
        {0xEC,"i"},{0xED,"i"},{0xEE,"i"},{0xEF,"i"},{0xF1,"n"},
        {0xF2,"o"},{0xF3,"o"},{0xF4,"o"},{0xF5,"o"},{0xF6,"o"},
        {0xF7,"/"},{0xF8,"o"},{0xF9,"u"},{0xFA,"u"},{0xFB,"u"},
        {0xFC,"u"},{0xFD,"y"},{0xFF,"y"},{0x100,"a"},{0x101,"a"},
        {0x102,"a"},{0x103,"a"},{0x104,"a"},{0x105,"a"},{0x106,"c"},
        {0x107,"c"},{0x108,"c"},{0x109,"c"},{0x10A,"c"},{0x10B,"c"},
        {0x10C,"c"},{0x10D,"c"},{0x10E,"d"},{0x10F,"d"},{0x110,"d"},
        {0x111,"d"},{0x112,"e"},{0x113,"e"},{0x114,"e"},{0x115,"e"},
        {0x116,"e"},{0x117,"e"},{0x118,"e"},{0x119,"e"},{0x11A,"e"},
        {0x11B,"e"},{0x11C,"g"},{0x11D,"g"},{0x11E,"g"},{0x11F,"g"},
        {0x120,"g"},{0x121,"g"},{0x122,"g"},{0x123,"g"},{0x124,"h"},
        {0x125,"h"},{0x128,"i"},{0x129,"i"},{0x12A,"i"},{0x12B,"i"},
        {0x12C,"i"},{0x12D,"i"},{0x12E,"i"},{0x12F,"i"},{0x130,"i"},
        {0x131,"i"},{0x132,"ij"},{0x133,"ij"},{0x134,"j"},{0x135,"j"},
        {0x136,"k"},{0x137,"k"},{0x139,"l"},{0x13A,"l"},{0x13B,"l"},
        {0x13C,"l"},{0x13D,"l"},{0x13E,"l"},{0x13F,"l"},{0x140,"l"},
        {0x141,"l"},{0x142,"l"},{0x143,"n"},{0x144,"n"},{0x145,"n"},
        {0x146,"n"},{0x147,"n"},{0x148,"n"},{0x14C,"o"},{0x14D,"o"},
        {0x14E,"o"},{0x14F,"o"},{0x150,"o"},{0x151,"o"},{0x152,"oe"},
        {0x153,"oe"},{0x154,"r"},{0x155,"r"},{0x156,"r"},{0x157,"r"},
        {0x158,"r"},{0x159,"r"},{0x15A,"s"},{0x15B,"s"},{0x15C,"s"},
        {0x15D,"s"},{0x15E,"s"},{0x15F,"s"},{0x160,"s"},{0x161,"s"},
        {0x162,"t"},{0x163,"t"},{0x164,"t"},{0x165,"t"},{0x166,"t"},
        {0x167,"t"},{0x168,"u"},{0x169,"u"},{0x16A,"u"},{0x16B,"u"},
        {0x16C,"u"},{0x16D,"u"},{0x16E,"u"},{0x16F,"u"},{0x170,"u"},
        {0x171,"u"},{0x172,"u"},{0x173,"u"},{0x174,"w"},{0x175,"w"},
        {0x176,"y"},{0x177,"y"},{0x178,"y"},{0x179,"z"},{0x17A,"z"},
        {0x17B,"z"},{0x17C,"z"},{0x17D,"z"},{0x17E,"z"},{0x1CD,"a"},
        {0x1CF,"i"},{0x1D1,"o"},{0x1D3,"u"},{0x1E5,"g"},{0x1E7,"g"},
        {0x1E9,"k"},{0x1EB,"e"},{0x1ED,"o"},{0x1EF,"y"},{0x1F1,"dz"},
        {0x1F3,"dz"},{0x1F5,"n"},{0x1F9,"n"},{0x1FB,"a"},{0x1FD,"ae"},
        {0x1FF,"o"},{0x201,"o"},{0x203,"o"},{0x205,"o"},{0x207,"o"},
        {0x209,"o"},{0x20B,"o"},{0x20D,"o"},{0x20F,"o"},{0x211,"o"},
        {0x213,"r"},{0x215,"r"},{0x217,"r"},{0x268,"i"},{0x1E01,"a"},
        {0x1E03,"b"},{0x1E05,"b"},{0x1E07,"b"},{0x1E0B,"d"},{0x1E0F,"d"},
        {0x1E15,"e"},{0x1E17,"e"},{0x1E1B,"e"},{0x1E1D,"e"},{0x1E21,"h"},
        {0x1E25,"h"},{0x1E2B,"h"},{0x1E2F,"i"},{0x1E31,"k"},{0x1E37,"l"},
        {0x1E3B,"l"},{0x1E41,"m"},{0x1E43,"m"},{0x1E45,"n"},{0x1E47,"n"},
        {0x1E49,"n"},{0x1E4B,"n"},{0x1E4D,"o"},{0x1E4F,"o"},{0x1E51,"o"},
        {0x1E53,"o"},{0x1E55,"p"},{0x1E57,"p"},{0x1E59,"r"},{0x1E5B,"r"},
        {0x1E5D,"r"},{0x1E5F,"r"},{0x1E61,"s"},{0x1E63,"s"},{0x1E65,"s"},
        {0x1E67,"s"},{0x1E69,"s"},{0x1E6B,"t"},{0x1E6D,"t"},{0x1E6F,"t"},
        {0x1E71,"t"},{0x1E73,"u"},{0x1E75,"u"},{0x1E77,"u"},{0x1E79,"u"},
        {0x1E7B,"u"},{0x1E7D,"v"},{0x1E7F,"v"},{0x1E81,"w"},{0x1E83,"w"},
        {0x1E85,"w"},{0x1E87,"w"},{0x1E89,"w"},{0x1E8B,"x"},{0x1E8D,"x"},
        {0x1E8F,"y"},{0x1E91,"z"},{0x1E93,"z"},{0x1E95,"z"},{0x1E97,"t"},
        {0x1E99,"y"},{0x2013,"-"},{0x2014,"-"},{0x2018,"'"},{0x2019,"'"},
        {0x201C,"\""},{0x201D,"\""},{0x2026,""},{0x2660,""},{0x2663,""},
        {0x2665,""},{0x2666,""}
    };
    size_t low = 0u, high = sizeof(table) / sizeof(table[0]);
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        if (table[middle].cp == cp) return table[middle].rep;
        if (table[middle].cp < cp) low = middle + 1u;
        else high = middle;
    }
    return NULL;
}

static bool xsl_input(const char* text, long long length)
{
    if (text == NULL || length < 0 || length > XSL_MAX_INPUT) {
        xrtSetErrorInfo(XERR_RANGE, XSL_DOMAIN, 0, "text exceeds 1 MiB or is null");
        return false;
    }
    if (memchr(text, '\0', (size_t)length) != NULL) {
        xsl_error("text contains an embedded NUL");
        return false;
    }
    return true;
}

static bool xsl_token(xbuffer* output, bool* separator, const char* text, size_t length)
{
    if (length == 0u) return true;
    if (*separator && output->Size != 0u && !xrtBufferAppendByte(output, '-'))
        return false;
    *separator = false;
    return xrtBufferAppend(output, (xbytesview){(cbytes)text, length});
}

/* 分隔符只标记边界，下一个有效词到来时再插入 '-'。 */
static bool xsl_ascii(xbuffer* output, bool* separator, unsigned char c)
{
    char lower = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9'))
        return xsl_token(output, separator, &lower, 1u);
    if (c == '&' || c == '@') {
        /* 替换词独立成词，避免 a&b 变成 aandb。 */
        *separator = true;
        if (!xsl_token(output, separator, c == '&' ? "and" : "at",
            c == '&' ? 3u : 2u)) return false;
        *separator = true;
        return true;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
        c == '\v' || c == '\f' ||
        c == '-' || c == '_' || c == '.' || c == '/' || c == '+' ||
        c == '=' || c == ':' || c == ',' || c == ';' || c == '?' || c == '!')
        *separator = true;
    /* 引号和其他 ASCII 符号被丢弃；不会制造额外连接符。 */
    return true;
}

static bool xsl_cjk(uint32 scalar)
{
    return (scalar >= 0x3400u && scalar <= 0x9fffu) ||
        (scalar >= 0xf900u && scalar <= 0xfaffu) ||
        (scalar >= 0x20000u && scalar <= 0x2fa1fu) ||
        /* Unicode 17: CJK 扩展 G、H、J。 */
        (scalar >= 0x30000u && scalar <= 0x3134au) ||
        (scalar >= 0x31350u && scalar <= 0x323afu) ||
        (scalar >= 0x323b0u && scalar <= 0x33479u) ||
        (scalar >= 0x3040u && scalar <= 0x30ffu) ||
        (scalar >= 0xac00u && scalar <= 0xd7afu);
}

void* xsl_slugify(const char* text, long long length)
{
    xbuffer output;
    size_t position = 0u;
    bool separator = false;
    void* result = NULL;
    xrtClearError();
    if (!xsl_input(text, length)) return NULL;
    if (!xrtBufferInit(&output)) return NULL;
    if (!xrtBufferReserve(&output, (size_t)length + 1u)) goto done;
    while (position < (size_t)length) {
        xstrview view = {text + position, (size_t)length - position};
        uint32 scalar;
        size_t read;
        const char* replacement;
        const char* p;
        if (xrtUtf8Decode(view, &scalar, &read) != XUTF_OK) {
            xsl_error("text is not valid UTF-8");
            goto done;
        }
        if (scalar >= 0xff01u && scalar <= 0xff5eu) scalar -= 0xfee0u;
        replacement = (scalar >= 0x80u && (scalar <= 0x0217u || scalar == 0x0268u ||
                       (scalar >= 0x1e00u && scalar <= 0x1effu) ||
                       (scalar >= 0x2018u && scalar <= 0x2026u) ||
                       (scalar >= 0x2663u && scalar <= 0x2666u)))
            ? xsl_latin1(scalar) : NULL;
        if (scalar < 0x80u) {
            if (!xsl_ascii(&output, &separator, (unsigned char)scalar)) goto done;
        } else if (replacement != NULL) {
            for (p = replacement; *p != '\0'; ++p)
                if (!xsl_ascii(&output, &separator, (unsigned char)*p)) goto done;
        } else if (scalar == 0x0085u || scalar == 0x00a0u ||
                   scalar == 0x1680u || scalar == 0x30fbu ||
                   (scalar >= 0x2000u && scalar <= 0x206fu) ||
                   (scalar >= 0x3000u && scalar <= 0x303fu) ||
                   (scalar >= 0xff00u && scalar <= 0xffefu)) {
            separator = true;
        } else if (xsl_cjk(scalar)) {
            if (!xsl_token(&output, &separator, text + position, read)) goto done;
        }
        position += read;
    }
    if (!xrtBufferAppendByte(&output, '\0')) goto done;
    result = xrtBufferTake(&output, NULL, NULL);
done:
    xrtBufferUnit(&output);
    return result;
}

int xsl_is_ascii(const char* text, long long length)
{
    size_t i;
    xrtClearError();
    if (!xsl_input(text, length)) return -1;
    for (i = 0u; i < (size_t)length; ++i)
        if ((unsigned char)text[i] >= 0x80u) break;
    if (i == (size_t)length) return 1;
    /* ASCII 输入保持单次扫描；仅非 ASCII 输入需要校验 UTF-8。 */
    if (!xrtUtf8Valid((xstrview){text, (size_t)length}, NULL)) {
        xsl_error("text is not valid UTF-8");
        return -1;
    }
    return 0;
}
