/* Link with --wrap=xrtCallocAt,--wrap=xrtStrDup against source objects.
 * Fault only the native callable metadata construction, after real parsing
 * and semantic analysis. Both lowering rollback and module disposal must
 * accept every partial initialization state. */
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/ir/xl6_ir.h"
#include <assert.h>
#include <stdio.h>

static XL6_IrModule* active;
static size_t fault, allocation;
static int injected;
void* __real_xrtCallocAt(size_t, size_t, const char*, uint32_t);
char* __real_xrtStrDup(const char*);

static XL6_IrClosureTarget* pending(void)
{
    if (!active || injected || !active->closureTargetCount) return NULL;
    XL6_IrClosureTarget* closure = &active->closureTargets[active->closureTargetCount - 1];
    return closure->kind == XL6_IR_CLOSURE_TARGET_C_VARIADIC_NATIVE &&
        closure->callParameterCount &&
        (!closure->callParameterNames || !closure->callParameterDefaultFunctions ||
         !closure->callParameterNames[0]) ? closure : NULL;
}

void* __wrap_xrtCallocAt(size_t count, size_t size, const char* file, uint32_t line)
{
    if (pending() && allocation++ == fault) { injected = 1; return NULL; }
    return __real_xrtCallocAt(count, size, file, line);
}

char* __wrap_xrtStrDup(const char* text)
{
    if (pending() && allocation++ == fault) { injected = 1; return NULL; }
    return __real_xrtStrDup(text);
}

int main(void)
{
    for (fault = 0; fault < 5; ++fault) {
        XL6_ParseResult parse; XL6_IrModule ir;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile("test/cases/extern_c_variadic_value.xl", &parse));
        assert(xl6StdlibExpandModule(parse.root, &parse.diags));
        assert(xl6SemCheckModule(parse.root, &parse.diags, "native_variadic_oom"));
        assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
        allocation = 0; injected = 0; active = &ir;
        int ok = xl6IrLowerModule(&ir, &parse.diags);
        active = NULL;
        assert(injected && !ok && parse.diags.count);
        assert(!ir.closureTargetCount && !ir.closureTargets);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    /* The generic disposer also owns partial modules that never reached the
     * lowering rollback. In particular, count does not imply array presence. */
    XL6_IrModule partial; xl6IrModuleInit(&partial);
    partial.closureTargets = xrtCalloc(1, sizeof(*partial.closureTargets));
    assert(partial.closureTargets);
    partial.closureTargetCount = 1;
    partial.closureTargets[0].callParameterCount = 2;
    xl6IrModuleFree(&partial);
    puts("native variadic metadata: 5 allocation failures and partial disposal safe");
    return 0;
}
