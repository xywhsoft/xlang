#include "src/common/xl6_xrt.h"
#include "src/compile/xl6_imports.h"
#include "src/parser/xl6_parser_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    assert(argc == 4);
    XL6_ParseResult parse; XL6_ImportDeps deps;
    xl6ParseResultInit(&parse); xl6ImportDepsInit(&deps);
    deps.nativeSelection = atoi(argv[2]);
    int okay = xl6ParseFile(argv[1], &parse) &&
        xl6ImportsExpandModule(parse.root, argv[1], &parse.diags, &deps) && !parse.diags.count;
    assert(!deps.nativeFeatureDepth);
    for (size_t i = 0; i < parse.diags.count; ++i)
        printf("%s:%d:%d: %s\n", parse.diags.items[i].file ? parse.diags.items[i].file : "<input>",
               parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    assert(okay == atoi(argv[3]));
    xl6ImportDepsFree(&deps); xl6ParseResultFree(&parse);
    puts("Actual source import feature decision passed");
    return 0;
}
