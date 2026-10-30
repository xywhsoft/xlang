/* 与 robots_policy.xl 同样的规则与断言，不访问网络或写文件。 */
#include "../include/sitemap_native.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char text[] = "User-agent: DemoBot\r# group policy\rDisallow: /draft\r"
        "Allow: /draft/public\rSitemap: https://example.test/site.xml\r"
        "User-agent: *\rDisallow: /private";
    xvalue *robots = xsm_robots_parse_n(text, sizeof(text) - 1);
    xvalue *maps;
    xstrview map;
    long long draft, published, fallback, specific;
    int result = 1;
    if (!robots || xrtValueType(robots) != XVALUE_OBJECT) goto done;
    draft = xsm_robots_allows(robots, "DemoBot", "/draft/new");
    published = xsm_robots_allows(robots, "DemoBot", "/draft/public/intro");
    fallback = xsm_robots_allows(robots, "Other", "/private/data");
    specific = xsm_robots_allows(robots, "DemoBot", "/private/data");
    if (draft != 0 || published != 1 || fallback != 0 || specific != 1) goto done;
    maps = xrtValueObjectGet(robots, (xstrview){"sitemaps", 8});
    if (!maps || xrtValueCount(maps) != 1 ||
        !xrtValueGetString(xrtValueArrayAt(maps, 0), &map) ||
        map.Size != sizeof("https://example.test/site.xml") - 1 ||
        memcmp(map.Data, "https://example.test/site.xml", map.Size)) goto done;
    printf("draft: false\npublished: true\nfallback: false\nspecific: true\n");
    result = 0;
done:
    xrtValueRelease(robots);
    return result;
}
