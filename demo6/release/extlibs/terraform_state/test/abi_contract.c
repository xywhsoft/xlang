#include <stdint.h>
#include "../include/terraform_state.h"

/* state 整数元数据与实例索引使用 XLang int 的 C ABI。 */
extern int64_t xtf_version(xvalue* state);
extern int64_t xtf_serial(xvalue* state);
extern xvalue* xtf_attributes(xvalue* state, const char* address, int64_t index);
