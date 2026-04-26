# Array Struct Array Library

> Dynamic struct array supporting elements of any size

[English](api-array.en.md) | [中文](api-array.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [Data Structures](#data-structures)
- [Array Operations](#array-operations)
- [Element Access](#element-access)
- [Use Cases](#use-cases)
- [Best Practices](#best-practices)

---

## Constant Definitions

| Constant | Value | Description |
|----------|-------|-------------|
| `XARRAY_PREASSIGNSTEP` | `256` | Default pre-allocation step (number of elements) |

### Index Rules

```
Element numbering rules (0 represents non-existent element):
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

**Important:** Index starts from **1**, not from 0!

---

## Data Structures

### xarray_struct

Struct array memory manager data structure.

**Definition:**
```c
typedef struct {
    str Memory;         // Manager memory pointer
    uint32 ItemLength;  // Memory length occupied by each element
    uint32 Count;       // Number of elements in the manager
    uint32 AllocCount;  // Number of allocated structures
    uint32 AllocStep;   // Pre-allocation memory step
} xarray_struct, *xarray;
```

**Member Description:**
- `Memory` - Contiguous memory block pointer storing all elements
- `ItemLength` - Bytes occupied by each element (specified at creation)
- `Count` - Current number of elements
- `AllocCount` - Allocated capacity (number of elements)
- `AllocStep` - Number of elements added on each expansion (default 256)

---

## Array Operations

### xrtArrayCreate

Create a struct array.

**Function Prototype:**
```c
XXAPI xarray xrtArrayCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Size of each element (bytes)

**Return Value:**
- Success: Array object
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtArrayDestroy` to release

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
    
    // Create student array
    xarray students = xrtArrayCreate(sizeof(Student));
    if (!students) {
        printf("Creation failed\n");
        xrtUnit();
        return 1;
    }
    
    printf("Element size: %u bytes\n", students->ItemLength);  // 40 or more
    printf("Current count: %u\n", students->Count);            // 0
    
    xrtArrayDestroy(students);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayDestroy

Destroy struct array and release all memory.

**Function Prototype:**
```c
XXAPI void xrtArrayDestroy(xarray pArr);
```

**Parameters:**
- `pArr` - Array object

**Notes:**
- Releases both internal memory and array struct
- If elements contain dynamically allocated memory, release them manually first

---

### xrtArrayInit

Initialize array data structure (for embedded array objects).

**Function Prototype:**
```c
XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength);
```

**Parameters:**
- `pArr` - Array object pointer
- `iItemLength` - Size of each element (bytes)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

typedef struct {
    char name[32];
    xarray_struct items;  // Embedded array
} Container;

int main() {
    xrtInit();
    
    Container cont;
    strcpy(cont.name, "MyContainer");
    
    // Initialize embedded array
    xrtArrayInit(&cont.items, sizeof(Item));
    
    // Use array
    uint32 pos = xrtArrayAppend(&cont.items, 1);
    Item* item = (Item*)xrtArrayGet(&cont.items, pos);
    item->value = 100;
    
    printf("%s: has %u elements\n", cont.name, cont.items.Count);
    
    // Release only internal memory, not the struct itself
    xrtArrayUnit(&cont.items);
    
    xrtUnit();
    return 0;
}
```

---

### xrtArrayUnit

Release array data structure (but not the array struct memory itself).

**Function Prototype:**
```c
XXAPI void xrtArrayUnit(xarray pArr);
```

**Parameters:**
- `pArr` - Array object

**Notes:**
- Releases internal memory, resets `Count` and `AllocCount` to 0
- Does not release `xarray` struct itself
- Paired with `xrtArrayInit`

**Aliases:**
- `xrtArrayRemoveAll` - Remove all members
- `xrtArrayClear` - Clear manager

---

### xrtArrayAlloc

Pre-allocate or trim array memory.

**Function Prototype:**
```c
XXAPI bool xrtArrayAlloc(xarray pArr, uint32 iCount);
```

**Parameters:**
- `pArr` - Array object
- `iCount` - Target capacity (number of elements)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xarray points = xrtArrayCreate(sizeof(Point));
    
    // Pre-allocate space for 1000 elements
    xrtArrayAlloc(points, 1000);
    printf("Allocated capacity: %u\n", points->AllocCount);  // 1000
    printf("Actual element count: %u\n", points->Count);     // 0
    
    // Adding elements won't trigger memory allocation
    for (int i = 0; i < 100; i++) {
        uint32 pos = xrtArrayAppend(points, 1);
        Point* p = (Point*)xrtArrayGet(points, pos);
        p->x = i;
        p->y = i * 2;
    }
    printf("Capacity after adding: %u\n", points->AllocCount);  // Still 1000
    
    // Trim excess space
    xrtArrayAlloc(points, points->Count);
    printf("Capacity after trim: %u\n", points->AllocCount);  // 100
    
    xrtArrayDestroy(points);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayInsert

Insert elements at specified position.

**Function Prototype:**
```c
XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount);
```

**Parameters:**
- `pArr` - Array object
- `iPos` - Insert position (0 = head insert, Count = tail insert)
- `iCount` - Number of elements to insert (0 is treated as 1)

**Return Value:**
- Success: Index of first new element (1-based)
- Failure: `0`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    char name[16];
} Item;

int main() {
    xrtInit();
    
    xarray arr = xrtArrayCreate(sizeof(Item));
    
    // Add B and D
    uint32 pos1 = xrtArrayAppend(arr, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos1))->name, "B");
    
    uint32 pos2 = xrtArrayAppend(arr, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos2))->name, "D");
    
    // Insert A at head
    uint32 pos3 = xrtArrayInsert(arr, 0, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos3))->name, "A");
    
    // Insert C after position 2 (between B and D)
    uint32 pos4 = xrtArrayInsert(arr, 2, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos4))->name, "C");
    
    // Print: A B C D
    for (uint32 i = 1; i <= arr->Count; i++) {
        Item* item = (Item*)xrtArrayGet(arr, i);
        printf("%s ", item->name);
    }
    printf("\n");
    
    xrtArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayAppend

Append elements at end.

**Function Prototype:**
```c
XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount);
```

**Parameters:**
- `pArr` - Array object
- `iCount` - Number of elements to add

**Return Value:**
- Success: Index of first new element (1-based)
- Failure: `0`

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
    
    xarray users = xrtArrayCreate(sizeof(User));
    
    // Add single user
    uint32 pos = xrtArrayAppend(users, 1);
    User* u1 = (User*)xrtArrayGet(users, pos);
    u1->id = 1;
    strcpy(u1->name, "Alice");
    
    // Batch add 3 users
    uint32 start = xrtArrayAppend(users, 3);
    for (uint32 i = 0; i < 3; i++) {
        User* u = (User*)xrtArrayGet(users, start + i);
        u->id = 2 + i;
        sprintf(u->name, "User%d", 2 + i);
    }
    
    printf("Total %u users:\n", users->Count);
    for (uint32 i = 1; i <= users->Count; i++) {
        User* u = (User*)xrtArrayGet(users, i);
        printf("  %d: %s\n", u->id, u->name);
    }
    
    xrtArrayDestroy(users);
    xrtUnit();
    return 0;
}
```

---

### xrtArraySwap

Swap positions of two elements.

**Function Prototype:**
```c
XXAPI bool xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB);
```

**Parameters:**
- `pArr` - Array object
- `iPosA` - First element index (1-based)
- `iPosB` - Second element index (1-based)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (index out of bounds)

---

### xrtArrayRemove

Remove elements.

**Function Prototype:**
```c
XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount);
```

**Parameters:**
- `pArr` - Array object
- `iPos` - Start position (1-based)
- `iCount` - Number to remove

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (index out of bounds or count is 0)

---

### xrtArraySort

Sort the array.

**Function Prototype:**
```c
XXAPI bool xrtArraySort(xarray pArr, ptr procCompar);
```

**Parameters:**
- `pArr` - Array object
- `procCompar` - Comparison function pointer (same as `qsort` comparison function)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int score;
    char name[16];
} Student;

// Sort by score descending
int CompareByScore(const void* a, const void* b) {
    Student* sa = (Student*)a;
    Student* sb = (Student*)b;
    return sb->score - sa->score;  // Descending
}

int main() {
    xrtInit();
    
    xarray students = xrtArrayCreate(sizeof(Student));
    
    // Add students
    const char* names[] = {"Alice", "Bob", "Charlie"};
    int scores[] = {85, 92, 78};
    
    for (int i = 0; i < 3; i++) {
        uint32 pos = xrtArrayAppend(students, 1);
        Student* s = (Student*)xrtArrayGet(students, pos);
        s->score = scores[i];
        strcpy(s->name, names[i]);
    }
    
    printf("Before sort:\n");
    for (uint32 i = 1; i <= students->Count; i++) {
        Student* s = (Student*)xrtArrayGet(students, i);
        printf("  %s: %d\n", s->name, s->score);
    }
    
    // Sort by score
    xrtArraySort(students, CompareByScore);
    
    printf("After sort (by score descending):\n");
    for (uint32 i = 1; i <= students->Count; i++) {
        Student* s = (Student*)xrtArrayGet(students, i);
        printf("  %s: %d\n", s->name, s->score);
    }
    
    xrtArrayDestroy(students);
    xrtUnit();
    return 0;
}
```

---

## Element Access

### xrtArrayGet

Get element pointer (safe version).

**Function Prototype:**
```c
XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos);
```

**Parameters:**
- `pArr` - Array object
- `iPos` - Element index (1-based)

**Return Value:**
- Success: Element pointer
- Failure: `NULL` (index out of bounds)

---

### xrtArrayGet_Unsafe

Get element pointer (unsafe version, no bounds checking).

**Function Prototype:**
```c
XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos);
```

**Warning:** Must ensure index is valid before calling, otherwise may cause out-of-bounds memory access!

---

### xrtArrayGet_Inline

Get element pointer (inline version, no bounds checking).

**Function Prototype:**
```c
static inline ptr xrtArrayGet_Inline(xarray pArr, uint32 iPos)
{
    return &(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
}
```

**Notes:**
- Used for high-performance traversal scenarios
- No function call overhead
- Does not check bounds, must ensure valid index before calling

---

## Use Cases

### 1. Batch Data Processing

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    double value;
    int processed;
} DataRecord;

int main() {
    xrtInit();
    
    xarray records = xrtArrayCreate(sizeof(DataRecord));
    
    // Pre-allocate space (avoid frequent expansion)
    xrtArrayAlloc(records, 10000);
    
    // Batch add data
    for (int i = 0; i < 10000; i++) {
        uint32 pos = xrtArrayAppend(records, 1);
        DataRecord* r = (DataRecord*)xrtArrayGet(records, pos);
        r->id = i + 1;
        r->value = i * 1.5;
        r->processed = 0;
    }
    
    // Batch process
    int processedCount = 0;
    for (uint32 i = 1; i <= records->Count; i++) {
        DataRecord* r = (DataRecord*)xrtArrayGet_Inline(records, i);
        if (r->value > 5000) {
            r->processed = 1;
            processedCount++;
        }
    }
    
    printf("Processed %d records\n", processedCount);
    
    xrtArrayDestroy(records);
    xrtUnit();
    return 0;
}
```

---

### 2. Task Priority Queue

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int priority;
    char task[64];
} Task;

int ComparePriority(const void* a, const void* b) {
    Task* ta = (Task*)a;
    Task* tb = (Task*)b;
    return tb->priority - ta->priority;  // Higher priority first
}

int main() {
    xrtInit();
    
    xarray tasks = xrtArrayCreate(sizeof(Task));
    
    // Add tasks
    struct { int priority; const char* name; } taskData[] = {
        {3, "Send email"},
        {1, "Code review"},
        {5, "Emergency fix"},
        {2, "Write documentation"}
    };
    
    for (int i = 0; i < 4; i++) {
        uint32 pos = xrtArrayAppend(tasks, 1);
        Task* t = (Task*)xrtArrayGet(tasks, pos);
        t->id = i + 1;
        t->priority = taskData[i].priority;
        strcpy(t->task, taskData[i].name);
    }
    
    // Sort by priority
    xrtArraySort(tasks, ComparePriority);
    
    printf("Task queue (by priority):\n");
    for (uint32 i = 1; i <= tasks->Count; i++) {
        Task* t = (Task*)xrtArrayGet(tasks, i);
        printf("  [Priority %d] %s\n", t->priority, t->task);
    }
    
    xrtArrayDestroy(tasks);
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Pre-allocate Memory

```c
// ✓ Recommended: Pre-allocate when count is known
xrtArrayAlloc(arr, 1000);
for (int i = 0; i < 1000; i++) {
    xrtArrayAppend(arr, 1);
}
```

### 2. Properly Release Dynamic Memory

```c
// ✓ Correct: Release element's dynamic memory before destroying array
for (uint32 i = 1; i <= arr->Count; i++) {
    DynamicItem* it = (DynamicItem*)xrtArrayGet(arr, i);
    xrtFree(it->name);
    xrtFree(it->data);
}
xrtArrayDestroy(arr);
```

### 3. High-Performance Traversal

```c
// ✓ High-performance: Use inline version
uint32 count = arr->Count;  // Cache Count
for (uint32 i = 1; i <= count; i++) {
    Num* n = (Num*)xrtArrayGet_Inline(arr, i);
    sum += n->value;
}
```

---

## Difference from PtrArray

| Feature | Array | PtrArray |
|---------|-------|----------|
| **Storage** | Contiguous memory stores struct itself | Stores pointers |
| **Element Size** | Any fixed size | Fixed `sizeof(ptr)` |
| **Memory Layout** | Cache-friendly | Indirect pointer access |
| **Use Case** | Large number of small structs | Large objects or shared references |
| **Sort Performance** | Moves entire struct | Only swaps pointers |

---

## Related Documentation

- [PtrArray Pointer Array](api-ptrarray.en.md)
- [BSMM Block Structure Memory Management](api-bsmm.en.md)
- [List](api-list.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#array-struct-array-library)

</div>
