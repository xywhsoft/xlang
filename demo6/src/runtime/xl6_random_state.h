#ifndef XL6_RANDOM_STATE_H
#define XL6_RANDOM_STATE_H
/* Private owned state shared by reached RNG adapters and resident cleanup.
 * xrng is inline state; owner is a thread ID, not a retained language thread. */
typedef struct xlir_random_state { xrng rng; uint64_t owner; } xlir_random_state;
void* xl6_xrt_rng_create(uint64_t seed,uint64_t stream);
void* xl6_xrt_rng_create_default(uint64_t seed);
void xl6_xrt_rng_destroy(void* handle);
#endif
