#include "blake3.h"
#include <string.h>

/* BLAKE3 portable C implementation (single-threaded, no SIMD) */

static const uint32_t IV[8] = {
	0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t MSG_SCHEDULE[7][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
	{ 2, 6, 3,10, 7, 0, 4,13, 1,11,12, 5, 9,14,15, 8},
	{ 3, 4,10,12,13, 2, 7,14, 6, 5, 9, 0,11,15, 8, 1},
	{10, 7,12, 9,14, 3,13,15, 4, 0,11, 2, 5, 8, 1, 6},
	{12,13, 9,11,15,10,14, 8, 7, 2, 5, 3, 0, 1, 6, 4},
	{ 9,14,11, 5, 8,12,15, 1,13, 3, 0,10, 2, 6, 4, 7},
	{11,15, 5, 0, 1, 9, 8, 6,14,10, 2,12, 3, 4, 7,13}
};

static uint32_t rotr32(uint32_t w, uint32_t c) { return (w >> c) | (w << (32u - c)); }

static void g(uint32_t* state, size_t a, size_t b, size_t c, size_t d,
	uint32_t mx, uint32_t my)
{
	state[a] = state[a] + state[b] + mx;
	state[d] = rotr32(state[d] ^ state[a], 16);
	state[c] = state[c] + state[d];
	state[b] = rotr32(state[b] ^ state[c], 12);
	state[a] = state[a] + state[b] + my;
	state[d] = rotr32(state[d] ^ state[a], 8);
	state[c] = state[c] + state[d];
	state[b] = rotr32(state[b] ^ state[c], 7);
}

static void round_fn(uint32_t state[16], const uint32_t m[16], size_t round)
{
	const uint8_t* s = MSG_SCHEDULE[round];
	g(state, 0, 4,  8, 12, m[s[ 0]], m[s[ 1]]);
	g(state, 1, 5,  9, 13, m[s[ 2]], m[s[ 3]]);
	g(state, 2, 6, 10, 14, m[s[ 4]], m[s[ 5]]);
	g(state, 3, 7, 11, 15, m[s[ 6]], m[s[ 7]]);
	g(state, 0, 5, 10, 15, m[s[ 8]], m[s[ 9]]);
	g(state, 1, 6, 11, 12, m[s[10]], m[s[11]]);
	g(state, 2, 7,  8, 13, m[s[12]], m[s[13]]);
	g(state, 3, 4,  9, 14, m[s[14]], m[s[15]]);
}

static uint32_t load32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store32(uint8_t* p, uint32_t v)
{
	p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void store64(uint8_t* p, uint64_t v)
{
	p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
	p[4]=(uint8_t)(v>>32); p[5]=(uint8_t)(v>>40); p[6]=(uint8_t)(v>>48); p[7]=(uint8_t)(v>>56);
}

/* Compress one block. Returns 16-word state (first 8 = output). */
static void compress(const uint32_t cv[8], const uint8_t block[64],
	uint64_t counter, uint32_t block_len, uint8_t flags, uint32_t out[16])
{
	uint32_t m[16];
	uint32_t state[16];
	size_t i;
	for (i = 0; i < 16; ++i) m[i] = load32(block + i * 4);
	state[0] = cv[0]; state[1] = cv[1]; state[2] = cv[2]; state[3] = cv[3];
	state[4] = cv[4]; state[5] = cv[5]; state[6] = cv[6]; state[7] = cv[7];
	state[8] = IV[0]; state[9] = IV[1]; state[10] = IV[2]; state[11] = IV[3];
	state[12] = (uint32_t)counter;
	state[13] = (uint32_t)(counter >> 32);
	state[14] = block_len;
	state[15] = (uint32_t)flags;
	for (i = 0; i < 7; ++i) round_fn(state, m, i);
	for (i = 0; i < 8; ++i) {
		out[i] = state[i] ^ state[i + 8];
		out[i + 8] = state[i + 8] ^ cv[i];
	}
}

/* Output chaining value (first 8 words of compression). */
static void compress_cv(const uint32_t cv[8], const uint8_t block[64],
	uint64_t counter, uint32_t block_len, uint8_t flags, uint32_t out_cv[8])
{
	uint32_t out[16];
	compress(cv, block, counter, block_len, flags, out);
	memcpy(out_cv, out, 32);
}

/* ---- Chunk state ---- */

static void chunk_state_init(blake3_chunk_state* cs, const uint32_t key[8],
	uint64_t chunk_counter, uint8_t flags)
{
	memcpy(cs->input_cv, key, 32);
	cs->chunk_counter = chunk_counter;
	memset(cs->buf, 0, BLAKE3_BLOCK_LEN);
	cs->buf_len = 0;
	cs->blocks_compressed = 0;
	cs->flags = flags;
}

static size_t chunk_state_len(const blake3_chunk_state* cs)
{
	return (size_t)cs->blocks_compressed * BLAKE3_BLOCK_LEN + (size_t)cs->buf_len;
}

static uint8_t chunk_state_start_flag(const blake3_chunk_state* cs)
{
	return cs->blocks_compressed == 0 ? CHUNK_START : 0;
}

static void chunk_state_update(blake3_chunk_state* cs, const uint8_t* input, size_t len)
{
	while (len > 0) {
		size_t take;
		if (cs->buf_len == BLAKE3_BLOCK_LEN) {
			uint8_t block_flags = cs->flags | chunk_state_start_flag(cs);
			uint32_t cv[8];
			compress_cv(cs->input_cv, cs->buf, cs->chunk_counter,
				BLAKE3_BLOCK_LEN, block_flags, cv);
			memcpy(cs->input_cv, cv, 32);
			cs->blocks_compressed++;
			memset(cs->buf, 0, BLAKE3_BLOCK_LEN);
			cs->buf_len = 0;
		}
		take = BLAKE3_BLOCK_LEN - (size_t)cs->buf_len;
		if (take > len) take = len;
		memcpy(cs->buf + cs->buf_len, input, take);
		cs->buf_len += (uint8_t)take;
		input += take;
		len -= take;
	}
}

/* Produce the chaining value of this chunk (without ROOT flag). */
static void chunk_state_cv(const blake3_chunk_state* cs, uint32_t out_cv[8])
{
	uint8_t block_flags = cs->flags | CHUNK_END | chunk_state_start_flag(cs);
	compress_cv(cs->input_cv, cs->buf, cs->chunk_counter,
		(uint32_t)cs->buf_len, block_flags, out_cv);
}

/* Produce extended output from this chunk (with ROOT flag). */
static void chunk_state_output(const blake3_chunk_state* cs, uint8_t* out, size_t out_len)
{
	uint8_t block_flags = cs->flags | CHUNK_END | ROOT | chunk_state_start_flag(cs);
	uint64_t output_block_counter = 0;
	size_t offset = 0;
	while (offset < out_len) {
		uint32_t words[16];
		size_t take = out_len - offset;
		size_t i;
		if (take > 64) take = 64;
		compress(cs->input_cv, cs->buf, output_block_counter,
			(uint32_t)cs->buf_len, block_flags, words);
		for (i = 0; i < take; ++i) {
			out[offset + i] = (uint8_t)(words[i / 4] >> (8 * (i % 4)));
		}
		offset += take;
		output_block_counter++;
	}
}

/* ---- Parent node ---- */

static void parent_cv(const uint32_t left_cv[8], const uint32_t right_cv[8],
	const uint32_t key[8], uint8_t flags, uint32_t out_cv[8])
{
	uint8_t block[64];
	store32(block,      left_cv[0]); store32(block + 4,  left_cv[1]);
	store32(block + 8,  left_cv[2]); store32(block + 12, left_cv[3]);
	store32(block + 16, left_cv[4]); store32(block + 20, left_cv[5]);
	store32(block + 24, left_cv[6]); store32(block + 28, left_cv[7]);
	store32(block + 32, right_cv[0]); store32(block + 36, right_cv[1]);
	store32(block + 40, right_cv[2]); store32(block + 44, right_cv[3]);
	store32(block + 48, right_cv[4]); store32(block + 52, right_cv[5]);
	store32(block + 56, right_cv[6]); store32(block + 60, right_cv[7]);
	compress_cv(key, block, 0, BLAKE3_BLOCK_LEN, flags | PARENT, out_cv);
}

static void parent_output(const uint32_t left_cv[8], const uint32_t right_cv[8],
	const uint32_t key[8], uint8_t flags, uint8_t* out, size_t out_len)
{
	uint8_t block[64];
	uint64_t output_block_counter = 0;
	size_t offset = 0;
	store32(block,      left_cv[0]); store32(block + 4,  left_cv[1]);
	store32(block + 8,  left_cv[2]); store32(block + 12, left_cv[3]);
	store32(block + 16, left_cv[4]); store32(block + 20, left_cv[5]);
	store32(block + 24, left_cv[6]); store32(block + 28, left_cv[7]);
	store32(block + 32, right_cv[0]); store32(block + 36, right_cv[1]);
	store32(block + 40, right_cv[2]); store32(block + 44, right_cv[3]);
	store32(block + 48, right_cv[4]); store32(block + 52, right_cv[5]);
	store32(block + 56, right_cv[6]); store32(block + 60, right_cv[7]);
	while (offset < out_len) {
		uint32_t words[16];
		size_t take = out_len - offset;
		size_t i;
		if (take > 64) take = 64;
		compress(key, block, output_block_counter, BLAKE3_BLOCK_LEN,
			flags | PARENT | ROOT, words);
		for (i = 0; i < take; ++i) {
			out[offset + i] = (uint8_t)(words[i / 4] >> (8 * (i % 4)));
		}
		offset += take;
		output_block_counter++;
	}
}

/* ---- Hasher ---- */

static uint8_t* cv_stack_at(blake3_hasher* self, size_t i)
{
	return self->cv_stack + (size_t)i * 32;
}

static const uint8_t* cv_stack_at_c(const blake3_hasher* self, size_t i)
{
	return self->cv_stack + (size_t)i * 32;
}

static void hasher_push_cv(blake3_hasher* self, const uint32_t cv[8])
{
	size_t i;
	uint8_t* dst = cv_stack_at(self, self->cv_stack_len);
	for (i = 0; i < 8; ++i) store32(dst + i * 4, cv[i]);
	self->cv_stack_len++;
}

static void hasher_pop_cv(blake3_hasher* self, uint32_t cv[8])
{
	size_t i;
	const uint8_t* src;
	self->cv_stack_len--;
	src = cv_stack_at_c(self, self->cv_stack_len);
	for (i = 0; i < 8; ++i) cv[i] = load32(src + i * 4);
}

/* Merge subtrees: while total_chunks is even, merge pairs. */
static void hasher_add_chunk_cv(blake3_hasher* self, uint32_t cv[8], uint64_t total_chunks)
{
	while ((total_chunks & 1) == 0) {
		uint32_t left[8];
		hasher_pop_cv(self, left);
		parent_cv(left, cv, self->key, self->chunk.flags, cv);
		total_chunks >>= 1;
	}
	hasher_push_cv(self, cv);
}

void blake3_hasher_init(blake3_hasher* self)
{
	memcpy(self->key, IV, 32);
	chunk_state_init(&self->chunk, self->key, 0, 0);
	self->cv_stack_len = 0;
}

void blake3_hasher_init_keyed(blake3_hasher* self, const uint8_t key[BLAKE3_KEY_LEN])
{
	size_t i;
	for (i = 0; i < 8; ++i) self->key[i] = load32(key + i * 4);
	chunk_state_init(&self->chunk, self->key, 0, KEYED_HASH);
	self->cv_stack_len = 0;
}

void blake3_hasher_init_derive_key(blake3_hasher* self, const char* context)
{
	blake3_hasher context_hasher;
	uint8_t context_key[BLAKE3_KEY_LEN];
	size_t i;
	/* Hash the context string with DERIVE_KEY_CONTEXT flag */
	memcpy(context_hasher.key, IV, 32);
	chunk_state_init(&context_hasher.chunk, context_hasher.key, 0, DERIVE_KEY_CONTEXT);
	context_hasher.cv_stack_len = 0;
	blake3_hasher_update(&context_hasher, context, strlen(context));
	blake3_hasher_finalize(&context_hasher, context_key, BLAKE3_KEY_LEN);
	/* Use context key for the actual hasher */
	for (i = 0; i < 8; ++i) self->key[i] = load32(context_key + i * 4);
	chunk_state_init(&self->chunk, self->key, 0, DERIVE_KEY_MATERIAL);
	self->cv_stack_len = 0;
}

void blake3_hasher_update(blake3_hasher* self, const void* input, size_t input_len)
{
	const uint8_t* in = (const uint8_t*)input;
	while (input_len > 0) {
		size_t take;
		/* If current chunk is full, finalize it and start a new one */
		if (chunk_state_len(&self->chunk) == BLAKE3_CHUNK_LEN) {
			uint32_t cv[8];
			uint64_t total_chunks;
			chunk_state_cv(&self->chunk, cv);
			total_chunks = self->chunk.chunk_counter + 1;
			hasher_add_chunk_cv(self, cv, total_chunks);
			chunk_state_init(&self->chunk, self->key,
				self->chunk.chunk_counter + 1, self->chunk.flags);
		}
		take = BLAKE3_CHUNK_LEN - chunk_state_len(&self->chunk);
		if (take > input_len) take = input_len;
		chunk_state_update(&self->chunk, in, take);
		in += take;
		input_len -= take;
	}
}

void blake3_hasher_finalize(const blake3_hasher* self, uint8_t* out, size_t out_len)
{
	/* Work on a copy so we don't mutate the hasher */
	blake3_hasher tmp = *self;
	/* Get the CV of the current (last) chunk */
	{
		uint32_t cv[8];
		uint64_t total_chunks = tmp.chunk.chunk_counter + 1;
		chunk_state_cv(&tmp.chunk, cv);
		/* Merge with the stack */
		while (tmp.cv_stack_len > 0) {
			uint32_t left[8];
			hasher_pop_cv(&tmp, left);
			parent_cv(left, cv, tmp.key, tmp.chunk.flags, cv);
			total_chunks >>= 1;
			while (total_chunks > 1 && (total_chunks & 1) == 0) {
				uint32_t left2[8];
				if (tmp.cv_stack_len == 0) break;
				hasher_pop_cv(&tmp, left2);
				parent_cv(left2, cv, tmp.key, tmp.chunk.flags, cv);
				total_chunks >>= 1;
			}
		}
		/* Now cv is the root parent. Produce output from the last chunk with ROOT. */
		/* Actually we need to produce output from the top-level node. */
		/* If stack is empty and we have a single chunk, output from chunk directly. */
		if (tmp.chunk.chunk_counter == 0 && self->cv_stack_len == 0) {
			/* Single chunk: output directly from chunk state */
			chunk_state_output(&self->chunk, out, out_len);
		} else {
			/* Multi-chunk: the final cv came from parent nodes.
			   We need to produce root output from the last parent. */
			/* Reconstruct: get left and right of the root parent */
			/* Simpler approach: redo the merge to get root left/right */
			blake3_hasher tmp2 = *self;
			uint32_t root_left[8], root_right[8];
			uint32_t cur_cv[8];
			chunk_state_cv(&tmp2.chunk, cur_cv);
			/* Merge all stack entries */
			while (tmp2.cv_stack_len > 0) {
				uint32_t left[8];
				hasher_pop_cv(&tmp2, left);
				root_left[0] = left[0]; root_left[1] = left[1];
				root_left[2] = left[2]; root_left[3] = left[3];
				root_left[4] = left[4]; root_left[5] = left[5];
				root_left[6] = left[6]; root_left[7] = left[7];
				root_right[0] = cur_cv[0]; root_right[1] = cur_cv[1];
				root_right[2] = cur_cv[2]; root_right[3] = cur_cv[3];
				root_right[4] = cur_cv[4]; root_right[5] = cur_cv[5];
				root_right[6] = cur_cv[6]; root_right[7] = cur_cv[7];
				parent_cv(left, cur_cv, tmp2.key, tmp2.chunk.flags, cur_cv);
			}
			/* root_left/root_right are the children of the root parent */
			parent_output(root_left, root_right, self->key,
				self->chunk.flags, out, out_len);
		}
	}
}
