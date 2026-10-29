#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/ir/xl6_ir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char source[] =
    "class Scalars\n"
    " c_char a\n c_schar b\n c_uchar c\n c_short d\n c_ushort e\n"
    " c_int f\n c_uint g\n c_long h\n c_ulong i\n c_longlong j\n"
    " c_ulonglong k\n c_size l\n c_ssize m\n c_ptrdiff n\n c_intptr o\n"
    " c_uintptr p\n c_bool q\n c_float r\n c_double s\n c_wchar t\n"
    " float32 narrow\n string text\n"
    "end\n"
    "class Derived extends Scalars\n c_int tail\nend\n"
    "class Box<T>\n T value\nend\n"
    "export \"scalar_make\" func make() as Derived\n return Derived()\nend\n"
    "export \"scalar_read\" func read(Derived input) as c_int\n return input.tail\nend\n"
    "func box() as Box<c_int>\n return Box::<c_int>()\nend\n";

static void reject(const XL6_IrModule* module)
{
    XL6_DiagList diagnostics = {0};
    size_t i; int found = 0;
    assert(!xl6IrVerifyModule(module, &diagnostics));
    for (i = 0; i < diagnostics.count; ++i)
        if (strstr(diagnostics.items[i].message, "class layout arena is malformed")) found = 1;
    assert(found); xl6DiagFree(&diagnostics);
}

int main(void)
{
    XL6_ParseResult parsed; XL6_IrModule module;
    size_t i, scalarFields = 0, managedFields = 0, mutations = 0;
    xl6ParseResultInit(&parsed); xl6IrModuleInit(&module);
    assert(xl6ParseSource(source, "class_scalar_defaults.xl", &parsed));
    assert(xl6StdlibExpandModule(parsed.root, &parsed.diags));
    assert(xl6SemCheckModule(parsed.root, &parsed.diags, "class_scalar_defaults"));
    assert(xl6IrBuildModuleFromAst(&module, parsed.root, &parsed.diags));
    {
        int lowered = xl6IrLowerModule(&module, &parsed.diags);
        if (!lowered)
            for (i = 0; i < parsed.diags.count; ++i)
                fprintf(stderr, "%s\n", parsed.diags.items[i].message);
        assert(lowered);
    }
    assert(xl6IrVerifyModule(&module, &parsed.diags));
    for (i = 0; i < module.classFieldCount; ++i) {
        XL6_IrClassField* field = &module.classFields[i];
        XL6_IrClassField saved = *field;
        const XL6_IrType* type = xl6IrTypeGet(&module.types, field->type);
        assert(type);
        /* Expanded stdlib classes also have pointer/resource fields. This
         * fixture has exactly 23 native/F32 scalar fields and one string. */
        if (xl6IrNativeScalarProfileForCAbi(type->cAbi) == XL6_NATIVE_SCALAR_NONE &&
            type->cAbi != XL6_IR_C_ABI_F32 && type->cAbi != XL6_IR_C_ABI_MANAGED_STRING)
            continue;
        assert(field->initializer == XL6_IR_NODE_INVALID);
        if (type->cAbi == XL6_IR_C_ABI_MANAGED_STRING) {
            ++managedFields; assert(field->initializerAbi == XL6_IR_INITIALIZER_STRING_EMPTY);
            field->initializerAbi = XL6_IR_INITIALIZER_SCALAR_ZERO;
            reject(&module); ++mutations; *field = saved;
        } else {
            ++scalarFields; assert(field->initializerAbi == XL6_IR_INITIALIZER_SCALAR_ZERO);
            field->initializerAbi = XL6_IR_INITIALIZER_STRING_EMPTY;
            reject(&module); ++mutations; *field = saved;
            field->initializerAbi = XL6_IR_INITIALIZER_EXPLICIT;
            reject(&module); ++mutations; *field = saved;
            field->initializer = field->declaration;
            reject(&module); ++mutations; *field = saved;
        }
        assert(xl6IrVerifyModule(&module, &parsed.diags));
    }
    assert(scalarFields == 23 && managedFields == 1 && mutations == scalarFields * 3 + managedFields);
    printf("Class scalar layout contract passed: %zu scalar fields, %zu managed fields, %zu rejected mutations; C scalar/float32 defaults, inherited and generic fields\n", scalarFields, managedFields, mutations);
    xl6IrModuleFree(&module); xl6ParseResultFree(&parsed); return 0;
}
