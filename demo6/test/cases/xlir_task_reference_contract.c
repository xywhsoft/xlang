/* Recipe, null and ownership mutations must fail before C/header emission. */
#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
static void load(XL6_ParseResult* parse,XL6_IrModule* ir,const char* path)
{
    xl6ParseResultInit(parse);xl6IrModuleInit(ir);
    assert(xl6ParseFile(path,parse));assert(xl6StdlibExpandModule(parse->root,&parse->diags));
    assert(xl6SemCheckModule(parse->root,&parse->diags,"task_reference_contract"));
    assert(xl6IrBuildModuleFromAst(ir,parse->root,&parse->diags));
    assert(xl6IrLowerModule(ir,&parse->diags));assert(xl6IrVerifyModule(ir,&parse->diags));
}
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    load(&parse,&ir,"test/cases/extern_owned_task_error_arrays.xl");
    XL6_IrType* array=NULL;
    for (size_t i=0;i<ir.types.count;++i)
        if (ir.types.items[i].sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_ERROR_REFERENCE_V1) {
            array=&ir.types.items[i];break;
        }
    assert(array);XL6_IrSequenceLayout saved=array->sequenceLayout;
    array->sequenceLayout.codecAbi=XL6_IR_SEQUENCE_CODEC_DIRECT;
    reject(&ir,"error Array lost its error Value codec",0);array->sequenceLayout=saved;
    array->sequenceLayout.slotAbi=XL6_IR_NATIVE_SLOT_FUTURE_PTR;
    reject(&ir,"error Array acquired a raw Future slot",0);array->sequenceLayout=saved;
    array->sequenceLayout.transferAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF;
    reject(&ir,"error Array acquired Future transfer",0);array->sequenceLayout=saved;
    array->sequenceLayout.nullable=!saved.nullable;
    reject(&ir,"error Array changed its admission nullability",0);array->sequenceLayout=saved;
    array->sequenceLayout.elementType=saved.baseElementType;
    reject(&ir,"error Array lost its logical nullable identity",0);array->sequenceLayout=saved;
    XL6_IrType* element=&ir.types.items[saved.elementType-1];
    XL6_IrNullAbi nullAbi=element->nullAbi;element->nullAbi=XL6_IR_NULL_ABI_NONE;
    reject(&ir,"nullable error lost its pointer sentinel",0);element->nullAbi=nullAbi;
    assert(xl6IrVerifyModule(&ir,&parse.diags));xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    load(&parse,&ir,"test/cases/extern_owned_task_nullable_callbacks.xl");
    XL6_IrInstruction* all=NULL;
    for (size_t i=0;i<ir.instructionCount;++i)
        if (ir.instructions[i].runtimeOp==XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
            ir.instructions[i].asyncResultAbi==XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE) {all=&ir.instructions[i];break;}
    assert(all);XL6_IrTypeId payload=all->payloadType;all->payloadType=XL6_IR_TYPE_INVALID;
    reject(&ir,"nullable closure aggregate lost its logical payload",0);all->payloadType=payload;
    XL6_IrAsyncResultAbi abi=all->asyncResultAbi;all->asyncResultAbi=XL6_IR_ASYNC_RESULT_RETAINED_FUTURE;
    reject(&ir,"nullable closure aggregate acquired Future ownership",0);all->asyncResultAbi=abi;
    assert(rejected==8 && xl6IrVerifyModule(&ir,&parse.diags));
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    puts("reference task contracts: 8 mutations rejected before C/header emission");return 0;
}
