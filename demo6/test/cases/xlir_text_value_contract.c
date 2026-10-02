#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output output;
    int ok = 0;
    unsigned checked = 0;
    unsigned products = 0;
    assert(argc == 3);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "text_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    /* Interpolation must use the same closed scalar ABI as string(char).
     * Changing it to integer formatting must be rejected, not guessed in C. */
    unsigned characterConversions = 0;
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->runtimeOp != XL6_IR_RUNTIME_CHAR_TO_STRING) continue;
        instruction->runtimeOp = XL6_IR_RUNTIME_I64_TO_STRING;
        assert(!xl6IrVerifyModule(&ir, NULL));
        instruction->runtimeOp = XL6_IR_RUNTIME_CHAR_TO_STRING;
        assert(xl6IrVerifyModule(&ir, NULL));
        ++characterConversions;
    }
    if (strstr(argv[1], "stdlib_text_interpolation.xl") != NULL) assert(characterConversions == 3);
    if (strstr(argv[1], "stdlib_text_literals_nested.xl") != NULL) {
        unsigned projections = 0;
        for (size_t i = 0; i < ir.instructionCount; ++i) {
            XL6_IrInstruction* instruction = &ir.instructions[i];
            assert(instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY);
            assert(instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY);
            if (instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY) continue;
            assert(xl6IrInstructionIsFallibleProjection(&ir, instruction));
            XL6_IrTypeId payload = instruction->payloadType;
            instruction->payloadType = XL6_IR_TYPE_INVALID;
            assert(!xl6IrVerifyModule(&ir, NULL));
            instruction->payloadType = payload;
            assert(xl6IrVerifyModule(&ir, NULL));
            ++projections;
        }
        assert(projections >= 3); /* array, stored sparse list, temporary list */
    }
    /* Generic product verification, including plain language functions with
     * no standard-library bridge. A corrupted closed ABI must never reach C. */
    for (size_t i=0; i<ir.types.count; ++i) {
        XL6_IrType* type = &ir.types.items[i];
        if (type->nativeProductAbi == XL6_IR_NATIVE_PRODUCT_NONE) continue;
        XL6_IrNativeProductAbi profile = type->nativeProductAbi;
        assert(profile == XL6_IR_NATIVE_PRODUCT_I64X3_V1);
        type->nativeProductAbi = XL6_IR_NATIVE_PRODUCT_NONE;
        assert(!xl6IrVerifyModule(&ir, NULL)); type->nativeProductAbi = profile;
        type->nativeProductAbi = (XL6_IrNativeProductAbi)999;
        assert(!xl6IrVerifyModule(&ir, NULL)); type->nativeProductAbi = profile;
        char spelling[sizeof(type->cName)]; memcpy(spelling,type->cName,sizeof(spelling));
        type->cName[0] = 'z'; assert(!xl6IrVerifyModule(&ir, NULL));
        memcpy(type->cName,spelling,sizeof(spelling));
        size_t fields = type->paramCount; --type->paramCount;
        assert(!xl6IrVerifyModule(&ir, NULL)); type->paramCount = fields;
        XL6_IrOwnershipAbi own = type->ownershipAbi;
        type->ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
        assert(!xl6IrVerifyModule(&ir, NULL)); type->ownershipAbi = own;
        for (size_t field=0; field<fields; ++field) {
            XL6_IrType* item = &ir.types.items[type->params[field]-1];
            XL6_IrIntegerTarget width = item->integerTarget;
            item->integerTarget = XL6_IR_INTEGER_TARGET_SIGNED_32;
            assert(!xl6IrVerifyModule(&ir, NULL)); item->integerTarget = width;
        }
        assert(xl6IrVerifyModule(&ir, NULL)); ++products;
    }
    for (size_t i=0; i<ir.callTargetCount; ++i) {
        XL6_IrCallTarget* t = &ir.callTargets[i];
        if (t->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE) continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        XL6_IrErrorAbi errorAbi = t->errorAbi;
        XL6_IrCallErrorPhysicalAbi errorPhysicalAbi = t->errorPhysicalAbi;
        assert(errorAbi == xl6IrRuntimeDirectBridgeErrorAbi(t->cName));
        t->errorAbi = errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->errorAbi = errorAbi;
        t->effectAbi = XL6_IR_CALL_EFFECT_PURE;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->effectAbi = XL6_IR_CALL_EFFECT_CONSERVATIVE;
        t->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK;
        t->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->errorPhysicalAbi = errorPhysicalAbi;
        size_t count = t->parameterCount;
        --t->parameterCount;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->parameterCount = count;
        if (count) {
            XL6_IrTypeId* types = t->parameterTypes;
            t->parameterTypes = NULL;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            t->parameterTypes = types;
        }
        for (size_t p=0; p<count; ++p) {
            XL6_IrType* scalar = &ir.types.items[t->parameterTypes[p] - 1];
            if (t->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW) {
                size_t arity=scalar->paramCount;scalar->paramCount=1;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));scalar->paramCount=arity;
                XL6_IrCallableErasureAbi erasure=scalar->callableErasureAbi;
                scalar->callableErasureAbi=XL6_IR_CALLABLE_ERASURE_OPEN;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));scalar->callableErasureAbi=erasure;
                XL6_IrTypeId physicalResult=scalar->callableResultType;scalar->callableResultType=XL6_IR_TYPE_INVALID;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));scalar->callableResultType=physicalResult;
                XL6_IrType* returned=&ir.types.items[physicalResult-1];
                XL6_IrCAbi returnedAbi=returned->cAbi;returned->cAbi=XL6_IR_C_ABI_I64;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));returned->cAbi=returnedAbi;
                t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));
                t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));
                t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW;
            }
            if (scalar->cAbi == XL6_IR_C_ABI_I64) {
                XL6_IrIntegerTarget width = scalar->integerTarget;
                scalar->integerTarget = XL6_IR_INTEGER_TARGET_SIGNED_32;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                scalar->integerTarget = width;
            }
            if (scalar->cAbi == XL6_IR_C_ABI_U64 || scalar->cAbi == XL6_IR_C_ABI_F64) {
                if (scalar->cAbi == XL6_IR_C_ABI_U64) {
                    XL6_IrIntegerTarget width = scalar->integerTarget;
                    scalar->integerTarget = XL6_IR_INTEGER_TARGET_SIGNED_64;
                    assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                    scalar->integerTarget = width;
                }
                XL6_IrCAbi abi = scalar->cAbi;
                scalar->cAbi = XL6_IR_C_ABI_I64;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                scalar->cAbi = abi;
            }
            if (t->parameterAbis[p] != XL6_IR_PARAMETER_ABI_BORROW) continue;
            t->parameterAbis[p] = XL6_IR_PARAMETER_ABI_VALUE;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            t->parameterAbis[p] = XL6_IR_PARAMETER_ABI_BORROW;
            if (t->parameterPhysicalAbis[p] == XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW) {
                XL6_IrNodeId field = t->parameterPhysicalFieldDeclarations[p];
                t->parameterPhysicalFieldDeclarations[p] = XL6_IR_NODE_INVALID;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                t->parameterPhysicalFieldDeclarations[p] = field;
                t->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                t->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW;
                const XL6_IrClassLayout* layout = NULL;
                for (size_t k=0; k<ir.classLayoutCount; ++k)
                    if (ir.classLayouts[k].type == t->parameterTypes[p]) layout = &ir.classLayouts[k];
                assert(layout && layout->fieldCount == 1);
                XL6_IrCallTarget* destroy = &ir.callTargets[0];
                XL6_SymbolId symbol = ir.classFields[layout->firstField].nativeDestroySymbol;
                for (size_t k=0; k<ir.callTargetCount; ++k) if (ir.callTargets[k].symbol == symbol) destroy = &ir.callTargets[k];
                char* name = destroy->cName; destroy->cName = "wrong_destroy_contract";
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                destroy->cName = name;
            }
            XL6_IrType* parameter = &ir.types.items[t->parameterTypes[p] - 1];
            if (parameter->cAbi==XL6_IR_C_ABI_RUNTIME_XVALUE_PTR && parameter->kind==XL6_IR_TYPE_BUILTIN &&
                parameter->collectionAbi==XL6_IR_COLLECTION_NONE) {
                size_t returns=parameter->returnCount;parameter->returnCount=1;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));parameter->returnCount=returns;
                XL6_IrOwnershipAbi own=parameter->ownershipAbi;parameter->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));parameter->ownershipAbi=own;
                XL6_IrNullAbi null=parameter->nullAbi;parameter->nullAbi=XL6_IR_NULL_ABI_POINTER_SENTINEL;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));parameter->nullAbi=null;
            }
            if (parameter->cAbi==XL6_IR_C_ABI_RUNTIME_ERROR_PTR) {
                XL6_IrCAbi abi=parameter->cAbi;parameter->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));parameter->cAbi=abi;
            }
            if (parameter->collectionAbi == XL6_IR_COLLECTION_ARRAY) {
                XL6_IrType* item = &ir.types.items[parameter->params[0] - 1];
                XL6_IrCAbi itemAbi = item->cAbi;
                item->cAbi = XL6_IR_C_ABI_CHAR_U32;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                item->cAbi = itemAbi;
                XL6_IrOwnershipAbi own = item->ownershipAbi;
                item->ownershipAbi = own == XL6_IR_OWNERSHIP_ABI_MANAGED
                    ? XL6_IR_OWNERSHIP_ABI_TRIVIAL : XL6_IR_OWNERSHIP_ABI_MANAGED;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                item->ownershipAbi = own;
                XL6_IrCollectionAbi collection = parameter->collectionAbi;
                parameter->collectionAbi = XL6_IR_COLLECTION_LIST;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
                parameter->collectionAbi = collection;
            }
        }
        if (t->returnType == XL6_IR_TYPE_INVALID) {++checked; continue;}
        if (t->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) {
            XL6_SymbolId destroy = t->resultPhysicalDestroySymbol;
            t->resultPhysicalDestroySymbol = 0;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            t->resultPhysicalDestroySymbol = destroy;
            XL6_IrNodeId field = t->resultPhysicalFieldDeclaration;
            t->resultPhysicalFieldDeclaration = XL6_IR_NODE_INVALID;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            t->resultPhysicalFieldDeclaration = field;
        }
        XL6_IrType* result = &ir.types.items[t->returnType - 1];
        if (result->kind == XL6_IR_TYPE_NULLABLE) {
            result->kind = XL6_IR_TYPE_BUILTIN;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            result->kind = XL6_IR_TYPE_NULLABLE;
        }
        XL6_IrCAbi abi = result->cAbi;
        if (abi == XL6_IR_C_ABI_PRODUCT) {
            XL6_IrNativeProductAbi profile = result->nativeProductAbi;
            assert(profile == XL6_IR_NATIVE_PRODUCT_I64X3_V1);
            result->nativeProductAbi = XL6_IR_NATIVE_PRODUCT_NONE;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));
            result->nativeProductAbi = profile;
            size_t fields = result->paramCount; --result->paramCount;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t)); result->paramCount = fields;
            char name[sizeof(result->cName)]; memcpy(name,result->cName,sizeof(name));
            result->cName[0] = 'z'; assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));
            memcpy(result->cName,name,sizeof(name));
            for (size_t field=0;field<fields;++field) {
                XL6_IrType* item = &ir.types.items[result->params[field]-1];
                XL6_IrIntegerTarget width = item->integerTarget;
                item->integerTarget = XL6_IR_INTEGER_TARGET_SIGNED_32;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t)); item->integerTarget = width;
                XL6_IrCAbi itemAbi = item->cAbi; item->cAbi = XL6_IR_C_ABI_U64;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t)); item->cAbi = itemAbi;
            }
        }
        XL6_IrIntegerTarget width = result->integerTarget;
        if (abi == XL6_IR_C_ABI_I64 || abi == XL6_IR_C_ABI_U64) {
            result->integerTarget = width == XL6_IR_INTEGER_TARGET_SIGNED_64
                ? XL6_IR_INTEGER_TARGET_UNSIGNED_8 : XL6_IR_INTEGER_TARGET_SIGNED_64;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            result->integerTarget = width;
        }
        if (result->collectionAbi == XL6_IR_COLLECTION_ARRAY || result->collectionAbi == XL6_IR_COLLECTION_LIST) {
            XL6_IrTypeKind kind=result->kind;result->kind=XL6_IR_TYPE_BUILTIN;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));result->kind=kind;
            XL6_IrOwnershipAbi outer=result->ownershipAbi;result->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));result->ownershipAbi=outer;
            XL6_IrCollectionAbi collection=result->collectionAbi;
            result->collectionAbi=collection==XL6_IR_COLLECTION_ARRAY?XL6_IR_COLLECTION_LIST:XL6_IR_COLLECTION_ARRAY;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));result->collectionAbi=collection;
            XL6_IrType* item = &ir.types.items[result->params[0] - 1];
            XL6_IrCAbi itemAbi = item->cAbi;
            item->cAbi = XL6_IR_C_ABI_U64;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            item->cAbi = itemAbi;
            if (itemAbi == XL6_IR_C_ABI_I64) {
                XL6_IrIntegerTarget itemWidth = item->integerTarget;
                item->integerTarget = XL6_IR_INTEGER_TARGET_SIGNED_32;
                assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir,t));
                item->integerTarget = itemWidth;
            }
            XL6_IrOwnershipAbi own = item->ownershipAbi;
            item->ownershipAbi = own == XL6_IR_OWNERSHIP_ABI_MANAGED
                ? XL6_IR_OWNERSHIP_ABI_TRIVIAL : XL6_IR_OWNERSHIP_ABI_MANAGED;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            item->ownershipAbi = own;
        }
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&ir, t)); ++checked;
    }
    assert(checked || products);
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags);
    xl6OutputClose(&output);
    printf("text contract: %u fixed ABI/effect/error/borrow signatures, %u generic native products\n", checked, products);
done:
    for (size_t i=0; i<parse.diags.count; ++i)
        fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return !ok;
}
