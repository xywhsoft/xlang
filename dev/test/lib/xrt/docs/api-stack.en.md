# Stack Static Stack Library

> Fixed-depth stack data structure supporting both struct and pointer modes

[English](api-stack.en.md) | [中文](api-stack.md) | [返回索引](README.md)

---

## 📑 Table of Contents

- [Data Structures](#data-structures)
- [Stack Operations](#stack-operations)
- [Push Operations](#push-operations)
- [Pop Operations](#pop-operations)
- [Stack Access](#stack-access)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)

---

## Data Structures

### xstack_struct

Static stack structure.

**Definition:**
```c
typedef struct {
    union {
        char* Memory;       // Stack data memory - struct mode
        ptr* PtrMem;        // Stack data memory - pointer mode
    };
    uint32 ItemLength;      // Stack member memory size
    uint32 MaxCount;        // Stack maximum capacity (depth)
    uint32 Count;           // Current element count (top position)
} xstack_struct, *xstack;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `Memory` / `PtrMem` | Union, use different access method based on mode |
| `ItemLength` | Size of each element (bytes) |
| `MaxCount` | Maximum stack capacity |
| `Count` | Current element count |

### Stack Modes

| Mode | ItemLength | Description |
|------|------------|-------------|
| **Struct Stack** | `sizeof(T)` | Stores any struct, elements stored directly in stack memory |
| **Pointer Stack** | `sizeof(ptr)` | Stores pointers, suitable for managing external objects |

---

## Stack Operations

### xrtStackCreate

Create fixed-depth stack.

**Function Prototype:**
```c
XXAPI xstack xrtStackCreate(uint32 iMaxCount, uint32 iItemLength);
```

**Parameters:**
- `iMaxCount` - Maximum depth (stack capacity)
- `iItemLength` - Bytes per element

**Return Value:**
- Success: Stack object pointer
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Item;

int main() {
    xrtInit();
    
    // Create struct stack (max 100 Items)
    xstack stk = xrtStackCreate(100, sizeof(Item));
    printf("Stack capacity: %u\n", stk->MaxCount);  // 100
    printf("Current count: %u\n", stk->Count);       // 0
    
    xrtStackDestroy(stk);
    
    // Create pointer stack
    xstack ptrStk = xrtStackCreate(50, sizeof(ptr));
    printf("Pointer stack capacity: %u\n", ptrStk->MaxCount);  // 50
    
    xrtStackDestroy(ptrStk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackDestroy

Destroy stack.

**Definition (Macro):**
```c
#define xrtStackDestroy xrtFree
```

**Notes:**
- Directly calls `xrtFree` to release stack memory
- Stack memory is contiguously allocated, one release is sufficient

---

## Push Operations

### xrtStackPush

Push struct element (allocate space).

**Function Prototype:**
```c
XXAPI ptr xrtStackPush(xstack objSTK);
```

**Parameters:**
- `objSTK` - Stack object

**Return Value:**
- Success: Memory pointer of new element
- Failure: `NULL` (stack full)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // Push elements and set values
    Item* item1 = (Item*)xrtStackPush(stk);
    item1->value = 100;
    
    Item* item2 = (Item*)xrtStackPush(stk);
    item2->value = 200;
    
    Item* item3 = (Item*)xrtStackPush(stk);
    item3->value = 300;
    
    printf("Stack elements: %u\n", stk->Count);  // 3
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackPushData

Push struct element (copy data).

**Function Prototype:**
```c
XXAPI uint32 xrtStackPushData(xstack objSTK, ptr pData);
```

**Parameters:**
- `objSTK` - Stack object
- `pData` - Data pointer to copy

**Return Value:**
- Success: New element position (1-based)
- Failure: `0` (stack full)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int x; int y; } Point;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Point));
    
    Point p1 = {10, 20};
    Point p2 = {30, 40};
    Point p3 = {50, 60};
    
    uint32 pos1 = xrtStackPushData(stk, &p1);
    uint32 pos2 = xrtStackPushData(stk, &p2);
    uint32 pos3 = xrtStackPushData(stk, &p3);
    
    printf("Positions: %u, %u, %u\n", pos1, pos2, pos3);  // 1, 2, 3
    
    // Verify data
    Point* top = (Point*)xrtStackTop(stk);
    printf("Top: (%d, %d)\n", top->x, top->y);  // (50, 60)
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackPushPtr

Push pointer element.

**Function Prototype:**
```c
XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal);
```

**Parameters:**
- `objSTK` - Stack object
- `pVal` - Pointer to store

**Return Value:**
- Success: New element position (1-based)
- Failure: `0` (stack full)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(ptr));
    
    str s1 = (str)"First";
    str s2 = (str)"Second";
    str s3 = (str)"Third";
    
    xrtStackPushPtr(stk, s1);
    xrtStackPushPtr(stk, s2);
    xrtStackPushPtr(stk, s3);
    
    printf("Pointers in stack: %u\n", stk->Count);  // 3
    
    // Pop and print
    while (stk->Count > 0) {
        str s = (str)xrtStackPopPtr(stk);
        printf("Popped: %s\n", s);
    }
    // Output: Third, Second, First
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

## Pop Operations

### xrtStackPop

Pop struct element.

**Function Prototype:**
```c
XXAPI ptr xrtStackPop(xstack objSTK);
```

**Parameters:**
- `objSTK` - Stack object

**Return Value:**
- Success: Pointer to popped element (valid until next Push)
- Failure: `NULL` (stack empty)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // Push
    for (int i = 1; i <= 5; i++) {
        Item* item = (Item*)xrtStackPush(stk);
        item->value = i * 10;
    }
    
    // Pop
    printf("Pop order: ");
    Item* item;
    while ((item = (Item*)xrtStackPop(stk)) != NULL) {
        printf("%d ", item->value);
    }
    printf("\n");  // 50 40 30 20 10
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

**Note:** Returned pointer is valid until next Push, may be overwritten afterwards.

---

### xrtStackPopPtr

Pop pointer element.

**Function Prototype:**
```c
XXAPI ptr xrtStackPopPtr(xstack objSTK);
```

**Parameters:**
- `objSTK` - Stack object

**Return Value:**
- Success: Stored pointer value
- Failure: `NULL` (stack empty)

---

## Stack Access

### xrtStackTop / xrtStackTopPtr

Get top element (without popping).

**Function Prototype:**
```c
XXAPI ptr xrtStackTop(xstack objSTK);      // Struct stack
XXAPI ptr xrtStackTopPtr(xstack objSTK);   // Pointer stack
```

**Return Value:**
- Success: Top element pointer/pointer value
- Failure: `NULL` (stack empty)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // Push 3 elements
    ((Item*)xrtStackPush(stk))->value = 10;
    ((Item*)xrtStackPush(stk))->value = 20;
    ((Item*)xrtStackPush(stk))->value = 30;
    
    // Peek at top (no pop)
    Item* top = (Item*)xrtStackTop(stk);
    printf("Top: %d\n", top->value);  // 30
    printf("Stack depth: %u\n", stk->Count);  // 3 (unchanged)
    
    // Modify top
    top->value = 99;
    
    // Pop to verify
    Item* popped = (Item*)xrtStackPop(stk);
    printf("Popped: %d\n", popped->value);  // 99
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackGetPos / xrtStackGetPos_Unsafe

Get struct element at any position.

**Function Prototype:**
```c
XXAPI ptr xrtStackGetPos(xstack objSTK, uint32 iPos);         // Safe version
XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint32 iPos);  // Unsafe version
```

**Parameters:**
- `objSTK` - Stack object
- `iPos` - Element position (**1-based**, 1 = bottom)

**Return Value:**
- Success: Element pointer
- Failure: `NULL` (out of bounds, safe version only)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // Push 5 elements
    for (int i = 1; i <= 5; i++) {
        ((Item*)xrtStackPush(stk))->value = i * 10;
    }
    // Stack contents: [10, 20, 30, 40, 50]
    //                 pos=1  2   3   4   5
    
    // Safe version access
    Item* item = (Item*)xrtStackGetPos(stk, 3);
    printf("Position 3: %d\n", item->value);  // 30
    
    // Out of bounds access (safe version returns NULL)
    Item* invalid = (Item*)xrtStackGetPos(stk, 10);
    printf("Out of bounds: %s\n", invalid ? "valid" : "NULL");  // NULL
    
    // Traverse all elements (from bottom to top)
    printf("All elements: ");
    for (uint32 i = 1; i <= stk->Count; i++) {
        Item* it = (Item*)xrtStackGetPos(stk, i);
        printf("%d ", it->value);
    }
    printf("\n");  // 10 20 30 40 50
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackGetPosPtr / xrtStackGetPosPtr_Unsafe

Get pointer element at any position.

**Function Prototype:**
```c
XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint32 iPos);         // Safe version
XXAPI ptr xrtStackGetPosPtr_Unsafe(xstack objSTK, uint32 iPos);  // Unsafe version
```

**Parameters:**
- `objSTK` - Stack object
- `iPos` - Element position (**1-based**)

**Return Value:**
- Success: Stored pointer value
- Failure: `NULL` (out of bounds, safe version only)

---

## Usage Scenarios

### 1. Expression Evaluation

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xstack numStack = xrtStackCreate(100, sizeof(int));
    
    // Simulate calculating 3 + 4 * 2
    // Push numbers
    *(int*)xrtStackPush(numStack) = 3;
    *(int*)xrtStackPush(numStack) = 4;
    *(int*)xrtStackPush(numStack) = 2;
    
    // Calculate 4 * 2
    int b = *(int*)xrtStackPop(numStack);  // 2
    int a = *(int*)xrtStackPop(numStack);  // 4
    *(int*)xrtStackPush(numStack) = a * b;  // 8
    
    // Calculate 3 + 8
    b = *(int*)xrtStackPop(numStack);  // 8
    a = *(int*)xrtStackPop(numStack);  // 3
    int result = a + b;  // 11
    
    printf("3 + 4 * 2 = %d\n", result);
    
    xrtStackDestroy(numStack);
    xrtUnit();
    return 0;
}
```

---

### 2. Bracket Matching

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

bool CheckBrackets(const char* expr) {
    xstack stk = xrtStackCreate(strlen(expr), sizeof(char));
    bool valid = TRUE;
    
    for (int i = 0; expr[i] && valid; i++) {
        char c = expr[i];
        if (c == '(' || c == '[' || c == '{') {
            *(char*)xrtStackPush(stk) = c;
        } else if (c == ')' || c == ']' || c == '}') {
            if (stk->Count == 0) {
                valid = FALSE;
            } else {
                char open = *(char*)xrtStackPop(stk);
                if ((c == ')' && open != '(') ||
                    (c == ']' && open != '[') ||
                    (c == '}' && open != '{')) {
                    valid = FALSE;
                }
            }
        }
    }
    
    valid = valid && (stk->Count == 0);
    xrtStackDestroy(stk);
    return valid;
}

int main() {
    xrtInit();
    
    printf("({[]}) : %s\n", CheckBrackets("({[]})") ? "valid" : "invalid");  // valid
    printf("({[}]) : %s\n", CheckBrackets("({[}])") ? "valid" : "invalid");  // invalid
    printf("((())  : %s\n", CheckBrackets("((())") ? "valid" : "invalid");   // invalid
    
    xrtUnit();
    return 0;
}
```

---

### 3. Path Backtracking

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Position;

int main() {
    xrtInit();
    
    xstack path = xrtStackCreate(100, sizeof(Position));
    
    // Simulate path recording
    Position moves[] = {{0,0}, {1,0}, {1,1}, {2,1}, {2,2}};
    for (int i = 0; i < 5; i++) {
        xrtStackPushData(path, &moves[i]);
    }
    
    printf("Path length: %u\n", path->Count);
    
    // Backtrack to start
    printf("Backtrack path: ");
    Position* pos;
    while ((pos = (Position*)xrtStackPop(path)) != NULL) {
        printf("(%d,%d) ", pos->x, pos->y);
    }
    printf("\n");  // (2,2) (2,1) (1,1) (1,0) (0,0)
    
    xrtStackDestroy(path);
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Choose Appropriate Stack Type

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Known maximum depth → Static stack (faster)
    xstack fixedStack = xrtStackCreate(100, sizeof(int));
    
    // Unknown depth → Dynamic stack (see api-dynstack.en.md)
    // xdynstack dynStack = xrtDynStackCreate(sizeof(int));
    
    printf("Static stack: Fixed capacity, contiguous memory, best performance\n");
    printf("Dynamic stack: Auto-expanding, unlimited depth\n");
    
    xrtStackDestroy(fixedStack);
    xrtUnit();
    return 0;
}
```

---

### 2. Stack Full Check

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(3, sizeof(Item));
    
    for (int i = 0; i < 5; i++) {
        // Check if there's space
        if (stk->Count >= stk->MaxCount) {
            printf("Stack full, cannot push element %d\n", i + 1);
            break;
        }
        
        Item* item = (Item*)xrtStackPush(stk);
        item->value = i;
        printf("Pushed: %d\n", i);
    }
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### 3. Avoid Dangling Pointers

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    ((Item*)xrtStackPush(stk))->value = 100;
    
    // Pop returns pointer valid until next Push
    Item* popped = (Item*)xrtStackPop(stk);
    printf("Just popped: %d\n", popped->value);  // 100 ✓
    
    // After Push, original pointer may be overwritten
    ((Item*)xrtStackPush(stk))->value = 200;
    // printf("%d\n", popped->value);  // Dangerous! May be 200
    
    // ✓ Correct approach: Use immediately or copy
    Item copy = *popped;  // Copy data
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [DynStack Dynamic Stack](api-dynstack.en.md)
- [Array Structure Array](api-array.en.md)
- [PtrArray Pointer Array](api-ptrarray.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#stack-static-stack-library)

</div>
