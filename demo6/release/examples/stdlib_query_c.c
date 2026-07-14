#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_query_at(int ok, const char* expr)
{
    if ( !ok ) {
        fprintf(stderr, "query=fail:%s\n", expr);
        exit(1);
    }
}

#define require_query(expr) require_query_at((expr), #expr)

static void require_text(str value, const char* expected)
{
    require_query(value != NULL);
    require_query(strcmp((const char*)value, expected) == 0);
    xrtFree(value);
}

int main(void)
{
    char query[256];
    size_t offset = 0u;
    const char* text = "hello world/\xE4\xB8\xAD\xE6\x96\x87";
    str encoded;
    xrtquerypair pair;

    xrtInit();

    query[0] = '\0';
    require_query(xrtQueryAppendPairTo(query, sizeof(query), &offset, "keyword", strlen("keyword"), "xlang demo6", strlen("xlang demo6"), TRUE, TRUE));
    require_query(xrtQueryAppendPairTo(query, sizeof(query), &offset, "page", strlen("page"), "1", strlen("1"), TRUE, TRUE));
    require_query(xrtQueryAppendPairTo(query, sizeof(query), &offset, "lang", strlen("lang"), "zh-cn", strlen("zh-cn"), TRUE, TRUE));
    require_query(strcmp(query, "keyword=xlang+demo6&page=1&lang=zh-cn") == 0);

    encoded = xrtFormUrlEncode(text, strlen(text), NULL);
    require_text(encoded, "hello+world%2F%E4%B8%AD%E6%96%87");
    require_text(xrtFormUrlDecode("hello+world%2F%E4%B8%AD%E6%96%87", strlen("hello+world%2F%E4%B8%AD%E6%96%87"), NULL), text);

    require_query(xrtQueryValidate(query, NULL));
    require_query(xrtQueryCountN(query, strlen(query)) == 3u);
    require_query(xrtQueryFindN(query, strlen(query), "page", strlen("page"), &pair));
    require_query(pair.tValue.iLen == 1u && pair.tValue.sPtr[0] == '1');

    printf("query=ok\n");
    xrtUnit();
    return 0;
}
