#include "src/common/xl6_xrt.h"
#include "src/compile/xl6_imports.h"
#include "src/parser/xl6_parser_api.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    int i;
    int ok = 1;

    if ( argc < 2 ) {
        fprintf(stderr, "usage: import_expand_host <file.xl>...\n");
        return 2;
    }

	for ( i = 1; i < argc; ++i ) {
        XL6_ParseResult parse;
        XL6_ImportDeps deps;
        size_t j;

        xl6ParseResultInit(&parse);
        xl6ImportDepsInit(&deps);
        if ( !xl6ParseFile(argv[i], &parse) ||
             !xl6ImportsExpandModule(parse.root, argv[i], &parse.diags, &deps) ||
             parse.diags.count != 0 ) {
            fprintf(stderr, "%s: import expansion failed\n", argv[i]);
            for ( j = 0; j < parse.diags.count; ++j ) {
                fprintf(stderr,
                        "%u:%u: %s\n",
                        (unsigned)parse.diags.items[j].line,
                        (unsigned)parse.diags.items[j].column,
                        parse.diags.items[j].message);
            }
            ok = 0;
        }
        xl6ImportDepsFree(&deps);
        xl6ParseResultFree(&parse);
    }
	return ok ? 0 : 1;
}
