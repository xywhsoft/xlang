#include "../include/sitemap_native.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    xvalue *urls = xrtValueArray();
    xvalue *entry = xrtValueObject();
    const char *locationText = "https://example.test/?a=1&b=2";
    xvalue *location = xrtValueString((xstrview){locationText, strlen(locationText)});
    const char *indexText = "https://example.test/site.xml";
    xvalue *indexes = NULL, *indexEntry = NULL, *indexLocation = NULL;
    xvalue *parsedUrls = NULL, *parsedIndexes = NULL, *field;
    xstrview parsedLocation, parsedIndexLocation;
    xvalue *robots = NULL;
    char *xml = NULL, *indexXml = NULL;
    long long allowed;
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
    parsedUrls = xml ? xsm_parse(xml) : NULL;
    if (parsedUrls == NULL || xrtValueType(parsedUrls) != XVALUE_ARRAY ||
        xrtValueCount(parsedUrls) != 1) goto done;
    field = xrtValueObjectGet(xrtValueArrayAt(parsedUrls, 0), (xstrview){"loc", 3});
    if (field == NULL || !xrtValueGetString(field, &parsedLocation) ||
        parsedLocation.Size != strlen(locationText) ||
        memcmp(parsedLocation.Data, locationText, parsedLocation.Size) != 0) goto done;

    /* 索引使用同一份结构模型，生成后再解析，展示两端的完整往返。 */
    indexes = xrtValueArray();
    indexEntry = xrtValueObject();
    indexLocation = xrtValueString((xstrview){indexText, strlen(indexText)});
    if (indexes == NULL || indexEntry == NULL || indexLocation == NULL) goto done;
    if (!xrtValueObjectSetNew(indexEntry, (xstrview){"loc", 3}, indexLocation)) {
        indexLocation = NULL;
        goto done;
    }
    indexLocation = NULL;
    if (!xrtValueArrayAppendNew(indexes, indexEntry)) {
        indexEntry = NULL;
        goto done;
    }
    indexEntry = NULL;
    indexXml = (char *)xsm_build_index(indexes);
    parsedIndexes = indexXml ? xsm_parse_index(indexXml) : NULL;
    if (parsedIndexes == NULL || xrtValueType(parsedIndexes) != XVALUE_ARRAY ||
        xrtValueCount(parsedIndexes) != 1) goto done;
    field = xrtValueObjectGet(xrtValueArrayAt(parsedIndexes, 0), (xstrview){"loc", 3});
    if (field == NULL || !xrtValueGetString(field, &parsedIndexLocation) ||
        parsedIndexLocation.Size != strlen(indexText) ||
        memcmp(parsedIndexLocation.Data, indexText, parsedIndexLocation.Size) != 0) goto done;

    robots = xsm_robots_parse("User-agent: *\n# site policy\n\nDisallow: /admin\n");
    if (robots == NULL || xrtValueType(robots) != XVALUE_OBJECT) goto done;
    allowed = xsm_robots_allows(robots, "Crawler", "/admin/panel");
    if (allowed != 0) goto done;
    printf("%sparsed= %.*s\n%sindex= %.*s\nfalse\n", xml,
           (int)parsedLocation.Size, parsedLocation.Data, indexXml,
           (int)parsedIndexLocation.Size, parsedIndexLocation.Data);
    ok = 1;
done:
    xrtFree(xml);
    xrtFree(indexXml);
    xrtValueRelease(parsedUrls);
    xrtValueRelease(parsedIndexes);
    xrtValueRelease(robots);
    xrtValueRelease(location);
    xrtValueRelease(indexLocation);
    xrtValueRelease(entry);
    xrtValueRelease(indexEntry);
    xrtValueRelease(urls);
    xrtValueRelease(indexes);
    return ok ? 0 : 1;
}
