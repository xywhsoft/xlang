/* xhttp 语言扩展接入后改用 xhttp 公共头与产品链接入口。 */
#define XRT_MODULE_URL
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 检查 URL 示例中的公开契约。 */
static void require_url_at(int ok, int line)
{
    if ( !ok ) {
        fprintf(stderr, "url=fail:%d\n", line);
        exit(1);
    }
}

#define require_url(Expr) require_url_at((Expr), __LINE__)



/* 从零结尾文本创建借用视图。 */
static xstrview make_view(const char* text)
{
    xstrview view;

    view.Data = text;
    view.Size = text != NULL ? strlen(text) : 0u;
    return view;
}



/* 比较 URL 组件视图。 */
static void require_view(xstrview view, const char* expected)
{
    size_t size = strlen(expected);

    require_url(view.Size == size);
    require_url(memcmp(view.Data, expected, size) == 0);
}



/* 演示解析、构建、规范化和相对引用解析。 */
int main(void)
{
    const char* page =
        "https://user@example.com:8443/docs/index.html?lang=zh#top";
    xurl url;
    xurl base;
    char target[256];
    char host[256];
    size_t outputSize = 0u;
    uint16 port = 0u;
    str resolved;
    str clean;
    str built;

    require_url(xrtUrlParse(make_view(page), &url));
    require_url((url.Flags & XURL_HAS_SCHEME) != 0u);
    require_url((url.Flags & XURL_HAS_AUTHORITY) != 0u);
    require_url(xrtUrlSecure(&url));
    require_view(url.Scheme, "https");
    require_view(url.Authority, "user@example.com:8443");
    require_view(url.UserInfo, "user");
    require_view(url.Host, "example.com");
    require_url(xrtUrlPort(&url, &port) && (port == 8443u));
    require_view(url.Path, "/docs/index.html");
    require_view(url.Query, "lang=zh");
    require_view(url.Fragment, "top");

    require_url(xrtUrlTargetWrite(&url, target, sizeof(target) - 1u, &outputSize));
    target[outputSize] = '\0';
    require_url(strcmp(target, "/docs/index.html?lang=zh") == 0);

    require_url(xrtUrlParse(
        make_view("https://example.com/app/page.html"), &base));
    resolved = xrtUrlResolveBuild(
        &base, make_view("../api/list?limit=10"), &outputSize);
    require_url(resolved != NULL);
    require_url(strcmp(resolved, "https://example.com/api/list?limit=10") == 0);

    clean = xrtUrlPathNormalizeBuild(
        make_view("/app/./data/../index.html"), &outputSize);
    require_url(clean != NULL);
    require_url(strcmp(clean, "/app/index.html") == 0);

    require_url(xrtUrlDefaultPort(make_view("HTTPS")) == 443u);
    require_url(xrtUrlParse(make_view("https://example.com/docs"), &url));
    require_url(
        xrtUrlPort(&url, &port) &&
        (port == xrtUrlDefaultPort(url.Scheme)));
    require_url(xrtUrlParse(
        make_view("https://example.com:8443/docs"), &url));
    require_url(xrtUrlHostWrite(&url, host, sizeof(host) - 1u, &outputSize));
    host[outputSize] = '\0';
    require_url(strcmp(host, "example.com:8443") == 0);

    memset(&url, 0, sizeof(url));
    url.Flags = XURL_HAS_SCHEME | XURL_HAS_AUTHORITY | XURL_HAS_HOST |
        XURL_HAS_PORT | XURL_PORT_VALUE | XURL_HAS_QUERY | XURL_HAS_FRAGMENT;
    url.Port = 443u;
    url.Scheme = make_view("https");
    url.Host = make_view("example.com");
    url.PortText = make_view("443");
    url.Path = make_view("/api");
    url.Query = make_view("page=1");
    url.Fragment = make_view("top");
    built = xrtUrlBuild(&url, &outputSize);
    require_url(built != NULL);
    require_url(strcmp(built, "https://example.com:443/api?page=1#top") == 0);

    xrtFree(built);
    xrtFree(clean);
    xrtFree(resolved);
    printf("url=ok\n");
    return 0;
}
