# Hash Computing Library

> High-performance hash function library (based on nmhash32x and rapidhash algorithms)

[English](api-hash.en.md) | [中文](api-hash.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Algorithm Introduction](#algorithm-introduction)
- [Constant Definitions](#constant-definitions)
- [32-bit Hash](#32-bit-hash)
- [64-bit Hash](#64-bit-hash)
- [Use Cases](#use-cases)
- [Performance Comparison](#performance-comparison)
- [Best Practices](#best-practices)

---

## Algorithm Introduction

### nmhash32x (32-bit Hash)

- **Source**: [smhasher](https://github.com/rurban/smhasher) project
- **Version**: Ver 2.0
- **Features**:
  - High-quality 32-bit hash
  - SIMD acceleration support (SSE2/AVX2/AVX512)
  - Suitable for hash tables, data checksums, etc.
- **License**: BSD 2-Clause

### rapidhash (64-bit Hash)

- **Source**: [rapidhash](https://github.com/Nicoshev/rapidhash) project
- **Version**: Ver 3.0
- **Features**:
  - Extremely high-performance 64-bit hash
  - Optimized based on wyhash algorithm
  - Provides three variants: Standard, Micro, Nano
- **License**: BSD 2-Clause

---

## Constant Definitions

### HASH32_SEED

Default seed for 32-bit hash.

**Definition:**
```c
#define HASH32_SEED 0
```

**Notes:**
- `xrtHash32` uses this value as default seed internally
- Custom seed can be specified via `xrtHash32_WithSeed`

---

## 32-bit Hash

### xrtHash32

Calculate 32-bit hash value with default seed.

**Function Prototype:**
```c
XXAPI uint32 xrtHash32(ptr key, size_t len);
```

**Parameters:**
- `key` - Data pointer
- `len` - Data length (bytes)

**Return Value:**
- 32-bit hash value (0 ~ 4294967295)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Calculate string hash
    str text = (str)"Hello, World!";
    uint32 hash = xrtHash32(text, strlen((char*)text));
    printf("Hash32: %u\n", hash);
    
    // Calculate binary data hash
    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32 hash2 = xrtHash32(data, sizeof(data));
    printf("Binary Hash32: %u\n", hash2);
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Uses nmhash32x algorithm internally
- Same input always returns same result
- Zero-length data also returns a valid hash value

---

### xrtHash32_WithSeed

Calculate 32-bit hash value with custom seed.

**Function Prototype:**
```c
XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed);
```

**Parameters:**
- `key` - Data pointer
- `len` - Data length (bytes)
- `seed` - Custom seed

**Return Value:**
- 32-bit hash value

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello";
    size_t len = strlen((char*)text);
    
    // Different seeds produce different hashes
    uint32 hash1 = xrtHash32_WithSeed(text, len, 0);
    uint32 hash2 = xrtHash32_WithSeed(text, len, 12345);
    uint32 hash3 = xrtHash32_WithSeed(text, len, 99999);
    
    printf("Seed 0:     %u\n", hash1);
    printf("Seed 12345: %u\n", hash2);
    printf("Seed 99999: %u\n", hash3);
    
    // Verify: same seed produces same result
    uint32 hash1_again = xrtHash32_WithSeed(text, len, 0);
    printf("Same: %s\n", hash1 == hash1_again ? "YES" : "NO");
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Different seeds produce different hash values for same data
- Can be used for hash table anti-collision attack (Hash-DoS protection)
- Can be used to generate multiple independent hashes (Bloom filters, etc.)

---

## 64-bit Hash

### xrtHash64

Calculate 64-bit hash value with default seed (standard version).

**Function Prototype:**
```c
XXAPI uint64 xrtHash64(ptr key, size_t len);
```

**Parameters:**
- `key` - Data pointer
- `len` - Data length (bytes)

**Return Value:**
- 64-bit hash value

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello, World!";
    uint64 hash = xrtHash64(text, strlen((char*)text));
    printf("Hash64: %" PRIu64 "\n", hash);
    
    // Calculate large data hash
    ptr data = xrtMalloc(1024 * 1024);  // 1MB
    memset(data, 0xAB, 1024 * 1024);
    uint64 hash2 = xrtHash64(data, 1024 * 1024);
    printf("1MB data hash: %" PRIu64 "\n", hash2);
    xrtFree(data);
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Uses rapidhash algorithm internally
- Suitable for large data hash calculation
- Provides highest quality and best distribution

---

### xrtHash64_WithSeed

Calculate 64-bit hash value with custom seed.

**Function Prototype:**
```c
XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed);
```

**Parameters:**
- `key` - Data pointer
- `len` - Data length (bytes)
- `seed` - 64-bit custom seed

**Return Value:**
- 64-bit hash value

---

### xrtHash64_Micro / xrtHash64_Micro_WithSeed

Micro version 64-bit hash (suitable for server applications).

**Function Prototype:**
```c
XXAPI uint64 xrtHash64_Micro(ptr key, size_t len);
XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed);
```

**Features:**
- Optimized for HPC and server applications
- Compiles to about 140 instructions, no stack usage
- Fastest for data under 512 bytes
- 15-20% slower than standard for data over 1KB

---

### xrtHash64_Nano / xrtHash64_Nano_WithSeed

Ultra-small version 64-bit hash (suitable for embedded and mobile applications).

**Function Prototype:**
```c
XXAPI uint64 xrtHash64_Nano(ptr key, size_t len);
XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed);
```

**Features:**
- Optimized for embedded and mobile applications
- Compiles to less than 100 instructions, no stack usage
- Fastest for data under 48 bytes
- May be noticeably slower for large data

---

## Use Cases

### 1. Hash Table Key Lookup

```c
#include "xrt.h"
#include <stdio.h>

#define BUCKET_COUNT 1024

uint32 GetBucket(str key) {
    uint32 hash = xrtHash32(key, strlen((char*)key));
    return hash % BUCKET_COUNT;
}

int main() {
    xrtInit();
    
    str keys[] = {(str)"apple", (str)"banana", (str)"cherry", (str)"date"};
    int count = sizeof(keys) / sizeof(keys[0]);
    
    for (int i = 0; i < count; i++) {
        uint32 bucket = GetBucket(keys[i]);
        printf("%s -> bucket %u\n", keys[i], bucket);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. File Integrity Check

```c
#include "xrt.h"
#include <stdio.h>

uint64 ComputeFileChecksum(str filepath) {
    ptr data = xrtFileGetAll(filepath);
    if (!data) return 0;
    
    size_t size = xCore.iRet;
    uint64 checksum = xrtHash64(data, size);
    xrtFree(data);
    
    return checksum;
}

int main() {
    xrtInit();
    
    str file1 = (str)"test/test.txt";
    uint64 cs1 = ComputeFileChecksum(file1);
    
    if (cs1 != 0) {
        printf("File checksum: %" PRIu64 "\n", cs1);
    } else {
        printf("File not found or empty\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 3. Bloom Filter (Multiple Hashes)

```c
#include "xrt.h"
#include <stdio.h>

#define BLOOM_SIZE 10000
#define NUM_HASHES 3

uint8 bloom_filter[BLOOM_SIZE] = {0};

void BloomAdd(str key) {
    size_t len = strlen((char*)key);
    for (int i = 0; i < NUM_HASHES; i++) {
        uint32 hash = xrtHash32_WithSeed(key, len, i * 12345);
        bloom_filter[hash % BLOOM_SIZE] = 1;
    }
}

int BloomCheck(str key) {
    size_t len = strlen((char*)key);
    for (int i = 0; i < NUM_HASHES; i++) {
        uint32 hash = xrtHash32_WithSeed(key, len, i * 12345);
        if (bloom_filter[hash % BLOOM_SIZE] == 0) {
            return 0;  // Definitely not present
        }
    }
    return 1;  // Possibly present
}

int main() {
    xrtInit();
    
    // Add elements
    BloomAdd((str)"apple");
    BloomAdd((str)"banana");
    BloomAdd((str)"cherry");
    
    // Check
    printf("apple: %s\n", BloomCheck((str)"apple") ? "probably yes" : "no");
    printf("grape: %s\n", BloomCheck((str)"grape") ? "probably yes" : "no");
    
    xrtUnit();
    return 0;
}
```

---

### 4. Cache Key Generation

```c
#include "xrt.h"
#include <stdio.h>

str GenerateCacheKey(str prefix, str params) {
    size_t prefix_len = strlen((char*)prefix);
    size_t params_len = strlen((char*)params);
    
    // Generate cache key with parameter hash
    uint64 hash = xrtHash64(params, params_len);
    return xrtFormat((str)"%s_%" PRIu64, prefix, hash);
}

int main() {
    xrtInit();
    
    str params = (str)"user=123&page=5&sort=name";
    str cache_key = GenerateCacheKey((str)"api_result", params);
    printf("Cache key: %s\n", cache_key);
    xrtFree(cache_key);
    
    xrtUnit();
    return 0;
}
```

---

## Performance Comparison

### 64-bit Hash Variant Comparison

| Variant | Instructions | Best Data Size | Use Case |
|---------|--------------|----------------|----------|
| **xrtHash64** | ~200+ | 1KB+ | General purpose, large data |
| **xrtHash64_Micro** | ~140 | 16-512B | HPC, servers, network packets |
| **xrtHash64_Nano** | <100 | 1-48B | Embedded, mobile, short strings |

### Selection Guide

```c
#include "xrt.h"
#include <stdio.h>

uint64 SmartHash(ptr data, size_t len) {
    if (len <= 48) {
        return xrtHash64_Nano(data, len);   // Ultra-small data
    } else if (len <= 512) {
        return xrtHash64_Micro(data, len);  // Medium data
    } else {
        return xrtHash64(data, len);        // Large data
    }
}

int main() {
    xrtInit();
    
    str small = (str)"hello";
    str medium = (str)"This is a medium length string for testing purposes";
    
    printf("Small hash: %" PRIu64 "\n", SmartHash(small, strlen((char*)small)));
    printf("Medium hash: %" PRIu64 "\n", SmartHash(medium, strlen((char*)medium)));
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Stable Hash (Serialization/Caching)

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✓ Use fixed seed to ensure cross-process/cross-machine consistency
    uint64 hash = xrtHash64_WithSeed((str)"data", 4, 0);
    printf("Stable hash: %" PRIu64 "\n", hash);
    
    // ✗ Don't use random seed for cache keys
    // uint64 bad = xrtHash64_WithSeed("data", 4, xrtRand64());
    
    xrtUnit();
    return 0;
}
```

### 2. Secure Hash (Anti-Collision Attack)

```c
#include "xrt.h"
#include <stdio.h>

// Use random seed to prevent Hash-DoS
uint64 g_hash_seed = 0;

void InitSecureHash() {
    g_hash_seed = xrtRand64();
}

uint32 SecureHash(str key, size_t len) {
    return xrtHash32_WithSeed(key, len, (uint32)g_hash_seed);
}

int main() {
    xrtInit();
    InitSecureHash();
    
    str user_input = (str)"user_provided_key";
    uint32 hash = SecureHash(user_input, strlen((char*)user_input));
    printf("Secure hash: %u\n", hash);
    
    xrtUnit();
    return 0;
}
```

### 3. Avoid Redundant Computation

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str key;
    size_t key_len;
    uint32 hash;  // Cache hash value
    int value;
} CachedEntry;

int main() {
    xrtInit();
    
    // ✓ Calculate once at creation, reuse later
    CachedEntry entry;
    entry.key = (str)"my_key";
    entry.key_len = strlen((char*)entry.key);
    entry.hash = xrtHash32(entry.key, entry.key_len);
    entry.value = 42;
    
    printf("Cached hash: %u, value: %d\n", entry.hash, entry.value);
    
    // ✗ Don't recalculate on every lookup
    // for (int i = 0; i < 1000; i++) {
    //     uint32 h = xrtHash32(entry.key, entry.key_len);  // Wasteful
    // }
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Dict Dictionary](api-dict.en.md) - Dictionary implementation using hash
- [Value Dynamic Type](api-value.en.md) - Table type uses hash
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#hash-computing-library)

</div>
