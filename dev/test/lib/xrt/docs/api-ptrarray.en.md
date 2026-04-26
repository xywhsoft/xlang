# PtrArray Pointer Array Library

> Dynamic pointer array with insert, delete, sort and other operations

[English](api-ptrarray.en.md) | [中文](api-ptrarray.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [Data Structures](#data-structures)
- [Array Operations](#array-operations)
- [Element Access](#element-access)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)

---

## Constant Definitions

| Constant | Value | Description |
|----------|-------|-------------|
| `XPARRAY_PREASSIGNSTEP` | `256` | Default pre-allocation step (element count) |

### Index Rules

```
Element numbering rules (0 indicates non-existent element):
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

**Important:** Index starts from **1**, not 0! 0 indicates non-existent element.

---

## Data Structures

### xparray_struct

Pointer array manager structure.

**Definition:**
```c
typedef struct {
    ptr* Memory;        // Pointer array memory
    uint32 Count;       // Current element count
    uint32 AllocCount;  // Allocated capacity
    uint32 AllocStep;   // Pre-allocation step
} xparray_struct, *xparray;
```

**Member Description:**
- `Memory` - Array storing pointers
- `Count` - Current element count (can be read directly)
- `AllocCount` - Maximum allocated capacity
- `AllocStep` - Increment when expanding

---

## Array Operations

### xrtPtrArrayCreate

Create pointer array manager.

**Function Prototype:**
```c
XXAPI xparray xrtPtrArrayCreate();
```

**Return Value:**
- Success: Pointer array object
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtPtrArrayDestroy` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    printf("Step: %u\n", arr->AllocStep);  // 256
    
    xrtPtrArrayAppend(arr, (ptr)"item1");
    xrtPtrArrayAppend(arr, (ptr)"item2");
    printf("Element count: %u\n", arr->Count);  // 2
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayDestroy

Destroy pointer array manager.

**Function Prototype:**
```c
XXAPI void xrtPtrArrayDestroy(xparray pObject);
```

**Notes:**
- Releases internal memory and manager structure
- Does not release memory pointed to by pointers stored in array

---

### xrtPtrArrayInit

Initialize pointer array (for stack or embedded structures).

**Function Prototype:**
```c
XXAPI void xrtPtrArrayInit(xparray pObject);
```

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Stack allocation
    xparray_struct arr;
    xrtPtrArrayInit(&arr);
    
    xrtPtrArrayAppend(&arr, (ptr)"data1");
    xrtPtrArrayAppend(&arr, (ptr)"data2");
    printf("Element count: %u\n", arr.Count);
    
    xrtPtrArrayUnit(&arr);  // Release internal memory
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayUnit

Release array internal memory (does not release structure).

**Function Prototype:**
```c
XXAPI void xrtPtrArrayUnit(xparray pObject);
```

**Aliases:**
- `xrtPtrArrayRemoveAll` - Remove all members
- `xrtPtrArrayClear` - Clear manager

---

### xrtPtrArrayMalloc

Pre-allocate or adjust array capacity.

**Function Prototype:**
```c
XXAPI bool xrtPtrArrayMalloc(xparray pObject, uint32 iCount);
```

**Parameters:**
- `pObject` - Array object
- `iCount` - Target capacity (element count)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    // Pre-allocate capacity for 1000 elements
    xrtPtrArrayMalloc(arr, 1000);
    printf("Allocated: %u\n", arr->AllocCount);  // 1000
    
    // Adding elements won't trigger reallocation
    for (int i = 0; i < 500; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    printf("Element count: %u\n", arr->Count);  // 500
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayInsert

Insert element at specified position.

**Function Prototype:**
```c
XXAPI uint32 xrtPtrArrayInsert(xparray pObject, uint32 iPos, ptr pVal);
```

**Parameters:**
- `pObject` - Array object
- `iPos` - Insert position (0 = head, Count = tail)
- `pVal` - Pointer to insert

**Return Value:**
- Success: New element's index (1-based)
- Failure: `0`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    xrtPtrArrayAppend(arr, (ptr)"B");
    xrtPtrArrayAppend(arr, (ptr)"D");
    
    // Insert at head
    xrtPtrArrayInsert(arr, 0, (ptr)"A");
    
    // Insert in middle (after position 2)
    xrtPtrArrayInsert(arr, 2, (ptr)"C");
    
    // Print: A B C D
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%s ", (char*)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayAppend

Append element to array end.

**Function Prototype:**
```c
XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal);
```

**Parameters:**
- `pObject` - Array object
- `pVal` - Pointer to append

**Return Value:**
- Success: New element's index (1-based)
- Failure: `0`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    uint32 idx1 = xrtPtrArrayAppend(arr, (ptr)"first");
    uint32 idx2 = xrtPtrArrayAppend(arr, (ptr)"second");
    uint32 idx3 = xrtPtrArrayAppend(arr, (ptr)"third");
    
    printf("Indices: %u, %u, %u\n", idx1, idx2, idx3);  // 1, 2, 3
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayAddAlt

Add element, automatically find gap (replace NULL value).

**Function Prototype:**
```c
XXAPI uint32 xrtPtrArrayAddAlt(xparray pObject, ptr pVal);
```

**Parameters:**
- `pObject` - Array object
- `pVal` - Pointer to add

**Return Value:**
- Success: Element index (1-based)
- Failure: `0`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    xrtPtrArrayAppend(arr, (ptr)"A");  // Index 1
    xrtPtrArrayAppend(arr, (ptr)"B");  // Index 2
    xrtPtrArrayAppend(arr, (ptr)"C");  // Index 3
    
    // Set index 2 to NULL
    xrtPtrArraySet(arr, 2, NULL);
    
    // AddAlt will find gap and fill
    uint32 idx = xrtPtrArrayAddAlt(arr, (ptr)"D");
    printf("Filled at index: %u\n", idx);  // 2
    
    // Print: A D C
    for (uint32 i = 1; i <= arr->Count; i++) {
        ptr p = xrtPtrArrayGet(arr, i);
        printf("%s ", p ? (char*)p : "(null)");
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Prioritizes reusing deleted slots (positions set to NULL)
- Appends at end when no gaps available

---

### xrtPtrArraySwap

Swap positions of two elements.

**Function Prototype:**
```c
XXAPI bool xrtPtrArraySwap(xparray pObject, uint32 iPosA, uint32 iPosB);
```

**Parameters:**
- `pObject` - Array object
- `iPosA` - First element index (1-based)
- `iPosB` - Second element index (1-based)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (index out of bounds)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"A");
    xrtPtrArrayAppend(arr, (ptr)"B");
    xrtPtrArrayAppend(arr, (ptr)"C");
    
    // Swap first and third elements
    xrtPtrArraySwap(arr, 1, 3);
    
    // Print: C B A
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%s ", (char*)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayRemove

Remove elements.

**Function Prototype:**
```c
XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount);
```

**Parameters:**
- `pObject` - Array object
- `iPos` - Start index (1-based)
- `iCount` - Count to remove

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (index out of bounds or count is 0)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    for (int i = 0; i < 5; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)(i + 1));
    }
    printf("Before removal: %u elements\n", arr->Count);  // 5
    
    // Remove 2 elements starting from position 2
    xrtPtrArrayRemove(arr, 2, 2);
    printf("After removal: %u elements\n", arr->Count);  // 3
    
    // Remaining: 1, 4, 5
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%zu ", (size_t)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArraySort

Sort array elements.

**Function Prototype:**
```c
XXAPI bool xrtPtrArraySort(xparray pObject, ptr procCompar);
```

**Parameters:**
- `pObject` - Array object
- `procCompar` - Comparison function pointer (qsort format)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// String comparison function
int CompareStr(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    return strcmp(sa, sb);
}

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"cherry");
    xrtPtrArrayAppend(arr, (ptr)"apple");
    xrtPtrArrayAppend(arr, (ptr)"banana");
    
    // Sort
    xrtPtrArraySort(arr, (ptr)CompareStr);
    
    // Print: apple banana cherry
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%s ", (char*)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## Element Access

### xrtPtrArrayGet

Get element pointer (safe version).

**Function Prototype:**
```c
XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos);
```

**Parameters:**
- `pObject` - Array object
- `iPos` - Element index (1-based)

**Return Value:**
- Success: Element pointer
- Failure: `NULL` (index out of bounds)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"first");
    xrtPtrArrayAppend(arr, (ptr)"second");
    
    // Note: Index starts from 1
    ptr item1 = xrtPtrArrayGet(arr, 1);
    ptr item2 = xrtPtrArrayGet(arr, 2);
    ptr item3 = xrtPtrArrayGet(arr, 3);  // Out of bounds, returns NULL
    
    printf("1: %s\n", item1 ? (char*)item1 : "(null)");
    printf("2: %s\n", item2 ? (char*)item2 : "(null)");
    printf("3: %s\n", item3 ? "exists" : "(null)");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayGet_Unsafe / xrtPtrArrayGet_Inline

Get element pointer (unsafe/inline version).

**Function Prototype:**
```c
XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos);
static inline ptr xrtPtrArrayGet_Inline(xparray pObject, uint32 iPos);
```

**Notes:**
- No bounds checking, higher performance
- Caller must ensure valid index

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    for (int i = 0; i < 1000; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    
    // High-performance traversal (ensure valid index)
    size_t sum = 0;
    for (uint32 i = 1; i <= arr->Count; i++) {
        sum += (size_t)xrtPtrArrayGet_Inline(arr, i);
    }
    printf("Sum: %zu\n", sum);  // 499500
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArraySet

Set element pointer (safe version).

**Function Prototype:**
```c
XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal);
```

**Parameters:**
- `pObject` - Array object
- `iPos` - Element index (1-based)
- `pVal` - New pointer value

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (index out of bounds)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"old1");
    xrtPtrArrayAppend(arr, (ptr)"old2");
    
    // Modify second element
    xrtPtrArraySet(arr, 2, (ptr)"new2");
    
    printf("%s %s\n", 
        (char*)xrtPtrArrayGet(arr, 1),
        (char*)xrtPtrArrayGet(arr, 2));  // old1 new2
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArraySet_Unsafe / xrtPtrArraySet_Inline

Set element pointer (unsafe/inline version).

**Function Prototype:**
```c
XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal);
static inline void xrtPtrArraySet_Inline(xparray pObject, uint32 iPos, ptr pVal);
```

**Notes:**
- No bounds checking, higher performance
- Caller must ensure valid index

---

### Direct Structure Access

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"a");
    xrtPtrArrayAppend(arr, (ptr)"b");
    
    // Direct access to structure fields
    uint32 count = arr->Count;        // Element count
    uint32 cap = arr->AllocCount;     // Allocated capacity
    ptr* memory = arr->Memory;        // Internal pointer array
    
    printf("Elements: %u, Capacity: %u\n", count, cap);
    
    // Direct access to internal array (index starts from 0)
    for (uint32 i = 0; i < count; i++) {
        printf("%s ", (char*)memory[i]);
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## Usage Scenarios

### 1. Object Collection Management

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    str name;
} User;

User* CreateUser(int id, str name) {
    User* u = xrtMalloc(sizeof(User));
    u->id = id;
    u->name = xrtCopyStr(name, 0);
    return u;
}

void FreeUser(User* u) {
    if (u) {
        xrtFree(u->name);
        xrtFree(u);
    }
}

int main() {
    xrtInit();
    
    xparray users = xrtPtrArrayCreate();
    
    // Add users
    xrtPtrArrayAppend(users, CreateUser(1, (str)"Alice"));
    xrtPtrArrayAppend(users, CreateUser(2, (str)"Bob"));
    xrtPtrArrayAppend(users, CreateUser(3, (str)"Charlie"));
    
    // Traverse
    for (uint32 i = 1; i <= users->Count; i++) {
        User* u = xrtPtrArrayGet(users, i);
        printf("User %d: %s\n", u->id, u->name);
    }
    
    // Free all users
    for (uint32 i = 1; i <= users->Count; i++) {
        FreeUser(xrtPtrArrayGet(users, i));
    }
    xrtPtrArrayDestroy(users);
    
    xrtUnit();
    return 0;
}
```

---

### 2. String List

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray lines = xrtPtrArrayCreate();
    
    // Add strings (copy)
    xrtPtrArrayAppend(lines, xrtCopyStr((str)"Line 1", 0));
    xrtPtrArrayAppend(lines, xrtCopyStr((str)"Line 2", 0));
    xrtPtrArrayAppend(lines, xrtCopyStr((str)"Line 3", 0));
    
    // Print
    for (uint32 i = 1; i <= lines->Count; i++) {
        printf("%u: %s\n", i, (char*)xrtPtrArrayGet(lines, i));
    }
    
    // Free strings
    for (uint32 i = 1; i <= lines->Count; i++) {
        xrtFree(xrtPtrArrayGet(lines, i));
    }
    xrtPtrArrayDestroy(lines);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Object Pool (Using AddAlt)

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    bool active;
} Connection;

int main() {
    xrtInit();
    
    xparray pool = xrtPtrArrayCreate();
    
    // Create connections
    for (int i = 0; i < 5; i++) {
        Connection* conn = xrtMalloc(sizeof(Connection));
        conn->id = i;
        conn->active = TRUE;
        xrtPtrArrayAppend(pool, conn);
    }
    printf("Initial connections: %u\n", pool->Count);  // 5
    
    // "Delete" connections 2 and 4 (set to NULL)
    xrtFree(xrtPtrArrayGet(pool, 2));
    xrtPtrArraySet(pool, 2, NULL);
    xrtFree(xrtPtrArrayGet(pool, 4));
    xrtPtrArraySet(pool, 4, NULL);
    
    // Add new connection (will reuse empty slot)
    Connection* newConn = xrtMalloc(sizeof(Connection));
    newConn->id = 100;
    newConn->active = TRUE;
    uint32 idx = xrtPtrArrayAddAlt(pool, newConn);
    printf("New connection position: %u\n", idx);  // 2 (reused empty slot)
    
    // Cleanup
    for (uint32 i = 1; i <= pool->Count; i++) {
        ptr p = xrtPtrArrayGet(pool, i);
        if (p) xrtFree(p);
    }
    xrtPtrArrayDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Pre-allocate Capacity

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    // Pre-allocate capacity for 10000 elements
    xrtPtrArrayMalloc(arr, 10000);
    
    // Adding elements won't trigger reallocation
    for (int i = 0; i < 10000; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    
    printf("Elements: %u\n", arr->Count);
    printf("Capacity: %u\n", arr->AllocCount);
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### 2. Properly Release Object Memory

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    // Add dynamically allocated strings
    xrtPtrArrayAppend(arr, xrtCopyStr((str)"str1", 0));
    xrtPtrArrayAppend(arr, xrtCopyStr((str)"str2", 0));
    
    // ✗ Wrong: Directly destroying array leaks string memory
    // xrtPtrArrayDestroy(arr);
    
    // ✓ Correct: Free each element first
    for (uint32 i = 1; i <= arr->Count; i++) {
        xrtFree(xrtPtrArrayGet(arr, i));
    }
    xrtPtrArrayDestroy(arr);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Use Inline Version for Performance

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    for (int i = 0; i < 1000000; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    
    // High-performance traversal (known valid indices)
    size_t sum = 0;
    for (uint32 i = 1; i <= arr->Count; i++) {
        sum += (size_t)xrtPtrArrayGet_Inline(arr, i);
    }
    printf("Sum: %zu\n", sum);
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Array Structure Array](api-array.en.md)
- [List](api-list.en.md)
- [BSMM Block Structure Memory Management](api-bsmm.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#ptrarray-pointer-array-library)

</div>
