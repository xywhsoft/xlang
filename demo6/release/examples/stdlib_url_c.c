#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_url(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "url=fail\n");
        exit(1);
    }
}

static void require_view(xrtstrview view, const char* expected)
{
    size_t len = strlen(expected);
    require_url(view.iLen == len);
    require_url(strncmp(view.sPtr, expected, len) == 0);
}

int main(void)
{
    const char* page = "https://user@example.com:8443/docs/index.html?lang=zh#top";
    xrturlview url;
    xrturlview base;
    char target[256];
    char resolved[256];
    char clean[256];
    size_t outLen = 0u;

    xrtInit();

    require_url(xrtUrlParseView(page, &url));
    require_url((url.iFlags & XRT_URL_F_ABSOLUTE) != 0u);
    require_url((url.iFlags & XRT_URL_F_SECURE) != 0u);

    require_view(url.tScheme, "https");
    require_view(url.tAuthority, "user@example.com:8443");
    require_view(url.tUserInfo, "user");
    require_view(url.tHost, "example.com");
    require_url(url.iPort == 8443u);
    require_view(url.tPath, "/docs/index.html");
    require_view(url.tQuery, "lang=zh");
    require_view(url.tFragment, "top");
    require_url(xrtUrlViewCopyTargetTo(&url, target, sizeof(target)));
    require_url(strcmp(target, "/docs/index.html?lang=zh") == 0);

    require_url(xrtUrlParseView("https://example.com/app/page.html", &base));
    require_url(xrtUrlResolveTo(&base, "../api/list?limit=10", strlen("../api/list?limit=10"), resolved, sizeof(resolved), &outLen));
    require_url(strcmp(resolved, "https://example.com/api/list?limit=10") == 0);

    require_url(xrtUrlNormalizePathTo("/app/./data/../index.html", strlen("/app/./data/../index.html"), clean, sizeof(clean), &outLen));
    require_url(strcmp(clean, "/app/index.html") == 0);

    printf("url=ok\n");
    xrtUnit();
    return 0;
}
