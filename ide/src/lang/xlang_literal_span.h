#ifndef XL6_LITERAL_SPAN_H
#define XL6_LITERAL_SPAN_H

#include <stddef.h>

/* A bounded, allocation-free delimiter scanner shared by Flex and Lemon.
 * It finds spans, not expressions or decoded values. Semantic validation stays
 * in the normal lexer/parser. The limit includes literals and expression groups
 * so adversarial nesting cannot consume an unbounded C call stack. */
#define XL6_LITERAL_MAX_DEPTH 128u

typedef struct XL6_LiteralFrame {
    char close;
    unsigned char width;
    unsigned char raw;
    unsigned char interpolated;
} XL6_LiteralFrame;

typedef struct XL6_LiteralSpan {
    size_t end; /* Exclusive on success; bounded stopping position on error. */
    const char* error;
} XL6_LiteralSpan;

static int xl6LiteralOpening(const char* text, size_t len, size_t pos,
                             XL6_LiteralFrame* frame, size_t* content)
{
    size_t quote = pos;
    int raw = 0;
    int interpolated = 0;
    if ( pos >= len ) return 0;
    if ( text[pos] == 'r' || text[pos] == 'b' || text[pos] == 'f' ) {
        raw = text[pos] == 'r';
        interpolated = text[pos] == 'f';
        ++quote;
        if ( quote < len && ((text[pos] == 'r' && text[quote] == 'f') ||
                             (text[pos] == 'b' && text[quote] == 'r')) ) {
            raw = 1;
            interpolated = text[quote] == 'f';
            ++quote;
        }
        if ( quote >= len || (text[quote] != '"' && text[quote] != '`') ) return 0;
        if ( text[pos] == 'b' && text[quote] == '`' ) return 0;
    } else if ( text[pos] != '"' && text[pos] != '\'' && text[pos] != '`' ) {
        return 0;
    }
    frame->close = text[quote];
    frame->width = (unsigned char)(text[quote] == '"' && len - quote >= 3 &&
                      text[quote + 1] == '"' && text[quote + 2] == '"' ? 3 : 1);
    frame->raw = (unsigned char)(raw || text[quote] == '`');
    frame->interpolated = (unsigned char)interpolated;
    *content = quote + frame->width;
    return 1;
}

static int xl6LiteralIdentByte(unsigned char ch)
{
    return ch >= 0x80 || ch == '_' || (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

/* expression=1 starts immediately after an interpolation's opening '{'. */
static XL6_LiteralSpan xl6ScanLiteralSpan(const char* text, size_t len,
                                        size_t pos, int expression)
{
    XL6_LiteralFrame frames[XL6_LITERAL_MAX_DEPTH];
    size_t depth = 1;
    size_t i = pos;
    XL6_LiteralSpan result = {pos <= len ? pos : len, NULL};
    if ( text == NULL || pos >= len ) {
        result.error = "未闭合的 f-string 插值表达式";
        return result;
    }
    frames[0].close = '}';
    frames[0].width = 0;
    frames[0].raw = frames[0].interpolated = 0;
    if ( !expression && !xl6LiteralOpening(text, len, pos, &frames[0], &i) ) {
        result.error = "无效的字符串起始符";
        return result;
    }
    while ( i < len ) {
        XL6_LiteralFrame* frame = &frames[depth - 1];
        char ch = text[i];
        if ( frame->width != 0 ) {
            if ( ch == frame->close && (frame->width == 1 ||
                 (len - i >= 3 && text[i + 1] == ch && text[i + 2] == ch)) ) {
                i += frame->width;
                if ( --depth == 0 ) { result.end = i; return result; }
                continue;
            }
            if ( (ch == '\r' || ch == '\n') && frame->width == 1 && frame->close != '`' ) {
                result.error = frame->close == '\'' ? "未闭合的字符字面量" : "未闭合的字符串";
                break;
            }
            if ( !frame->raw && ch == '\\' && len - i >= 2 &&
                 (!frame->interpolated || (text[i + 1] != '{' && text[i + 1] != '}')) ) {
                i += 2;
                continue;
            }
            if ( frame->interpolated && (ch == '{' || ch == '}') ) {
                if ( len - i >= 2 && text[i + 1] == ch ) { i += 2; continue; }
                if ( ch == '{' ) {
                    if ( depth == XL6_LITERAL_MAX_DEPTH ) goto too_deep;
                    frames[depth].close = '}';
                    frames[depth].width = 0;
                    frames[depth].raw = frames[depth].interpolated = 0;
                    ++depth;
                }
                /* A lone '}' is preserved for the parser's existing diagnostic. */
            }
            ++i;
            continue;
        }
        if ( ch == '/' && len - i >= 2 && text[i + 1] == '/' ) {
            i += 2;
            while ( i < len && text[i] != '\r' && text[i] != '\n' ) ++i;
            continue;
        }
        if ( ch == '/' && len - i >= 2 && text[i + 1] == '*' ) {
            size_t comments = 1;
            i += 2;
            while ( i < len && comments != 0 ) {
                if ( len - i >= 2 && text[i] == '/' && text[i + 1] == '*' ) {
                    if ( comments == XL6_LITERAL_MAX_DEPTH ) goto too_deep;
                    ++comments;
                    i += 2;
                } else if ( len - i >= 2 && text[i] == '*' && text[i + 1] == '/' ) {
                    --comments;
                    i += 2;
                } else ++i;
            }
            if ( comments != 0 ) { result.error = "未闭合的块注释"; break; }
            continue;
        }
        if ( ch == '#' ) {
            ++i;
            while ( i < len && text[i] != '#' && text[i] != '\r' && text[i] != '\n' ) ++i;
            if ( i == len || text[i] != '#' ) { result.error = "未闭合的时间字面量"; break; }
            ++i;
            continue;
        }
        {
            XL6_LiteralFrame literal;
            size_t content;
            if ( xl6LiteralOpening(text, len, i, &literal, &content) ) {
                if ( depth == XL6_LITERAL_MAX_DEPTH ) goto too_deep;
                frames[depth++] = literal;
                i = content;
                continue;
            }
        }
        if ( xl6LiteralIdentByte((unsigned char)ch) ) {
            do { ++i; } while ( i < len && xl6LiteralIdentByte((unsigned char)text[i]) );
            continue;
        }
        if ( ch == '(' || ch == '[' || ch == '{' ) {
            if ( depth == XL6_LITERAL_MAX_DEPTH ) goto too_deep;
            frames[depth].close = ch == '(' ? ')' : (ch == '[' ? ']' : '}');
            frames[depth].width = 0;
            frames[depth].raw = frames[depth].interpolated = 0;
            ++depth;
        } else if ( ch == ')' || ch == ']' || ch == '}' ) {
            if ( ch != frame->close ) { result.error = "f-string 插值表达式的括号不匹配"; break; }
            if ( --depth == 0 ) { result.end = i + 1; return result; }
        }
        ++i;
    }
    if ( result.error == NULL ) {
        result.error = frames[depth - 1].width == 0 ? "未闭合的 f-string 插值表达式" :
            (frames[depth - 1].close == '\'' ? "未闭合的字符字面量" : "未闭合的字符串");
    }
    result.end = i;
    return result;
too_deep:
    result.end = i;
    result.error = "f-string 嵌套超过 128 层限制";
    return result;
}

#endif
