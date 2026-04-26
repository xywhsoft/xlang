# LList - Doubly Linked List Library

> Doubly linked list with automatic memory management (based on FSMemPool)

[English](api-llist.en.md) | [中文](api-llist.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [List Management](#list-management)
- [Node Operations](#node-operations)
- [Traversal](#traversal)
- [Usage Scenarios](#usage-scenarios)
- [Comparison with LList Base](#comparison-with-llist-base)
- [Best Practices](#best-practices)

---

## Overview

LList (Linked List) is a doubly linked list implementation based on FSMemPool, with automatic node memory allocation and deallocation.

### Core Features

- **Automatic Memory Management**: Uses FSMemPool to manage node memory
- **Bidirectional Traversal**: Supports forward and backward traversal
- **O(1) Insert/Delete**: Constant time insertion and deletion at any position
- **GC Support**: Underlying memory pool supports garbage collection

### Node Memory Layout

```
Node Memory Structure:
┌────────────────────────────────────────┐
│           xllistnode_struct            │  ← node pointer points here
│   ┌──────────────┬──────────────┐      │
│   │     Prev     │     Next     │      │
│   │  (pointer)   │  (pointer)   │      │
│   └──────────────┴──────────────┘      │
├────────────────────────────────────────┤
│             User Data                  │  ← &node[1] points here
│         (iItemLength bytes)            │
└────────────────────────────────────────┘
```

**Key:** User data is located at `&node[1]`, i.e., after the node structure!

---

## Data Structures

### xllist_struct

List management structure.

**Definition:**
```c
typedef struct {
    xllistnode FirstNode;       // First node pointer
    xllistnode LastNode;        // Last node pointer
    uint32 Count;               // Node count
    xfsmempool_struct objMM;    // Memory pool manager
} xllist_struct, *xllist;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `FirstNode` | List first node, `NULL` for empty list |
| `LastNode` | List last node, `NULL` for empty list |
| `Count` | Current node count |
| `objMM` | FSMemPool memory pool, auto-manages node memory |

---

## List Management

### xrtLListCreate

Create a list.

**Function Prototype:**
```c
XXAPI xllist xrtLListCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Node user data size (bytes)

**Return Value:**
- Success: List object
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtLListDestroy` to free

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Person;

int main() {
    xrtInit();
    
    // Create list with nodes storing Person structure
    xllist list = xrtLListCreate(sizeof(Person));
    printf("List created, node count: %u\n", list->Count);  // 0
    
    // Add node
    xllistnode node = xrtLListInsertNext(list, NULL);
    Person* p = (Person*)&node[1];  // Get user data pointer
    p->id = 1;
    strcpy(p->name, "Alice");
    
    printf("After adding, node count: %u\n", list->Count);  // 1
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtLListDestroy

Destroy a list.

**Function Prototype:**
```c
XXAPI void xrtLListDestroy(xllist objLL);
```

**Parameters:**
- `objLL` - List object

**Notes:**
- Releases all node memory
- Releases memory pool
- Releases list structure itself

---

### xrtLListInit

Initialize list (for stack or embedded usage).

**Function Prototype:**
```c
XXAPI void xrtLListInit(xllist objLL, uint32 iItemLength);
```

**Parameters:**
- `objLL` - List pointer
- `iItemLength` - Node user data size

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllist_struct taskList;
    int maxTasks;
} TaskManager;

int main() {
    xrtInit();
    
    // List embedded in other structure
    TaskManager mgr;
    mgr.maxTasks = 100;
    xrtLListInit(&mgr.taskList, sizeof(int));
    
    // Add task
    xllistnode node = xrtLListInsertNext(&mgr.taskList, NULL);
    int* taskId = (int*)&node[1];
    *taskId = 42;
    
    printf("Task count: %u\n", mgr.taskList.Count);  // 1
    
    xrtLListUnit(&mgr.taskList);
    xrtUnit();
    return 0;
}
```

---

### xrtLListUnit

Release list resources (does not free structure itself).

**Function Prototype:**
```c
XXAPI void xrtLListUnit(xllist objLL);
```

**Parameters:**
- `objLL` - List object

**Notes:**
- Used paired with `xrtLListInit`
- Releases all nodes and memory pool, but not `objLL` itself

---

### xrtLListRemoveAll / xrtLListClear

Remove all members / Clear list (macros).

**Definition:**
```c
#define xrtLListRemoveAll xrtLListUnit
#define xrtLListClear xrtLListUnit
```

**Notes:** These macros are equivalent to `xrtLListUnit`, releasing all nodes.

---

## Node Operations

### xrtLListInsertNext

Insert a new node after a reference node.

**Function Prototype:**
```c
XXAPI xllistnode xrtLListInsertNext(xllist objLL, xllistnode objNode);
```

**Parameters:**
- `objLL` - List object
- `objNode` - Reference node (`NULL` means insert at end of list)

**Return Value:**
- Success: New node pointer
- Failure: `NULL`

**Behavior:**
- `objNode != NULL`: Insert after `objNode`
- `objNode == NULL`: Insert at end of list (becomes new `LastNode`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} IntData;

void PrintList(xllist list) {
    printf("List: ");
    xllistnode node = list->FirstNode;
    while (node) {
        IntData* d = (IntData*)&node[1];
        printf("%d ", d->value);
        node = node->Next;
    }
    printf("(total %u)\n", list->Count);
}

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(IntData));
    
    // Append to end (objNode = NULL)
    for (int i = 1; i <= 3; i++) {
        xllistnode node = xrtLListInsertNext(list, NULL);
        IntData* d = (IntData*)&node[1];
        d->value = i * 10;
    }
    PrintList(list);  // List: 10 20 30 (total 3)
    
    // Insert after first node
    xllistnode n15 = xrtLListInsertNext(list, list->FirstNode);
    ((IntData*)&n15[1])->value = 15;
    PrintList(list);  // List: 10 15 20 30 (total 4)
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtLListInsertPrev

Insert a new node before a reference node.

**Function Prototype:**
```c
XXAPI xllistnode xrtLListInsertPrev(xllist objLL, xllistnode objNode);
```

**Parameters:**
- `objLL` - List object
- `objNode` - Reference node (`NULL` means insert at beginning of list)

**Return Value:**
- Success: New node pointer
- Failure: `NULL`

**Behavior:**
- `objNode != NULL`: Insert before `objNode`
- `objNode == NULL`: Insert at beginning of list (becomes new `FirstNode`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } IntData;

void PrintList(xllist list) {
    printf("List: ");
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", ((IntData*)&n[1])->value);
        n = n->Next;
    }
    printf("\n");
}

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(IntData));
    
    // Insert at beginning (objNode = NULL)
    for (int i = 3; i >= 1; i--) {
        xllistnode n = xrtLListInsertPrev(list, NULL);
        ((IntData*)&n[1])->value = i * 10;
    }
    PrintList(list);  // List: 10 20 30
    
    // Insert before last node
    xllistnode n25 = xrtLListInsertPrev(list, list->LastNode);
    ((IntData*)&n25[1])->value = 25;
    PrintList(list);  // List: 10 20 25 30
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtLListRemove

Remove a node.

**Function Prototype:**
```c
XXAPI void xrtLListRemove(xllist objLL, xllistnode objNode);
```

**Parameters:**
- `objLL` - List object
- `objNode` - Node to remove

**Notes:**
- Removes node from list
- Automatically frees node memory (returns to memory pool)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } IntData;

void PrintList(xllist list) {
    printf("List (total %u): ", list->Count);
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", ((IntData*)&n[1])->value);
        n = n->Next;
    }
    printf("\n");
}

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(IntData));
    
    // Create list: 10 -> 20 -> 30 -> 40
    xllistnode nodes[4];
    for (int i = 0; i < 4; i++) {
        nodes[i] = xrtLListInsertNext(list, NULL);
        ((IntData*)&nodes[i][1])->value = (i + 1) * 10;
    }
    PrintList(list);  // List (total 4): 10 20 30 40
    
    // Remove middle node (20)
    xrtLListRemove(list, nodes[1]);
    PrintList(list);  // List (total 3): 10 30 40
    
    // Remove first node
    xrtLListRemove(list, list->FirstNode);
    PrintList(list);  // List (total 2): 30 40
    
    // Remove last node
    xrtLListRemove(list, list->LastNode);
    PrintList(list);  // List (total 1): 30
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## Traversal

### Forward Traversal

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Person;

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(Person));
    
    // Add data
    char* names[] = {"Alice", "Bob", "Charlie"};
    for (int i = 0; i < 3; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        Person* p = (Person*)&n[1];
        p->id = i + 1;
        strcpy(p->name, names[i]);
    }
    
    // Forward traversal (head to tail)
    printf("Forward traversal:\n");
    xllistnode node = list->FirstNode;
    while (node) {
        Person* p = (Person*)&node[1];
        printf("  ID=%d, Name=%s\n", p->id, p->name);
        node = node->Next;
    }
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### Reverse Traversal

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(int));
    
    // Add 1, 2, 3
    for (int i = 1; i <= 3; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        *(int*)&n[1] = i * 10;
    }
    
    // Reverse traversal (tail to head)
    printf("Reverse traversal: ");
    xllistnode node = list->LastNode;
    while (node) {
        printf("%d ", *(int*)&node[1]);
        node = node->Prev;
    }
    printf("\n");  // Reverse traversal: 30 20 10
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### Safe Traversal (Supports Deletion)

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(int));
    
    // Add 10, 20, 30, 40, 50
    for (int i = 1; i <= 5; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        *(int*)&n[1] = i * 10;
    }
    
    // Safe traversal, delete even numbers
    xllistnode current = list->FirstNode;
    while (current) {
        xllistnode next = current->Next;  // Save next first
        int value = *(int*)&current[1];
        
        if (value % 20 == 0) {
            printf("Deleting %d\n", value);
            xrtLListRemove(list, current);
        }
        
        current = next;  // Use saved next
    }
    
    // Print remaining
    printf("Remaining: ");
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", *(int*)&n[1]);
        n = n->Next;
    }
    printf("\n");  // Remaining: 10 30 50
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## Usage Scenarios

### 1. Queue Implementation

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllist queue;
} Queue;

Queue* CreateQueue(size_t itemSize) {
    Queue* q = xrtMalloc(sizeof(Queue));
    q->queue = xrtLListCreate(itemSize);
    return q;
}

void DestroyQueue(Queue* q) {
    xrtLListDestroy(q->queue);
    xrtFree(q);
}

void Enqueue(Queue* q, ptr item, size_t size) {
    xllistnode node = xrtLListInsertNext(q->queue, NULL);
    if (node) {
        memcpy(&node[1], item, size);
    }
}

bool Dequeue(Queue* q, ptr item, size_t size) {
    if (!q->queue->FirstNode) return FALSE;
    xllistnode node = q->queue->FirstNode;
    memcpy(item, &node[1], size);
    xrtLListRemove(q->queue, node);
    return TRUE;
}

int main() {
    xrtInit();
    
    Queue* q = CreateQueue(sizeof(int));
    
    // Enqueue
    for (int i = 1; i <= 5; i++) {
        Enqueue(q, &i, sizeof(int));
        printf("Enqueue: %d\n", i);
    }
    
    // Dequeue
    int value;
    while (Dequeue(q, &value, sizeof(int))) {
        printf("Dequeue: %d\n", value);
    }
    
    DestroyQueue(q);
    xrtUnit();
    return 0;
}
```

---

### 2. LRU Cache

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int key;
    int value;
} CacheItem;

int main() {
    xrtInit();
    
    xllist lru_list = xrtLListCreate(sizeof(CacheItem));
    int capacity = 3;
    
    // Add cache items
    for (int i = 1; i <= 4; i++) {
        // If over capacity, evict oldest (tail)
        if (lru_list->Count >= (uint32)capacity) {
            xllistnode oldest = lru_list->LastNode;
            CacheItem* item = (CacheItem*)&oldest[1];
            printf("Evicting: key=%d, value=%d\n", item->key, item->value);
            xrtLListRemove(lru_list, oldest);
        }
        
        // Insert at head (most recently used)
        xllistnode node = xrtLListInsertPrev(lru_list, NULL);
        CacheItem* item = (CacheItem*)&node[1];
        item->key = i;
        item->value = i * 100;
        printf("Adding: key=%d, value=%d\n", item->key, item->value);
    }
    
    // Print current cache
    printf("\nCurrent cache (newest to oldest):\n");
    xllistnode n = lru_list->FirstNode;
    while (n) {
        CacheItem* item = (CacheItem*)&n[1];
        printf("  key=%d, value=%d\n", item->key, item->value);
        n = n->Next;
    }
    
    xrtLListDestroy(lru_list);
    xrtUnit();
    return 0;
}
```

---

### 3. Task Scheduler

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int priority;  // Lower number = higher priority
    char name[32];
} Task;

void InsertByPriority(xllist list, Task* newTask) {
    xllistnode pos = list->FirstNode;
    
    // Find first position with lower priority
    while (pos) {
        Task* t = (Task*)&pos[1];
        if (t->priority > newTask->priority) {
            break;
        }
        pos = pos->Next;
    }
    
    xllistnode node;
    if (pos) {
        node = xrtLListInsertPrev(list, pos);
    } else {
        node = xrtLListInsertNext(list, NULL);
    }
    
    if (node) {
        memcpy(&node[1], newTask, sizeof(Task));
    }
}

int main() {
    xrtInit();
    
    xllist taskList = xrtLListCreate(sizeof(Task));
    
    // Add tasks (unordered)
    Task tasks[] = {
        {1, 3, "Low Priority"},
        {2, 1, "High Priority"},
        {3, 2, "Medium Priority"},
        {4, 1, "High Priority 2"}
    };
    
    for (int i = 0; i < 4; i++) {
        InsertByPriority(taskList, &tasks[i]);
    }
    
    // Print sorted task list
    printf("Task list (by priority):\n");
    xllistnode n = taskList->FirstNode;
    while (n) {
        Task* t = (Task*)&n[1];
        printf("  [%d] %s (priority=%d)\n", t->id, t->name, t->priority);
        n = n->Next;
    }
    
    xrtLListDestroy(taskList);
    xrtUnit();
    return 0;
}
```

---

## Comparison with LList Base

| Feature | LList | LList Base |
|---------|-------|------------|
| **Memory Management** | Automatic (FSMemPool) | Manual |
| **Node Allocation** | `xrtLListInsert*` auto allocates | User allocates |
| **Node Deallocation** | `xrtLListRemove` auto frees | User frees |
| **GC Support** | ✅ Underlying FSMemPool supports | ❌ None |
| **Flexibility** | Medium | Highest |
| **Use Case** | General list needs | Custom memory management |
| **Header File** | `llist.h` | `llist_base.h` |

### Selection Guide

- **Use LList**: Most scenarios, need automatic memory management
- **Use LList Base**: Embedded environments, custom memory allocators, need complete memory control

---

## Best Practices

### 1. Correctly Access User Data

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
    char name[32];
} MyData;

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(MyData));
    xllistnode node = xrtLListInsertNext(list, NULL);
    
    // ✅ Correct: Use &node[1] to get user data
    MyData* data = (MyData*)&node[1];
    data->value = 42;
    strcpy(data->name, "Test");
    
    // ❌ Wrong: Directly use node as data (node is list node structure)
    // MyData* wrong = (MyData*)node;  // This would overwrite Prev/Next pointers!
    
    printf("Value: %d, Name: %s\n", data->value, data->name);
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 2. Safe Deletion Traversal

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(int));
    
    // Add 1-10
    for (int i = 1; i <= 10; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        *(int*)&n[1] = i;
    }
    
    // ❌ Wrong: Continue using node->Next after deletion
    /*
    xllistnode node = list->FirstNode;
    while (node) {
        if (*(int*)&node[1] % 2 == 0) {
            xrtLListRemove(list, node);  // node is now invalid
        }
        node = node->Next;  // Error! node has been freed
    }
    */
    
    // ✅ Correct: Save Next first
    xllistnode current = list->FirstNode;
    while (current) {
        xllistnode next = current->Next;  // Save first
        if (*(int*)&current[1] % 2 == 0) {
            xrtLListRemove(list, current);
        }
        current = next;  // Use saved value
    }
    
    printf("Remaining (odd): ");
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", *(int*)&n[1]);
        n = n->Next;
    }
    printf("\n");  // 1 3 5 7 9
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 3. Use Helper Macros to Simplify Code

```c
#include "xrt.h"
#include <stdio.h>

// Define helper macros
#define LLIST_DATA(node, type) ((type*)&(node)[1])
#define LLIST_FOREACH(list, node) \
    for (xllistnode node = (list)->FirstNode; node; node = node->Next)
#define LLIST_FOREACH_REVERSE(list, node) \
    for (xllistnode node = (list)->LastNode; node; node = node->Prev)

typedef struct {
    int id;
    char name[32];
} Item;

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(Item));
    
    // Add data
    char* names[] = {"Alpha", "Beta", "Gamma"};
    for (int i = 0; i < 3; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        Item* item = LLIST_DATA(n, Item);
        item->id = i + 1;
        strcpy(item->name, names[i]);
    }
    
    // Use macro for traversal
    printf("Forward:\n");
    LLIST_FOREACH(list, n) {
        Item* item = LLIST_DATA(n, Item);
        printf("  [%d] %s\n", item->id, item->name);
    }
    
    printf("Reverse:\n");
    LLIST_FOREACH_REVERSE(list, n) {
        Item* item = LLIST_DATA(n, Item);
        printf("  [%d] %s\n", item->id, item->name);
    }
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Stack](api-stack.en.md)
- [List](api-list.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#llist---doubly-linked-list-library)

</div>
