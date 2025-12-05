# DynStack Dynamic Stack Library

> Unlimited depth dynamic stack with automatic capacity expansion

[English](api-dynstack.en.md) | [中文](api-dynstack.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Stack Operations](#stack-operations)
- [Push Operations](#push-operations)
- [Pop Operations](#pop-operations)
- [Stack Access](#stack-access)
- [Use Cases](#use-cases)
- [Comparison with Static Stack](#comparison-with-static-stack)
- [Best Practices](#best-practices)

---

## Overview

DynStack (Dynamic Stack) is an unlimited depth dynamic stack supporting both struct and pointer modes. Unlike static stacks, dynamic stacks automatically expand capacity when needed.

### Core Features

- **Unlimited depth**: Auto-expands, no need to preset maximum capacity
- **Block management**: Each block stores 256 elements, new blocks allocated on demand
- **Delayed release**: Memory blocks are lazily released on pop to avoid critical oscillation
- **Two modes**: Supports struct mode and pointer mode

### Memory Layout

```
DynStack Structure:
┌──────────────────────────────────────────────────────┐
│                    xdynstack_struct                   │
├─────────────┬─────────────┬──────────────────────────┤
│ ItemLength  │    Count    │         MMU (PtrArray)   │
│  (4 bytes)  │  (4 bytes)  │   Memory block ptr array │
└─────────────┴─────────────┴──────────────────────────┘
                                      │
                    ┌─────────────────┼─────────────────┐
                    ▼                 ▼                 ▼
              ┌──────────┐      ┌──────────┐      ┌──────────┐
              │ Block 0  │      │ Block 1  │      │ Block 2  │
              │ 256 elems│      │ 256 elems│      │ 256 elems│
              └──────────┘      └──────────┘      └──────────┘
```

### Index Rules

```
Member numbering rules (0 is non-existent member number):
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

**Important:** Index starts from **1**, not 0!

---

## Data Structures

### xdynstack_struct

Dynamic stack structure.

**Definition:**
```c
typedef struct {
    uint32 ItemLength;      // Memory length per stack member
    uint32 Count;           // Number of members in stack (top position)
    xparray_struct MMU;     // Memory block manager (PtrArray)
} xdynstack_struct, *xdynstack;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `ItemLength` | Size of each element (bytes). `sizeof(ptr)` in pointer mode |
| `Count` | Current number of elements in stack, also the top position |
| `MMU` | Memory block manager, each block stores 256 elements |

**Mode Description:**

| Mode | ItemLength | Storage Content |
|------|------------|-----------------|
| Struct mode | `sizeof(struct)` | Struct data |
| Pointer mode | `sizeof(ptr)` | Pointer values |

---

## Stack Operations

### xrtDynStackCreate

Create a dynamic stack.

**Function Prototype:**
```c
XXAPI xdynstack xrtDynStackCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Element size (bytes)

**Return Value:**
- Success: Dynamic stack object
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Task;

int main() {
    xrtInit();
    
    // Create struct stack
    xdynstack taskStack = xrtDynStackCreate(sizeof(Task));
    printf("Created struct stack, element size: %u\n", taskStack->ItemLength);
    
    // Create pointer stack
    xdynstack ptrStack = xrtDynStackCreate(sizeof(ptr));
    printf("Created pointer stack, element size: %u\n", ptrStack->ItemLength);
    
    xrtDynStackDestroy(taskStack);
    xrtDynStackDestroy(ptrStack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackDestroy

Destroy dynamic stack.

**Function Prototype:**
```c
XXAPI void xrtDynStackDestroy(xdynstack objSTK);
```

**Parameters:**
- `objSTK` - Dynamic stack object

**Notes:**
- Releases all memory blocks
- Releases MMU manager
- Releases stack struct itself

---

### xrtDynStackInit

Initialize dynamic stack (for stack or embedded use).

**Function Prototype:**
```c
XXAPI void xrtDynStackInit(xdynstack objSTK, uint32 iItemLength);
```

**Parameters:**
- `objSTK` - Dynamic stack pointer
- `iItemLength` - Element size

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Stack allocation
    xdynstack_struct stack;
    xrtDynStackInit(&stack, sizeof(int));
    
    // Use stack
    int* p = (int*)xrtDynStackPush(&stack);
    *p = 42;
    printf("Pushed: %d\n", *p);
    
    int* top = (int*)xrtDynStackPop(&stack);
    printf("Popped: %d\n", *top);
    
    xrtDynStackUnit(&stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackUnit

Release dynamic stack resources (not the struct itself).

**Function Prototype:**
```c
XXAPI void xrtDynStackUnit(xdynstack objSTK);
```

**Parameters:**
- `objSTK` - Dynamic stack object

**Notes:**
- Pairs with `xrtDynStackInit`
- Releases all memory blocks and MMU, but not `objSTK` itself

---

## Push Operations

### xrtDynStackPush

Push and return element pointer (struct mode).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackPush(xdynstack objSTK);
```

**Parameters:**
- `objSTK` - Dynamic stack object

**Return Value:**
- Success: Pointer to new element (can write data directly)
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Point));
    
    // Push point (10, 20)
    Point* p1 = (Point*)xrtDynStackPush(stack);
    p1->x = 10;
    p1->y = 20;
    
    // Push point (30, 40)
    Point* p2 = (Point*)xrtDynStackPush(stack);
    p2->x = 30;
    p2->y = 40;
    
    printf("Elements in stack: %u\n", stack->Count);  // 2
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

**Memory Allocation Strategy:**
```
Push flow:
1. Calculate target block: iBlock = Count >> 8 (Count / 256)
2. If block exists → use directly
3. If block doesn't exist → allocate new block (256 elements)
4. Calculate offset in block: iPos = Count & 0xFF (Count % 256)
5. Return element pointer
```

---

### xrtDynStackPushData

Push and copy data.

**Function Prototype:**
```c
XXAPI uint32 xrtDynStackPushData(xdynstack objSTK, ptr pData);
```

**Parameters:**
- `objSTK` - Dynamic stack object
- `pData` - Source data pointer

**Return Value:**
- Success: Current stack element count
- Failure: `0`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Item));
    
    Item items[] = {{100}, {200}, {300}};
    for (int i = 0; i < 3; i++) {
        uint32 count = xrtDynStackPushData(stack, &items[i]);
        printf("Pushed %d, current count: %u\n", items[i].value, count);
    }
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackPushPtr

Push pointer value (pointer mode).

**Function Prototype:**
```c
XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);
```

**Parameters:**
- `objSTK` - Dynamic stack object
- `pVal` - Pointer value to push

**Return Value:**
- Success: Current stack element count
- Failure: `0`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(ptr));
    
    // Push string pointers
    xrtDynStackPushPtr(stack, (ptr)"First");
    xrtDynStackPushPtr(stack, (ptr)"Second");
    xrtDynStackPushPtr(stack, (ptr)"Third");
    
    printf("Pointer count in stack: %u\n", stack->Count);  // 3
    
    // Pop and print
    while (stack->Count > 0) {
        str s = (str)xrtDynStackPopPtr(stack);
        printf("Popped: %s\n", s);
    }
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

## Pop Operations

### xrtDynStackPop

Pop and return element pointer (struct mode).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackPop(xdynstack objSTK);
```

**Parameters:**
- `objSTK` - Dynamic stack object

**Return Value:**
- Top element pointer

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Item));
    
    // Push
    for (int i = 1; i <= 5; i++) {
        Item* p = (Item*)xrtDynStackPush(stack);
        p->value = i * 10;
    }
    
    // Pop
    while (stack->Count > 0) {
        Item* p = (Item*)xrtDynStackPop(stack);
        printf("Popped: %d\n", p->value);
    }
    // Output: 50, 40, 30, 20, 10
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

**Delayed Release Strategy:**
```
Memory release rules on pop:
- Condition: (block_count * 256) > (Count + 288)
- When: Free capacity exceeds 288 elements, release last block
- Purpose: Avoid frequent alloc/free at critical state
```

---

### xrtDynStackPopPtr

Pop and return pointer value (pointer mode).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);
```

**Parameters:**
- `objSTK` - Dynamic stack object

**Return Value:**
- Pointer value stored at top (not element pointer)

---

## Stack Access

### xrtDynStackTop

Get top element pointer (without pop).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackTop(xdynstack objSTK);
```

**Parameters:**
- `objSTK` - Dynamic stack object

**Return Value:**
- Top element pointer

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    int* p = (int*)xrtDynStackPush(stack);
    *p = 100;
    
    // View top (without pop)
    int* top = (int*)xrtDynStackTop(stack);
    printf("Top: %d, count: %u\n", *top, stack->Count);  // 100, 1
    
    // Modify top
    *top = 200;
    printf("After modify, top: %d\n", *(int*)xrtDynStackTop(stack));  // 200
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackTopPtr

Get top pointer value (pointer mode, without pop).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK);
```

---

### xrtDynStackGetPos

Get element pointer at any position (safe version).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, uint32 iPos);
```

**Parameters:**
- `objSTK` - Dynamic stack object
- `iPos` - Position index (1-based)

**Return Value:**
- Success: Element pointer
- Failure: `NULL` (index out of bounds)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    // Push 5 elements
    for (int i = 1; i <= 5; i++) {
        int* p = (int*)xrtDynStackPush(stack);
        *p = i * 10;
    }
    
    // Random access (note: index starts from 1)
    int* item1 = (int*)xrtDynStackGetPos(stack, 1);  // First
    int* item3 = (int*)xrtDynStackGetPos(stack, 3);  // Third
    int* item5 = (int*)xrtDynStackGetPos(stack, 5);  // Fifth
    int* itemX = (int*)xrtDynStackGetPos(stack, 10); // Out of bounds
    
    printf("Position 1: %d\n", item1 ? *item1 : -1);  // 10
    printf("Position 3: %d\n", item3 ? *item3 : -1);  // 30
    printf("Position 5: %d\n", item5 ? *item5 : -1);  // 50
    printf("Position 10: %s\n", itemX ? "exists" : "NULL");  // NULL
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackGetPos_Unsafe

Get element pointer at any position (unsafe version).

**Function Prototype:**
```c
XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, uint32 iPos);
```

**⚠️ Warning:** Does not check bounds, index out of bounds leads to undefined behavior!

---

### xrtDynStackGetPosPtr / xrtDynStackGetPosPtr_Unsafe

Get pointer value at any position (pointer mode).

**Function Prototypes:**
```c
XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, uint32 iPos);
XXAPI ptr xrtDynStackGetPosPtr_Unsafe(xdynstack objSTK, uint32 iPos);
```

---

## Use Cases

### 1. Depth First Search (DFS)

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int node;
    int depth;
} DFSState;

void DFS(int startNode) {
    xdynstack stack = xrtDynStackCreate(sizeof(DFSState));
    
    // Push starting node
    DFSState* s = (DFSState*)xrtDynStackPush(stack);
    s->node = startNode;
    s->depth = 0;
    
    while (stack->Count > 0) {
        DFSState* current = (DFSState*)xrtDynStackPop(stack);
        printf("Visiting node %d, depth %d\n", current->node, current->depth);
        
        // Simulate adding child nodes (based on graph structure in real app)
        if (current->depth < 3) {
            for (int i = 0; i < 2; i++) {
                DFSState* child = (DFSState*)xrtDynStackPush(stack);
                child->node = current->node * 10 + i;
                child->depth = current->depth + 1;
            }
        }
    }
    
    xrtDynStackDestroy(stack);
}

int main() {
    xrtInit();
    DFS(1);
    xrtUnit();
    return 0;
}
```

---

### 2. Expression Evaluation

```c
#include "xrt.h"
#include <stdio.h>
#include <ctype.h>

int EvaluatePostfix(str expr) {
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    while (*expr) {
        if (isdigit(*expr)) {
            // Push number
            int* p = (int*)xrtDynStackPush(stack);
            *p = *expr - '0';
        } else if (*expr == '+' || *expr == '*') {
            // Operator: pop two operands
            int* b = (int*)xrtDynStackPop(stack);
            int* a = (int*)xrtDynStackPop(stack);
            int* result = (int*)xrtDynStackPush(stack);
            
            if (*expr == '+') {
                *result = *a + *b;
            } else {
                *result = (*a) * (*b);
            }
        }
        expr++;
    }
    
    int* result = (int*)xrtDynStackTop(stack);
    int value = *result;
    xrtDynStackDestroy(stack);
    return value;
}

int main() {
    xrtInit();
    
    // Postfix expression: 3 4 + 2 * = (3+4)*2 = 14
    int result = EvaluatePostfix((str)"34+2*");
    printf("Result: %d\n", result);  // 14
    
    xrtUnit();
    return 0;
}
```

---

### 3. Undo/Redo Functionality

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char action[64];
    int data;
} Operation;

typedef struct {
    xdynstack_struct undoStack;
    xdynstack_struct redoStack;
} Editor;

void EditorInit(Editor* e) {
    xrtDynStackInit(&e->undoStack, sizeof(Operation));
    xrtDynStackInit(&e->redoStack, sizeof(Operation));
}

void EditorUnit(Editor* e) {
    xrtDynStackUnit(&e->undoStack);
    xrtDynStackUnit(&e->redoStack);
}

void DoAction(Editor* e, str action, int data) {
    Operation* op = (Operation*)xrtDynStackPush(&e->undoStack);
    strncpy(op->action, (char*)action, 63);
    op->data = data;
    printf("Execute: %s %d\n", action, data);
    
    // Clear redo stack
    while (e->redoStack.Count > 0) {
        xrtDynStackPop(&e->redoStack);
    }
}

void Undo(Editor* e) {
    if (e->undoStack.Count == 0) {
        printf("No operation to undo\n");
        return;
    }
    
    Operation* op = (Operation*)xrtDynStackPop(&e->undoStack);
    printf("Undo: %s %d\n", op->action, op->data);
    
    // Move to redo stack
    Operation* redo = (Operation*)xrtDynStackPush(&e->redoStack);
    memcpy(redo, op, sizeof(Operation));
}

int main() {
    xrtInit();
    
    Editor editor;
    EditorInit(&editor);
    
    DoAction(&editor, (str)"Insert", 100);
    DoAction(&editor, (str)"Delete", 50);
    DoAction(&editor, (str)"Modify", 75);
    
    Undo(&editor);  // Undo Modify
    Undo(&editor);  // Undo Delete
    
    EditorUnit(&editor);
    xrtUnit();
    return 0;
}
```

---

## Comparison with Static Stack

| Feature | Static Stack | Dynamic Stack (DynStack) |
|---------|--------------|--------------------------|
| **Capacity** | Fixed (specified at creation) | Unlimited (auto-expand) |
| **Memory Layout** | Contiguous memory | Block memory (256 elems/block) |
| **Memory Efficiency** | Pre-allocated, may waste | Allocated on demand, more efficient |
| **Access Speed** | O(1), direct offset | O(1), two-level addressing |
| **Use Case** | Predictable depth | Unpredictable depth |
| **Memory Overhead** | Lower | Higher (MMU manager) |

### Selection Guide

- **Use Static Stack**: Predictable depth, need maximum performance
- **Use Dynamic Stack**: Unpredictable depth, deep recursion

---

## Best Practices

### 1. Choose Appropriate Stack Type

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    // Scenario 1: Predictable depth → Static stack
    xstack staticStack = xrtStackCreate(sizeof(Item), 100);
    printf("Static stack: max depth 100\n");
    xrtStackDestroy(staticStack);
    
    // Scenario 2: Unpredictable depth → Dynamic stack
    xdynstack dynStack = xrtDynStackCreate(sizeof(Item));
    printf("Dynamic stack: unlimited depth\n");
    xrtDynStackDestroy(dynStack);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Check Push Result

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    // Safe push
    int* p = (int*)xrtDynStackPush(stack);
    if (p == NULL) {
        printf("Memory allocation failed!\n");
        xrtDynStackDestroy(stack);
        xrtUnit();
        return 1;
    }
    *p = 42;
    printf("Push successful: %d\n", *p);
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### 3. Use PushData to Simplify Code

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Point));
    
    // Method 1: Push + assignment
    Point* p1 = (Point*)xrtDynStackPush(stack);
    p1->x = 10;
    p1->y = 20;
    
    // Method 2: PushData (more concise)
    Point pt = {30, 40};
    xrtDynStackPushData(stack, &pt);
    
    printf("Elements in stack: %u\n", stack->Count);  // 2
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Stack Static Stack](api-stack.en.md)
- [PtrArray Pointer Array](api-ptrarray.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#dynstack-dynamic-stack-library)

</div>
