# MemPool - General Memory Pool

> Variable-size memory pool supporting multiple sizes of memory allocation and GC collection

[English](api-mempool.en.md) | [中文](api-mempool.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Memory Pool Operations](#memory-pool-operations)
- [Memory Allocation](#memory-allocation)
- [GC Collection](#gc-collection)
- [Usage Scenarios](#usage-scenarios)
- [Comparison with Other Memory Managers](#comparison-with-other-memory-managers)
- [Best Practices](#best-practices)

---

## Overview

MemPool (Memory Pool) is XRT library's general memory pool supporting **variable-size** memory allocation. Unlike FSMemPool (fixed-size), MemPool can allocate memory blocks of any size.

### Core Features

- **Variable Size**: Supports allocation of different sized memory blocks
- **FSB Binary Tree**: Uses binary tree for fast lookup of appropriate memory block size
- **Two Presets**: Small memory scheme (1-512B) and large memory scheme (1-4096B)
- **Large Memory Fallback**: Memory exceeding range uses malloc allocation
- **GC Support**: Supports mark-sweep garbage collection

### Architecture Diagram

```
MemPool Structure:
┌─────────────────────────────────────────────────────────────┐
│                        MemPool                              │
├─────────────────────────────────────────────────────────────┤
│  FSB_RootNode (Binary Tree Root)                            │
│       ↓                                                     │
│  ┌────────────────────────────────────────────────────┐    │
│  │            FSB Binary Tree (iCustom=1 example)     │    │
│  │                        160                          │    │
│  │              ┌──────────┴──────────┐               │    │
│  │             64                    320               │    │
│  │         ┌────┴────┐          ┌────┴────┐          │    │
│  │        32        96         224       448          │    │
│  │       ┌─┴─┐    ┌─┴─┐      ┌─┴─┐     ┌─┴─┐        │    │
│  │      16  48   80  128    192 256   384 512        │    │
│  └────────────────────────────────────────────────────┘    │
│                                                             │
│  Each FSB_Item manages a MemUnit linked list for a size    │
│  range:                                                     │
│  ┌─────────────┐                                           │
│  │ FSB_Item    │  MinLength: 1, MaxLength: 16              │
│  │ LL_Idle ────┼──> [MMU] -> [MMU] -> NULL                 │
│  │ LL_Full ────┼──> [MMU] -> NULL                          │
│  │ LL_Null ────┼──> [MMU] (spare)                          │
│  │ LL_Free ────┼──> [released MMU positions]               │
│  └─────────────┘                                           │
│                                                             │
│  arrMMU (BSMM): Manages all MemUnits                        │
│  BigMM (BSMM): Manages all large memory allocation info     │
│  LL_BigFree: Released large memory linked list              │
└─────────────────────────────────────────────────────────────┘
```

### Allocation Strategy

1. **Small Memory Allocation** (within FSB range):
   - Lookup matching FSB_Item via binary tree
   - Allocate memory from corresponding MemUnit
   - O(log n) lookup + O(1) allocation

2. **Large Memory Allocation** (exceeding FSB range):
   - Directly use malloc allocation
   - Record allocation info in BigMM

---

## Data Structures

### MP_MemHead

Large memory header structure (for allocations exceeding FSB range).

**Definition:**
```c
typedef struct {
    uint32 Index;    // Index in BigMM
    uint32 Flag;     // Flag conforming to MemUnit standard
} MP_MemHead;
```

---

### MP_BigInfoLL

Large memory info linked list structure.

**Definition:**
```c
typedef struct MP_BigInfoLL {
    uint32 Size;                  // Requested memory size
    ptr Ptr;                      // Pointer address
    struct MP_BigInfoLL* Next;    // Next linked list node (for release list)
} MP_BigInfoLL;
```

---

### FSB_Item

Fixed-size block management structure.

**Definition:**
```c
typedef struct FSB_Item {
    uint32 MinLength;             // Minimum supported memory length
    uint32 MaxLength;             // Maximum supported memory length
    MMU_LLNode* LL_Idle;          // Idle MemUnit linked list
    MMU_LLNode* LL_Full;          // Full MemUnit linked list
    MMU_LLNode* LL_Null;          // Completely empty MemUnit (spare)
    MMU_LLNode* LL_Free;          // Released MemUnit linked list
    struct FSB_Item* left;        // Left subtree
    struct FSB_Item* right;       // Right subtree
} FSB_Item;
```

**Linked List Description:**

| List | Description |
|------|-------------|
| `LL_Idle` | MemUnits with available slots, priority allocation from here |
| `LL_Full` | Full MemUnits, no allocation |
| `LL_Null` | Completely empty MemUnit (max 1), to avoid critical oscillation |
| `LL_Free` | Destroyed MemUnit positions, prioritized for reuse when creating new MemUnits |

---

### xmempool_struct

General memory pool structure.

**Definition:**
```c
typedef struct {
    FSB_Item* FSB_Memory;       // FSB array memory
    FSB_Item* FSB_RootNode;     // FSB binary tree root node
    xbsmm_struct arrMMU;        // MemUnit array management
    xbsmm_struct BigMM;         // Large memory info management
    MP_BigInfoLL* LL_BigFree;   // Released large memory linked list
} xmempool_struct, *xmempool;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `FSB_Memory` | Memory pointer for FSB array |
| `FSB_RootNode` | Root node of FSB binary tree |
| `arrMMU` | Uses BSMM to manage all MemUnits |
| `BigMM` | Uses BSMM to manage large memory allocation info |
| `LL_BigFree` | Released large memory info linked list |

---

## Memory Pool Operations

### xrtMemPoolCreate

Create a general memory pool.

**Function Prototype:**
```c
XXAPI xmempool xrtMemPoolCreate(int iCustom);
```

**Parameters:**
- `iCustom` - Preset configuration:
  - `1` - Small memory scheme (4-layer tree, 1-512 bytes, 15 blocks)
  - `2` - Large memory scheme (5-layer tree, 1-4096 bytes, 31 blocks)
  - Other - Don't create preset, need manual FSB configuration

**Return Value:**
- Success: Memory pool object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtMemPoolDestroy` to free

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create small memory scheme (1-512B)
    xmempool pool1 = xrtMemPoolCreate(1);
    printf("Small memory pool: supports 1-512 bytes\n");
    
    // Create large memory scheme (1-4096B)
    xmempool pool2 = xrtMemPoolCreate(2);
    printf("Large memory pool: supports 1-4096 bytes\n");
    
    // Allocate various sizes of memory
    ptr p1 = xrtMemPoolAlloc(pool1, 32);
    ptr p2 = xrtMemPoolAlloc(pool1, 128);
    ptr p3 = xrtMemPoolAlloc(pool1, 500);
    ptr p4 = xrtMemPoolAlloc(pool1, 1024);  // Exceeds range, uses malloc
    
    printf("Allocate 32B: %p\n", p1);
    printf("Allocate 128B: %p\n", p2);
    printf("Allocate 500B: %p\n", p3);
    printf("Allocate 1024B (large memory): %p\n", p4);
    
    // Release
    xrtMemPoolFree(pool1, p1);
    xrtMemPoolFree(pool1, p2);
    xrtMemPoolFree(pool1, p3);
    xrtMemPoolFree(pool1, p4);
    
    xrtMemPoolDestroy(pool1);
    xrtMemPoolDestroy(pool2);
    
    xrtUnit();
    return 0;
}
```

**Preset Block Configuration:**

**iCustom = 1 (Small Memory Scheme):**

| Block | Range (bytes) |
|-------|---------------|
| 1 | 1-16 |
| 2 | 17-32 |
| 3 | 33-48 |
| 4 | 49-64 |
| 5 | 65-80 |
| 6 | 81-96 |
| 7 | 97-128 |
| 8 | 129-160 |
| 9 | 161-192 |
| 10 | 193-224 |
| 11 | 225-256 |
| 12 | 257-320 |
| 13 | 321-384 |
| 14 | 385-448 |
| 15 | 449-512 |

**iCustom = 2 (Large Memory Scheme):**

Adds to small memory scheme:

| Block | Range (bytes) |
|-------|---------------|
| 16 | 513-640 |
| 17 | 641-768 |
| ... | ... |
| 31 | 3841-4096 |

---

### xrtMemPoolDestroy

Destroy memory pool.

**Function Prototype:**
```c
XXAPI void xrtMemPoolDestroy(xmempool objMP);
```

**Parameters:**
- `objMP` - Memory pool object

**Notes:**
- Releases all MemUnits
- Releases all large memory blocks
- Releases FSB array
- Releases memory pool structure itself

---

### xrtMemPoolInit

Initialize memory pool (for stack or embedded scenarios).

**Function Prototype:**
```c
XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom);
```

**Parameters:**
- `objMP` - Memory pool object pointer
- `iCustom` - Preset configuration (same as Create)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Stack allocation
    xmempool_struct pool;
    xrtMemPoolInit(&pool, 1);
    
    ptr p = xrtMemPoolAlloc(&pool, 64);
    printf("Allocated: %p\n", p);
    xrtMemPoolFree(&pool, p);
    
    xrtMemPoolUnit(&pool);  // Note: Use Unit not Destroy
    
    xrtUnit();
    return 0;
}
```

---

### xrtMemPoolUnit

Release memory pool internal resources (does not free structure itself).

**Function Prototype:**
```c
XXAPI void xrtMemPoolUnit(xmempool objMP);
```

**Notes:**
- Used with `xrtMemPoolInit`
- Releases all internal resources but not the structure itself

---

## Memory Allocation

### xrtMemPoolAlloc

Allocate memory from memory pool.

**Function Prototype:**
```c
XXAPI ptr xrtMemPoolAlloc(xmempool objMP, uint32 iSize);
```

**Parameters:**
- `objMP` - Memory pool object
- `iSize` - Requested memory size (bytes)

**Return Value:**
- Success: Memory pointer
- Failure: `NULL`

**Allocation Strategy:**
1. Lookup matching block in FSB binary tree
2. If found: Allocate from corresponding MemUnit
3. If not found: Use malloc for large memory allocation

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[32];
} User;

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // Allocate different sizes of memory
    User* user = xrtMemPoolAlloc(pool, sizeof(User));
    user->id = 1;
    strcpy(user->name, "Alice");
    
    char* buffer = xrtMemPoolAlloc(pool, 256);
    strcpy(buffer, "Hello, World!");
    
    // Large memory allocation (exceeds FSB range)
    char* largeBuffer = xrtMemPoolAlloc(pool, 8192);
    memset(largeBuffer, 'X', 8192);
    
    printf("User: id=%d, name=%s\n", user->id, user->name);
    printf("Buffer: %s\n", buffer);
    printf("Large buffer allocated: %p\n", largeBuffer);
    
    xrtMemPoolFree(pool, user);
    xrtMemPoolFree(pool, buffer);
    xrtMemPoolFree(pool, largeBuffer);
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

### xrtMemPoolFree

Release memory allocated from memory pool.

**Function Prototype:**
```c
XXAPI void xrtMemPoolFree(xmempool objMP, ptr ptr);
```

**Parameters:**
- `objMP` - Memory pool object
- `ptr` - Memory pointer to release

**Release Strategy:**
1. Check memory header flag
2. If large memory: use free to release, add to LL_BigFree for reuse
3. If FSB memory: release to corresponding MemUnit, update linked list state

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // Allocate multiple memory blocks
    ptr blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = xrtMemPoolAlloc(pool, 32 * (i + 1));
        printf("Allocate %d bytes: %p\n", 32 * (i + 1), blocks[i]);
    }
    
    // Release all memory blocks
    for (int i = 0; i < 10; i++) {
        xrtMemPoolFree(pool, blocks[i]);
        printf("Release block[%d]\n", i);
    }
    
    // Allocate again (will reuse previous memory)
    ptr reused = xrtMemPoolAlloc(pool, 64);
    printf("Reused allocation: %p\n", reused);
    xrtMemPoolFree(pool, reused);
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

## GC Collection

### xrtMemPoolGC_Mark

Mark memory as in-use (macro definition).

**Macro Definition:**
```c
#define xrtMemPoolGC_Mark  xrtMemUnitGC_Mark
```

**Function Prototype:**
```c
void xrtMemPoolGC_Mark(ptr pVal);
```

**Parameters:**
- `pVal` - Memory pointer

**Notes:**
- Mark memory still in use before GC
- Used with `xrtMemPoolGC`

---

### xrtMemPoolGC

Execute garbage collection.

**Function Prototype:**
```c
XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark);
```

**Parameters:**
- `objMP` - Memory pool object
- `bFreeMark` - GC mode:
  - `TRUE` - Collect **marked** memory
  - `FALSE` - Collect **unmarked** memory (common)

**GC Flow (bFreeMark = FALSE):**
1. Mark phase: Traverse all active objects, call `xrtMemPoolGC_Mark`
2. Sweep phase: Call `xrtMemPoolGC`, unmarked memory is collected
3. Clear phase: GC automatically clears marks from marked objects

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // Allocate multiple memory blocks
    ptr p1 = xrtMemPoolAlloc(pool, 32);
    ptr p2 = xrtMemPoolAlloc(pool, 64);
    ptr p3 = xrtMemPoolAlloc(pool, 128);
    ptr p4 = xrtMemPoolAlloc(pool, 256);
    
    printf("Allocated 4 memory blocks\n");
    
    // Assume p1 and p3 still in use, p2 and p4 no longer used
    // Mark memory still in use
    xrtMemPoolGC_Mark(p1);
    xrtMemPoolGC_Mark(p3);
    
    // Execute GC, collect unmarked memory (p2 and p4)
    xrtMemPoolGC(pool, FALSE);
    
    printf("GC complete, p2 and p4 have been collected\n");
    
    // p1 and p3 are still valid
    // Note: Marks are cleared after GC, need to re-mark before next GC
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

## Usage Scenarios

### 1. Multi-Type Object Allocation

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct { int x, y; } Point;
typedef struct { int x, y, w, h; } Rect;
typedef struct { char name[64]; int value; } Config;

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // Allocate different types of objects
    Point* p1 = xrtMemPoolAlloc(pool, sizeof(Point));
    p1->x = 10; p1->y = 20;
    
    Rect* r1 = xrtMemPoolAlloc(pool, sizeof(Rect));
    r1->x = 0; r1->y = 0; r1->w = 100; r1->h = 50;
    
    Config* c1 = xrtMemPoolAlloc(pool, sizeof(Config));
    strcpy(c1->name, "MaxConnections");
    c1->value = 100;
    
    printf("Point: (%d, %d)\n", p1->x, p1->y);
    printf("Rect: (%d, %d, %d, %d)\n", r1->x, r1->y, r1->w, r1->h);
    printf("Config: %s = %d\n", c1->name, c1->value);
    
    xrtMemPoolFree(pool, p1);
    xrtMemPoolFree(pool, r1);
    xrtMemPoolFree(pool, c1);
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

### 2. JSON Parser Memory Management

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct JsonNode {
    int type;  // 0=null, 1=string, 2=number, 3=object, 4=array
    union {
        char* str;
        double num;
        struct JsonNode* children;
    } value;
    struct JsonNode* next;
} JsonNode;

xmempool jsonPool;

JsonNode* CreateStringNode(const char* value) {
    JsonNode* node = xrtMemPoolAlloc(jsonPool, sizeof(JsonNode));
    node->type = 1;
    size_t len = strlen(value);
    node->value.str = xrtMemPoolAlloc(jsonPool, len + 1);
    strcpy(node->value.str, value);
    node->next = NULL;
    return node;
}

JsonNode* CreateNumberNode(double value) {
    JsonNode* node = xrtMemPoolAlloc(jsonPool, sizeof(JsonNode));
    node->type = 2;
    node->value.num = value;
    node->next = NULL;
    return node;
}

int main() {
    xrtInit();
    
    jsonPool = xrtMemPoolCreate(1);
    
    // Simulate JSON parsing
    JsonNode* root = CreateStringNode("Hello, JSON!");
    JsonNode* num = CreateNumberNode(3.14159);
    root->next = num;
    
    printf("String node: %s\n", root->value.str);
    printf("Number node: %f\n", num->value.num);
    
    // Release all memory at once when destroying
    xrtMemPoolDestroy(jsonPool);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Script Engine with GC

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct ScriptVar {
    int type;  // 0=int, 1=string
    union {
        int intVal;
        char* strVal;
    };
    bool marked;  // For GC marking
} ScriptVar;

xmempool varPool;
ScriptVar* variables[256];
int varCount = 0;

ScriptVar* CreateInt(int value) {
    ScriptVar* var = xrtMemPoolAlloc(varPool, sizeof(ScriptVar));
    var->type = 0;
    var->intVal = value;
    var->marked = FALSE;
    variables[varCount++] = var;
    return var;
}

ScriptVar* CreateString(const char* value) {
    ScriptVar* var = xrtMemPoolAlloc(varPool, sizeof(ScriptVar));
    var->type = 1;
    size_t len = strlen(value);
    var->strVal = xrtMemPoolAlloc(varPool, len + 1);
    strcpy(var->strVal, value);
    var->marked = FALSE;
    variables[varCount++] = var;
    return var;
}

void MarkVariable(ScriptVar* var) {
    if (!var->marked) {
        var->marked = TRUE;
        xrtMemPoolGC_Mark(var);
        if (var->type == 1 && var->strVal) {
            xrtMemPoolGC_Mark(var->strVal);
        }
    }
}

void RunGC() {
    // Mark phase (simplified, should start from root set in practice)
    for (int i = 0; i < varCount; i++) {
        if (variables[i] && variables[i]->marked) {
            MarkVariable(variables[i]);
        }
    }
    
    // Sweep phase
    xrtMemPoolGC(varPool, FALSE);
    
    // Clear marks
    for (int i = 0; i < varCount; i++) {
        if (variables[i]) {
            variables[i]->marked = FALSE;
        }
    }
}

int main() {
    xrtInit();
    
    varPool = xrtMemPoolCreate(1);
    
    // Create variables
    ScriptVar* a = CreateInt(42);
    ScriptVar* b = CreateString("Hello");
    ScriptVar* c = CreateString("World");
    
    printf("Created 3 variables\n");
    
    // Simulate b still in use
    b->marked = TRUE;
    
    // Run GC
    RunGC();
    
    printf("GC complete\n");
    
    xrtMemPoolDestroy(varPool);
    
    xrtUnit();
    return 0;
}
```

---

## Comparison with Other Memory Managers

| Feature | MemPool | FSMemPool | MemUnit | malloc/free |
|---------|---------|-----------|---------|-------------|
| **Allocation Size** | Variable | Fixed | Fixed | Variable |
| **Capacity** | Unlimited | Unlimited | 256 | Unlimited |
| **Large Memory Support** | ✅ Auto fallback | ❌ | ❌ | ✅ |
| **GC Support** | ✅ | ✅ | ✅ | ❌ |
| **Allocation Speed** | O(log n) + O(1) | O(1) | O(1) | O(?) |
| **Memory Fragmentation** | No fragmentation within blocks | No fragmentation | No fragmentation | Has fragmentation |
| **Use Case** | General allocation | Fixed-type objects | Low-level components | General |

### Selection Guide

- **MemPool**: Use when allocating different sizes of memory
- **FSMemPool**: Use when only allocating fixed-size objects (better performance)
- **MemUnit**: As low-level component, usually not used directly
- **malloc/free**: Simple scenarios or when interfacing with other libraries

---

## Best Practices

### 1. Choose Appropriate Preset

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Mostly small objects (<512B)
    xmempool smallPool = xrtMemPoolCreate(1);
    
    // Mostly large objects (<4096B)
    xmempool largePool = xrtMemPoolCreate(2);
    
    // Allocation examples
    ptr p1 = xrtMemPoolAlloc(smallPool, 64);   // Uses FSB
    ptr p2 = xrtMemPoolAlloc(smallPool, 1024); // Uses malloc (out of range)
    
    ptr p3 = xrtMemPoolAlloc(largePool, 64);   // Uses FSB
    ptr p4 = xrtMemPoolAlloc(largePool, 1024); // Uses FSB (in range)
    ptr p5 = xrtMemPoolAlloc(largePool, 8192); // Uses malloc (out of range)
    
    printf("Small pool - 64B: FSB, 1024B: malloc\n");
    printf("Large pool - 64B: FSB, 1024B: FSB, 8192B: malloc\n");
    
    xrtMemPoolFree(smallPool, p1);
    xrtMemPoolFree(smallPool, p2);
    xrtMemPoolFree(largePool, p3);
    xrtMemPoolFree(largePool, p4);
    xrtMemPoolFree(largePool, p5);
    
    xrtMemPoolDestroy(smallPool);
    xrtMemPoolDestroy(largePool);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Separate Memory Pools by Purpose

```c
#include "xrt.h"
#include <stdio.h>

// Different purposes use different memory pools
xmempool configPool;    // Config objects (long-lived)
xmempool requestPool;   // Request objects (short-lived, can GC)

void Init() {
    configPool = xrtMemPoolCreate(1);
    requestPool = xrtMemPoolCreate(1);
}

void Cleanup() {
    xrtMemPoolDestroy(configPool);
    xrtMemPoolDestroy(requestPool);
}

void ProcessRequest() {
    // Allocate request-related memory
    ptr req = xrtMemPoolAlloc(requestPool, 256);
    
    // Process request...
    
    // After processing, can choose:
    // 1. Manual release
    xrtMemPoolFree(requestPool, req);
    
    // 2. Or use GC for batch collection
}

void GCRequests() {
    // Periodically collect unmarked memory from request pool
    xrtMemPoolGC(requestPool, FALSE);
}

int main() {
    xrtInit();
    
    Init();
    
    // Allocate config (long-lived)
    ptr config = xrtMemPoolAlloc(configPool, 128);
    printf("Config allocated\n");
    
    // Process multiple requests
    for (int i = 0; i < 5; i++) {
        ProcessRequest();
    }
    
    // Periodic GC
    GCRequests();
    printf("Request pool GC'd\n");
    
    xrtMemPoolFree(configPool, config);
    
    Cleanup();
    
    xrtUnit();
    return 0;
}
```

---

### 3. Avoid Cross-Pool Release

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xmempool pool1 = xrtMemPoolCreate(1);
    xmempool pool2 = xrtMemPoolCreate(1);
    
    ptr p1 = xrtMemPoolAlloc(pool1, 64);
    ptr p2 = xrtMemPoolAlloc(pool2, 64);
    
    // ✅ Correct: Release from same pool
    xrtMemPoolFree(pool1, p1);
    xrtMemPoolFree(pool2, p2);
    
    // ❌ Wrong: Cross-pool release (may cause crash or data corruption)
    // xrtMemPoolFree(pool1, p2);  // Don't do this!
    
    xrtMemPoolDestroy(pool1);
    xrtMemPoolDestroy(pool2);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [FSMemPool Fixed-Size Memory Pool](api-mempool-fs.en.md)
- [MemUnit Memory Unit Management](api-memunit.en.md)
- [BSMM Block Structured Memory Management](api-bsmm.en.md)
- [Base Basic Function Library](api-base.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#mempool---general-memory-pool)

</div>
