#include "lib/xrt/xrt.h"
#include "src/compile/xl6_imports.h"
#include "src/parser/xl6_parser_api.h"

#include <stdio.h>
#include <string.h>

static int text_equals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int expect_size(const char* name, size_t actual, size_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "%s expected %u, got %u\n", name, (unsigned)expected, (unsigned)actual);
        return 0;
    }
    return 1;
}

static int expect_text(const char* name, const char* actual, const char* expected)
{
    if ( !text_equals(actual, expected) ) {
        fprintf(stderr, "%s expected '%s', got '%s'\n", name, expected, actual != NULL ? actual : "<null>");
        return 0;
    }
    return 1;
}

int main(int argc, char** argv)
{
    const char* path;
    XL6_ParseResult parse;
    XL6_ImportDeps deps;
    int ok;

    path = argc > 1 ? argv[1] : "test\\cases\\import_metadata_combo.xl";

    xrtInit();
    xl6ParseResultInit(&parse);
    xl6ImportDepsInit(&deps);

    ok = xl6ParseFile(path, &parse);
    if ( ok ) {
        ok = xl6ImportsExpandModule(parse.root, path, &parse.diags, &deps);
    }
    if ( !ok || parse.diags.count != 0 ) {
        fprintf(stderr, "import dependency snapshot load failed\n");
        xl6ImportDepsFree(&deps);
        xl6ParseResultFree(&parse);
        xrtUnit();
        return 1;
    }

    if ( !expect_size("package count", deps.packageCount, 1) ||
         !expect_text("package id", deps.packageIds[0], "metadata_combo@1.0.0#abi1") ||
         !expect_size("cSources count", deps.cSources.count, 1) ||
         !expect_text("cSource[0]", deps.cSources.items[0], "test\\fixtures\\metadata_combo\\metadata_combo_native.c") ||
         !expect_size("links count", deps.links.count, 1) ||
         !expect_text("link[0]", deps.links.items[0], "test\\fixtures\\metadata_combo\\../../../release/tmp/metadata_combo_link.a") ||
         !expect_size("loadlibs count", deps.loadlibs.count, 1) ||
         !expect_text("loadlib[0]", deps.loadlibs.items[0], "test\\fixtures\\metadata_combo\\../../../release/tmp/metadata_combo_loadlib.dll") ||
         !expect_size("native count", deps.nativeCount, 1) ||
         !expect_text("native package", deps.natives[0].packageId, "metadata_combo@1.0.0#abi1") ||
         !expect_text("native loadName", deps.natives[0].loadName, "metadata_combo_runtime.dll") ||
         !expect_text("native version", deps.natives[0].version, "1.0.0") ) {
        xl6ImportDepsFree(&deps);
        xl6ParseResultFree(&parse);
        xrtUnit();
        return 1;
    }

    xl6ImportDepsFree(&deps);
    xl6ParseResultFree(&parse);
    xrtUnit();
    return 0;
}
