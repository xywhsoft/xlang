#ifndef XL_TERRAFORM_STATE_H
#define XL_TERRAFORM_STATE_H

#include <stddef.h>
#include <stdint.h>
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 输入为原始 tfstate v4 JSON，而非 terraform show -json 的输出。 */
xvalue* xtf_parse_len(const char* text, size_t length);
xvalue* xtf_parse(const char* text);
xvalue* xtf_parse_value(xvalue* text);
bool xtf_valid(const char* text);
bool xtf_valid_value(xvalue* text);
xvalue* xtf_parse_span(xstrview text);
bool xtf_valid_span(xstrview text);

int64_t xtf_version(xvalue* state);
int64_t xtf_serial(xvalue* state);
char* xtf_lineage(xvalue* state); /* 拥有型结果，由调用方 xrtFree。 */
const char* xtf_lineage_borrowed(xvalue* state); /* 随 state 存活；不得修改。 */
xvalue* xtf_addresses(xvalue* state);
xvalue* xtf_find(xvalue* state, const char* address);
xvalue* xtf_instances(xvalue* state, const char* address);
xvalue* xtf_attributes(xvalue* state, const char* address, int64_t index);
xvalue* xtf_outputs(xvalue* state);
xvalue* xtf_output(xvalue* state, const char* name);
const char* xtf_last_error(void); /* 借用的线程局部错误文本。 */

#ifdef __cplusplus
}
#endif
#endif
