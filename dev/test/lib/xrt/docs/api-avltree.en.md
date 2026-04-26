# AVLTree Balanced Binary Tree Library

> Self-balancing binary search tree with automatic memory management

[English](api-avltree.en.md) | [中文](api-avltree.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Callback Function Types](#callback-function-types)
- [Tree Operations](#tree-operations)
- [Node Operations](#node-operations)
- [Traversal](#traversal)
- [Use Cases](#use-cases)
- [Comparison with AVLTree Base](#comparison-with-avltree-base)
- [Best Practices](#best-practices)

---

## Overview

AVLTree is a high-level AVL tree implementation based on AVLTree Base, with automatic memory management (using FSMemPool).

### Core Features

- **Automatic memory management**: Uses FSMemPool to allocate nodes
- **Node caching**: Optimizes continuous insertion performance
- **Inheritance tree support**: Can set parent tree (Parent) for lookup
- **Custom release**: Supports custom release callback for node data

### Relationship with AVLTree Base

```
AVLTree Architecture:
┌─────────────────────────────────────┐
│           AVLTree (High-level)      │
│  ┌─────────────────────────────────┐│
│  │    FSMemPool (Node Memory Mgmt) ││
│  └─────────────────────────────────┘│
│  ┌─────────────────────────────────┐│
│  │    AVLTree Base (Tree Ops)      ││
│  └─────────────────────────────────┘│
└─────────────────────────────────────┘
```

---

## Data Structures

### xavltree_struct

AVL tree structure.

**Definition:**
```c
typedef struct xavltree_struct {
    xavltnode RootNode;           // Root node
    uint32 Count;                  // Node count
    struct xavltree_struct* Parent; // Parent tree (for inheritance lookup)
    AVLTree_CompProc CompProc;    // Comparison function
    AVLTree_FreeProc FreeProc;    // Node release callback (optional)
    xfsmempool_struct objMM;      // Built-in memory pool
    xavltnode NodeCache;          // Node cache
} xavltree_struct, *xavltree;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `RootNode` | Tree root node |
| `Count` | Current node count |
| `Parent` | Parent tree pointer, Search will look in parent if not found |
| `CompProc` | Comparison function for node sorting |
| `FreeProc` | Release callback, called when deleting nodes (can be NULL) |
| `objMM` | Built-in FSMemPool, manages node memory |
| `NodeCache` | Pre-allocated node cache, optimizes continuous insertion |

---

## Callback Function Types

### AVLTree_CompProc

Comparison callback function (inherited from AVLTree Base).

**Definition:**
```c
typedef int (*AVLTree_CompProc)(ptr pNode, ptr pKey);
```

**Parameters:**
- `pNode` - Data pointer of node in tree
- `pKey` - Key pointer to find/compare

**Return Value:**
- `< 0` - pNode less than pKey
- `= 0` - Equal
- `> 0` - pNode greater than pKey

---

### AVLTree_FreeProc

Node release callback function (optional).

**Definition:**
```c
typedef void (*AVLTree_FreeProc)(ptr objTree, ptr pNode);
```

**Parameters:**
- `objTree` - Tree object
- `pNode` - Node data pointer

**Purpose:** Used when node data contains resources that need additional release.

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    str name;  // Needs to be freed
} User;

void FreeUser(ptr tree, ptr pNode) {
    User* user = (User*)pNode;
    if (user->name) {
        xrtFree(user->name);
    }
}

int CompareUser(ptr pNode, ptr pKey) {
    return ((User*)pNode)->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(User), CompareUser);
    tree->FreeProc = FreeUser;  // Set release callback
    
    // Insert data
    int id = 1;
    bool isNew;
    User* user = (User*)xrtAVLTreeInsert(tree, &id, &isNew);
    if (isNew) {
        user->id = id;
        user->name = xrtCopyStr((str)"Alice", 0);
    }
    
    // FreeUser will be called automatically when destroyed to free name
    xrtAVLTreeDestroy(tree);
    
    xrtUnit();
    return 0;
}
```

---

## Tree Operations

### xrtAVLTreeCreate

Create an AVL tree.

**Function Prototype:**
```c
XXAPI xavltree xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp);
```

**Parameters:**
- `iItemLength` - Node data size (bytes)
- `procComp` - Comparison function

**Return Value:**
- Success: Tree object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtAVLTreeDestroy` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // Insert data
    int keys[] = {5, 3, 7, 1, 9, 4, 6};
    for (int i = 0; i < 7; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &keys[i], &isNew);
        if (isNew) {
            *data = keys[i];
        }
    }
    
    printf("Node count: %u\n", tree->Count);  // 7
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeDestroy

Destroy an AVL tree.

**Function Prototype:**
```c
XXAPI void xrtAVLTreeDestroy(xavltree objAVLT);
```

**Parameters:**
- `objAVLT` - Tree object

**Description:**
- Releases all nodes and tree structure
- If `FreeProc` is set, traverses all nodes and calls release callback first

---

### xrtAVLTreeInit

Initialize an AVL tree (for stack or embedded structure).

**Function Prototype:**
```c
XXAPI void xrtAVLTreeInit(xavltree objAVLT, uint32 iItemLength, AVLTree_CompProc procComp);
```

**Parameters:**
- `objAVLT` - Tree object pointer
- `iItemLength` - Node data size
- `procComp` - Comparison function

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    // Stack allocation
    xavltree_struct treeData;
    xrtAVLTreeInit(&treeData, sizeof(int), CompareInt);
    
    // Usage
    int key = 42;
    bool isNew;
    int* data = (int*)xrtAVLTreeInsert(&treeData, &key, &isNew);
    *data = key;
    
    printf("Node count: %u\n", treeData.Count);
    
    // Release internal resources (not the struct itself)
    xrtAVLTreeUnit(&treeData);
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeUnit

Release AVL tree internal resources (not the struct itself).

**Function Prototype:**
```c
XXAPI void xrtAVLTreeUnit(xavltree objAVLT);
```

**Description:**
- Pairs with `xrtAVLTreeInit`
- Releases built-in FSMemPool and all nodes

**Macro Aliases:**
- `xrtAVLTreeRemoveAll` - Remove all nodes
- `xrtAVLTreeClear` - Clear tree

---

## Node Operations

### xrtAVLTreeInsert

Insert a node.

**Function Prototype:**
```c
XXAPI ptr xrtAVLTreeInsert(xavltree objAVLT, ptr pKey, bool* bNew);
```

**Parameters:**
- `objAVLT` - Tree object
- `pKey` - Key pointer (for comparison)
- `bNew` - Output parameter, indicates if it's a new node (can be NULL)

**Return Value:**
- Success: Node data pointer
- Failure: `NULL`

**Description:**
- If key exists, returns existing node's data pointer, `*bNew = FALSE`
- If key doesn't exist, creates new node, `*bNew = TRUE`
- New node needs to be filled with data after return

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int value;
} Item;

int CompareItem(ptr pNode, ptr pKey) {
    return ((Item*)pNode)->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(Item), CompareItem);
    
    // Insert new node
    int id1 = 100;
    bool isNew;
    Item* item1 = (Item*)xrtAVLTreeInsert(tree, &id1, &isNew);
    if (isNew) {
        item1->id = id1;
        item1->value = 1000;
        printf("Inserted new node: id=%d, value=%d\n", item1->id, item1->value);
    }
    
    // Try to insert duplicate key
    Item* item2 = (Item*)xrtAVLTreeInsert(tree, &id1, &isNew);
    if (!isNew) {
        printf("Key exists: id=%d, value=%d\n", item2->id, item2->value);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeSearch

Search for a node.

**Function Prototype:**
```c
XXAPI ptr xrtAVLTreeSearch(xavltree objAVLT, ptr pKey);
```

**Parameters:**
- `objAVLT` - Tree object
- `pKey` - Key pointer

**Return Value:**
- Found: Node data pointer
- Not found: `NULL`

**Inheritance Lookup:** If not found in current tree and `Parent` is set, continues searching in parent tree.

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // Insert data
    for (int i = 1; i <= 10; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &i, &isNew);
        *data = i * 100;
    }
    
    // Search
    int key = 5;
    int* found = (int*)xrtAVLTreeSearch(tree, &key);
    if (found) {
        printf("Found: key=%d, value=%d\n", key, *found);  // 500
    }
    
    // Search non-existent key
    int notExist = 20;
    int* notFound = (int*)xrtAVLTreeSearch(tree, &notExist);
    if (!notFound) {
        printf("Not found: key=%d\n", notExist);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeRemove

Remove a node.

**Function Prototype:**
```c
XXAPI bool xrtAVLTreeRemove(xavltree objAVLT, ptr pKey);
```

**Parameters:**
- `objAVLT` - Tree object
- `pKey` - Key pointer

**Return Value:**
- `TRUE` - Successfully removed
- `FALSE` - Key doesn't exist

**Description:**
- Node memory is automatically freed after removal
- If `FreeProc` is set, release callback is called before deletion

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // Insert data
    for (int i = 1; i <= 5; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &i, &isNew);
        *data = i;
    }
    printf("Before removal: %u nodes\n", tree->Count);  // 5
    
    // Remove existing key
    int key = 3;
    if (xrtAVLTreeRemove(tree, &key)) {
        printf("Successfully removed key=%d\n", key);
    }
    printf("After removal: %u nodes\n", tree->Count);  // 4
    
    // Remove non-existent key
    int notExist = 100;
    if (!xrtAVLTreeRemove(tree, &notExist)) {
        printf("Removal failed: key=%d doesn't exist\n", notExist);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

## Traversal

### xrtAVLTreeWalk / xrtAVLTreeWalkEx

Traverse all nodes in the tree (macro definitions, using AVLTree Base implementation).

**Macro Definitions:**
```c
#define xrtAVLTreeWalk    xrtAVLTB_Walk
#define xrtAVLTreeWalkEx  xrtAVLTB_WalkEx
```

**Function Prototypes:**
```c
// Basic traversal
bool xrtAVLTreeWalk(xavltbase objBase, int WalkOrder, AVLTree_EachProc procEach, ptr pArg);

// Extended traversal (supports explicit termination)
bool xrtAVLTreeWalkEx(xavltbase objBase, int WalkOrder, AVLTree_EachProc procEach, ptr pArg, ptr pExit);
```

**WalkOrder Parameters:**
- `AVLTREE_WALK_PREORDER` (1) - Pre-order traversal
- `AVLTREE_WALK_INORDER` (2) - In-order traversal (sorted)
- `AVLTREE_WALK_POSTORDER` (3) - Post-order traversal

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

bool PrintNode(ptr pNode, ptr pArg) {
    printf("%d ", *(int*)pNode);
    return TRUE;  // Continue traversal
}

bool SumNodes(ptr pNode, ptr pArg) {
    int* sum = (int*)pArg;
    *sum += *(int*)pNode;
    return TRUE;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // Insert data
    int keys[] = {5, 3, 7, 1, 9, 4, 6};
    for (int i = 0; i < 7; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &keys[i], &isNew);
        *data = keys[i];
    }
    
    // In-order traversal (sorted output)
    printf("In-order traversal: ");
    xrtAVLTreeWalk((xavltbase)tree, AVLTREE_WALK_INORDER, PrintNode, NULL);
    printf("\n");  // 1 3 4 5 6 7 9
    
    // Calculate sum
    int sum = 0;
    xrtAVLTreeWalk((xavltbase)tree, AVLTREE_WALK_INORDER, SumNodes, &sum);
    printf("Sum: %d\n", sum);  // 35
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

## Use Cases

### 1. Integer Index

```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // Insert data
    for (int i = 1; i <= 100; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &i, &isNew);
        *data = i;
    }
    
    // Search
    int search = 50;
    int* found = (int*)xrtAVLTreeSearch(tree, &search);
    if (found) {
        printf("Found: %d\n", *found);
    }
    
    // Statistics
    printf("Node count: %u\n", tree->Count);
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 2. String Key Index

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char key[32];
    int value;
} StringKeyNode;

int CompareStringKey(ptr pNode, ptr pKey) {
    return strcmp(((StringKeyNode*)pNode)->key, (char*)pKey);
}

void FreeStringNode(ptr tree, ptr pNode) {
    // Handle additional resources to release here if any
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(StringKeyNode), CompareStringKey);
    
    // Insert
    char* keys[] = {"apple", "banana", "cherry", "date"};
    int values[] = {100, 200, 300, 400};
    
    for (int i = 0; i < 4; i++) {
        bool isNew;
        StringKeyNode* node = (StringKeyNode*)xrtAVLTreeInsert(tree, keys[i], &isNew);
        if (isNew) {
            strcpy(node->key, keys[i]);
            node->value = values[i];
        }
    }
    
    // Search
    char* searchKey = "banana";
    StringKeyNode* found = (StringKeyNode*)xrtAVLTreeSearch(tree, searchKey);
    if (found) {
        printf("Found %s: value=%d\n", found->key, found->value);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 3. Inheritance Tree (Parent)

```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    // Create parent tree (global config)
    xavltree parentTree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    int globalKeys[] = {100, 200, 300};
    for (int i = 0; i < 3; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(parentTree, &globalKeys[i], &isNew);
        *data = globalKeys[i];
    }
    
    // Create child tree (local config)
    xavltree childTree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    childTree->Parent = parentTree;  // Set parent tree
    
    int localKeys[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(childTree, &localKeys[i], &isNew);
        *data = localKeys[i];
    }
    
    // Search in child tree
    int key1 = 2;
    int* found1 = (int*)xrtAVLTreeSearch(childTree, &key1);
    printf("Search %d in child tree: %s\n", key1, found1 ? "Found" : "Not found");
    
    // Search key that doesn't exist in child but exists in parent
    int key2 = 200;
    int* found2 = (int*)xrtAVLTreeSearch(childTree, &key2);
    printf("Search %d in child tree: %s\n", key2, found2 ? "Found (from parent)" : "Not found");
    
    xrtAVLTreeDestroy(childTree);
    xrtAVLTreeDestroy(parentTree);
    xrtUnit();
    return 0;
}
```

---

## Comparison with AVLTree Base

| Feature | AVLTree | AVLTree Base |
|---------|---------|--------------|
| **Memory Management** | Automatic (FSMemPool) | Manual |
| **Node Allocation** | Automatic | User handles |
| **Node Cache** | ✅ Supported | ❌ Not supported |
| **Inheritance Tree** | ✅ Parent supported | ❌ Not supported |
| **Release Callback** | ✅ FreeProc | ❌ Not supported |
| **Use Case** | General needs | Custom memory management |

### Selection Guide

- **Use AVLTree**: Most scenarios, no need to worry about memory management
- **Use AVLTree Base**: Need custom memory allocation strategy or embedded scenarios

---

## Best Practices

### 1. Comparison Function Design

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// ✅ Correct: Returns positive/zero/negative
int CompareInt(ptr pNode, ptr pKey) {
    int a = *(int*)pNode;
    int b = *(int*)pKey;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

// ✅ Simplified version (safe for small values)
int CompareIntSimple(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

// ❌ Wrong: Large value overflow
int CompareIntWrong(ptr pNode, ptr pKey) {
    int64 a = *(int64*)pNode;
    int64 b = *(int64*)pKey;
    return (int)(a - b);  // Overflow risk!
}

// ✅ Safe 64-bit comparison
int CompareInt64Safe(ptr pNode, ptr pKey) {
    int64 a = *(int64*)pNode;
    int64 b = *(int64*)pKey;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    int key = 42;
    bool isNew;
    int* data = (int*)xrtAVLTreeInsert(tree, &key, &isNew);
    *data = key;
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 2. Composite Key Design

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int category;
    int id;
    char name[32];
} Record;

// Multi-level comparison: compare category first, then id
int CompareRecord(ptr pNode, ptr pKey) {
    Record* a = (Record*)pNode;
    Record* b = (Record*)pKey;
    
    // First level: category
    if (a->category != b->category) {
        return a->category - b->category;
    }
    // Second level: id
    return a->id - b->id;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(Record), CompareRecord);
    
    // Insert records
    Record records[] = {
        {1, 100, "Item A"},
        {1, 200, "Item B"},
        {2, 100, "Item C"},
        {2, 200, "Item D"}
    };
    
    for (int i = 0; i < 4; i++) {
        bool isNew;
        Record* node = (Record*)xrtAVLTreeInsert(tree, &records[i], &isNew);
        if (isNew) {
            *node = records[i];
        }
    }
    
    // Search
    Record searchKey = {1, 200, ""};
    Record* found = (Record*)xrtAVLTreeSearch(tree, &searchKey);
    if (found) {
        printf("Found: category=%d, id=%d, name=%s\n", 
               found->category, found->id, found->name);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 3. Usage with Release Callback

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    str data;       // Dynamically allocated
    ptr extra;      // Extra resources
} ComplexNode;

void FreeComplexNode(ptr tree, ptr pNode) {
    ComplexNode* node = (ComplexNode*)pNode;
    if (node->data) {
        xrtFree(node->data);
        node->data = NULL;
    }
    if (node->extra) {
        xrtFree(node->extra);
        node->extra = NULL;
    }
}

int CompareComplex(ptr pNode, ptr pKey) {
    return ((ComplexNode*)pNode)->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(ComplexNode), CompareComplex);
    tree->FreeProc = FreeComplexNode;  // Set release callback
    
    // Insert nodes
    for (int i = 1; i <= 5; i++) {
        bool isNew;
        ComplexNode* node = (ComplexNode*)xrtAVLTreeInsert(tree, &i, &isNew);
        if (isNew) {
            node->id = i;
            node->data = xrtFormat((str)"Data for %d", i);
            node->extra = xrtMalloc(100);
        }
    }
    
    printf("Created %u nodes\n", tree->Count);
    
    // Remove single node (will call FreeComplexNode)
    int delKey = 3;
    xrtAVLTreeRemove(tree, &delKey);
    printf("After removal, %u nodes remaining\n", tree->Count);
    
    // Destroy tree (will traverse all nodes and call FreeComplexNode)
    xrtAVLTreeDestroy(tree);
    printf("All resources released\n");
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [AVLTree Base](api-avltree-base.en.md)
- [Dict Dictionary](api-dict.en.md)
- [FSMemPool Fixed-Size Memory Pool](api-mempool-fs.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#avltree-balanced-binary-tree-library)

</div>
