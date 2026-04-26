# AVLTree Base Library

> Self-balancing binary search tree low-level implementation, user manages node memory manually

[English](api-avltree-base.en.md) | [中文](api-avltree-base.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Overview](#overview)
- [Constants](#constants)
- [Data Structures](#data-structures)
- [Callback Function Types](#callback-function-types)
- [Macro Definitions](#macro-definitions)
- [Tree Operations](#tree-operations)
- [Traversal](#traversal)
- [Use Cases](#use-cases)
- [Comparison with AVLTree](#comparison-with-avltree)
- [Best Practices](#best-practices)

---

## Overview

AVLTree Base is the low-level implementation of AVL trees, providing basic self-balancing binary search tree operations. Unlike the high-level version (AVLTree), users need to **manage node memory manually**.

### Core Features

- **Self-balancing**: Automatically balances after insert/delete, guarantees O(log n) operations
- **Low-level operations**: Only manages tree structure, not node memory
- **High flexibility**: Nodes can come from any memory allocator
- **Maximum height 48**: Supports approximately 2^48 nodes

### AVL Tree Properties

```
AVL Balance Rule: Height difference between left and right subtrees of any node does not exceed 1

Balanced AVL Tree:              Unbalanced (will auto-adjust):
       4                               4
      / \                             / \
     2   6                           2   7
    / \ / \                         /     \
   1  3 5  7                       1       8
                                            \
                                             9
```

### Node Memory Layout

```
Node Memory Structure:
┌────────────────────────────────────────┐
│           xavltnode_struct             │  ← node pointer points here
│   ┌──────┬──────┬──────────┐           │
│   │ left │right │  height  │           │
│   │(ptr) │(ptr) │  (int)   │           │
│   └──────┴──────┴──────────┘           │
├────────────────────────────────────────┤
│              User Data                 │  ← &node[1] points here
│         (iItemLength bytes)            │
└────────────────────────────────────────┘
```

---

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `AVLTree_MAX_HEIGHT` | `48` | Maximum tree height, supports approximately 2^48 nodes |

---

## Data Structures

### xavltnode_struct

AVL tree node structure.

**Definition:**
```c
typedef struct xavltnode_struct {
    struct xavltnode_struct* left;   // Left child node
    struct xavltnode_struct* right;  // Right child node
    int height;                       // Node height
} xavltnode_struct, *xavltnode;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `left` | Left child pointer, `NULL` if no left child |
| `right` | Right child pointer, `NULL` if no right child |
| `height` | Node height, leaf node height is 1 |

---

### xavltbase_struct

AVL tree management structure.

**Definition:**
```c
typedef struct {
    xavltnode RootNode;  // Root node pointer
    uint32 Count;        // Node count
} xavltbase_struct, *xavltbase;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `RootNode` | Root node, `NULL` for empty tree |
| `Count` | Current node count |

---

## Callback Function Types

### AVLTree_CompProc

Comparison callback function type.

**Definition:**
```c
typedef int (*AVLTree_CompProc)(ptr pNode, ptr pKey);
```

**Parameters:**
- `pNode` - Node data pointer (`&node[1]`)
- `pKey` - Search key pointer

**Return Value:**
- `< 0`: pNode < pKey, continue searching in left subtree
- `> 0`: pNode > pKey, continue searching in right subtree
- `= 0`: Found matching node

**Example:**
```c
// Integer comparison
int IntCompare(ptr pNode, ptr pKey) {
    int nodeVal = *(int*)pNode;
    int keyVal = *(int*)pKey;
    if (nodeVal < keyVal) return -1;
    if (nodeVal > keyVal) return 1;
    return 0;
}

// String comparison
int StrCompare(ptr pNode, ptr pKey) {
    return strcmp((char*)pNode, (char*)pKey);
}
```

---

### AVLTree_EachProc

Traversal callback function type.

**Definition:**
```c
typedef bool (*AVLTree_EachProc)(ptr pNode, ptr pArg);
```

**Parameters:**
- `pNode` - Node data pointer (`&node[1]`)
- `pArg` - User-defined parameter

**Return Value:**
- `FALSE`: Continue traversal
- `TRUE`: Stop traversal

---

## Macro Definitions

### xrtAVLTreeGetNodeBase

Get node structure pointer from data pointer.

**Definition:**
```c
#define xrtAVLTreeGetNodeBase(p) ((xavltnode)((ptr)p - sizeof(xavltnode_struct)))
```

**Parameters:**
- `p` - User data pointer (`&node[1]`)

**Return Value:**
- Node structure pointer (`xavltnode`)

---

### xrtAVLTreeGetNodeData

Get user data pointer from node structure.

**Definition:**
```c
#define xrtAVLTreeGetNodeData(p) ((ptr)(&p[1]))
```

**Parameters:**
- `p` - Node structure pointer (`xavltnode`)

**Return Value:**
- User data pointer

---

### xrtAVLTreeGetRootData

Get user data of root node.

**Definition:**
```c
#define xrtAVLTreeGetRootData(obj) xrtAVLTreeGetNodeData(obj->RootNode)
```

---

### xrtAVLTB_Init

Initialize AVL tree.

**Definition:**
```c
#define xrtAVLTB_Init(o) (o)->RootNode = NULL; (o)->Count = 0
```

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    printf("Node count: %u\n", tree.Count);  // 0
    printf("Root node: %p\n", tree.RootNode);  // (nil)
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_Unit / xrtAVLTB_RemoveAll / xrtAVLTB_Clear

Release AVL tree (only resets pointers, does not free node memory).

**Definition:**
```c
#define xrtAVLTB_Unit xrtAVLTB_Init
#define xrtAVLTB_RemoveAll xrtAVLTB_Unit
#define xrtAVLTB_Clear xrtAVLTB_Unit
```

**Warning:** These macros only reset tree structure, **they do NOT free node memory**! Users must traverse and free all nodes first.

---

## Tree Operations

### xrtAVLTB_Insert

Insert a node into the tree.

**Function Prototype:**
```c
XXAPI xavltnode xrtAVLTB_Insert(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey, xavltnode pNewNode);
```

**Parameters:**
- `objAVLT` - Tree object
- `procComp` - Comparison function
- `pKey` - Key pointer (for comparison and positioning)
- `pNewNode` - Pre-allocated new node

**Return Value:**
- Success (newly inserted): Returns `pNewNode`
- Key exists: Returns existing node
- Failure (exceeds height): Returns `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;  // Node structure must be first
    int key;
    char value[32];
} MyNode;

int MyCompare(ptr pNode, ptr pKey) {
    int nodeKey = *(int*)pNode;
    int searchKey = *(int*)pKey;
    return nodeKey - searchKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Allocate and insert node
    MyNode* n1 = xrtMalloc(sizeof(MyNode));
    n1->key = 10;
    strcpy(n1->value, "Ten");
    
    int key = 10;
    xavltnode result = xrtAVLTB_Insert(&tree, MyCompare, &key, (xavltnode)n1);
    
    if (result == (xavltnode)n1) {
        printf("Insert successful, node count: %u\n", tree.Count);  // 1
    }
    
    // Try to insert duplicate key
    MyNode* n2 = xrtMalloc(sizeof(MyNode));
    n2->key = 10;
    strcpy(n2->value, "Duplicate");
    
    result = xrtAVLTB_Insert(&tree, MyCompare, &key, (xavltnode)n2);
    if (result != (xavltnode)n2) {
        printf("Key exists, returning existing node\n");
        xrtFree(n2);  // Free unused node
    }
    
    // Cleanup
    xrtFree(n1);
    xrtAVLTB_Unit(&tree);
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_Remove

Remove a node from the tree.

**Function Prototype:**
```c
XXAPI xavltnode xrtAVLTB_Remove(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
```

**Parameters:**
- `objAVLT` - Tree object
- `procComp` - Comparison function
- `pKey` - Key to delete

**Return Value:**
- Success: Returns deleted node pointer (user needs to free)
- Failure: Returns `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int key;
} IntNode;

int IntCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Insert nodes 1, 2, 3
    for (int i = 1; i <= 3; i++) {
        IntNode* n = xrtMalloc(sizeof(IntNode));
        n->key = i;
        xrtAVLTB_Insert(&tree, IntCompare, &i, (xavltnode)n);
    }
    printf("Node count after insert: %u\n", tree.Count);  // 3
    
    // Remove node 2
    int key = 2;
    xavltnode removed = xrtAVLTB_Remove(&tree, IntCompare, &key);
    if (removed) {
        printf("Remove successful, node count: %u\n", tree.Count);  // 2
        xrtFree(removed);  // Free removed node
    }
    
    // Try to remove non-existent node
    key = 100;
    removed = xrtAVLTB_Remove(&tree, IntCompare, &key);
    if (!removed) {
        printf("Node does not exist\n");
    }
    
    // Cleanup remaining nodes (should traverse and free in real application)
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_Search

Search for a node in the tree.

**Function Prototype:**
```c
XXAPI xavltnode xrtAVLTB_Search(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
```

**Parameters:**
- `objAVLT` - Tree object
- `procComp` - Comparison function
- `pKey` - Key to search

**Return Value:**
- Found: Node pointer
- Not found: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int id;
    char name[32];
} UserNode;

int UserCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Insert user
    UserNode* u1 = xrtMalloc(sizeof(UserNode));
    u1->id = 100;
    strcpy(u1->name, "Alice");
    int key = 100;
    xrtAVLTB_Insert(&tree, UserCompare, &key, (xavltnode)u1);
    
    // Search user
    key = 100;
    xavltnode found = xrtAVLTB_Search(&tree, UserCompare, &key);
    if (found) {
        UserNode* user = (UserNode*)found;
        printf("Found user: ID=%d, Name=%s\n", user->id, user->name);
    }
    
    // Search non-existent user
    key = 999;
    found = xrtAVLTB_Search(&tree, UserCompare, &key);
    if (!found) {
        printf("User %d does not exist\n", key);
    }
    
    xrtFree(u1);
    xrtUnit();
    return 0;
}
```

---

## Traversal

### xrtAVLTB_Walk

In-order traversal of all nodes (ascending order).

**Macro Definition:**
```c
#define xrtAVLTB_Walk(obj, p, a) xrtAVLTB_WalkRecuProc(obj->RootNode, (ptr)p, (ptr)a)
```

**Parameters:**
- `obj` - Tree object
- `p` - Traversal callback function (`AVLTree_EachProc`)
- `a` - User parameter

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int value;
} IntNode;

int IntCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

bool PrintNode(ptr pNode, ptr pArg) {
    int value = *(int*)pNode;
    printf("%d ", value);
    return FALSE;  // Continue traversal
}

bool SumNodes(ptr pNode, ptr pArg) {
    int value = *(int*)pNode;
    int* sum = (int*)pArg;
    *sum += value;
    return FALSE;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Insert 5, 3, 7, 1, 9
    int values[] = {5, 3, 7, 1, 9};
    IntNode* nodes[5];
    for (int i = 0; i < 5; i++) {
        nodes[i] = xrtMalloc(sizeof(IntNode));
        nodes[i]->value = values[i];
        xrtAVLTB_Insert(&tree, IntCompare, &values[i], (xavltnode)nodes[i]);
    }
    
    // In-order traversal (ascending output)
    printf("Ascending: ");
    xrtAVLTB_Walk(&tree, PrintNode, NULL);
    printf("\n");  // Ascending: 1 3 5 7 9
    
    // Calculate sum
    int sum = 0;
    xrtAVLTB_Walk(&tree, SumNodes, &sum);
    printf("Sum: %d\n", sum);  // 25
    
    // Cleanup
    for (int i = 0; i < 5; i++) {
        xrtFree(nodes[i]);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_WalkEx

Extended traversal (supports pre-order, in-order, post-order callbacks).

**Macro Definition:**
```c
#define xrtAVLTB_WalkEx(obj, p1, p2, p3, a) xrtAVLTB_WalkExRecuProc(obj->RootNode, (ptr)p1, (ptr)p2, (ptr)p3, (ptr)a)
```

**Parameters:**
- `obj` - Tree object
- `p1` - Pre-order callback (before entering node)
- `p2` - In-order callback (after traversing left subtree)
- `p3` - Post-order callback (before leaving node)
- `a` - User parameter

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

bool PreOrder(ptr pNode, ptr pArg) {
    printf("Pre(%d) ", *(int*)pNode);
    return FALSE;
}

bool InOrder(ptr pNode, ptr pArg) {
    printf("In(%d) ", *(int*)pNode);
    return FALSE;
}

bool PostOrder(ptr pNode, ptr pArg) {
    printf("Post(%d) ", *(int*)pNode);
    return FALSE;
}

int main() {
    xrtInit();
    
    // ... Create tree and insert nodes ...
    
    // Extended traversal
    xrtAVLTB_WalkEx(&tree, PreOrder, InOrder, PostOrder, NULL);
    
    xrtUnit();
    return 0;
}
```

---

## Use Cases

### 1. Custom Memory Management

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int key;
    char data[64];
} CustomNode;

int NodeCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

// Use BSMM to manage node memory
int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Use BSMM to manage fixed-size nodes
    xbsmm nodeMgr = xrtBsmmCreate(sizeof(CustomNode));
    
    // Allocate and insert nodes
    for (int i = 0; i < 100; i++) {
        CustomNode* n = xrtBsmmAlloc(nodeMgr);
        n->key = i;
        sprintf(n->data, "Data_%d", i);
        xrtAVLTB_Insert(&tree, NodeCompare, &i, (xavltnode)n);
    }
    
    printf("Node count: %u\n", tree.Count);  // 100
    
    // Search
    int key = 50;
    xavltnode found = xrtAVLTB_Search(&tree, NodeCompare, &key);
    if (found) {
        CustomNode* n = (CustomNode*)found;
        printf("Found: key=%d, data=%s\n", n->key, n->data);
    }
    
    // Destroy by freeing entire BSMM (all nodes freed together)
    xrtBsmmDestroy(nodeMgr);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Embedded Index Structure

```c
#include "xrt.h"
#include <stdio.h>

// Main data structure (array storage)
typedef struct {
    int id;
    char name[32];
    float score;
} Student;

// Index node (contains only key and index)
typedef struct {
    xavltnode_struct node;
    int id;         // Key
    int arrayIndex; // Index pointing to Students array
} StudentIndex;

Student students[100];
int studentCount = 0;

int IndexCompare(ptr pNode, ptr pKey) {
    StudentIndex* idx = (StudentIndex*)((char*)pNode - sizeof(xavltnode_struct));
    return idx->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct idIndex;
    xrtAVLTB_Init(&idIndex);
    
    // Add students and build index
    students[0] = (Student){1001, "Alice", 95.5};
    students[1] = (Student){1003, "Bob", 87.0};
    students[2] = (Student){1002, "Charlie", 92.3};
    studentCount = 3;
    
    StudentIndex* indices = xrtMalloc(sizeof(StudentIndex) * 3);
    for (int i = 0; i < 3; i++) {
        indices[i].id = students[i].id;
        indices[i].arrayIndex = i;
        xrtAVLTB_Insert(&idIndex, IndexCompare, &students[i].id, (xavltnode)&indices[i]);
    }
    
    // Quick lookup via index
    int searchId = 1002;
    xavltnode found = xrtAVLTB_Search(&idIndex, IndexCompare, &searchId);
    if (found) {
        StudentIndex* idx = (StudentIndex*)found;
        Student* s = &students[idx->arrayIndex];
        printf("Found student: ID=%d, Name=%s, Score=%.1f\n", 
               s->id, s->name, s->score);
    }
    
    xrtFree(indices);
    xrtUnit();
    return 0;
}
```

---

## Comparison with AVLTree

| Feature | AVLTree Base | AVLTree |
|---------|--------------|---------|
| **Memory Management** | Manual | Automatic (FSMemPool) |
| **Node Allocation** | User allocates | `xrtAVLTreeInsert` auto allocates |
| **Node Deallocation** | User frees | `xrtAVLTreeRemove` auto frees |
| **Comparison Function** | Passed on each operation | Specified once at creation |
| **GC Support** | ❌ None | ✅ FSMemPool supports |
| **Flexibility** | Highest | Medium |
| **Use Case** | Custom memory, embedded | General applications |

### Selection Guide

- **Use AVLTree Base**: Custom memory allocator, embedded environment, need full memory control
- **Use AVLTree**: Most scenarios, need automatic memory management

---

## Best Practices

### 1. Node Structure Definition

```c
#include "xrt.h"
#include <stdio.h>

// ✅ Correct: Node structure at the front
typedef struct {
    xavltnode_struct node;  // Must be first
    int key;
    char value[32];
} GoodNode;

// ❌ Wrong: Node structure not at the front
typedef struct {
    int key;                // Wrong!
    xavltnode_struct node;
    char value[32];
} BadNode;

int main() {
    xrtInit();
    
    // Verify memory layout
    GoodNode good;
    printf("GoodNode: node offset=%zu, key offset=%zu\n",
           (char*)&good.node - (char*)&good,   // 0
           (char*)&good.key - (char*)&good);   // sizeof(xavltnode_struct)
    
    xrtUnit();
    return 0;
}
```

---

### 2. Safe Deletion of All Nodes

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int key;
} IntNode;

bool FreeNodeCallback(ptr pNode, ptr pArg) {
    // pNode points to &node[1], need to go back to node beginning
    xavltnode nodePtr = xrtAVLTreeGetNodeBase(pNode);
    xrtFree(nodePtr);
    return FALSE;  // Continue traversal
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Insert nodes...
    
    // ✅ Correct: Traverse and free all nodes first, then clear tree
    xrtAVLTB_Walk(&tree, FreeNodeCallback, NULL);
    xrtAVLTB_Unit(&tree);
    
    // ❌ Wrong: Clear directly (memory leak)
    // xrtAVLTB_Unit(&tree);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Interrupt Traversal

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int value;
} IntNode;

// Find first value greater than target
bool FindFirstGreater(ptr pNode, ptr pArg) {
    int value = *(int*)pNode;
    int* target = (int*)pArg;
    
    if (value > *target) {
        printf("Found: %d > %d\n", value, *target);
        return TRUE;  // Interrupt traversal
    }
    return FALSE;  // Continue
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // Insert 1, 3, 5, 7, 9
    int values[] = {1, 3, 5, 7, 9};
    IntNode nodes[5];
    int IntCompare(ptr pNode, ptr pKey) {
        return *(int*)pNode - *(int*)pKey;
    }
    
    for (int i = 0; i < 5; i++) {
        nodes[i].value = values[i];
        xrtAVLTB_Insert(&tree, IntCompare, &values[i], (xavltnode)&nodes[i]);
    }
    
    // Find first > 4
    int target = 4;
    xrtAVLTB_Walk(&tree, FindFirstGreater, &target);  // Found: 5 > 4
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [AVLTree](api-avltree.en.md)
- [Dict Dictionary](api-dict.en.md)
- [BSMM Block Structure Memory Management](api-bsmm.en.md)
- [FSMemPool Fixed-Size Memory Pool](api-mempool-fs.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#avltree-base-library)

</div>
