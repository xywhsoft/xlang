#ifndef XL6_SEQUENCE_ABI_H
#define XL6_SEQUENCE_ABI_H
#include <stddef.h>
#include <stdint.h>
typedef struct xrttype xrttype;
/* Declaration-only physical tuple shared by collection and Channel calls.
 * Logical admission is a separate independently owned resident policy. */
typedef struct XL6_SequenceLayoutV1 {
    uint64_t identity,elementIdentity;
    unsigned int transfer;
    size_t size;
    const xrttype* type;
} XL6_SequenceLayoutV1;
#endif
