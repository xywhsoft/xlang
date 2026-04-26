# FSMemPool - Fixed-Size Memory Pool

> Fixed-Size Memory Pool - High-performance memory pool for fixed-size objects

[English](api-mempool-fs.en.md) | [中文](api-mempool-fs.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Memory Pool Operations](#memory-pool-operations)
- [Memory Allocation](#memory-allocation)
- [GC Collection](#gc-collection)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)

---

## Overview

FSMemPool (Fixed-Size Memory Pool) is a high-performance memory pool based on MemUnit, designed for frequent allocation and deallocation of **fixed-size** objects, with no capacity limit.

### Core Features

- **Unlimited Capacity**: Auto-expands, no 256 limit
- **Four-List Management**: Idle/Full/Null/Free categories for MemUnits
- **O(1) Allocation**: Constant time allocation in most cases
- **Smart Reuse**: Empty unit caching, avoids frequent allocation at critical states
- **GC Support**: Supports mark-sweep garbage collection

### Architecture Diagram

```
FSMemPool Structure:
┌─────────────────────────────────────────────────────────────────┐
│ ItemLength │ arrMMU (BSMM) │ LL_Idle │ LL_Full │ LL_Null │ LL_Free │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
        ┌───────────────────────────────────┐
        │         MMU_LLNode Array          │
        │  ┌─────┬─────┬─────┬─────┬─────┐  │
        │  │ [0] │ [1] │ [2] │ [3] │ ... │  │
        │  └──┬──┴──┬──┴──┬──┴──┬──┴─────┘  │
        └─────┼─────┼─────┼─────┼───────────┘
              ▼     ▼     ▼     ▼
           MemUnit MemUnit MemUnit MemUnit
           (256ea) (256ea) (256ea) (256ea)

Four-List Categories:
┌────────┬────────────────────────────────────────────────────────┐
│ LL_Idle │ MemUnits with available slots (priority allocation)   │
├────────┼────────────────────────────────────────────────────────┤
│ LL_Full │ Fully loaded MemUnits (no allocation, await release)  │
├────────┼────────────────────────────────────────────────────────┤
│ LL_Null │ Completely empty MemUnits (cache, max 1)              │
├────────┼────────────────────────────────────────────────────────┤
│ LL_Free │ Released Flag slots (reuse Flags to avoid conflicts) │
└────────┴────────────────────────────────────────────────────────┘
```

### Allocation Strategy

1. Allocate from MemUnit at head of `LL_Idle` list
2. If `LL_Idle` is empty:
   - Priority: use cached spare unit from `LL_Null`
   - Second: reuse Flag slot from `LL_Free` to create new unit
   - Last: create brand new MemUnit
3. After allocation check: if MemUnit is full, move to `LL_Full`

---

## Data Structures

### MMU_LLNode

MemUnit linked list node structure.

**Definition:**
```c
typedef struct MMU_LLNode {
    uint32 Flag;                    // Flag prefix (for identifying owner unit)
    xmemunit objMMU;                // Pointer to MemUnit
    struct MMU_LLNode* Prev;        // Previous node
    struct MMU_LLNode* Next;        // Next node
} MMU_LLNode;
```

**Notes:**
- `Flag` stored in high bits of each element's `ItemFlag`, used to locate MemUnit during release
- Doubly linked list structure supports fast insertion and deletion

---

### xfsmempool_struct

Fixed-size memory pool structure.

**Definition:**
```c
typedef struct {
    uint32 ItemLength;          // Element memory length
    xbsmm_struct arrMMU;        // MMU_LLNode array manager
    MMU_LLNode* LL_Idle;        // List of units with available slots
    MMU_LLNode* LL_Full;        // List of fully loaded units
    MMU_LLNode* LL_Null;        // Spare empty unit (max 1)
    MMU_LLNode* LL_Free;        // List of released Flag slots
} xfsmempool_struct, *xfsmempool;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `ItemLength` | Size of each element's data |
| `arrMMU` | Uses BSMM to manage MMU_LLNode array |
| `LL_Idle` | Head of list with MemUnits having available slots |
| `LL_Full` | Head of list with fully loaded MemUnits |
| `LL_Null` | Cached empty MemUnit (avoids critical state oscillation) |
| `LL_Free` | List of released unit Flags (for Flag reuse) |

---

## Memory Pool Operations

### xrtFSMemPoolCreate

Create a fixed-size memory pool.

**Function Prototype:**
```c
XXAPI xfsmempool xrtFSMemPoolCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Size of each element's data (excluding 4-byte header)

**Return Value:**
- Success: Memory pool pointer
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} User;

int main() {
    xrtInit();
    
    // Create User object pool
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(User));
    if (pool) {
        printf("Memory pool created successfully\n");
        printf("Element size: %u bytes\n", pool->ItemLength);
        
        xrtFSMemPoolDestroy(pool);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFSMemPoolDestroy

Destroy memory pool.

**Function Prototype:**
```c
XXAPI void xrtFSMemPoolDestroy(xfsmempool objMM);
```

**Parameters:**
- `objMM` - Memory pool object

**Notes:**
- Releases all MemUnits and internal structures
- All allocated pointers become invalid after destruction

---

### xrtFSMemPoolInit

Initialize memory pool (for stack or embedded structures).

**Function Prototype:**
```c
XXAPI void xrtFSMemPoolInit(xfsmempool objMM, uint32 iItemLength);
```

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

typedef struct {
    int config;
    xfsmempool_struct itemPool;  // Embedded memory pool
} Manager;

int main() {
    xrtInit();
    
    Manager mgr;
    mgr.config = 100;
    xrtFSMemPoolInit(&mgr.itemPool, sizeof(Item));
    
    // Use embedded memory pool
    Item* item = (Item*)xrtFSMemPoolAlloc(&mgr.itemPool);
    item->value = 42;
    printf("Value: %d\n", item->value);
    
    xrtFSMemPoolFree(&mgr.itemPool, item);
    xrtFSMemPoolUnit(&mgr.itemPool);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFSMemPoolUnit

Release memory pool internal resources (for stack or embedded structures).

**Function Prototype:**
```c
XXAPI void xrtFSMemPoolUnit(xfsmempool objMM);
```

---

## Memory Allocation

### xrtFSMemPoolAlloc

Allocate one element from memory pool.

**Function Prototype:**
```c
XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM);
```

**Parameters:**
- `objMM` - Memory pool object

**Return Value:**
- Success: Element pointer
- Failure: `NULL` (out of memory)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    double value;
} Record;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Record));
    
    // Allocate multiple objects (no 256 limit)
    Record* records[1000];
    for (int i = 0; i < 1000; i++) {
        records[i] = (Record*)xrtFSMemPoolAlloc(pool);
        records[i]->id = i;
        records[i]->value = i * 1.5;
    }
    
    printf("Successfully allocated 1000 Records\n");
    printf("Record[500]: id=%d, value=%.1f\n", 
           records[500]->id, records[500]->value);
    
    // Release
    for (int i = 0; i < 1000; i++) {
        xrtFSMemPoolFree(pool, records[i]);
    }
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtFSMemPoolFree

Release element from memory pool.

**Function Prototype:**
```c
XXAPI void xrtFSMemPoolFree(xfsmempool objMM, ptr p);
```

**Parameters:**
- `objMM` - Memory pool object
- `p` - Element pointer

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Item));
    
    Item* item1 = (Item*)xrtFSMemPoolAlloc(pool);
    Item* item2 = (Item*)xrtFSMemPoolAlloc(pool);
    Item* item3 = (Item*)xrtFSMemPoolAlloc(pool);
    
    item1->value = 10;
    item2->value = 20;
    item3->value = 30;
    
    // Release middle element
    xrtFSMemPoolFree(pool, item2);
    
    // Next allocation may reuse item2's slot
    Item* item4 = (Item*)xrtFSMemPoolAlloc(pool);
    printf("item4 address: %p (may be same as item2: %p)\n", item4, item2);
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

**Release Flow:**
1. Read `ItemFlag` from 4 bytes before pointer
2. Extract MemUnit index (high bits) and element index (low bits)
3. Call corresponding MemUnit's release function
4. Check MemUnit state, adjust list category if necessary

---

## GC Collection

### xrtFSMemPoolGC_Mark

Mark element as GC in-use.

**Definition (macro):**
```c
#define xrtFSMemPoolGC_Mark xrtMemUnitGC_Mark
```

**Parameters:**
- Element pointer

---

### xrtFSMemPoolGC

Execute one round of garbage collection.

**Function Prototype:**
```c
XXAPI void xrtFSMemPoolGC(xfsmempool objMM, bool bFreeMark);
```

**Parameters:**
- `objMM` - Memory pool object
- `bFreeMark` - Collection mode
  - `TRUE` - Collect **marked** elements
  - `FALSE` - Collect **unmarked** elements (standard Mark-Sweep)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int refCount;
} Object;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Object));
    
    // Allocate 10 objects
    Object* objs[10];
    for (int i = 0; i < 10; i++) {
        objs[i] = (Object*)xrtFSMemPoolAlloc(pool);
        objs[i]->id = i;
        objs[i]->refCount = (i % 3 == 0) ? 1 : 0;  // 0,3,6,9 have references
    }
    
    printf("Allocated 10 objects before GC\n");
    
    // Mark referenced objects
    for (int i = 0; i < 10; i++) {
        if (objs[i]->refCount > 0) {
            xrtFSMemPoolGC_Mark(objs[i]);
            printf("Marked object %d\n", objs[i]->id);
        }
    }
    
    // Execute GC (collect unmarked objects)
    xrtFSMemPoolGC(pool, FALSE);
    printf("GC complete, collected 6 objects\n");
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

**GC Flow:**
1. Traverse all MemUnits in `LL_Idle` and `LL_Full`
2. Call `xrtMemUnitGC` on each MemUnit
3. Reclassify MemUnits: empty ones to `LL_Null`/`LL_Free`, partially used to `LL_Idle`

---

## Usage Scenarios

### 1. High-Frequency Object Allocation

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int type;
    char data[64];
} Message;

xfsmempool g_msgPool = NULL;

Message* CreateMessage(int type, const char* data) {
    Message* msg = (Message*)xrtFSMemPoolAlloc(g_msgPool);
    if (msg) {
        msg->type = type;
        strncpy(msg->data, data, 63);
        msg->data[63] = '\0';
    }
    return msg;
}

void DestroyMessage(Message* msg) {
    xrtFSMemPoolFree(g_msgPool, msg);
}

int main() {
    xrtInit();
    
    g_msgPool = xrtFSMemPoolCreate(sizeof(Message));
    
    // Simulate high-frequency message processing
    for (int i = 0; i < 10000; i++) {
        Message* msg = CreateMessage(i % 10, "test message");
        // Process message...
        DestroyMessage(msg);
    }
    
    printf("Processed 10000 messages\n");
    
    xrtFSMemPoolDestroy(g_msgPool);
    xrtUnit();
    return 0;
}
```

---

### 2. Linked List Node Pool

```c
#include "xrt.h"
#include <stdio.h>

typedef struct ListNode {
    struct ListNode* next;
    int value;
} ListNode;

int main() {
    xrtInit();
    
    xfsmempool nodePool = xrtFSMemPoolCreate(sizeof(ListNode));
    ListNode* head = NULL;
    
    // Build linked list
    for (int i = 0; i < 100; i++) {
        ListNode* node = (ListNode*)xrtFSMemPoolAlloc(nodePool);
        node->value = i;
        node->next = head;
        head = node;
    }
    
    // Traverse linked list
    int count = 0;
    ListNode* current = head;
    while (current) {
        count++;
        current = current->next;
    }
    printf("Linked list node count: %d\n", count);
    
    // Release linked list
    current = head;
    while (current) {
        ListNode* next = current->next;
        xrtFSMemPoolFree(nodePool, current);
        current = next;
    }
    
    xrtFSMemPoolDestroy(nodePool);
    xrtUnit();
    return 0;
}
```

---

### 3. Object Management with GC

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int marked;  // For application-level marking
} GCObject;

xfsmempool g_heap = NULL;
GCObject* g_roots[10];  // Root objects
int g_rootCount = 0;

GCObject* AllocObject(int id) {
    GCObject* obj = (GCObject*)xrtFSMemPoolAlloc(g_heap);
    obj->id = id;
    obj->marked = 0;
    return obj;
}

void MarkPhase() {
    // Mark from roots
    for (int i = 0; i < g_rootCount; i++) {
        if (g_roots[i]) {
            xrtFSMemPoolGC_Mark(g_roots[i]);
        }
    }
}

void SweepPhase() {
    xrtFSMemPoolGC(g_heap, FALSE);
}

int main() {
    xrtInit();
    
    g_heap = xrtFSMemPoolCreate(sizeof(GCObject));
    
    // Allocate objects
    g_roots[g_rootCount++] = AllocObject(1);
    g_roots[g_rootCount++] = AllocObject(2);
    AllocObject(3);  // No root reference
    AllocObject(4);  // No root reference
    g_roots[g_rootCount++] = AllocObject(5);
    
    printf("Allocated 5 objects, 3 have root references\n");
    
    // GC cycle
    MarkPhase();
    SweepPhase();
    
    printf("GC complete, collected 2 unreferenced objects\n");
    
    xrtFSMemPoolDestroy(g_heap);
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Choose the Right Memory Manager

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // FSMemPool is suitable for:
    // - All objects are same size
    // - Object count may exceed 256
    // - Need GC support
    
    // Not suitable for:
    // - Variable object sizes → use MemPool
    // - Fixed small object count → use Array
    // - Need key-value lookup → use Dict
    
    printf("FSMemPool best scenarios:\n");
    printf("1. Linked list/tree node allocation\n");
    printf("2. Message/event objects\n");
    printf("3. Temporary computation objects\n");
    printf("4. Object heap needing GC\n");
    
    xrtUnit();
    return 0;
}
```

---

### 2. Avoid Cross-Pool Release

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xfsmempool pool1 = xrtFSMemPoolCreate(sizeof(Item));
    xfsmempool pool2 = xrtFSMemPoolCreate(sizeof(Item));
    
    Item* item1 = (Item*)xrtFSMemPoolAlloc(pool1);
    Item* item2 = (Item*)xrtFSMemPoolAlloc(pool2);
    
    // ✓ Correct: release to corresponding pool
    xrtFSMemPoolFree(pool1, item1);
    xrtFSMemPoolFree(pool2, item2);
    
    // ✗ Wrong: cross-pool release causes undefined behavior
    // xrtFSMemPoolFree(pool1, item2);  // Dangerous!
    
    xrtFSMemPoolDestroy(pool1);
    xrtFSMemPoolDestroy(pool2);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Batch Operation Optimization

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int data[16]; } Block;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Block));
    
    // Pre-allocate to avoid frequent expansion
    Block* blocks[1000];
    
    // Batch allocation
    for (int i = 0; i < 1000; i++) {
        blocks[i] = (Block*)xrtFSMemPoolAlloc(pool);
    }
    printf("Batch allocated 1000 blocks\n");
    
    // Batch release (order doesn't matter)
    for (int i = 999; i >= 0; i--) {
        xrtFSMemPoolFree(pool, blocks[i]);
    }
    printf("Batch release complete\n");
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

## Comparison with Other Memory Managers

| Feature | FSMemPool | MemUnit | MemPool | malloc/free |
|---------|-----------|---------|---------|-------------|
| **Capacity** | Unlimited | 256 | Unlimited | Unlimited |
| **Element Size** | Fixed | Fixed | Variable | Variable |
| **Allocation Speed** | O(1) | O(1) | O(1) | O(n) |
| **Memory Fragmentation** | None | None | Low | High |
| **GC Support** | ✅ | ✅ | ✅ | ❌ |
| **Complexity** | Medium | Low | High | - |
| **Use Case** | Fixed-size object pool | Low-level components | Multi-size objects | General |

---

## Related Documentation

- [MemUnit Memory Unit Management](api-memunit.en.md)
- [BSMM Block Structured Memory Management](api-bsmm.en.md)
- [MemPool General Memory Pool](api-mempool.en.md)
- [LList Doubly Linked List](api-llist.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#fsmempool---fixed-size-memory-pool)

</div>
