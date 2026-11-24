#ifndef XL6_TEST_XUI_CODE_COMMAND_PAYLOAD_H
#define XL6_TEST_XUI_CODE_COMMAND_PAYLOAD_H

#include <xge3_runtime.h>

static inline bool fixtureSameCommandValue(const xvalue* first, const xvalue* second)
{
    return first && first == second;
}

#endif
