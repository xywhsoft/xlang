/* Inspect the real compiled enum and production ownership function. Macros,
 * delegated families and COUNT are C contracts, not regex-shaped source text.
 * This checks input effects only; it does not admit a retired RuntimeOp. */
#include "src/ir/xl6_ir.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static XL6_IrOperandOwnership expected(XL6_IrRuntimeOp op,size_t operand)
{
    switch(op) {
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_READ:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_READ:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW:
    case XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW:
    case XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW:
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW:
    case XL6_IR_RUNTIME_NAMED_SET_LITERAL_TAKE:
        return operand==1 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE:
    /* Erasure transfers a typed Stack owner even on failure. Projection
     * consumes its separate witness; its second/source operand is borrowed.
     * Checked against the resident Take ABI, not derived from this function. */
    case XL6_IR_RUNTIME_STACK_ERASE:
    case XL6_IR_RUNTIME_NATIVE_CONTAINER_PROJECT_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST:
    case XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND:
    case XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_LIST_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_SET_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_DICT_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_XVALUE_TYPE_BIND:
    case XL6_IR_RUNTIME_XVALUE_ENUM_BIND:
    case XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED:
    case XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND:
    case XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT:
    /* A checked C byte length consumes the already anchored allocation,
     * releasing it on rejection; copying and reading C carriers only borrow. */
    case XL6_IR_RUNTIME_CSTRING_FROM_STRING:
    case XL6_IR_RUNTIME_CBYTES_FROM_BYTES:
    case XL6_IR_RUNTIME_CBYTES_SIZE_TAKE:
    case XL6_IR_RUNTIME_CSTRING_SIZE_TAKE:
        return operand==0 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW:
    case XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE:
    case XL6_IR_RUNTIME_DICT_SET_VALUE_TAKE:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW:
    case XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW:
        return operand==2 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD:
        return operand!=0 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    default: return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    }
}
int main(void)
{
    size_t checked=0,consumed=0;
    /* Operand effects are independent of arity, which the IR verifier checks
     * separately. Include tail/size bounds in the dynamic consume family. */
    const size_t operands[]={0,1,2,3,4,8,64,SIZE_MAX};
    for(int code=XL6_IR_RUNTIME_NONE+1; code<XL6_IR_RUNTIME_COUNT; ++code) {
        for(size_t index=0; index<sizeof(operands)/sizeof(*operands); ++index) {
            XL6_IrOperandOwnership effect=xl6IrRuntimeOperandOwnership((XL6_IrRuntimeOp)code,operands[index]);
            if(effect!=expected((XL6_IrRuntimeOp)code,operands[index]))
                fprintf(stderr,"RuntimeOp=%d operand=%zu effect=%d expected=%d\n",code,operands[index],
                    (int)effect,(int)expected((XL6_IrRuntimeOp)code,operands[index]));
            assert(effect==expected((XL6_IrRuntimeOp)code,operands[index]));
            ++checked; consumed+=effect==XL6_IR_OPERAND_OWNERSHIP_CONSUME;
        }
    }
    const XL6_IrRuntimeOp invalid[]={XL6_IR_RUNTIME_NONE,XL6_IR_RUNTIME_COUNT,
        (XL6_IrRuntimeOp)-1,(XL6_IrRuntimeOp)(XL6_IR_RUNTIME_COUNT+1),(XL6_IrRuntimeOp)INT_MAX};
    size_t refused=0;
    for(size_t i=0; i<sizeof(invalid)/sizeof(*invalid); ++i)
        for(size_t j=0; j<sizeof(operands)/sizeof(*operands); ++j) {
            assert(xl6IrRuntimeOperandOwnership(invalid[i],operands[j])==XL6_IR_OPERAND_OWNERSHIP_INVALID);
            ++refused;
        }
    assert(checked && consumed && refused==40);
    printf("compiled RuntimeOp catalog: operations=%d effects=%zu consumes=%zu invalid=%zu rejected\n",
        XL6_IR_RUNTIME_COUNT-1,checked,consumed,refused);
    return 0;
}
