# Math 数学运算库

> 随机数生成和数学运算功能（基于 PCG 算法）

[English](api-math.en.md) | [中文](api-math.md) | [返回索引](README.md)

---

## 📑 目录

- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [随机数生成](#随机数生成)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)
- [性能提示](#性能提示)
- [算法质量](#算法质量)
- [常见错误](#常见错误)

---

## 数据类型

### pcg32_random_t

PCG 算法随机数生成器状态结构（内部使用）。

**定义：**
```c
struct pcg_state_setseq_64 {
    uint64_t state;  // RNG 状态，所有值都有效
    uint64_t inc;    // 控制 RNG 序列（流），必须始终为奇数
};
typedef struct pcg_state_setseq_64 pcg32_random_t;
```

**说明：**
- 这是 PCG 算法的内部状态结构
- 通常不需要直接操作，由 `xrtSetRandSeed32` 等函数管理
- 库内部维护全局实例用于 `xrtRand32`/`xrtRand64`

---

## 常量定义

### PCG32_INITIALIZER

静态初始化 PCG 随机数生成器的推荐值。

**定义：**
```c
#define PCG32_INITIALIZER   { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
```

**说明：**
- 如果需要静态初始化随机数生成器，推荐使用此值
- 通常不需要直接使用，`xrtInit()` 会自动初始化

---

## 随机数生成

### xrtRand32

生成32位随机数。

**函数原型：**
```c
XXAPI uint32 xrtRand32();
```

**返回值：**
- 返回 0 ~ 4294967295 (2^32-1) 范围的随机数

**说明：**
- 使用 PCG32 算法（高质量伪随机数生成器）
- 自动在 `xrtInit()` 中初始化种子
- 线程不安全（多线程需要同步）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 生成单个随机数
    uint32 random = xrtRand32();
    printf("Random: %u\n", random);
    
    // 生成多个随机数
    printf("10 random numbers: ");
    for (int i = 0; i < 10; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetRandSeed32

设置32位随机数种子。

**函数原型：**
```c
XXAPI void xrtSetRandSeed32(uint64 seed, uint64 seq);
```

**参数：**
- `seed` - 随机数种子
- `seq` - 序列号（用于生成不同序列，必须为奇数）

**说明：**
- 相同的种子和序列产生相同的随机数序列
- `xrtInit()` 自动使用时间和 CPU 时钟初始化
- 可用于可重现的随机序列（测试、调试）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

int main() {
    xrtInit();
    
    // 使用固定种子（可重现）
    xrtSetRandSeed32(12345, 1);
    printf("固定种子结果: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    // 重置种子，结果相同
    xrtSetRandSeed32(12345, 1);
    printf("重置后结果: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    // 使用时间作为种子（不可重现）
    xrtSetRandSeed32((uint64)time(NULL), 1);
    printf("时间种子结果: %u\n", xrtRand32());
    
    // 不同序列号产生不同结果
    xrtSetRandSeed32(12345, 1);  // 序列A
    printf("序列A: %u\n", xrtRand32());
    xrtSetRandSeed32(12345, 3);  // 序列B（序列号不同）
    printf("序列B: %u\n", xrtRand32());
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `seq` 参数内部会被转换为奇数（`(seq << 1) | 1`）
- 不同的 `seq` 值会产生完全不同的随机数序列

---

### xrtRand64

生成64位随机数。

**函数原型：**
```c
XXAPI uint64 xrtRand64();
```

**返回值：**
- 返回 0 ~ 18446744073709551615 (2^64-1) 范围的随机数

**说明：**
- 内部通过组合两个 32 位随机数实现（高位 + 低位）
- 自动在 `xrtInit()` 中初始化
- 适用于需要更大范围的场景（如生成唯一ID）
- 线程不安全

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 生成64位随机数
    uint64 random = xrtRand64();
    printf("Random64: %" PRIu64 "\n", random);
    
    // 生成大范围随机ID
    for (int i = 0; i < 5; i++) {
        uint64 id = xrtRand64();
        printf("ID %d: 0x%016" PRIX64 "\n", i, id);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部实现：`((uint64)高位32 << 32) | (uint64)低位32`
- 使用两个独立的 PCG32 生成器，保证随机性

---

### xrtSetRandSeed64

设置64位随机数种子。

**函数原型：**
```c
XXAPI void xrtSetRandSeed64(
    uint64 lowseed, 
    uint64 lowseq,
    uint64 highseed, 
    uint64 highseq
);
```

**参数：**
- `lowseed` - 低位种子
- `lowseq` - 低位序列号
- `highseed` - 高位种子
- `highseq` - 高位序列号

**说明：**
- 64位随机数由两个独立的 32 位生成器组合，因此需要4个参数
- `xrtInit()` 自动使用复杂算法初始化

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 自定义64位种子
    xrtSetRandSeed64(
        0x123456789ABCDEF0ULL, 1,   // 低位种子和序列
        0xFEDCBA9876543210ULL, 1    // 高位种子和序列
    );
    
    // 生成可重现的 64 位随机数
    printf("可重现的随机数: ");
    for (int i = 0; i < 3; i++) {
        printf("0x%016" PRIX64 " ", xrtRand64());
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 序列号内部同样会被转换为奇数
- 四个参数完全相同时才会产生相同的随机序列

---

### xrtRandRange

生成指定范围的随机整数。

**函数原型：**
```c
XXAPI int xrtRandRange(int min, int max);
```

**参数：**
- `min` - 最小值（包含）
- `max` - 最大值（包含）

**返回值：**
- 返回 [min, max] 范围内的随机整数
- 如果 `min == max`，返回 0

**说明：**
- 基于 `xrtRand32()` 实现，使用无偏差算法
- **包含**最大值和最小值
- 支持负数范围
- 如果 `min > max`，会自动交换

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 生成 1-100 的随机数
    int value = xrtRandRange(1, 100);
    printf("1-100: %d\n", value);
    
    // 生成 0-9 的随机数字
    int digit = xrtRandRange(0, 9);
    printf("0-9: %d\n", digit);
    
    // 生成负数范围
    int signed_val = xrtRandRange(-10, 10);
    printf("-10 to 10: %d\n", signed_val);
    
    // 骰子模拟（6面骰）
    printf("掷骰子 10 次: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", xrtRandRange(1, 6));
    }
    printf("\n");
    
    // min > max 时自动交换
    int swapped = xrtRandRange(100, 1);  // 等价于 xrtRandRange(1, 100)
    printf("自动交换: %d\n", swapped);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部使用 `pcg32_boundedrand_r` 函数，避免模运算偏差
- 平均情况下 82.25% 的调用只需一次循环

---

## 使用场景

### 1. 随机数组生成

```c
#include "xrt.h"
#include <stdio.h>

// 生成随机整数数组
void GenerateRandomArray(int* arr, int count, int min, int max) {
    for (int i = 0; i < count; i++) {
        arr[i] = xrtRandRange(min, max);
    }
}

int main() {
    xrtInit();
    
    int numbers[20];
    GenerateRandomArray(numbers, 20, 1, 100);
    
    printf("20个 1-100 随机数:\n");
    for (int i = 0; i < 20; i++) {
        printf("%3d ", numbers[i]);
        if ((i + 1) % 10 == 0) printf("\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. 随机字符串生成

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// 生成随机字符串
str GenerateRandomString(int length) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int charset_size = (int)strlen(chars);
    
    str result = xrtMalloc(length + 1);
    for (int i = 0; i < length; i++) {
        int idx = xrtRandRange(0, charset_size - 1);
        result[i] = chars[idx];
    }
    result[length] = '\0';
    
    return result;
}

int main() {
    xrtInit();
    
    // 生成8位随机密码
    str password = GenerateRandomString(8);
    printf("随机密码: %s\n", password);
    xrtFree(password);
    
    // 生成16位随机令牌
    str token = GenerateRandomString(16);
    printf("随机令牌: %s\n", token);
    xrtFree(token);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 数组随机打乱

```c
#include "xrt.h"
#include <stdio.h>

// Fisher-Yates 洗牌算法
void ShuffleArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        // 交换 arr[i] 和 arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

int main() {
    xrtInit();
    
    // 初始化扑克牌（52张）
    int cards[52];
    for (int i = 0; i < 52; i++) {
        cards[i] = i + 1;
    }
    
    printf("洗牌前: ");
    for (int i = 0; i < 10; i++) printf("%d ", cards[i]);
    printf("...\n");
    
    // 洗牌
    ShuffleArray(cards, 52);
    
    printf("洗牌后: ");
    for (int i = 0; i < 10; i++) printf("%d ", cards[i]);
    printf("...\n");
    
    xrtUnit();
    return 0;
}
```

---

### 4. 概率事件

```c
#include "xrt.h"
#include <stdio.h>

// 按概率返回 TRUE/FALSE
bool RandomChance(double probability) {
    // probability: 0.0 ~ 1.0
    uint32 threshold = (uint32)(probability * 0xFFFFFFFF);
    return xrtRand32() < threshold;
}

// 模拟抽奖
bool IsWinner(double win_rate) {
    return RandomChance(win_rate);
}

int main() {
    xrtInit();
    
    // 测试 30% 概率
    int triggered = 0;
    int total = 10000;
    for (int i = 0; i < total; i++) {
        if (RandomChance(0.3)) {
            triggered++;
        }
    }
    printf("30%% 概率测试: %d/%d = %.2f%%\n", 
           triggered, total, (double)triggered / total * 100);
    
    // 模拟抽奖 (1% 中奖率)
    int wins = 0;
    for (int i = 0; i < 10000; i++) {
        if (IsWinner(0.01)) {
            wins++;
        }
    }
    printf("1%% 中奖率: 抽奉10000次，中奖 %d 次\n", wins);
    
    xrtUnit();
    return 0;
}
```

---

### 5. 随机采样

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// Fisher-Yates 洗牌
void ShuffleIntArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

// 从数组中随机选择 N 个元素
void RandomSample(int* source, int src_count, int* dest, int sample_count) {
    // 复制源数组
    int* temp = xrtMalloc(src_count * sizeof(int));
    memcpy(temp, source, src_count * sizeof(int));
    
    // 洗牌
    ShuffleIntArray(temp, src_count);
    
    // 取前 N 个
    memcpy(dest, temp, sample_count * sizeof(int));
    xrtFree(temp);
}

int main() {
    xrtInit();
    
    // 源数据: 1-20
    int source[20];
    for (int i = 0; i < 20; i++) source[i] = i + 1;
    
    // 随机采样 5 个
    int sample[5];
    RandomSample(source, 20, sample, 5);
    
    printf("从 1-20 中随机采样 5 个: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", sample[i]);
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### 6. 随机延迟

```c
#include "xrt.h"
#include <stdio.h>

// 随机延迟（防止同步）
void RandomDelay(int min_ms, int max_ms) {
    int delay = xrtRandRange(min_ms, max_ms);
    printf("延迟 %d 毫秒...\n", delay);
    xrtSleep(delay);
}

int main() {
    xrtInit();
    
    printf("模拟网络请求随机延迟:\n");
    for (int i = 0; i < 3; i++) {
        printf("请求 %d: ", i + 1);
        RandomDelay(100, 500);  // 100-500ms 随机延迟
    }
    
    xrtUnit();
    return 0;
}
```

---

### 7. UUID/GUID 生成

```c
#include "xrt.h"
#include <stdio.h>

// 简化版 UUID 生成（非标准，仅用于演示）
str GenerateSimpleUUID() {
    return xrtFormat(
        "%08X-%04X-%04X-%04X-%012" PRIX64,
        xrtRand32(),
        xrtRand32() & 0xFFFF,
        xrtRand32() & 0xFFFF,
        xrtRand32() & 0xFFFF,
        xrtRand64() & 0xFFFFFFFFFFFFULL
    );
}

int main() {
    xrtInit();
    
    printf("生成 5 个随机 UUID:\n");
    for (int i = 0; i < 5; i++) {
        str uuid = GenerateSimpleUUID();
        printf("  %s\n", uuid);
        xrtFree(uuid);
    }
    
    xrtUnit();
    return 0;
}
```

**注意：** 如需标准 UUID，请使用 [XID 分布式ID模块](api-xid.md)。

---

## 最佳实践

### 1. 初始化种子

```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

#if !defined(_WIN32) && !defined(_WIN64)
    #include <unistd.h>
    #define GET_PID() getpid()
#else
    #include <process.h>
    #define GET_PID() _getpid()
#endif

int main() {
    xrtInit();  // 自动初始化随机数种子
    
    // 如需自定义种子（结合时间和进程ID）
    xrtSetRandSeed32((uint64)time(NULL), (uint64)GET_PID());
    
    printf("随机数: %u\n", xrtRand32());
    
    xrtUnit();
    return 0;
}
```

---

### 2. 可重现的随机序列

```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

int main() {
    xrtInit();
    
#ifdef DEBUG
    // 测试环境：使用固定种子，结果可重现
    xrtSetRandSeed32(12345, 1);
    printf("调试模式: 使用固定种子\n");
#else
    // 生产环境：使用时间种子
    xrtSetRandSeed32((uint64)time(NULL), 1);
    printf("生产模式: 使用时间种子\n");
#endif
    
    printf("随机序列: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### 3. 避免模运算偏差

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ 有偏差的方法
    // int bad_random = xrtRand32() % 100;  // 0-99，有轻微偏差
    
    // ✓ 无偏差的方法
    int good_random = xrtRandRange(0, 99);  // 内部处理偏差
    printf("无偏差随机数 (0-99): %d\n", good_random);
    
    // 统计分布验证
    int counts[10] = {0};
    for (int i = 0; i < 100000; i++) {
        int r = xrtRandRange(0, 9);
        counts[r]++;
    }
    
    printf("\n分布统计 (100000次, 理想值每个10000):\n");
    for (int i = 0; i < 10; i++) {
        printf("  %d: %d\n", i, counts[i]);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. 浮点随机数

```c
#include "xrt.h"
#include <stdio.h>

// 生成 [0.0, 1.0) 的浮点随机数
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// 生成 [min, max) 的浮点随机数
double RandomDoubleRange(double min, double max) {
    return min + (max - min) * RandomDouble();
}

int main() {
    xrtInit();
    
    printf("10个 [0, 1) 随机浮点数:\n");
    for (int i = 0; i < 10; i++) {
        printf("  %.6f\n", RandomDouble());
    }
    
    printf("\n5个 [10.0, 100.0) 随机价格:\n");
    for (int i = 0; i < 5; i++) {
        double price = RandomDoubleRange(10.0, 100.0);
        printf("  $%.2f\n", price);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 5. 多线程安全

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    static CRITICAL_SECTION rand_mutex;
    #define MUTEX_INIT()    InitializeCriticalSection(&rand_mutex)
    #define MUTEX_LOCK()    EnterCriticalSection(&rand_mutex)
    #define MUTEX_UNLOCK()  LeaveCriticalSection(&rand_mutex)
    #define MUTEX_DESTROY() DeleteCriticalSection(&rand_mutex)
#else
    #include <pthread.h>
    static pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;
    #define MUTEX_INIT()    pthread_mutex_init(&rand_mutex, NULL)
    #define MUTEX_LOCK()    pthread_mutex_lock(&rand_mutex)
    #define MUTEX_UNLOCK()  pthread_mutex_unlock(&rand_mutex)
    #define MUTEX_DESTROY() pthread_mutex_destroy(&rand_mutex)
#endif

// 线程安全的随机数生成
uint32 ThreadSafeRand32() {
    MUTEX_LOCK();
    uint32 result = xrtRand32();
    MUTEX_UNLOCK();
    return result;
}

int main() {
    xrtInit();
    MUTEX_INIT();
    
    // 在多线程环境中安全使用
    printf("线程安全随机数: %u\n", ThreadSafeRand32());
    
    MUTEX_DESTROY();
    xrtUnit();
    return 0;
}
```

---

### 6. 权重随机选择

```c
#include "xrt.h"
#include <stdio.h>

// 生成 [0.0, 1.0) 的浮点随机数
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// 根据权重随机选择
int WeightedRandom(int* items, double* weights, int count) {
    double total = 0;
    for (int i = 0; i < count; i++) {
        total += weights[i];
    }
    
    double random = RandomDouble() * total;
    double cumulative = 0;
    
    for (int i = 0; i < count; i++) {
        cumulative += weights[i];
        if (random < cumulative) {
            return items[i];
        }
    }
    
    return items[count - 1];
}

int main() {
    xrtInit();
    
    // 物品和权重
    int items[] = {1, 2, 3, 4};
    double weights[] = {0.1, 0.2, 0.3, 0.4};  // 10%, 20%, 30%, 40%
    
    // 统计选中次数
    int counts[5] = {0};
    for (int i = 0; i < 10000; i++) {
        int selected = WeightedRandom(items, weights, 4);
        counts[selected]++;
    }
    
    printf("权重随机选择结果 (10000次):\n");
    for (int i = 1; i <= 4; i++) {
        printf("  物品%d: %d次 (%.1f%%)\n", i, counts[i], counts[i] / 100.0);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 性能提示

### 1. 批量生成

```c
#include "xrt.h"
#include <stdio.h>

void Process(int value) {
    // 处理随机数...
}

int main() {
    xrtInit();
    
    int count = 100000;
    
    // ✗ 低效：生成和处理交替
    printf("方式1: 生成和处理交替\n");
    for (int i = 0; i < count; i++) {
        int r = xrtRandRange(1, 100);
        Process(r);
    }
    
    // ✓ 高效：预生成所有随机数
    printf("方式2: 预生成所有随机数\n");
    int* randoms = xrtMalloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        randoms[i] = xrtRandRange(1, 100);
    }
    for (int i = 0; i < count; i++) {
        Process(randoms[i]);
    }
    xrtFree(randoms);
    
    printf("完成\n");
    xrtUnit();
    return 0;
}
```

---

### 2. 避免重复初始化

```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

// ✗ 错误：每次都重置种子
uint32 BadRandom() {
    xrtSetRandSeed32((uint64)time(NULL), 1);  // 同一秒内结果相同！
    return xrtRand32();
}

// ✓ 正确：只在开始时初始化一次
void Setup() {
    xrtSetRandSeed32((uint64)time(NULL), 1);
}

uint32 GoodRandom() {
    return xrtRand32();  // 每次调用结果不同
}

int main() {
    xrtInit();
    
    // 演示错误用法：同一秒内结果相同
    printf("错误用法 (同一秒内结果相同):\n");
    for (int i = 0; i < 3; i++) {
        printf("  %u\n", BadRandom());
    }
    
    // 演示正确用法
    printf("\n正确用法 (每次不同):\n");
    Setup();
    for (int i = 0; i < 3; i++) {
        printf("  %u\n", GoodRandom());
    }
    
    xrtUnit();
    return 0;
}
```

---

## 算法质量

### PCG 随机数生成器

XRT 使用 **PCG (Permuted Congruential Generator)** 算法：

**优点：**
- ✅ 高质量随机性
- ✅ 速度快（比标准 `rand()` 快）
- ✅ 周期长（PCG32: 2^64, PCG64: 2^128）
- ✅ 通过多项随机性测试（TestU01）

**对比：**
- 比 `rand()` 质量更高
- 比 `MT19937` 更快
- 适合游戏、模拟、测试等场景

---

## 常见错误

### 1. 忘记初始化

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    // ✗ 错误：未初始化
    // int r = xrtRand32();  // 可能结果不理想
    
    // ✓ 正确：先初始化
    xrtInit();  // 初始化种子
    int r = xrtRand32();
    printf("随机数: %u\n", r);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 范围理解错误

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // xrtRandRange 包含两端
    printf("模拟掷骰子 (1-6):\n");
    for (int i = 0; i < 10; i++) {
        int r = xrtRandRange(1, 6);  // 可能是 1,2,3,4,5,6
        printf("%d ", r);
    }
    printf("\n");
    
    // 如需 [1, 6) 范围（不包含6）
    printf("\n范围 [1, 6) 不包含6:\n");
    for (int i = 0; i < 10; i++) {
        int r = xrtRandRange(1, 5);  // 1,2,3,4,5
        printf("%d ", r);
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Base 基础函数库](api-base.md) - 库初始化
- [String 字符串处理](api-string.md) - 随机字符串生成
- [Time 时间处理](api-time.md) - 时间种子
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#math-数学运算库)

</div>
