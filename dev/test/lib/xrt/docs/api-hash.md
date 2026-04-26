# Hash 哈希计算库

> 高性能哈希函数库（基于 nmhash32x 和 rapidhash 算法）

[English](api-hash.en.md) | [中文](api-hash.md) | [返回索引](README.md)

---

## 📑 目录

- [算法介绍](#算法介绍)
- [常量定义](#常量定义)
- [32位哈希](#32位哈希)
- [64位哈希](#64位哈希)
- [使用场景](#使用场景)
- [性能比较](#性能比较)
- [最佳实践](#最佳实践)

---

## 算法介绍

### nmhash32x（32位哈希）

- **来源**：[smhasher](https://github.com/rurban/smhasher) 项目
- **版本**：Ver 2.0
- **特点**：
  - 高质量 32 位哈希
  - 支持 SIMD 加速（SSE2/AVX2/AVX512）
  - 适用于哈希表、数据校验等场景
- **许可证**：BSD 2-Clause

### rapidhash（64位哈希）

- **来源**：[rapidhash](https://github.com/Nicoshev/rapidhash) 项目
- **版本**：Ver 3.0
- **特点**：
  - 极高性能的 64 位哈希
  - 基于 wyhash 算法优化
  - 提供三种变体：标准版、Micro、Nano
- **许可证**：BSD 2-Clause

---

## 常量定义

### HASH32_SEED

32位哈希默认种子。

**定义：**
```c
#define HASH32_SEED 0
```

**说明：**
- `xrtHash32` 内部使用此值作为默认种子
- 可通过 `xrtHash32_WithSeed` 指定自定义种子

---

## 32位哈希

### xrtHash32

使用默认种子计算32位哈希值。

**函数原型：**
```c
XXAPI uint32 xrtHash32(ptr key, size_t len);
```

**参数：**
- `key` - 数据指针
- `len` - 数据长度（字节）

**返回值：**
- 32位哈希值（0 ~ 4294967295）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 计算字符串哈希
    str text = (str)"Hello, World!";
    uint32 hash = xrtHash32(text, strlen((char*)text));
    printf("Hash32: %u\n", hash);
    
    // 计算二进制数据哈希
    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32 hash2 = xrtHash32(data, sizeof(data));
    printf("Binary Hash32: %u\n", hash2);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部使用 nmhash32x 算法
- 相同输入始终返回相同结果
- 零长度数据也会返回有效哈希值

---

### xrtHash32_WithSeed

使用自定义种子计算32位哈希值。

**函数原型：**
```c
XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed);
```

**参数：**
- `key` - 数据指针
- `len` - 数据长度（字节）
- `seed` - 自定义种子

**返回值：**
- 32位哈希值

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello";
    size_t len = strlen((char*)text);
    
    // 使用不同种子产生不同哈希
    uint32 hash1 = xrtHash32_WithSeed(text, len, 0);
    uint32 hash2 = xrtHash32_WithSeed(text, len, 12345);
    uint32 hash3 = xrtHash32_WithSeed(text, len, 99999);
    
    printf("Seed 0:     %u\n", hash1);
    printf("Seed 12345: %u\n", hash2);
    printf("Seed 99999: %u\n", hash3);
    
    // 验证：相同种子产生相同结果
    uint32 hash1_again = xrtHash32_WithSeed(text, len, 0);
    printf("Same: %s\n", hash1 == hash1_again ? "YES" : "NO");
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 不同种子对相同数据会产生不同哈希值
- 可用于哈希表抗碰撞攻击（Hash-DoS 防护）
- 可用于生成多个独立哈希（布隆过滤器等）

---

## 64位哈希

### xrtHash64

使用默认种子计算64位哈希值（标准版）。

**函数原型：**
```c
XXAPI uint64 xrtHash64(ptr key, size_t len);
```

**参数：**
- `key` - 数据指针
- `len` - 数据长度（字节）

**返回值：**
- 64位哈希值

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello, World!";
    uint64 hash = xrtHash64(text, strlen((char*)text));
    printf("Hash64: %" PRIu64 "\n", hash);
    
    // 计算大数据哈希
    ptr data = xrtMalloc(1024 * 1024);  // 1MB
    memset(data, 0xAB, 1024 * 1024);
    uint64 hash2 = xrtHash64(data, 1024 * 1024);
    printf("1MB data hash: %" PRIu64 "\n", hash2);
    xrtFree(data);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部使用 rapidhash 算法
- 适用于大数据量哈希计算
- 提供最高质量和最好的分布性

---

### xrtHash64_WithSeed

使用自定义种子计算64位哈希值。

**函数原型：**
```c
XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed);
```

**参数：**
- `key` - 数据指针
- `len` - 数据长度（字节）
- `seed` - 64位自定义种子

**返回值：**
- 64位哈希值

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Test String";
    size_t len = strlen((char*)text);
    
    // 不同种子产生不同哈希
    uint64 hash1 = xrtHash64_WithSeed(text, len, 0);
    uint64 hash2 = xrtHash64_WithSeed(text, len, 0x123456789ABCDEFULL);
    
    printf("Seed 0: %" PRIu64 "\n", hash1);
    printf("Custom: %" PRIu64 "\n", hash2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtHash64_Micro / xrtHash64_Micro_WithSeed

微型版本 64 位哈希（适合服务器应用）。

**函数原型：**
```c
XXAPI uint64 xrtHash64_Micro(ptr key, size_t len);
XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed);
```

**参数：**
- `key` - 数据指针
- `len` - 数据长度（字节）
- `seed` - 自定义种子（可选）

**返回值：**
- 64位哈希值

**特点：**
- 针对 HPC 和服务器应用优化
- 编译后约 140 条指令，无栈使用
- 对 512 字节以下数据最快
- 对 1KB 以上数据比标准版慢 15-20%

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 适合处理小数据包（网络协议、序列化等）
    uint8 packet[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint64 hash = xrtHash64_Micro(packet, sizeof(packet));
    printf("Packet hash: %" PRIu64 "\n", hash);
    
    // 使用种子
    uint64 hash2 = xrtHash64_Micro_WithSeed(packet, sizeof(packet), 12345);
    printf("With seed: %" PRIu64 "\n", hash2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtHash64_Nano / xrtHash64_Nano_WithSeed

超小版本 64 位哈希（适合嵌入式和移动应用）。

**函数原型：**
```c
XXAPI uint64 xrtHash64_Nano(ptr key, size_t len);
XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed);
```

**参数：**
- `key` - 数据指针
- `len` - 数据长度（字节）
- `seed` - 自定义种子（可选）

**返回值：**
- 64位哈希值

**特点：**
- 针对嵌入式和移动应用优化
- 编译后不到 100 条指令，无栈使用
- 对 48 字节以下数据最快
- 对大数据可能明显变慢

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 适合处理短字符串（键名、ID等）
    str key = (str)"user_id_12345";
    uint64 hash = xrtHash64_Nano(key, strlen((char*)key));
    printf("Key hash: %" PRIu64 "\n", hash);
    
    // 使用种子
    uint64 hash2 = xrtHash64_Nano_WithSeed(key, strlen((char*)key), 0xABCDEF);
    printf("With seed: %" PRIu64 "\n", hash2);
    
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 哈希表键查找

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

### 2. 文件完整性校验

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

### 3. 布隆过滤器（多哈希）

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
            return 0;  // 肯定不存在
        }
    }
    return 1;  // 可能存在
}

int main() {
    xrtInit();
    
    // 添加元素
    BloomAdd((str)"apple");
    BloomAdd((str)"banana");
    BloomAdd((str)"cherry");
    
    // 检查
    printf("apple: %s\n", BloomCheck((str)"apple") ? "probably yes" : "no");
    printf("grape: %s\n", BloomCheck((str)"grape") ? "probably yes" : "no");
    
    xrtUnit();
    return 0;
}
```

---

### 4. 缓存键生成

```c
#include "xrt.h"
#include <stdio.h>

str GenerateCacheKey(str prefix, str params) {
    size_t prefix_len = strlen((char*)prefix);
    size_t params_len = strlen((char*)params);
    
    // 用参数哈希生成缓存键
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

## 性能比较

### 64位哈希变体比较

| 变体 | 指令数 | 最佳数据大小 | 适用场景 |
|------|--------|------------|----------|
| **xrtHash64** | ~200+ | 1KB+ | 通用场景，大数据量 |
| **xrtHash64_Micro** | ~140 | 16-512B | HPC、服务器、网络包 |
| **xrtHash64_Nano** | <100 | 1-48B | 嵌入式、移动端、短字符串 |

### 选择建议

```c
#include "xrt.h"
#include <stdio.h>

uint64 SmartHash(ptr data, size_t len) {
    if (len <= 48) {
        return xrtHash64_Nano(data, len);   // 超小数据
    } else if (len <= 512) {
        return xrtHash64_Micro(data, len);  // 中等数据
    } else {
        return xrtHash64(data, len);        // 大数据
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

## 最佳实践

### 1. 稳定哈希（序列化/缓存）

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✓ 使用固定种子保证跨进程/跨机器一致性
    uint64 hash = xrtHash64_WithSeed((str)"data", 4, 0);
    printf("Stable hash: %" PRIu64 "\n", hash);
    
    // ✗ 不要使用随机种子做缓存键
    // uint64 bad = xrtHash64_WithSeed("data", 4, xrtRand64());
    
    xrtUnit();
    return 0;
}
```

### 2. 安全哈希（防碰撞攻击）

```c
#include "xrt.h"
#include <stdio.h>

// 使用随机种子防止 Hash-DoS
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

### 3. 避免重复计算

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str key;
    size_t key_len;
    uint32 hash;  // 缓存哈希值
    int value;
} CachedEntry;

int main() {
    xrtInit();
    
    // ✓ 创建时计算一次，后续复用
    CachedEntry entry;
    entry.key = (str)"my_key";
    entry.key_len = strlen((char*)entry.key);
    entry.hash = xrtHash32(entry.key, entry.key_len);
    entry.value = 42;
    
    printf("Cached hash: %u, value: %d\n", entry.hash, entry.value);
    
    // ✗ 不要每次查找都重新计算
    // for (int i = 0; i < 1000; i++) {
    //     uint32 h = xrtHash32(entry.key, entry.key_len);  // 浪费
    // }
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Dict 字典](api-dict.md) - 使用哈希的字典实现
- [Value 动态类型](api-value.md) - Table 类型使用哈希
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#hash-哈希计算库)

</div>
