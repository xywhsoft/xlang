# BSMM Block Structure Memory Management Library

> Blocks Struct Memory Management - Efficient fixed-size struct memory pool

[English](api-bsmm.en.md) | [中文](api-bsmm.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Manager Operations](#manager-operations)
- [Memory Allocation](#memory-allocation)
- [Use Cases](#use-cases)
- [Comparison with Other Memory Managers](#comparison-with-other-memory-managers)
- [Best Practices](#best-practices)

---

## Overview

BSMM (Blocks Struct Memory Management) is a block structure memory manager designed for frequent allocation and deallocation of **fixed-size** structures.

### Core Features

- **Paged management**: Each page stores 256 elements, new pages allocated on demand
- **Free list reuse**: Released memory managed via linked list, prioritized for reuse
- **O(1) allocation**: Both allocation and deallocation are constant time operations
- **No fragmentation**: All allocations are same size, no memory fragmentation

### Memory Layout

```
Memory Page Management:
┌─────────┬─────────┬─────────┬─────────┐
│ Page 0  │ Page 1  │ Page 2  │  ...    │  (256 elements per page)
└─────────┴─────────┴─────────┴─────────┘

Index Rules (0 is non-existent member number):
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

---

## Data Structures

### MemPtr_LLNode

Memory pointer singly-linked list node (internal use).

**Definition:**
```c
typedef struct MemPtr_LLNode {
    ptr Ptr;                      // Memory pointer
    struct MemPtr_LLNode* Next;   // Next node
} MemPtr_LLNode;
```

---

### xbsmm_struct

Block structure memory manager data structure.

**Definition:**
```c
typedef struct {
    uint32 ItemLength;        // Memory length per member
    uint32 Count;             // How many members exist in manager
    xparray_struct PageMMU;   // Memory page manager (pointer array)
    MemPtr_LLNode* LL_Free;   // Free memory block linked list
} xbsmm_struct, *xbsmm;
```

**Member Description:**
- `ItemLength` - Byte size of each element
- `Count` - Total allocated elements (including freed ones)
- `PageMMU` - Memory page pointer array, 256 elements per page
- `LL_Free` - Free memory block linked list for reusing released memory

---

## Manager Operations

### xrtBsmmCreate

Create a block structure memory manager.

**Function Prototype:**
```c
XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Size of each element (bytes)

**Return Value:**
- Success: Manager object
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtBsmmDestroy` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
    float score;
} Student;

int main() {
    xrtInit();
    
    // Create student object pool
    xbsmm pool = xrtBsmmCreate(sizeof(Student));
    if (!pool) {
        printf("Creation failed\n");
        xrtUnit();
        return 1;
    }
    
    printf("Element size: %u bytes\n", pool->ItemLength);
    printf("Current count: %u\n", pool->Count);
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmDestroy

Destroy block structure memory manager and release all memory.

**Function Prototype:**
```c
XXAPI void xrtBsmmDestroy(xbsmm objBSMM);
```

**Parameters:**
- `objBSMM` - Manager object

**Notes:**
- Releases all memory pages
- Releases free linked list
- Releases manager structure itself

---

### xrtBsmmInit

Initialize block structure memory manager (for embedded structures).

**Function Prototype:**
```c
XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength);
```

**Parameters:**
- `objBSMM` - Manager object pointer
- `iItemLength` - Size of each element (bytes)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

typedef struct {
    char name[32];
    xbsmm_struct itemPool;  // Embedded manager
} Container;

int main() {
    xrtInit();
    
    Container cont;
    strcpy(cont.name, "MyContainer");
    
    // Initialize embedded manager
    xrtBsmmInit(&cont.itemPool, sizeof(Item));
    
    // Use manager
    Item* item = (Item*)xrtBsmmAlloc(&cont.itemPool);
    item->value = 100;
    printf("Value: %d\n", item->value);
    
    // Release embedded manager memory (not struct itself)
    xrtBsmmUnit(&cont.itemPool);
    
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmUnit

Release internal data of block structure memory manager (but not struct itself).

**Function Prototype:**
```c
XXAPI void xrtBsmmUnit(xbsmm objBSMM);
```

**Parameters:**
- `objBSMM` - Manager object

**Notes:**
- Pairs with `xrtBsmmInit`
- Releases all memory pages and free linked list
- Does not release manager structure itself

---

## Memory Allocation

### xrtBsmmAlloc

Allocate a block of memory from the manager.

**Function Prototype:**
```c
XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM);
```

**Parameters:**
- `objBSMM` - Manager object

**Return Value:**
- Success: Memory pointer
- Failure: `NULL`

**Allocation Strategy:**
1. Prioritize reusing released memory from free linked list
2. If no free memory, allocate from current page
3. If current page is full, allocate new page (256 elements)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xbsmm pool = xrtBsmmCreate(sizeof(Point));
    
    // Allocate point objects
    Point* p1 = (Point*)xrtBsmmAlloc(pool);
    p1->x = 10;
    p1->y = 20;
    
    Point* p2 = (Point*)xrtBsmmAlloc(pool);
    p2->x = 30;
    p2->y = 40;
    
    printf("P1: (%d, %d)\n", p1->x, p1->y);
    printf("P2: (%d, %d)\n", p2->x, p2->y);
    printf("Allocated: %u\n", pool->Count);
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmFree

Free a block of memory (add to free linked list).

**Function Prototype:**
```c
XXAPI void xrtBsmmFree(xbsmm objBSMM, ptr p);
```

**Parameters:**
- `objBSMM` - Manager object
- `p` - Memory pointer to free

**Notes:**
- Memory is not actually freed, but added to free linked list
- Next `xrtBsmmAlloc` will prioritize reusing it
- Actual memory release waits until `xrtBsmmDestroy`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int id; } Node;

int main() {
    xrtInit();
    
    xbsmm pool = xrtBsmmCreate(sizeof(Node));
    
    // Allocate 3 nodes
    Node* n1 = (Node*)xrtBsmmAlloc(pool);
    Node* n2 = (Node*)xrtBsmmAlloc(pool);
    Node* n3 = (Node*)xrtBsmmAlloc(pool);
    
    n1->id = 1;
    n2->id = 2;
    n3->id = 3;
    
    printf("After allocation: Count = %u\n", pool->Count);  // 3
    
    // Free middle node
    xrtBsmmFree(pool, n2);
    
    // Allocate another, will reuse n2's memory
    Node* n4 = (Node*)xrtBsmmAlloc(pool);
    n4->id = 4;
    
    printf("After reuse: Count = %u\n", pool->Count);  // Still 3
    printf("n4 address == n2 address: %s\n", (n4 == n2) ? "Yes" : "No");  // Yes
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmGetPtr_Inline

Get element pointer by index (inline version).

**Function Prototype:**
```c
static inline ptr xrtBsmmGetPtr_Inline(xbsmm objBSMM, uint32 iIdx)
{
    uint32 iBlock = iIdx >> 8;
    uint32 iPos = iIdx & 0xFF;
    str pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
    if ( pBlock ) {
        return &pBlock[iPos * objBSMM->ItemLength];
    } else {
        return NULL;
    }
}
```

**Parameters:**
- `objBSMM` - Manager object
- `iIdx` - Element index (0-based)

**Return Value:**
- Success: Element pointer
- Failure: `NULL`

**Warning:** This function is not recommended for regular use, only for special needs (like traversing all allocated elements).

---

## Use Cases

### 1. Object Pool Pattern

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int health;
    int x, y;
    int active;
} Enemy;

typedef struct {
    xbsmm pool;
    int nextId;
} EnemyManager;

EnemyManager* CreateEnemyManager() {
    EnemyManager* mgr = xrtMalloc(sizeof(EnemyManager));
    mgr->pool = xrtBsmmCreate(sizeof(Enemy));
    mgr->nextId = 1;
    return mgr;
}

Enemy* SpawnEnemy(EnemyManager* mgr, int x, int y) {
    Enemy* e = (Enemy*)xrtBsmmAlloc(mgr->pool);
    if (e) {
        e->id = mgr->nextId++;
        e->health = 100;
        e->x = x;
        e->y = y;
        e->active = 1;
    }
    return e;
}

void KillEnemy(EnemyManager* mgr, Enemy* e) {
    e->active = 0;
    xrtBsmmFree(mgr->pool, e);
}

void DestroyEnemyManager(EnemyManager* mgr) {
    xrtBsmmDestroy(mgr->pool);
    xrtFree(mgr);
}

int main() {
    xrtInit();
    
    EnemyManager* mgr = CreateEnemyManager();
    
    // Spawn enemies
    Enemy* e1 = SpawnEnemy(mgr, 100, 200);
    Enemy* e2 = SpawnEnemy(mgr, 300, 400);
    
    printf("Enemy %d: position (%d, %d)\n", e1->id, e1->x, e1->y);
    printf("Enemy %d: position (%d, %d)\n", e2->id, e2->x, e2->y);
    
    // Kill enemy
    KillEnemy(mgr, e1);
    
    // Spawn new enemy (reuses e1's memory)
    Enemy* e3 = SpawnEnemy(mgr, 500, 600);
    printf("Enemy %d: position (%d, %d)\n", e3->id, e3->x, e3->y);
    
    DestroyEnemyManager(mgr);
    xrtUnit();
    return 0;
}
```

---

### 2. Linked List Node Allocation

```c
#include "xrt.h"
#include <stdio.h>

typedef struct ListNode {
    int value;
    struct ListNode* next;
} ListNode;

typedef struct {
    xbsmm nodePool;
    ListNode* head;
} LinkedList;

void ListInit(LinkedList* list) {
    xrtBsmmInit(&list->nodePool, sizeof(ListNode));
    list->head = NULL;
}

void ListPush(LinkedList* list, int value) {
    ListNode* node = (ListNode*)xrtBsmmAlloc(&list->nodePool);
    node->value = value;
    node->next = list->head;
    list->head = node;
}

void ListPrint(LinkedList* list) {
    ListNode* cur = list->head;
    while (cur) {
        printf("%d -> ", cur->value);
        cur = cur->next;
    }
    printf("NULL\n");
}

void ListDestroy(LinkedList* list) {
    xrtBsmmUnit(&list->nodePool);
    list->head = NULL;
}

int main() {
    xrtInit();
    
    LinkedList list;
    ListInit(&list);
    
    ListPush(&list, 10);
    ListPush(&list, 20);
    ListPush(&list, 30);
    
    ListPrint(&list);  // 30 -> 20 -> 10 -> NULL
    
    ListDestroy(&list);
    xrtUnit();
    return 0;
}
```

---

### 3. Tree Node Allocation

```c
#include "xrt.h"
#include <stdio.h>

typedef struct TreeNode {
    int value;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;

xbsmm g_nodePool;

TreeNode* CreateNode(int value) {
    TreeNode* node = (TreeNode*)xrtBsmmAlloc(g_nodePool);
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

void InOrder(TreeNode* node) {
    if (!node) return;
    InOrder(node->left);
    printf("%d ", node->value);
    InOrder(node->right);
}

int main() {
    xrtInit();
    
    g_nodePool = xrtBsmmCreate(sizeof(TreeNode));
    
    // Build binary tree
    //       5
    //      / \
    //     3   7
    //    / \
    //   1   4
    TreeNode* root = CreateNode(5);
    root->left = CreateNode(3);
    root->right = CreateNode(7);
    root->left->left = CreateNode(1);
    root->left->right = CreateNode(4);
    
    printf("In-order traversal: ");
    InOrder(root);
    printf("\n");  // 1 3 4 5 7
    
    printf("Total nodes: %u\n", g_nodePool->Count);  // 5
    
    xrtBsmmDestroy(g_nodePool);
    xrtUnit();
    return 0;
}
```

---

## Comparison with Other Memory Managers

| Feature | BSMM | Array | malloc/free |
|---------|------|-------|-------------|
| **Fixed size** | ✅ Required | ✅ Required | ❌ Any |
| **Memory reuse** | ✅ Free list | ❌ Manual | ❌ System managed |
| **Index access** | ⚠️ Not recommended | ✅ Efficient | ❌ Not supported |
| **Contiguous memory** | ⚠️ Within page | ✅ Globally | ❌ Not guaranteed |
| **Fragmentation** | ✅ None | ✅ None | ⚠️ Possible |
| **Allocation speed** | ✅ O(1) | ✅ O(1) amortized | ⚠️ System dependent |
| **Use case** | Frequent alloc/free | Sequential access | General purpose |

---

## Best Practices

### 1. Choose Appropriate Scenarios

```c
// ✅ Suitable for BSMM: frequently created/destroyed objects
xbsmm bulletPool = xrtBsmmCreate(sizeof(Bullet));
// May create/destroy hundreds of bullets per frame

// ❌ Not suitable for BSMM: collections needing sequential traversal
// Use xarray or xparray instead
```

### 2. Reuse Rather Than Rebuild

```c
// ✅ Correct: reuse manager
void GameLoop() {
    static xbsmm pool = NULL;
    if (!pool) {
        pool = xrtBsmmCreate(sizeof(Particle));
    }
    // Use pool...
}

// ❌ Wrong: rebuild manager each time
void BadGameLoop() {
    xbsmm pool = xrtBsmmCreate(sizeof(Particle));
    // Use pool...
    xrtBsmmDestroy(pool);  // Wasteful!
}
```

### 3. Watch for Dangling Pointers

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xbsmm pool = xrtBsmmCreate(sizeof(Item));
    
    Item* item = (Item*)xrtBsmmAlloc(pool);
    item->value = 42;
    
    xrtBsmmFree(pool, item);
    
    // ⚠️ Warning: item is now a dangling pointer!
    // printf("%d\n", item->value);  // Undefined behavior
    
    // ✅ Correct practice: set to NULL after free
    item = NULL;
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [MemUnit Memory Unit Management](api-memunit.en.md)
- [FSMemPool Fixed-Size Memory Pool](api-mempool-fs.en.md)
- [MemPool General Memory Pool](api-mempool.en.md)
- [PtrArray Pointer Array](api-ptrarray.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#bsmm-block-structure-memory-management-library)

</div>
