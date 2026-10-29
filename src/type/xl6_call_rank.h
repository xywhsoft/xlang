#ifndef XL6_CALL_RANK_H
#define XL6_CALL_RANK_H
#include <stdint.h>
/* Exact conversions win before omitted defaults, including method values. */
enum { XL6_CALL_GENERIC_MATCH_COST = 40 };
static inline int xl6CallRankAdd(uint64_t* rank, uint32_t conversion, uint32_t defaults)
{
    uint32_t oldConversion=(uint32_t)(*rank>>32),oldDefaults=(uint32_t)*rank;
    if(conversion>UINT32_MAX-oldConversion || defaults>UINT32_MAX-oldDefaults)return 0;
    *rank=((uint64_t)(oldConversion+conversion)<<32)|(oldDefaults+defaults);
    return 1;
}
#endif
