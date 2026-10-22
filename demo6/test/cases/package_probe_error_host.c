#include "src/common/xl6_xrt.h"
#include "src/package/xl6_package.h"
#include <assert.h>
#include <stdio.h>

static void probe(const char* target, bool expected, xerror* prior)
{
    XL6_PackageResolveResult result;
    assert(xrtGetError() == prior);
    int found = xl6PackageResolveImport("test/cases/import_metadata.xl", target, &result);
    assert((found != 0) == expected && (result.resolved != 0) == expected);
    assert(xrtGetError() == prior);
    xl6PackageResolveResultFree(&result);
    assert(xrtGetError() == prior);
}

int main(void)
{
    assert(!xrtGetError());
    for (int round = 0; round != 2; ++round) {
        xerror* prior = round ? xrtErrorCreate(XERR_STATE, "package-test", 41, "prior owner") : NULL;
        assert(!round || prior);
        xrtSetErrorTake(prior);
        probe("./import_source_math.xl", false, prior);
        probe("../fixtures/package_manifest_strict/invalid_trailing.json", false, prior);
        probe("../fixtures/package_manifest_strict/invalid_schema.json", false, prior);
        probe("../fixtures/package_manifest_strict/valid_long_fields.json", true, prior);
        probe("../fixtures/network_services_noop", true, prior);
        probe("./nonexistent-package-probe", false, prior);
        xrtClearError();
    }
    /* Do not turn explicit invalid-manifest loading into a silent success. */
    XL6_PackageManifest manifest;
    xl6PackageManifestInit(&manifest);
    assert(!xl6PackageLoadManifest("test/fixtures/package_manifest_strict/invalid_trailing.json", &manifest));
    assert(xrtGetError());
    xrtClearError();
    xl6PackageManifestFree(&manifest);
    assert(!xrtGetError());
    puts("Package probe: clean hits/misses, exact prior error ownership, explicit load diagnostic");
    return 0;
}
