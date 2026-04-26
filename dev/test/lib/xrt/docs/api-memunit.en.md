# MemUnit - Memory Unit Management Library

> Memory Unit - 256-element fixed page memory management unit

[English](api-memunit.en.md) | [中文](api-memunit.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Constants](#constants)
- [Data Structures](#data-structures)
- [Manager Operations](#manager-operations)
- [Memory Allocation](#memory-allocation)
- [GC Collection](#gc-collection)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)

---

## Overview

MemUnit (Memory Unit) is XRT library's low-level memory management unit, with each unit managing **256 fixed-size** elements. It is the foundational component for FSMemPool and MemPool.

### Core Features

- **Fixed Capacity**: Each unit stores up to 256 elements
- **Free Reuse**: Released memory managed via circular queue, prioritizing reuse
- **O(1) Operations**: Allocation and deallocation are constant time
- **GC Support**: Supports mark-sweep garbage collection
- **Metadata Header**: Each element has 4-byte flag prefix

### Memory Layout

```
MemUnit Structure:
┌─────────────────────────────────────────────────────────────┐
│ FreeList[256] │ ItemLength │ Count │ FreeCount │ FreeOffset │
├─────────────────────────────────────────────────────────────┤
│                        Memory[]                              │
│  ┌──────────┬──────────┬──────────┬─────┬──────────┐        │
│  │ [Flag+0] │ [Flag+1] │ [Flag+2] │ ... │ [Flag+255]│        │
│  │  Data0   │  Data1   │  Data2   │     │  Data255  │        │
│  └──────────┴──────────┴──────────┴─────┴──────────┘        │
└─────────────────────────────────────────────────────────────┘

Each Element:
┌──────────────┬──────────────────────────────────────┐
│ ItemFlag(4B) │ User Data (ItemLength - 4)           │
└──────────────┴──────────────────────────────────────┘
```

---

## Constants

### Flag Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MMU_FLAG_MASK` | `0x3FFFFFFF` | Valid ID range mask (30 bits) |
| `MMU_FLAG_USE` | `0x80000000` | Struct usage status flag (highest bit) |
| `MMU_FLAG_GC` | `0x40000000` | GC collection flag bit (second highest) |
| `MMU_FLAG_EXT` | `0xBFFFFFFF` | Non-memory manager managed memory flag |

### Flag Layout

```
ItemFlag (32 bits):
┌───┬───┬──────────────────────────────┐
│ U │ G │          ID / Index          │
├───┼───┼──────────────────────────────┤
│31 │30 │          29 - 0              │
└───┴───┴──────────────────────────────┘
  U = MMU_FLAG_USE (in use)
  G = MMU_FLAG_GC (GC marked)
```

---

## Data Structures

### MMU_Value

Element header flag structure.

**Definition:**
```c
typedef struct {
    uint32 ItemFlag;    // Flags: usage status + GC mark + index
} MMU_Value, *MMU_ValuePtr;
```

**Notes:**
- Each element has 4-byte `ItemFlag` prefix
- Used to track element status and position in unit

---

### xmemunit_struct

Memory unit manager structure.

**Definition:**
```c
typedef struct {
    uint8 FreeList[256];    // Released member index circular queue
    uint32 ItemLength;      // Member memory length (including 4-byte header)
    uint16 Count;           // Current used member count
    uint8 FreeCount;        // Released member count in circular queue
    uint8 FreeOffset;       // Circular queue first element offset
    uint32 Flag;            // Value Flag prefix (from parent manager)
    char Memory[];          // Flexible array, stores 256 elements
} xmemunit_struct, *xmemunit;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `FreeList` | Circular queue storing released element indices |
| `ItemLength` | Total length of each element (user requested + 4) |
| `Count` | Currently allocated and unreleased element count |
| `FreeCount` | Reusable released element count |
| `FreeOffset` | Next reusable element position in FreeList |
| `Flag` | Flag prefix passed from parent manager |
| `Memory` | Actual data storage memory area |

---

## Manager Operations

### xrtMemUnitCreate

Create a memory management unit.

**Function Prototype:**
```c
XXAPI xmemunit xrtMemUnitCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Each element's data size (excluding 4-byte header)

**Return Value:**
- Success: Memory unit pointer
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Record;

int main() {
    xrtInit();
    
    // Create memory unit that can store 256 Records
    xmemunit unit = xrtMemUnitCreate(sizeof(Record));
    if (unit) {
        printf("Memory unit created successfully\n");
        printf("Element length: %u (including 4-byte header)\n", unit->ItemLength);
        printf("Currently used: %u\n", unit->Count);
        
        xrtMemUnitDestroy(unit);
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `iItemLength` automatically adds 4 bytes for storing `ItemFlag`
- Total memory unit size = `sizeof(xmemunit_struct) + 256 * (iItemLength + 4)`

---

### xrtMemUnitDestroy

Destroy memory management unit.

**Definition (macro):**
```c
#define xrtMemUnitDestroy xrtFree
```

**Notes:**
- Directly calls `xrtFree` to release entire memory unit
- Does not call destructors for each element
- Ensure no elements in unit are being used before destroying

---

## Memory Allocation

### xrtMemUnitAlloc

Allocate an element from memory unit.

**Function Prototype:**
```c
XXAPI ptr xrtMemUnitAlloc(xmemunit objUnit);
```

**Parameters:**
- `objUnit` - Memory unit object

**Return Value:**
- Success: Element pointer (skips header, points directly to user data)
- Failure: `NULL` (unit is empty or full)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // Allocate 5 elements
    for (int i = 0; i < 5; i++) {
        Item* item = (Item*)xrtMemUnitAlloc(unit);
        if (item) {
            item->value = i * 10;
            printf("Allocated element %d: value = %d\n", i, item->value);
        }
    }
    
    printf("Currently used: %u elements\n", unit->Count);
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

**Allocation Strategy:**
1. Priority reuse released slots from FreeList
2. If no reusable slots, allocate new slot (index = Count)
3. Return `NULL` if exceeds 256 elements

---

### xrtMemUnitAlloc_Inline

Inline version of element allocation (high performance).

**Function Prototype:**
```c
static inline ptr xrtMemUnitAlloc_Inline(xmemunit objUnit);
```

**Notes:**
- Does not check if `objUnit` is `NULL`
- Does not check if exceeds 256 limit
- Caller must ensure conditions are met

---

### xrtMemUnitFree

Release element from memory unit (by pointer).

**Function Prototype:**
```c
XXAPI bool xrtMemUnitFree(xmemunit objUnit, ptr obj);
```

**Parameters:**
- `objUnit` - Memory unit object
- `obj` - Element pointer

**Return Value:**
- `TRUE` - Release successful
- `FALSE` - Failed (unit empty, element not in use, or invalid pointer)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // Allocate
    Item* item1 = (Item*)xrtMemUnitAlloc(unit);
    Item* item2 = (Item*)xrtMemUnitAlloc(unit);
    Item* item3 = (Item*)xrtMemUnitAlloc(unit);
    
    printf("After allocation: %u elements\n", unit->Count);  // 3
    
    // Release middle element
    xrtMemUnitFree(unit, item2);
    printf("After release: %u elements\n", unit->Count);  // 2
    
    // Next allocation reuses item2's slot
    Item* item4 = (Item*)xrtMemUnitAlloc(unit);
    printf("item4 address: %p (should be same as item2: %p)\n", item4, item2);
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

---

### xrtMemUnitFreeIdx

Release element from memory unit (by index).

**Function Prototype:**
```c
XXAPI bool xrtMemUnitFreeIdx(xmemunit objUnit, uint8 idx);
```

**Parameters:**
- `objUnit` - Memory unit object
- `idx` - Element index (0-255)

**Return Value:**
- `TRUE` - Release successful
- `FALSE` - Failed

**Notes:**
- Faster than `xrtMemUnitFree` when element index is known
- Index can be obtained from `ItemFlag & 0xFF`

---

### xrtMemUnitFree_Inline / xrtMemUnitFreeIdx_Inline

Inline versions of release functions.

**Note:**
- `xrtMemUnitFreeIdx_Inline` does not clear `ItemFlag`
- Caller is responsible for clearing flags (if needed)

---

## GC Collection

### xrtMemUnitGC_Mark

Mark element as GC in-use.

**Definition (macro):**
```c
#define xrtMemUnitGC_Mark(p) \
    (((MMU_ValuePtr)((void*)p - sizeof(MMU_Value)))->ItemFlag |= MMU_FLAG_GC)
```

**Parameters:**
- `p` - Element pointer

**Notes:**
- During GC cycle, mark elements that need to be retained
- Sets `MMU_FLAG_GC` bit

---

### xrtMemUnitGC

Execute one round of garbage collection.

**Function Prototype:**
```c
XXAPI int xrtMemUnitGC(xmemunit objUnit, bool bFreeMark);
```

**Parameters:**
- `objUnit` - Memory unit object
- `bFreeMark` - Collection mode
  - `TRUE` - Collect **marked** elements
  - `FALSE` - Collect **unmarked** elements (standard Mark-Sweep)

**Return Value:**
- Number of elements collected

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // Allocate 5 elements
    Item* items[5];
    for (int i = 0; i < 5; i++) {
        items[i] = (Item*)xrtMemUnitAlloc(unit);
        items[i]->value = i;
    }
    printf("After allocation: %u elements\n", unit->Count);  // 5
    
    // Mark elements to retain (0, 2, 4)
    xrtMemUnitGC_Mark(items[0]);
    xrtMemUnitGC_Mark(items[2]);
    xrtMemUnitGC_Mark(items[4]);
    
    // Collect unmarked elements (1, 3)
    int freed = xrtMemUnitGC(unit, FALSE);
    printf("Collected %d elements\n", freed);  // 2
    printf("Remaining: %u elements\n", unit->Count);  // 3
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

**GC Flow:**

1. **Mark Phase**: Traverse all active references, call `xrtMemUnitGC_Mark` to mark
2. **Sweep Phase**: Call `xrtMemUnitGC(unit, FALSE)` to collect unmarked elements
3. **Clear Marks**: `xrtMemUnitGC` automatically clears GC marks from retained elements

---

## Usage Scenarios

### 1. Low-Level Memory Pool Component

MemUnit is typically used as internal component for higher-level memory managers:

```c
#include "xrt.h"
#include <stdio.h>

// FSMemPool internally uses MemUnit to manage memory pages
// This is a simplified illustration:

typedef struct {
    xmemunit pages[4];  // Max 4 pages, total 1024 elements
    int pageCount;
} SimplePool;

ptr SimplePoolAlloc(SimplePool* pool, size_t itemSize) {
    // Try to allocate from existing pages
    for (int i = 0; i < pool->pageCount; i++) {
        if (pool->pages[i]->Count < 256) {
            return xrtMemUnitAlloc(pool->pages[i]);
        }
    }
    // Need new page
    if (pool->pageCount < 4) {
        pool->pages[pool->pageCount] = xrtMemUnitCreate(itemSize);
        return xrtMemUnitAlloc(pool->pages[pool->pageCount++]);
    }
    return NULL;  // Pool is full
}

int main() {
    xrtInit();
    printf("MemUnit is typically used as low-level component\n");
    printf("Recommend using FSMemPool or MemPool directly\n");
    xrtUnit();
    return 0;
}
```

---

### 2. High-Performance Object Cache

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char data[60];
} CacheEntry;

int main() {
    xrtInit();
    
    // 256 cache slots
    xmemunit cache = xrtMemUnitCreate(sizeof(CacheEntry));
    
    // Simulate cache operations
    CacheEntry* entries[10];
    for (int i = 0; i < 10; i++) {
        entries[i] = (CacheEntry*)xrtMemUnitAlloc(cache);
        entries[i]->id = i;
        snprintf(entries[i]->data, 60, "Cache item %d", i);
    }
    
    printf("Cache usage: %u / 256\n", cache->Count);
    
    // Release some cache entries
    xrtMemUnitFree(cache, entries[3]);
    xrtMemUnitFree(cache, entries[7]);
    
    printf("After release: %u / 256\n", cache->Count);
    
    xrtMemUnitDestroy(cache);
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Prefer High-Level Memory Managers

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ❌ Direct MemUnit usage (unless special needs)
    // xmemunit unit = xrtMemUnitCreate(sizeof(MyStruct));
    
    // ✅ Recommended: Use FSMemPool (no 256 limit)
    // xfsmempool pool = xrtFSMemPoolCreate(sizeof(MyStruct));
    
    // ✅ Recommended: Use MemPool (multi-size support)
    // xmempool pool = xrtMemPoolCreate();
    
    printf("MemUnit is suitable for:\n");
    printf("1. Implementing custom memory managers\n");
    printf("2. Need precise control of 256-element pages\n");
    printf("3. Low-level performance optimization scenarios\n");
    
    xrtUnit();
    return 0;
}
```

---

### 2. Capacity Check

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // Check capacity before allocation
    for (int i = 0; i < 300; i++) {
        if (unit->Count >= 256) {
            printf("Unit is full, cannot allocate element %d\n", i + 1);
            break;
        }
        Item* item = (Item*)xrtMemUnitAlloc(unit);
        item->value = i;
    }
    
    printf("Final usage: %u elements\n", unit->Count);  // 256
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

---

### 3. GC Cycle Management

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int id; int refCount; } Object;

// Simulate reference traversal
void MarkReachable(xmemunit unit, Object* roots[], int rootCount) {
    for (int i = 0; i < rootCount; i++) {
        if (roots[i] && roots[i]->refCount > 0) {
            xrtMemUnitGC_Mark(roots[i]);
        }
    }
}

int main() {
    xrtInit();
    
    xmemunit heap = xrtMemUnitCreate(sizeof(Object));
    
    // Allocate objects
    Object* objs[10];
    for (int i = 0; i < 10; i++) {
        objs[i] = (Object*)xrtMemUnitAlloc(heap);
        objs[i]->id = i;
        objs[i]->refCount = (i % 2 == 0) ? 1 : 0;  // Even numbers have references
    }
    
    printf("Before GC: %u objects\n", heap->Count);
    
    // GC cycle
    MarkReachable(heap, objs, 10);
    int freed = xrtMemUnitGC(heap, FALSE);
    
    printf("After GC: %u objects, collected %d\n", heap->Count, freed);
    
    xrtMemUnitDestroy(heap);
    xrtUnit();
    return 0;
}
```

---

## Comparison with Other Memory Managers

| Feature | MemUnit | FSMemPool | MemPool |
|---------|---------|-----------|---------|
| **Capacity** | Fixed 256 | Unlimited | Unlimited |
| **Element Size** | Fixed | Fixed | Variable |
| **GC Support** | ✅ | ✅ | ✅ |
| **Use Case** | Low-level component | Fixed-size objects | General object pool |
| **Complexity** | Low | Medium | High |

---

## Related Documentation

- [BSMM Block Structured Memory Management](api-bsmm.en.md)
- [FSMemPool Fixed-Size Memory Pool](api-mempool-fs.en.md)
- [MemPool General Memory Pool](api-mempool.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#memunit---memory-unit-management-library)

</div>
