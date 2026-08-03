#ifndef BLAKE3_H
#define BLAKE3_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLAKE3_VERSION_STRING "1.5.0"
#define BLAKE3_KEY_LEN 32
#define BLAKE3_OUT_LEN 32
#define BLAKE3_BLOCK_LEN 64
#define BLAKE3_CHUNK_LEN 1024
#define BLAKE3_MAX_DEPTH 54

/* Flags */
#define CHUNK_START 1
#define CHUNK_END 2
#define PARENT 4
#define ROOT 8
#define KEYED_HASH 16
#define DERIVE_KEY_CONTEXT 32
#define DERIVE_KEY_MATERIAL 64

typedef struct {
	uint32_t input_cv[8];
	uint64_t chunk_counter;
	uint8_t buf[BLAKE3_BLOCK_LEN];
	uint8_t buf_len;
	uint8_t blocks_compressed;
	uint8_t flags;
} blake3_chunk_state;

typedef struct {
	uint32_t key[8];
	blake3_chunk_state chunk;
	uint8_t cv_stack_len;
	uint8_t cv_stack[BLAKE3_MAX_DEPTH * 32];
} blake3_hasher;

void blake3_hasher_init(blake3_hasher* self);
void blake3_hasher_init_keyed(blake3_hasher* self, const uint8_t key[BLAKE3_KEY_LEN]);
void blake3_hasher_init_derive_key(blake3_hasher* self, const char* context);
void blake3_hasher_update(blake3_hasher* self, const void* input, size_t input_len);
void blake3_hasher_finalize(const blake3_hasher* self, uint8_t* out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
