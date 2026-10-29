/* Preinclude current generated C. Binding callbacks must never execute;
 * invalid inputs remain retryable, accepted input is copied and immutable. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static int fakeBegin(XL6_OwnershipReadScopeV1* scope) { (void)scope; assert(0); return 0; }
static bool fakeAdmits(const XL6_OwnershipReadScopeV1* scope,struct xrtownershipref ref)
{ (void)scope; (void)ref; assert(0); return false; }
static void fakeEnd(XL6_OwnershipReadScopeV1* scope) { (void)scope; assert(0); }
void read_binding_probe(void) {}
int main(int argc,char** argv)
{
    assert(argc == 2); bool bind = argv[1][0] == '1', fail = argv[1][0] == '2';
    xl6ProductMemoryBegin(SIZE_MAX);
    XL6_OwnershipReadBoundaryV1 valid = {sizeof(valid), fakeBegin, fakeAdmits, fakeEnd};
    if (bind) {
        xl6ProductMemoryBegin(0);
        assert(!xl_script_bind_ownership_read_v1(NULL));
        for (size_t i = 0; i < 5; ++i) {
            XL6_OwnershipReadBoundaryV1 bad = valid;
            if (i == 0) --bad.size; else if (i == 1) ++bad.size;
            else if (i == 2) bad.Begin = NULL; else if (i == 3) bad.Admits = NULL; else bad.End = NULL;
            assert(!xl_script_bind_ownership_read_v1(&bad));
            assert(!read_binding_bound_cap());
        }
        assert(xl_script_bind_ownership_read_v1(&valid));
        valid = (XL6_OwnershipReadBoundaryV1){0};
        const XL6_OwnershipReadBoundaryV1* copy = read_binding_bound_cap();
        assert(copy && copy->size == sizeof(*copy) && copy->Begin == fakeBegin && copy->Admits == fakeAdmits && copy->End == fakeEnd);
        assert(!xl_script_bind_ownership_read_v1(copy));
        assert(!xl6ProductMemoryEnd());
    }
    valid = (XL6_OwnershipReadBoundaryV1){sizeof(valid), fakeBegin, fakeAdmits, fakeEnd};
    if (fail) {
        xl6ProductMemoryBegin(0);
        assert(!xlir_module_init() && xl6ProductMemoryEnd());
        assert(!xl_script_bind_ownership_read_v1(&valid));
        xrtClearError(); xlir_module_fini();
    }
    // Reopen the heap baseline after the zero-allocation binding probe.
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xlir_module_init());
    assert(!xl_script_bind_ownership_read_v1(&valid));
    assert(read_binding_entry() == 17);
    xlir_module_fini(); assert(!xrtGetError());
    assert(!xl_script_bind_ownership_read_v1(&valid));
    assert(xlir_module_init()); xlir_module_fini();
    assert(!xl_script_bind_ownership_read_v1(&valid));
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("read binding copied once, zero allocations, sealed across unit/init, bind=%d balanced\n", bind); return 0;
}
