/* Included before the original IR module lifetime functions. The opaque
 * result is the only API exposing borrowed fragment text; ordinary compiler
 * modules keep their original individually owned strings and layout. */
#include "analysis/frontend_ir_memo.h"
typedef struct xls_ir_unit xls_ir_unit;
typedef struct xls_ir_persistent_storage xls_ir_persistent_storage;
struct xls_ir_result {
    XL6_IrModule module;
    xls_ir_memo* memo;
    xarray fragments,borrowed;
    xls_ir_memo_work work;
    uint64_t textNodes,textBytes;
    size_t storageLimit;
    xls_ir_persistent_storage* persistent;
    xatomic32 persistentState;
    struct xls_typed_module* input;
    xls_ir_persistent_work persistentWork;
};
static _Thread_local xls_ir_result* xlsActiveIrOwner;
static int xlsIrOwnerBorrowing(const XL6_IrModule* module)
{return xlsActiveIrOwner && &xlsActiveIrOwner->module==module && xlsActiveIrOwner->storageLimit;}
static void xlsIrOwnerFreeText(const XL6_IrModule* module,size_t index,void* text)
{
    xls_ir_result* owner=xlsActiveIrOwner;
    if(!owner || &owner->module!=module || index/8>=owner->borrowed.Count ||
       !(((const unsigned char*)owner->borrowed.Data)[index/8]&(1u<<(index%8))))xrtFree(text);
}
static void xlsIrOwnerReset(const XL6_IrModule* module);
