#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t rejected;
static void reject(XL6_IrModule* ir, const char* label)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    if (xl6IrVerifyModule(ir, &diags)) { fprintf(stderr, "instantiation mutation escaped: %s\n", label); assert(0); }
    assert(diags.count && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_CONTEXT", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, count, nested = 0; size_t* memberships;
    assert(argc == 5); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argv[1], &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "instantiation_contract"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    count = ir.instantiationCount; assert(count);
    for (i = 0; i < count; ++i) {
        XL6_IrInstantiation saved = ir.instantiations[i];
        XL6_IrInstruction* commit = &ir.instructions[saved.commitInstruction];
        XL6_IrNodeId execution = xl6IrInstructionExecutionOrigin(&ir, commit);
        XL6_IrInstruction outside = *commit;
        size_t membership;
        assert(execution && execution != commit->origin);
        assert(commit->sourceSpan.line == xl6IrNodeGet(&ir, commit->origin)->span.line);
        outside.origin = ir.root;
        assert(xl6IrInstructionExecutionOrigin(&ir, &outside) == ir.root);
        nested += saved.parent != 0;
        ir.instantiations[i].parent = i + 1; reject(&ir, "cyclic parent"); ir.instantiations[i] = saved;
        ir.instantiations[i].field = 0; reject(&ir, "missing field"); ir.instantiations[i] = saved;
        ir.instantiations[i].function = ir.functionCount + 1; reject(&ir, "wrong activation"); ir.instantiations[i] = saved;
        ir.instantiations[i].site = ir.classFields[saved.field - 1].declaration;
        reject(&ir, "declaration used as invocation"); ir.instantiations[i] = saved;
        ir.instantiations[i].layoutType = XL6_IR_TYPE_INVALID; reject(&ir, "missing layout"); ir.instantiations[i] = saved;
        ir.instantiations[i].receiver = XL6_IR_VALUE_INVALID; reject(&ir, "missing receiver"); ir.instantiations[i] = saved;
        ir.instantiations[i].firstInstruction++; reject(&ir, "shifted interval"); ir.instantiations[i] = saved;
        ir.instantiations[i].instructionCount--; reject(&ir, "shortened interval"); ir.instantiations[i] = saved;
        ir.instantiations[i].commitInstruction = ir.instructionCount; reject(&ir, "missing commit"); ir.instantiations[i] = saved;
        membership = commit->instantiation; commit->instantiation = 0;
        reject(&ir, "unbound commit"); commit->instantiation = membership;
        if (saved.parent != 0) {
            ir.instantiations[i].parent = 0; reject(&ir, "detached nested initializer"); ir.instantiations[i] = saved;
        }
    }
    memberships = (size_t*)xrtMalloc(ir.instructionCount * sizeof(*memberships)); assert(memberships);
    for (i = 0; i < ir.instructionCount; ++i) {
        memberships[i] = ir.instructions[i].instantiation;
        ir.instructions[i].instantiation = 0;
    }
    ir.instantiationCount = 0;
    assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
    reject(&ir, "all contexts coherently erased with rebuilt generator caches");
    ir.instantiationCount = count;
    for (i = 0; i < ir.instructionCount; ++i) ir.instructions[i].instantiation = memberships[i];
    xrtFree(memberships);
    assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); assert(xl6IrCgenEmitHeader(&out, &ir, "INSTANTIATION_CONTEXT", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[4])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("instantiation context: %zu instances, %zu nested, %zu mutations rejected before C/header output\n", count, nested, rejected);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
