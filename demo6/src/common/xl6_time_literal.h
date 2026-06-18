#ifndef XL6_TIME_LITERAL_H
#define XL6_TIME_LITERAL_H

#include "src/common/xl6_common.h"

typedef enum XL6_TimeLiteralKind {
    XL6_TIME_LITERAL_INVALID = 0,
    XL6_TIME_LITERAL_DATE,
    XL6_TIME_LITERAL_TIME,
    XL6_TIME_LITERAL_DATETIME
} XL6_TimeLiteralKind;

typedef struct XL6_TimeLiteralParts {
    XL6_TimeLiteralKind kind;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} XL6_TimeLiteralParts;

static int xl6TimeLiteralDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static int xl6TimeLiteralParseNDigits(const char* text, size_t count, int* out)
{
    size_t i;
    int value;

    if ( text == NULL || out == NULL ) {
        return 0;
    }
    value = 0;
    for ( i = 0; i < count; ++i ) {
        if ( !xl6TimeLiteralDigit(text[i]) ) {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
    }
    *out = value;
    return 1;
}

static int xl6TimeLiteralParseDate(const char* text, XL6_TimeLiteralParts* out)
{
    if ( text == NULL || out == NULL || text[4] != '-' || text[7] != '-' ) {
        return 0;
    }
    if ( !xl6TimeLiteralParseNDigits(text, 4, &out->year) ||
         !xl6TimeLiteralParseNDigits(text + 5, 2, &out->month) ||
         !xl6TimeLiteralParseNDigits(text + 8, 2, &out->day) ) {
        return 0;
    }
    return out->month >= 1 && out->month <= 12 && out->day >= 1 && out->day <= 31;
}

static int xl6TimeLiteralParseTime(const char* text, XL6_TimeLiteralParts* out)
{
    if ( text == NULL || out == NULL || text[2] != ':' || text[5] != ':' ) {
        return 0;
    }
    if ( !xl6TimeLiteralParseNDigits(text, 2, &out->hour) ||
         !xl6TimeLiteralParseNDigits(text + 3, 2, &out->minute) ||
         !xl6TimeLiteralParseNDigits(text + 6, 2, &out->second) ) {
        return 0;
    }
    return out->hour >= 0 && out->hour <= 23 &&
           out->minute >= 0 && out->minute <= 59 &&
           out->second >= 0 && out->second <= 59;
}

static int xl6TimeLiteralParse(const char* literal, XL6_TimeLiteralParts* out)
{
    size_t len;
    const char* body;
    size_t bodyLen;

    if ( literal == NULL || out == NULL ) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    len = strlen(literal);
    if ( len < 3 || literal[0] != '#' || literal[len - 1] != '#' ) {
        return 0;
    }

    body = literal + 1;
    bodyLen = len - 2;
    if ( bodyLen == 10 ) {
        out->kind = XL6_TIME_LITERAL_DATE;
        return xl6TimeLiteralParseDate(body, out);
    }
    if ( bodyLen == 8 ) {
        out->kind = XL6_TIME_LITERAL_TIME;
        return xl6TimeLiteralParseTime(body, out);
    }
    if ( bodyLen == 19 && body[10] == ' ' ) {
        out->kind = XL6_TIME_LITERAL_DATETIME;
        return xl6TimeLiteralParseDate(body, out) &&
               xl6TimeLiteralParseTime(body + 11, out);
    }

    return 0;
}

#endif
