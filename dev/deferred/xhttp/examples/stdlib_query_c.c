/* xhttp 语言扩展接入后改用 xhttp 公共头与产品链接入口。 */
#define XRT_MODULE_FORM_URLENCODED
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出失败表达式，便于定位示例契约。 */
static void require_query_at(int ok, const char* expression)
{
    if ( !ok ) {
        fprintf(stderr, "query=fail:%s\n", expression);
        exit(1);
    }
}

#define require_query(Expr) require_query_at((Expr), #Expr)



/* 从零结尾文本创建字符串视图。 */
static xstrview text_view(const char* text)
{
    xstrview view = {text, text != NULL ? strlen(text) : 0u};

    return view;
}



/* 从零结尾文本创建字节视图。 */
static xbytesview bytes_view(const char* text)
{
    xbytesview view = {
        (const uint8*)text,
        text != NULL ? strlen(text) : 0u
    };

    return view;
}



/* 检查并释放所有权文本。 */
static void require_text(str value, const char* expected)
{
    require_query(value != NULL);
    require_query(strcmp(value, expected) == 0);
    xrtFree(value);
}



/* 演示 form-urlencoded 构建和原始 Query 遍历。 */
int main(void)
{
    const char* text = "hello world/\xE4\xB8\xAD\xE6\x96\x87";
    xformfield fields[] = {
        {bytes_view("keyword"), bytes_view("xlang demo6")},
        {bytes_view("page"), bytes_view("1")},
        {bytes_view("lang"), bytes_view("zh-cn")}
    };
    xquerypair pair;
    size_t outputSize = 0u;
    size_t count = 0u;
    size_t offset = 0u;
    str query = xrtFormBuild(fields, 3u, &outputSize);
    str encoded;
    bytes decoded;

    require_query(query != NULL);
    require_query(
        strcmp(query, "keyword=xlang+demo6&page=1&lang=zh-cn") == 0);

    encoded = xrtFormEncodeNew(text, strlen(text), &outputSize);
    require_text(encoded, "hello+world%2F%E4%B8%AD%E6%96%87");
    decoded = xrtFormDecodeNew(
        text_view("hello+world%2F%E4%B8%AD%E6%96%87"), &outputSize);
    require_text((str)decoded, text);

    require_query(xrtQueryValidate(text_view(query), NULL, &count));
    require_query(count == 3u);
    require_query(
        xrtQueryFind(text_view(query), text_view("page"), &offset, &pair) ==
        XQUERY_NEXT_ITEM);
    require_query((pair.Value.Size == 1u) && (pair.Value.Data[0] == '1'));

    xrtFree(query);
    printf("query=ok\n");
    return 0;
}
