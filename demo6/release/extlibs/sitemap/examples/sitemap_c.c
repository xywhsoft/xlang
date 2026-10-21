#include "../include/sitemap_native.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    xvalue *urls = xrtValueArray();
    xvalue *entry = xrtValueObject();
    const char *locationText = "https://example.test/?a=1&b=2";
    xvalue *location = xrtValueString((xstrview){locationText, strlen(locationText)});
    xvalue *robots = NULL;
    char *xml = NULL;
    int ok = 0;
    if (urls == NULL || entry == NULL || location == NULL) goto done;
    ok = xrtValueObjectSetNew(entry, (xstrview){"loc", 3}, location);
    location = NULL;
    if (!ok) goto done;
    ok = xrtValueArrayAppendNew(urls, entry);
    entry = NULL;
    if (!ok) goto done;
    ok = 0;
    xml = (char *)xsm_build(urls);
    robots = xsm_robots_parse("User-agent: *\n# site policy\n\nDisallow: /admin\n");
    if (xml == NULL || robots == NULL || xrtValueType(robots) != XVALUE_OBJECT) goto done;
    printf("%s%s\n", xml,
           xsm_robots_allows(robots, "Crawler", "/admin/panel") ? "true" : "false");
    ok = 1;
done:
    xrtFree(xml);
    xrtValueRelease(robots);
    xrtValueRelease(location);
    xrtValueRelease(entry);
    xrtValueRelease(urls);
    return ok ? 0 : 1;
}
