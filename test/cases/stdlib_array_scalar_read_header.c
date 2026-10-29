#include "src/runtime/xl6_array_scalar_read.h"
_Static_assert(_Generic(&xl6ArrayReadI64V1,
    int64_t (*)(const xvalue*, uint64_t, uint64_t, int64_t): 1, default: 0), "I64 ABI");
_Static_assert(_Generic(&xl6ArrayReadU64V1,
    uint64_t (*)(const xvalue*, uint64_t, uint64_t, int64_t): 1, default: 0), "U64 ABI");
_Static_assert(_Generic(&xl6ArrayReadBoolV1,
    bool (*)(const xvalue*, uint64_t, uint64_t, int64_t): 1, default: 0), "bool ABI");
_Static_assert(_Generic(&xl6ArrayReadF64V1,
    double (*)(const xvalue*, uint64_t, uint64_t, int64_t): 1, default: 0), "F64 ABI");
int main(void) { return 0; }
