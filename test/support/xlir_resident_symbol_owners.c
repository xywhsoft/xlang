#include "src/compile/xl6_artifact_runtime.h"
#include <stdio.h>
#include <string.h>

/* Independent compiled lookup of actual Cgen entry points. Private C linkage
 * is still audited through each object's complete undefined-symbol closure. */
static const struct { const char* name; unsigned owner; } declared[] = {
#define XL6_ARTIFACT_SYMBOL(Name, Component) {Name, XL6_ARTIFACT_##Component},
#include "src/compile/xl6_artifact_runtime_symbols.inc"
#undef XL6_ARTIFACT_SYMBOL
};

int main(void)
{
    char symbol[1024];
    for (size_t i = 0; i < sizeof(declared) / sizeof(*declared); ++i) {
        if (xl6ArtifactRuntimeSymbolOwner(declared[i].name) != declared[i].owner) return 4;
        printf("D\t%s\t%u\n", declared[i].name, declared[i].owner);
    }
    while (fgets(symbol, sizeof(symbol), stdin)) {
        size_t length = strlen(symbol);
        if (!length || symbol[length - 1] != '\n') return 2;
        symbol[--length] = 0;
        if (!length) return 2;
        printf("A\t%s\t%u\n", symbol, (unsigned)xl6ArtifactRuntimeSymbolOwner(symbol));
    }
    return ferror(stdin) || fflush(stdout) ? 3 : 0;
}
