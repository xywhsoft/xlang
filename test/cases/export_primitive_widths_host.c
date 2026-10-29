#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef XL6_DIFFERENTIAL_INLINE_DECL
uint64_t xl6_pick_u64(
    int8_t i8, uint8_t u8, int16_t i16, uint16_t u16,
    int32_t i32, uint32_t u32, int64_t i64, uint64_t u64,
    float f32, double f64, bool ok, void* handle);
#else
#include "export_primitive_widths.h"
#endif

#ifdef XL6_LEGACY_RUNTIME
struct XL_InitOptions;
extern int xlInit(const struct XL_InitOptions* options);
extern void xlUnit(void);
#endif

int main(void)
{
    const uint64_t expected = UINT64_C(0xFEDCBA9876543210);
    int result;
#ifdef XL6_LEGACY_RUNTIME
    if ( !xlInit(NULL) ) return 2;
#endif
    result = xl6_pick_u64(
        INT8_MIN, UINT8_MAX, INT16_MIN, UINT16_MAX,
        INT32_MIN, UINT32_MAX, INT64_MIN, expected,
        1.25f, 2.5, true, (void*)(uintptr_t)0x1234) == expected
        ? 0 : 1;
#ifdef XL6_LEGACY_RUNTIME
    xlUnit();
#endif
    return result;
}
