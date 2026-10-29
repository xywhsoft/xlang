/* Syntax-independent function-value calls must be checked before XLIR, and
 * every accepted call must carry the exact callable/result TypeIds. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned rejections;
static const char declarations[] =
    "typedef Pair = func(int) as (int, string)\n"
    "func pair(int value) as (int, string)\nreturn value, \"ok\"\nend\n"
    "func factory() as Pair\nreturn pair\nend\n"
    "func consume(int value) as int\nreturn value\nend\n"
    "class Holder\nPair callback = pair\n"
    "get same() as Pair\nreturn callback\nend\nend\n";

static void reject(const char* source, const char* diagnostic)
{
    XL6_ParseResult parse; int found = 0;
    xl6ParseResultInit(&parse);
    if (!xl6ParseSource(source, "function_value_invalid", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags)) {
        for (size_t d = 0; d < parse.diags.count; ++d)
            fprintf(stderr, "invalid test did not reach sem: %s\n", parse.diags.items[d].message);
        assert(!"negative fixture must parse and expand");
    }
    int rejected = !xl6SemCheckModule(parse.root, &parse.diags, "function_value_invalid");
    if (!rejected) fprintf(stderr, "invalid program accepted:\n%s\n", source);
    assert(rejected);
    for (size_t d = 0; d < parse.diags.count; ++d)
        if (strstr(parse.diags.items[d].message, diagnostic)) found = 1;
    if (!found) {
        fprintf(stderr, "missing semantic diagnostic %s in:\n%s\n", diagnostic, source);
        for (size_t d = 0; d < parse.diags.count; ++d)
            fprintf(stderr, "%s\n", parse.diags.items[d].message);
    }
    assert(found); xl6ParseResultFree(&parse); ++rejections;
}

static void check_language_boundary(void)
{
    const struct { const char* binding; const char* callee; } forms[] = {
        {"Pair callback = pair\n", "callback"},
        {"array<Pair> callbacks = [pair]\n", "callbacks[0]"},
        {"Holder holder = Holder()\n", "holder.callback"},
        {"", "factory()"},
        {"array<Pair> callbacks = [pair]\n", "(true ? callbacks[0] : factory())"},
        {"Holder holder = Holder()\n", "holder.same"}
    };
    const struct { const char* result; const char* statement; const char* diagnostic; } bad[] = {
        {"int", "%s(\"wrong\")\nreturn 0\n", "func 调用实参类型和签名不匹配"},
        {"int", "%s()\nreturn 0\n", "func 调用实参数量和签名不匹配"},
        {"int", "%s(1, 2)\nreturn 0\n", "func 调用实参数量和签名不匹配"},
        {"int", "%s(value = 1)\nreturn 0\n", "func 值调用不支持命名参数"},
        {"int", "auto a, b, c = %s(1)\nreturn 0\n", "多返回值声明数量和函数返回值数量不匹配"},
        {"int", "int a = 0\nstring b = \"\"\nint c = 0\na, b, c = %s(1)\nreturn 0\n",
            "多返回值赋值数量和函数返回值数量不匹配"},
        {"int", "consume(%s(1))\nreturn 0\n", "多返回值函数调用不能作为单个实参"},
        {"int", "return %s(1)\n", "多返回值函数调用不能作为单个 return 表达式"},
        {"(int, int)", "return %s(1)\n", "return 多返回值类型和函数声明不匹配"},
        {"(int, string, int)", "return %s(1)\n", "return 多返回值数量和函数声明不匹配"}
    };
    char source[4096], statement[2048];
    for (size_t f = 0; f < sizeof(forms) / sizeof(forms[0]); ++f) {
        for (size_t b = 0; b < sizeof(bad) / sizeof(bad[0]); ++b) {
            int n = snprintf(statement, sizeof(statement), bad[b].statement, forms[f].callee);
            assert(n > 0 && (size_t)n < sizeof(statement));
            n = snprintf(source, sizeof(source), "%sfunc rejected() as %s\n%s%send\n",
                declarations, bad[b].result, forms[f].binding, statement);
            assert(n > 0 && (size_t)n < sizeof(source)); reject(source, bad[b].diagnostic);
        }
    }
    const char* nullable[] = {
        "Pair? callback = null\ncallback(1)\n",
        "array<Pair?> callbacks = [null]\ncallbacks[0](1)\n",
        "(func(int) as int)? callback = null\ncallback(1)\n"
    };
    for (size_t n = 0; n < sizeof(nullable) / sizeof(nullable[0]); ++n) {
        assert(snprintf(source, sizeof(source), "%s%s", declarations, nullable[n]) > 0);
        reject(source, "nullable function must be checked for null before calling");
    }
    assert(snprintf(source, sizeof(source), "%s(func(int value) as int => value)(\"wrong\")\n", declarations) > 0);
    reject(source, "func 调用实参类型和签名不匹配");
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    unsigned calls = 0, products = 0;
    assert(argc == 4); check_language_boundary();
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_function_value_calls.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "test/cases/xlir_function_value_calls.xl") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (size_t i = 0; i < ir.callSiteCount; ++i) {
        const XL6_IrCallSite* call = &ir.callSites[i];
        if (call->callKind != XL6_SEM_CALL_FUNCTION_VALUE) continue;
        const XL6_IrNode* receiver = xl6IrNodeGet(&ir, call->receiver);
        const XL6_IrType* type = receiver ? xl6IrTypeGet(&ir.types, receiver->type) : NULL;
        assert(type && type->kind == XL6_IR_TYPE_FUNCTION);
        assert(call->resultType == type->callableResultType);
        assert(call->argumentCount == type->paramCount);
        ++calls; products += type->returnCount > 1;
    }
    assert(calls >= 15 && products >= 14);
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "FUNCTION_VALUE_CALLS", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("function-value calls: %u semantic rejections, %u exact calls, %u products\n", rejections, calls, products);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t d = 0; d < parse.diags.count; ++d) fprintf(stderr, "%s\n", parse.diags.items[d].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
