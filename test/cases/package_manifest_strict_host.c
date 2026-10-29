#include "src/common/xl6_xrt.h"
#include "src/package/xl6_package.h"

#include <stdio.h>
#include <string.h>

static int expect_rejected(const char* path)
{
    XL6_PackageManifest manifest;
    int loaded;

    xl6PackageManifestInit(&manifest);
    loaded = xl6PackageLoadManifest(path, &manifest);
    xl6PackageManifestFree(&manifest);
    if ( loaded ) {
        fprintf(stderr, "invalid package manifest accepted: %s\n", path);
        return 0;
    }
    return 1;
}

int main(void)
{
    XL6_PackageManifest manifest;
    XL6_PackageResolveResult resolved;
    int ok;

	xl6PackageManifestInit(&manifest);
    ok = xl6PackageLoadManifest(
        "test/fixtures/package_manifest_strict/valid_unicode.json", &manifest);
    if ( !ok || manifest.name == NULL || strcmp(manifest.name, "pkg中文") != 0 ||
         manifest.dependencyCount != 1 || manifest.dependencies[0].name == NULL ||
         strcmp(manifest.dependencies[0].name, "dep中") != 0 ||
         !manifest.xrtModulesDeclared || manifest.xrtModules.count != 2 ||
         strcmp(manifest.xrtModules.items[0], "XRT_MODULE_UNICODE") != 0 ||
         strcmp(manifest.xrtModules.items[1], "XRUNTIME_MODULE_TYPED_ARRAY_VALUE") != 0 ) {
        fprintf(stderr, "valid Unicode package manifest was not decoded exactly\n");
        xl6PackageManifestFree(&manifest);
		return 1;
    }
    xl6PackageManifestFree(&manifest);

    xl6PackageResolveImport(
        "test/cases/import_metadata.xl",
        "../fixtures/package_manifest_strict/valid_long_fields.json",
        &resolved);
    if ( !resolved.resolved || resolved.packageName == NULL ||
         resolved.packageVersion == NULL || resolved.abiVersion == NULL ||
         resolved.packageId == NULL || strlen(resolved.packageName) <= 300u ||
         strlen(resolved.packageVersion) <= 64u || strlen(resolved.abiVersion) <= 64u ||
         strlen(resolved.packageId) <= 500u ) {
        fprintf(stderr, "long package metadata was truncated or not resolved\n");
        xl6PackageResolveResultFree(&resolved);
		return 1;
    }
    xl6PackageResolveResultFree(&resolved);

    ok = expect_rejected("test/fixtures/package_manifest_strict/invalid_trailing.json") &&
         expect_rejected("test/fixtures/package_manifest_strict/invalid_escape.json") &&
         expect_rejected("test/fixtures/package_manifest_strict/invalid_schema.json") &&
         expect_rejected("test/fixtures/package_manifest_strict/invalid_xrt_module.json");
	return ok ? 0 : 1;
}
