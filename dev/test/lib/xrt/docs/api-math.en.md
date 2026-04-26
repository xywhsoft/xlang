# Math - Mathematics Library

> Random number generation and mathematical operations (based on PCG algorithm)

[English](api-math.en.md) | [中文](api-math.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Data Types](#data-types)
- [Constants](#constants)
- [Random Number Generation](#random-number-generation)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)
- [Performance Tips](#performance-tips)
- [Algorithm Quality](#algorithm-quality)
- [Common Errors](#common-errors)

---

## Data Types

### pcg32_random_t

PCG algorithm random number generator state structure (internal use).

**Definition:**
```c
struct pcg_state_setseq_64 {
    uint64_t state;  // RNG state, all values are valid
    uint64_t inc;    // Controls RNG sequence (stream), must always be odd
};
typedef struct pcg_state_setseq_64 pcg32_random_t;
```

**Notes:**
- This is the internal state structure of the PCG algorithm
- Usually no need to operate directly, managed by `xrtSetRandSeed32` etc.
- Library maintains global instance for `xrtRand32`/`xrtRand64`

---

## Constants

### PCG32_INITIALIZER

Recommended value for static initialization of PCG random number generator.

**Definition:**
```c
#define PCG32_INITIALIZER   { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
```

**Notes:**
- Use this value if you need to statically initialize random number generator
- Usually not needed directly, `xrtInit()` auto-initializes

---

## Random Number Generation

### xrtRand32

Generate a 32-bit random number.

**Function Prototype:**
```c
XXAPI uint32 xrtRand32();
```

**Return Value:**
- Returns a random number in range 0 ~ 4294967295 (2^32-1)

**Notes:**
- Uses PCG32 algorithm (high-quality pseudo-random number generator)
- Automatically initialized in `xrtInit()`
- Not thread-safe (needs synchronization for multi-threading)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate single random number
    uint32 random = xrtRand32();
    printf("Random: %u\n", random);
    
    // Generate multiple random numbers
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

Set 32-bit random number seed.

**Function Prototype:**
```c
XXAPI void xrtSetRandSeed32(uint64 seed, uint64 seq);
```

**Parameters:**
- `seed` - Random number seed
- `seq` - Sequence number (for generating different sequences, must be odd)

**Notes:**
- Same seed and sequence produce same random number sequence
- `xrtInit()` auto-initializes using time and CPU clock
- Can be used for reproducible random sequences (testing, debugging)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

int main() {
    xrtInit();
    
    // Use fixed seed (reproducible)
    xrtSetRandSeed32(12345, 1);
    printf("Fixed seed result: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    // Reset seed, same result
    xrtSetRandSeed32(12345, 1);
    printf("After reset result: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    // Use time as seed (non-reproducible)
    xrtSetRandSeed32((uint64)time(NULL), 1);
    printf("Time seed result: %u\n", xrtRand32());
    
    // Different sequence numbers produce different results
    xrtSetRandSeed32(12345, 1);  // Sequence A
    printf("Sequence A: %u\n", xrtRand32());
    xrtSetRandSeed32(12345, 3);  // Sequence B (different sequence number)
    printf("Sequence B: %u\n", xrtRand32());
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `seq` parameter is internally converted to odd (`(seq << 1) | 1`)
- Different `seq` values produce completely different random number sequences

---

### xrtRand64

Generate a 64-bit random number.

**Function Prototype:**
```c
XXAPI uint64 xrtRand64();
```

**Return Value:**
- Returns a random number in range 0 ~ 18446744073709551615 (2^64-1)

**Notes:**
- Internally implemented by combining two 32-bit random numbers (high + low)
- Auto-initialized in `xrtInit()`
- Suitable for scenarios needing larger range (e.g., generating unique IDs)
- Not thread-safe

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate 64-bit random number
    uint64 random = xrtRand64();
    printf("Random64: %" PRIu64 "\n", random);
    
    // Generate large range random IDs
    for (int i = 0; i < 5; i++) {
        uint64 id = xrtRand64();
        printf("ID %d: 0x%016" PRIX64 "\n", i, id);
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internal implementation: `((uint64)high32 << 32) | (uint64)low32`
- Uses two independent PCG32 generators to ensure randomness

---

### xrtSetRandSeed64

Set 64-bit random number seed.

**Function Prototype:**
```c
XXAPI void xrtSetRandSeed64(
    uint64 lowseed, 
    uint64 lowseq,
    uint64 highseed, 
    uint64 highseq
);
```

**Parameters:**
- `lowseed` - Low-bits seed
- `lowseq` - Low-bits sequence number
- `highseed` - High-bits seed
- `highseq` - High-bits sequence number

**Notes:**
- 64-bit random numbers are combined from two independent 32-bit generators, so 4 parameters needed
- `xrtInit()` auto-initializes using complex algorithm

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Custom 64-bit seed
    xrtSetRandSeed64(
        0x123456789ABCDEF0ULL, 1,   // Low-bits seed and sequence
        0xFEDCBA9876543210ULL, 1    // High-bits seed and sequence
    );
    
    // Generate reproducible 64-bit random numbers
    printf("Reproducible random numbers: ");
    for (int i = 0; i < 3; i++) {
        printf("0x%016" PRIX64 " ", xrtRand64());
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Sequence numbers are also internally converted to odd
- All four parameters must be same to produce same random sequence

---

### xrtRandRange

Generate a random integer within specified range.

**Function Prototype:**
```c
XXAPI int xrtRandRange(int min, int max);
```

**Parameters:**
- `min` - Minimum value (inclusive)
- `max` - Maximum value (inclusive)

**Return Value:**
- Returns a random integer in [min, max] range
- If `min == max`, returns 0

**Notes:**
- Based on `xrtRand32()` implementation, uses unbiased algorithm
- **Includes** both max and min values
- Supports negative ranges
- If `min > max`, automatically swaps

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate 1-100 random number
    int value = xrtRandRange(1, 100);
    printf("1-100: %d\n", value);
    
    // Generate 0-9 random digit
    int digit = xrtRandRange(0, 9);
    printf("0-9: %d\n", digit);
    
    // Generate negative range
    int signed_val = xrtRandRange(-10, 10);
    printf("-10 to 10: %d\n", signed_val);
    
    // Dice simulation (6-sided die)
    printf("Roll dice 10 times: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", xrtRandRange(1, 6));
    }
    printf("\n");
    
    // Auto swap when min > max
    int swapped = xrtRandRange(100, 1);  // Equivalent to xrtRandRange(1, 100)
    printf("Auto swapped: %d\n", swapped);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internally uses `pcg32_boundedrand_r` function, avoids modulo bias
- On average 82.25% of calls need only one loop iteration

---

## Usage Scenarios

### 1. Random Array Generation

```c
#include "xrt.h"
#include <stdio.h>

// Generate random integer array
void GenerateRandomArray(int* arr, int count, int min, int max) {
    for (int i = 0; i < count; i++) {
        arr[i] = xrtRandRange(min, max);
    }
}

int main() {
    xrtInit();
    
    int numbers[20];
    GenerateRandomArray(numbers, 20, 1, 100);
    
    printf("20 random numbers 1-100:\n");
    for (int i = 0; i < 20; i++) {
        printf("%3d ", numbers[i]);
        if ((i + 1) % 10 == 0) printf("\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. Random String Generation

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// Generate random string
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
    
    // Generate 8-character random password
    str password = GenerateRandomString(8);
    printf("Random password: %s\n", password);
    xrtFree(password);
    
    // Generate 16-character random token
    str token = GenerateRandomString(16);
    printf("Random token: %s\n", token);
    xrtFree(token);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Array Shuffle

```c
#include "xrt.h"
#include <stdio.h>

// Fisher-Yates shuffle algorithm
void ShuffleArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        // Swap arr[i] and arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

int main() {
    xrtInit();
    
    // Initialize deck (52 cards)
    int cards[52];
    for (int i = 0; i < 52; i++) {
        cards[i] = i + 1;
    }
    
    printf("Before shuffle: ");
    for (int i = 0; i < 10; i++) printf("%d ", cards[i]);
    printf("...\n");
    
    // Shuffle
    ShuffleArray(cards, 52);
    
    printf("After shuffle: ");
    for (int i = 0; i < 10; i++) printf("%d ", cards[i]);
    printf("...\n");
    
    xrtUnit();
    return 0;
}
```

---

### 4. Probability Events

```c
#include "xrt.h"
#include <stdio.h>

// Return TRUE/FALSE by probability
bool RandomChance(double probability) {
    // probability: 0.0 ~ 1.0
    uint32 threshold = (uint32)(probability * 0xFFFFFFFF);
    return xrtRand32() < threshold;
}

// Simulate lottery
bool IsWinner(double win_rate) {
    return RandomChance(win_rate);
}

int main() {
    xrtInit();
    
    // Test 30% probability
    int triggered = 0;
    int total = 10000;
    for (int i = 0; i < total; i++) {
        if (RandomChance(0.3)) {
            triggered++;
        }
    }
    printf("30%% probability test: %d/%d = %.2f%%\n", 
           triggered, total, (double)triggered / total * 100);
    
    // Simulate lottery (1% win rate)
    int wins = 0;
    for (int i = 0; i < 10000; i++) {
        if (IsWinner(0.01)) {
            wins++;
        }
    }
    printf("1%% win rate: 10000 draws, won %d times\n", wins);
    
    xrtUnit();
    return 0;
}
```

---

### 5. Random Sampling

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// Fisher-Yates shuffle
void ShuffleIntArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

// Randomly select N elements from array
void RandomSample(int* source, int src_count, int* dest, int sample_count) {
    // Copy source array
    int* temp = xrtMalloc(src_count * sizeof(int));
    memcpy(temp, source, src_count * sizeof(int));
    
    // Shuffle
    ShuffleIntArray(temp, src_count);
    
    // Take first N
    memcpy(dest, temp, sample_count * sizeof(int));
    xrtFree(temp);
}

int main() {
    xrtInit();
    
    // Source data: 1-20
    int source[20];
    for (int i = 0; i < 20; i++) source[i] = i + 1;
    
    // Randomly sample 5
    int sample[5];
    RandomSample(source, 20, sample, 5);
    
    printf("Randomly sample 5 from 1-20: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", sample[i]);
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### 6. Random Delay

```c
#include "xrt.h"
#include <stdio.h>

// Random delay (prevent synchronization)
void RandomDelay(int min_ms, int max_ms) {
    int delay = xrtRandRange(min_ms, max_ms);
    printf("Delay %d milliseconds...\n", delay);
    xrtSleep(delay);
}

int main() {
    xrtInit();
    
    printf("Simulate network request random delay:\n");
    for (int i = 0; i < 3; i++) {
        printf("Request %d: ", i + 1);
        RandomDelay(100, 500);  // 100-500ms random delay
    }
    
    xrtUnit();
    return 0;
}
```

---

### 7. UUID/GUID Generation

```c
#include "xrt.h"
#include <stdio.h>

// Simplified UUID generation (non-standard, for demonstration only)
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
    
    printf("Generate 5 random UUIDs:\n");
    for (int i = 0; i < 5; i++) {
        str uuid = GenerateSimpleUUID();
        printf("  %s\n", uuid);
        xrtFree(uuid);
    }
    
    xrtUnit();
    return 0;
}
```

**Note:** For standard UUIDs, use [XID Distributed ID Module](api-xid.en.md).

---

## Best Practices

### 1. Initialize Seed

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
    xrtInit();  // Auto-initialize random number seed
    
    // For custom seed (combine time and process ID)
    xrtSetRandSeed32((uint64)time(NULL), (uint64)GET_PID());
    
    printf("Random number: %u\n", xrtRand32());
    
    xrtUnit();
    return 0;
}
```

---

### 2. Reproducible Random Sequence

```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

int main() {
    xrtInit();
    
#ifdef DEBUG
    // Test environment: use fixed seed, reproducible results
    xrtSetRandSeed32(12345, 1);
    printf("Debug mode: using fixed seed\n");
#else
    // Production environment: use time seed
    xrtSetRandSeed32((uint64)time(NULL), 1);
    printf("Production mode: using time seed\n");
#endif
    
    printf("Random sequence: ");
    for (int i = 0; i < 5; i++) {
        printf("%u ", xrtRand32());
    }
    printf("\n");
    
    xrtUnit();
    return 0;
}
```

---

### 3. Avoid Modulo Bias

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ Biased method
    // int bad_random = xrtRand32() % 100;  // 0-99, slight bias
    
    // ✓ Unbiased method
    int good_random = xrtRandRange(0, 99);  // Internally handles bias
    printf("Unbiased random (0-99): %d\n", good_random);
    
    // Verify distribution
    int counts[10] = {0};
    for (int i = 0; i < 100000; i++) {
        int r = xrtRandRange(0, 9);
        counts[r]++;
    }
    
    printf("\nDistribution stats (100000 times, ideal 10000 each):\n");
    for (int i = 0; i < 10; i++) {
        printf("  %d: %d\n", i, counts[i]);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. Floating-Point Random Numbers

```c
#include "xrt.h"
#include <stdio.h>

// Generate [0.0, 1.0) floating-point random number
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// Generate [min, max) floating-point random number
double RandomDoubleRange(double min, double max) {
    return min + (max - min) * RandomDouble();
}

int main() {
    xrtInit();
    
    printf("10 [0, 1) random floats:\n");
    for (int i = 0; i < 10; i++) {
        printf("  %.6f\n", RandomDouble());
    }
    
    printf("\n5 [10.0, 100.0) random prices:\n");
    for (int i = 0; i < 5; i++) {
        double price = RandomDoubleRange(10.0, 100.0);
        printf("  $%.2f\n", price);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 5. Multi-Thread Safety

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

// Thread-safe random number generation
uint32 ThreadSafeRand32() {
    MUTEX_LOCK();
    uint32 result = xrtRand32();
    MUTEX_UNLOCK();
    return result;
}

int main() {
    xrtInit();
    MUTEX_INIT();
    
    // Safe to use in multi-threaded environment
    printf("Thread-safe random number: %u\n", ThreadSafeRand32());
    
    MUTEX_DESTROY();
    xrtUnit();
    return 0;
}
```

---

### 6. Weighted Random Selection

```c
#include "xrt.h"
#include <stdio.h>

// Generate [0.0, 1.0) floating-point random number
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// Random select by weight
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
    
    // Items and weights
    int items[] = {1, 2, 3, 4};
    double weights[] = {0.1, 0.2, 0.3, 0.4};  // 10%, 20%, 30%, 40%
    
    // Count selections
    int counts[5] = {0};
    for (int i = 0; i < 10000; i++) {
        int selected = WeightedRandom(items, weights, 4);
        counts[selected]++;
    }
    
    printf("Weighted random selection results (10000 times):\n");
    for (int i = 1; i <= 4; i++) {
        printf("  Item %d: %d times (%.1f%%)\n", i, counts[i], counts[i] / 100.0);
    }
    
    xrtUnit();
    return 0;
}
```

---

## Performance Tips

### 1. Batch Generation

```c
#include "xrt.h"
#include <stdio.h>

void Process(int value) {
    // Process random number...
}

int main() {
    xrtInit();
    
    int count = 100000;
    
    // ✗ Inefficient: generate and process alternating
    printf("Method 1: Generate and process alternating\n");
    for (int i = 0; i < count; i++) {
        int r = xrtRandRange(1, 100);
        Process(r);
    }
    
    // ✓ Efficient: pre-generate all random numbers
    printf("Method 2: Pre-generate all random numbers\n");
    int* randoms = xrtMalloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        randoms[i] = xrtRandRange(1, 100);
    }
    for (int i = 0; i < count; i++) {
        Process(randoms[i]);
    }
    xrtFree(randoms);
    
    printf("Done\n");
    xrtUnit();
    return 0;
}
```

---

### 2. Avoid Repeated Initialization

```c
#include "xrt.h"
#include <stdio.h>
#include <time.h>

// ✗ Wrong: reset seed every time
uint32 BadRandom() {
    xrtSetRandSeed32((uint64)time(NULL), 1);  // Same result within same second!
    return xrtRand32();
}

// ✓ Correct: initialize only once at start
void Setup() {
    xrtSetRandSeed32((uint64)time(NULL), 1);
}

uint32 GoodRandom() {
    return xrtRand32();  // Different result each call
}

int main() {
    xrtInit();
    
    // Demonstrate wrong usage: same result within same second
    printf("Wrong usage (same result within same second):\n");
    for (int i = 0; i < 3; i++) {
        printf("  %u\n", BadRandom());
    }
    
    // Demonstrate correct usage
    printf("\nCorrect usage (different each time):\n");
    Setup();
    for (int i = 0; i < 3; i++) {
        printf("  %u\n", GoodRandom());
    }
    
    xrtUnit();
    return 0;
}
```

---

## Algorithm Quality

### PCG Random Number Generator

XRT uses **PCG (Permuted Congruential Generator)** algorithm:

**Advantages:**
- ✅ High quality randomness
- ✅ Fast (faster than standard `rand()`)
- ✅ Long period (PCG32: 2^64, PCG64: 2^128)
- ✅ Passes multiple randomness tests (TestU01)

**Comparison:**
- Higher quality than `rand()`
- Faster than `MT19937`
- Suitable for games, simulations, testing, etc.

---

## Common Errors

### 1. Forgetting to Initialize

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    // ✗ Wrong: not initialized
    // int r = xrtRand32();  // Results may not be ideal
    
    // ✓ Correct: initialize first
    xrtInit();  // Initialize seed
    int r = xrtRand32();
    printf("Random number: %u\n", r);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Range Misunderstanding

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // xrtRandRange includes both ends
    printf("Simulate dice roll (1-6):\n");
    for (int i = 0; i < 10; i++) {
        int r = xrtRandRange(1, 6);  // Can be 1,2,3,4,5,6
        printf("%d ", r);
    }
    printf("\n");
    
    // If need [1, 6) range (not including 6)
    printf("\nRange [1, 6) not including 6:\n");
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

## Related Documentation

- [Base Basic Function Library](api-base.en.md) - Library initialization
- [String String Processing](api-string.en.md) - Random string generation
- [Time Time Processing](api-time.en.md) - Time seed
- [Main Index](README.en.md) - Return to documentation home

---

<div align="center">

[Back to Top](#math---mathematics-library)

</div>
