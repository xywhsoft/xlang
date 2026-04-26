# LList Base - Linked List Base Library

> Doubly linked list low-level operations with user-managed node memory

[English](api-llist-base.en.md) | [中文](api-llist-base.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [List Management](#list-management)
- [Node Operations](#node-operations)
- [Usage Scenarios](#usage-scenarios)
- [Comparison with LList](#comparison-with-llist)
- [Best Practices](#best-practices)

---

## Overview

LList Base (Linked List Base) is the underlying implementation of doubly linked lists, providing basic linked list operations. Unlike the high-level linked list (LList), users need to **manage node memory themselves**.

### Core Features

- **Low-level Operations**: Only manages list structure, not node memory
- **High Flexibility**: Nodes can come from any memory allocator
- **Zero Overhead**: No additional memory management overhead
- **Embedded-Friendly**: Suitable for custom memory management scenarios

### List Structure

```
xllistbase_struct:
┌────────────┬────────────┬────────────┐
│ FirstNode  │  LastNode  │   Count    │
└─────┬──────┴─────┬──────┴────────────┘
      │            │
      ▼            ▼
┌─────────┐   ┌─────────┐   ┌─────────┐
│  Node1  │◄─►│  Node2  │◄─►│  Node3  │
│ Prev=N  │   │         │   │ Next=N  │
└─────────┘   └─────────┘   └─────────┘
      ▲                           ▲
      │                           │
  FirstNode                   LastNode
```

### Node Memory Layout

When defining custom node structures, **`xllistnode_struct` must be the first member**:

```c
// Custom node structure
typedef struct {
    xllistnode_struct node;  // Must be first member!
    // User data...
    int id;
    char name[32];
} MyNode;
```

---

## Data Structures

### xllistnode_struct

Linked list node base structure.

**Definition:**
```c
typedef struct LList_NodeBase {
    struct LList_NodeBase* Prev;  // Previous node pointer
    struct LList_NodeBase* Next;  // Next node pointer
} xllistnode_struct, *xllistnode;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `Prev` | Points to previous node, `NULL` for first node |
| `Next` | Points to next node, `NULL` for last node |

---

### xllistbase_struct

Linked list management structure.

**Definition:**
```c
typedef struct {
    xllistnode FirstNode;  // First node pointer
    xllistnode LastNode;   // Last node pointer
    uint32 Count;          // Node count
} xllistbase_struct, *xllistbase;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `FirstNode` | List first node, `NULL` for empty list |
| `LastNode` | List last node, `NULL` for empty list |
| `Count` | Current node count |

---

## List Management

### xrtLLB_Init

Initialize list (macro).

**Definition:**
```c
#define xrtLLB_Init(o) (o)->FirstNode = NULL; (o)->LastNode = NULL; (o)->Count = 0
```

**Parameters:**
- `o` - List object pointer

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Stack-allocated list manager
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    printf("List initialized\n");
    printf("First node: %p\n", list.FirstNode);  // (nil)
    printf("Last node: %p\n", list.LastNode);    // (nil)
    printf("Node count: %u\n", list.Count);      // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_Unit

Release list (macro).

**Definition:**
```c
#define xrtLLB_Unit xrtLLB_Init
```

**Notes:**
- Only resets list state, **does not free node memory**
- User must free all nodes themselves

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} MyNode;

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // Add nodes (user allocates memory)
    MyNode* n1 = xrtMalloc(sizeof(MyNode));
    n1->value = 100;
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n1);
    
    MyNode* n2 = xrtMalloc(sizeof(MyNode));
    n2->value = 200;
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n2);
    
    printf("Node count: %u\n", list.Count);  // 2
    
    // Free all nodes (user responsibility)
    xllistnode current = list.FirstNode;
    while (current) {
        xllistnode next = current->Next;
        xrtFree(current);
        current = next;
    }
    
    // Reset list state
    xrtLLB_Unit(&list);
    printf("After free node count: %u\n", list.Count);  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_RemoveAll / xrtLLB_Clear

Remove all members / Clear manager (macros).

**Definition:**
```c
#define xrtLLB_RemoveAll xrtLLB_Unit
#define xrtLLB_Clear xrtLLB_Unit
```

**Warning:** These macros only reset list state, they do not free node memory!

---

## Node Operations

### xrtLLB_InsertPrev

Insert a new node before a reference node.

**Function Prototype:**
```c
XXAPI void xrtLLB_InsertPrev(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
```

**Parameters:**
- `objLLB` - List object
- `objNode` - Reference node (`NULL` means insert at the front of list)
- `objNewNode` - New node to insert

**Behavior:**
- `objNode != NULL`: Insert before `objNode`
- `objNode == NULL`: Insert at front of list (becomes new `FirstNode`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

IntNode* CreateNode(int value) {
    IntNode* n = xrtMalloc(sizeof(IntNode));
    n->value = value;
    return n;
}

void PrintList(xllistbase list) {
    printf("List: ");
    xllistnode current = list->FirstNode;
    while (current) {
        printf("%d ", ((IntNode*)current)->value);
        current = current->Next;
    }
    printf("(total %u)\n", list->Count);
}

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // Insert in empty list (becomes first node)
    IntNode* n2 = CreateNode(20);
    xrtLLB_InsertPrev(&list, NULL, (xllistnode)n2);
    PrintList(&list);  // List: 20 (total 1)
    
    // Insert before first node
    IntNode* n1 = CreateNode(10);
    xrtLLB_InsertPrev(&list, list.FirstNode, (xllistnode)n1);
    PrintList(&list);  // List: 10 20 (total 2)
    
    // Insert before specific node
    IntNode* n15 = CreateNode(15);
    xrtLLB_InsertPrev(&list, (xllistnode)n2, (xllistnode)n15);
    PrintList(&list);  // List: 10 15 20 (total 3)
    
    // Cleanup
    xllistnode c = list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_InsertNext

Insert a new node after a reference node.

**Function Prototype:**
```c
XXAPI void xrtLLB_InsertNext(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
```

**Parameters:**
- `objLLB` - List object
- `objNode` - Reference node (`NULL` means insert at the end of list)
- `objNewNode` - New node to insert

**Behavior:**
- `objNode != NULL`: Insert after `objNode`
- `objNode == NULL`: Insert at end of list (becomes new `LastNode`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

IntNode* CreateNode(int value) {
    IntNode* n = xrtMalloc(sizeof(IntNode));
    n->value = value;
    return n;
}

void PrintList(xllistbase list) {
    printf("List: ");
    xllistnode current = list->FirstNode;
    while (current) {
        printf("%d ", ((IntNode*)current)->value);
        current = current->Next;
    }
    printf("\n");
}

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // Append to end (objNode = NULL)
    xrtLLB_InsertNext(&list, NULL, (xllistnode)CreateNode(10));
    xrtLLB_InsertNext(&list, NULL, (xllistnode)CreateNode(20));
    xrtLLB_InsertNext(&list, NULL, (xllistnode)CreateNode(30));
    PrintList(&list);  // List: 10 20 30
    
    // Insert after first node
    IntNode* n15 = CreateNode(15);
    xrtLLB_InsertNext(&list, list.FirstNode, (xllistnode)n15);
    PrintList(&list);  // List: 10 15 20 30
    
    // Cleanup
    xllistnode c = list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_Remove

Remove a node from the list.

**Function Prototype:**
```c
XXAPI void xrtLLB_Remove(xllistbase objLLB, xllistnode objNode);
```

**Parameters:**
- `objLLB` - List object
- `objNode` - Node to remove

**Notes:**
- Only removes node from list, **does not free node memory**
- User must free removed nodes themselves

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

IntNode* CreateNode(int value) {
    IntNode* n = xrtMalloc(sizeof(IntNode));
    n->value = value;
    return n;
}

void PrintList(xllistbase list) {
    printf("List: ");
    xllistnode c = list->FirstNode;
    while (c) {
        printf("%d ", ((IntNode*)c)->value);
        c = c->Next;
    }
    printf("(total %u)\n", list->Count);
}

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // Create list: 10 -> 20 -> 30 -> 40
    IntNode* n1 = CreateNode(10);
    IntNode* n2 = CreateNode(20);
    IntNode* n3 = CreateNode(30);
    IntNode* n4 = CreateNode(40);
    
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n1);
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n2);
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n3);
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n4);
    PrintList(&list);  // List: 10 20 30 40 (total 4)
    
    // Remove middle node
    xrtLLB_Remove(&list, (xllistnode)n2);
    xrtFree(n2);  // User frees memory
    PrintList(&list);  // List: 10 30 40 (total 3)
    
    // Remove first node
    xrtLLB_Remove(&list, (xllistnode)n1);
    xrtFree(n1);
    PrintList(&list);  // List: 30 40 (total 2)
    
    // Remove last node
    xrtLLB_Remove(&list, (xllistnode)n4);
    xrtFree(n4);
    PrintList(&list);  // List: 30 (total 1)
    
    // Cleanup last node
    xrtFree(n3);
    
    xrtUnit();
    return 0;
}
```

---

## Usage Scenarios

### 1. Custom Memory Management

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int id;
    char data[128];
} DataNode;

// Use BSMM to manage node memory
typedef struct {
    xllistbase_struct list;
    xbsmm_struct nodeMgr;
} CustomList;

void CustomListInit(CustomList* cl) {
    xrtLLB_Init(&cl->list);
    xrtBsmmInit(&cl->nodeMgr, sizeof(DataNode));
}

void CustomListUnit(CustomList* cl) {
    xrtLLB_Unit(&cl->list);
    xrtBsmmUnit(&cl->nodeMgr);
}

DataNode* CustomListAppend(CustomList* cl, int id, str data) {
    DataNode* n = (DataNode*)xrtBsmmAlloc(&cl->nodeMgr);
    n->id = id;
    strncpy(n->data, (char*)data, 127);
    xrtLLB_InsertNext(&cl->list, NULL, (xllistnode)n);
    return n;
}

void CustomListRemove(CustomList* cl, DataNode* n) {
    xrtLLB_Remove(&cl->list, (xllistnode)n);
    xrtBsmmFree(&cl->nodeMgr, n);
}

int main() {
    xrtInit();
    
    CustomList myList;
    CustomListInit(&myList);
    
    // Add nodes
    CustomListAppend(&myList, 1, (str)"First");
    CustomListAppend(&myList, 2, (str)"Second");
    DataNode* n3 = CustomListAppend(&myList, 3, (str)"Third");
    
    printf("Node count: %u\n", myList.list.Count);  // 3
    
    // Remove node
    CustomListRemove(&myList, n3);
    printf("After removal: %u\n", myList.list.Count);  // 2
    
    CustomListUnit(&myList);
    xrtUnit();
    return 0;
}
```

---

### 2. Embedded Linked List (Node Embedded in Other Structures)

```c
#include "xrt.h"
#include <stdio.h>

// Task structure (linked list node is part of it)
typedef struct {
    xllistnode_struct readyNode;   // Ready queue node
    xllistnode_struct waitNode;    // Wait queue node
    int taskId;
    int priority;
} Task;

// Macros to get task pointer from node pointer
#define TASK_FROM_READY_NODE(n) ((Task*)(n))
#define TASK_FROM_WAIT_NODE(n) ((Task*)((char*)(n) - offsetof(Task, waitNode)))

int main() {
    xrtInit();
    
    // Two task queues
    xllistbase_struct readyQueue;
    xllistbase_struct waitQueue;
    xrtLLB_Init(&readyQueue);
    xrtLLB_Init(&waitQueue);
    
    // Create tasks
    Task* t1 = xrtMalloc(sizeof(Task));
    t1->taskId = 1;
    t1->priority = 5;
    
    Task* t2 = xrtMalloc(sizeof(Task));
    t2->taskId = 2;
    t2->priority = 10;
    
    // t1 in ready queue
    xrtLLB_InsertNext(&readyQueue, NULL, &t1->readyNode);
    
    // t2 in both queues
    xrtLLB_InsertNext(&readyQueue, NULL, &t2->readyNode);
    xrtLLB_InsertNext(&waitQueue, NULL, &t2->waitNode);
    
    printf("Ready queue: %u tasks\n", readyQueue.Count);  // 2
    printf("Wait queue: %u tasks\n", waitQueue.Count);    // 1
    
    // Remove t2 from wait queue
    xrtLLB_Remove(&waitQueue, &t2->waitNode);
    printf("Wait queue: %u tasks\n", waitQueue.Count);    // 0
    
    // Cleanup
    xrtFree(t1);
    xrtFree(t2);
    
    xrtUnit();
    return 0;
}
```

---

### 3. LRU Cache

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    xllistnode_struct node;
    char key[32];
    char value[128];
} CacheEntry;

typedef struct {
    xllistbase_struct list;
    uint32 maxSize;
} LRUCache;

void LRUInit(LRUCache* cache, uint32 maxSize) {
    xrtLLB_Init(&cache->list);
    cache->maxSize = maxSize;
}

CacheEntry* LRUFind(LRUCache* cache, str key) {
    xllistnode c = cache->list.FirstNode;
    while (c) {
        CacheEntry* e = (CacheEntry*)c;
        if (strcmp(e->key, (char*)key) == 0) {
            // Move to front (most recently used)
            xrtLLB_Remove(&cache->list, c);
            xrtLLB_InsertPrev(&cache->list, NULL, c);
            return e;
        }
        c = c->Next;
    }
    return NULL;
}

void LRUPut(LRUCache* cache, str key, str value) {
    // Check if already exists
    CacheEntry* e = LRUFind(cache, key);
    if (e) {
        strncpy(e->value, (char*)value, 127);
        return;
    }
    
    // Evict oldest (tail)
    if (cache->list.Count >= cache->maxSize) {
        xllistnode last = cache->list.LastNode;
        xrtLLB_Remove(&cache->list, last);
        printf("Evicting: %s\n", ((CacheEntry*)last)->key);
        xrtFree(last);
    }
    
    // Add new entry to front
    e = xrtMalloc(sizeof(CacheEntry));
    strncpy(e->key, (char*)key, 31);
    strncpy(e->value, (char*)value, 127);
    xrtLLB_InsertPrev(&cache->list, NULL, (xllistnode)e);
}

void LRUUnit(LRUCache* cache) {
    xllistnode c = cache->list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    xrtLLB_Unit(&cache->list);
}

int main() {
    xrtInit();
    
    LRUCache cache;
    LRUInit(&cache, 3);
    
    LRUPut(&cache, (str)"a", (str)"value_a");
    LRUPut(&cache, (str)"b", (str)"value_b");
    LRUPut(&cache, (str)"c", (str)"value_c");
    printf("Cache size: %u\n", cache.list.Count);  // 3
    
    // Access a (moves to front)
    CacheEntry* e = LRUFind(&cache, (str)"a");
    printf("Access a: %s\n", e->value);
    
    // Add d, evicts oldest b
    LRUPut(&cache, (str)"d", (str)"value_d");  // Output: Evicting: b
    
    LRUUnit(&cache);
    xrtUnit();
    return 0;
}
```

---

## Comparison with LList

| Feature | LList Base | LList |
|---------|------------|-------|
| **Memory Management** | User-managed | Auto-managed (FSMemPool) |
| **Node Allocation** | User allocates | `xrtLListInsert*` auto allocates |
| **Node Deallocation** | User frees | `xrtLListRemove` auto frees |
| **Flexibility** | High | Medium |
| **Complexity** | High | Low |
| **Use Case** | Custom memory management, embedded | General purpose |

### Selection Guide

- **Use LList Base**: Need custom memory management, nodes belong to multiple lists, embedded systems
- **Use LList**: General purpose, want automatic memory management

---

## Best Practices

### 1. Node Structure Definition

```c
#include "xrt.h"

// ✅ Correct: xllistnode_struct as first member
typedef struct {
    xllistnode_struct node;  // Must be first!
    int id;
    char name[32];
} CorrectNode;

// ❌ Wrong: xllistnode_struct is not first member
typedef struct {
    int id;
    xllistnode_struct node;  // Wrong position!
    char name[32];
} WrongNode;

int main() {
    xrtInit();
    
    CorrectNode* n = xrtMalloc(sizeof(CorrectNode));
    n->id = 1;
    
    // ✅ Correct: can directly cast
    xllistnode nodePtr = (xllistnode)n;
    CorrectNode* back = (CorrectNode*)nodePtr;
    printf("ID: %d\n", back->id);  // 1
    
    xrtFree(n);
    xrtUnit();
    return 0;
}
```

---

### 2. Safe Traversal (Supports Deletion)

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // Add nodes
    for (int i = 1; i <= 5; i++) {
        IntNode* n = xrtMalloc(sizeof(IntNode));
        n->value = i * 10;
        xrtLLB_InsertNext(&list, NULL, (xllistnode)n);
    }
    
    // ✅ Safe traversal (can delete)
    xllistnode current = list.FirstNode;
    while (current) {
        xllistnode next = current->Next;  // Save next first
        IntNode* n = (IntNode*)current;
        
        if (n->value == 30) {
            xrtLLB_Remove(&list, current);
            xrtFree(current);
            printf("Deleted 30\n");
        } else {
            printf("Kept %d\n", n->value);
        }
        
        current = next;  // Use saved next
    }
    
    // Cleanup remaining nodes
    current = list.FirstNode;
    while (current) {
        xllistnode next = current->Next;
        xrtFree(current);
        current = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

### 3. Reverse Traversal

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // Add nodes: 10 -> 20 -> 30
    for (int i = 1; i <= 3; i++) {
        IntNode* n = xrtMalloc(sizeof(IntNode));
        n->value = i * 10;
        xrtLLB_InsertNext(&list, NULL, (xllistnode)n);
    }
    
    // Forward traversal
    printf("Forward: ");
    xllistnode c = list.FirstNode;
    while (c) {
        printf("%d ", ((IntNode*)c)->value);
        c = c->Next;
    }
    printf("\n");  // Forward: 10 20 30
    
    // Reverse traversal
    printf("Reverse: ");
    c = list.LastNode;
    while (c) {
        printf("%d ", ((IntNode*)c)->value);
        c = c->Prev;
    }
    printf("\n");  // Reverse: 30 20 10
    
    // Cleanup
    c = list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [LList Doubly Linked List](api-llist.en.md)
- [BSMM Block Structured Memory Management](api-bsmm.en.md)
- [FSMemPool Fixed-Size Memory Pool](api-mempool-fs.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#llist-base---linked-list-base-library)

</div>
