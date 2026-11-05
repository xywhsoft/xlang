/* Real manifest parser. No synthetic runtime or linker implementation. */
#include "src/package/xl6_package.h"
#include <assert.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    assert(argc == 3);
    XL6_PackageManifest manifest;
    xl6PackageManifestInit(&manifest);
    int expected = atoi(argv[2]);
    int loaded = xl6PackageLoadManifest(argv[1], &manifest);
    assert(loaded == (expected >= 0));
    if (loaded) assert(manifest.nativeLinkFlags == (unsigned)expected);
    else assert(manifest.nativeLinkFlags == 0);
    xl6PackageManifestFree(&manifest);
    return 0;
}
