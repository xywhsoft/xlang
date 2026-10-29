#ifndef XL6_COMPONENT_SET_H
#define XL6_COMPONENT_SET_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Bounded, caller-owned bit spans. Width comes from the graph population,
 * not a fixed mask capacity. No allocation, global state or shift by >= 64.
 * The caller supplies ceil(bits/64) words; unused tail bits must be zero. */
static inline size_t xl6ComponentSetWordCount(size_t bits)
{ return bits / 64u + (bits % 64u != 0); }
static inline int xl6ComponentSetValid(const uint64_t* words, size_t bits)
{
    size_t count = xl6ComponentSetWordCount(bits), tail = bits % 64u;
    return (!count || words != NULL) &&
        (!tail || !(words[count-1] & ~((UINT64_C(1) << tail)-1u)));
}
static inline void xl6ComponentSetClear(uint64_t* words, size_t bits)
{ if (words != NULL) memset(words,0,xl6ComponentSetWordCount(bits)*sizeof(*words)); }
static inline int xl6ComponentSetHas(const uint64_t* words, size_t bits, size_t id)
{ return words != NULL && id < bits && (words[id/64u] & (UINT64_C(1) << (id%64u))) != 0; }
static inline int xl6ComponentSetAdd(uint64_t* words, size_t bits, size_t id)
{
    if (words == NULL || id >= bits) return 0;
    words[id/64u] |= UINT64_C(1) << (id%64u);
    return 1;
}
static inline int xl6ComponentSetAddMany(uint64_t* words, size_t bits,
    const size_t* ids, size_t count)
{
    if (!xl6ComponentSetValid(words,bits) || (count && ids == NULL)) return 0;
    /* Validate before modifying, including repeated/invalid IDs. */
    for (size_t i=0;i<count;++i) if (ids[i] >= bits) return 0;
    for (size_t i=0;i<count;++i) (void)xl6ComponentSetAdd(words,bits,ids[i]);
    return 1;
}
static inline int xl6ComponentSetUnion(uint64_t* target, const uint64_t* source, size_t bits)
{
    if (!xl6ComponentSetValid(target,bits) || !xl6ComponentSetValid(source,bits)) return 0;
    for (size_t i=0;i<xl6ComponentSetWordCount(bits);++i) target[i] |= source[i];
    return 1;
}
static inline int xl6ComponentSetEqual(const uint64_t* left, const uint64_t* right, size_t bits)
{
    if (!xl6ComponentSetValid(left,bits) || !xl6ComponentSetValid(right,bits)) return 0;
    for (size_t i=0;i<xl6ComponentSetWordCount(bits);++i) if (left[i] != right[i]) return 0;
    return 1;
}
static inline int xl6ComponentSetEmpty(const uint64_t* words, size_t bits)
{
    if (!xl6ComponentSetValid(words,bits)) return 0;
    for (size_t i=0;i<xl6ComponentSetWordCount(bits);++i) if (words[i]) return 0;
    return 1;
}

typedef int (*XL6_ComponentDependencies)(void* context, size_t id,
    const size_t** dependencies, size_t* count);

/* Close the actual graph monotonically. Cycles, duplicates and self-edges
 * are valid. Bad roots/edges/provider failure clear all output words. The
 * immutable graph callback must return a live dependency span for the call. */
static inline int xl6ComponentSetClose(uint64_t* words, size_t bits,
    XL6_ComponentDependencies lookup, void* context)
{
    int changed;
    if (!xl6ComponentSetValid(words,bits) || lookup == NULL) goto invalid;
    do {
        changed = 0;
        for (size_t id=0; id<bits; ++id) {
            const size_t* dependencies = NULL;
            size_t count = 0;
            if (!xl6ComponentSetHas(words,bits,id)) continue;
            if (!lookup(context,id,&dependencies,&count) || (count && dependencies == NULL)) goto invalid;
            for (size_t i=0; i<count; ++i) {
                size_t dependency = dependencies[i];
                if (dependency >= bits) goto invalid;
                if (!xl6ComponentSetHas(words,bits,dependency)) {
                    (void)xl6ComponentSetAdd(words,bits,dependency); changed = 1;
                }
            }
        }
    } while (changed);
    return 1;
invalid:
    xl6ComponentSetClear(words,bits);
    return 0;
}
#endif
