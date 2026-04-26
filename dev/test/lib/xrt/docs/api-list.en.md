# List Library

> AVL tree-based integer key list with support for arbitrary int64 indices

[English](api-list.en.md) | [中文](api-list.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Callback Function Types](#callback-function-types)
- [List Operations](#list-operations)
- [Element Operations](#element-operations)
- [Traversal](#traversal)
- [Usage Scenarios](#usage-scenarios)
- [Comparison with Other Data Structures](#comparison-with-other-data-structures)
- [Best Practices](#best-practices)

---

## Overview

List is an AVL tree-based integer key list that uses `int64` as keys, supporting arbitrary integer indices (including negative numbers and sparse indices).

### Core Features

- **AVL Tree Implementation**: Guarantees O(log n) lookup, insertion, and deletion performance
- **int64 Keys**: Supports arbitrary integer indices (positive/negative/sparse)
- **Sparse Storage**: Only stores actually used indices
- **Auto Sorting**: Traversal follows key value ascending order

### Architecture Diagram

```
List Structure:
┌────────────────────────────────────────┐
│              xlist_struct              │
├────────────────────────────────────────┤
│ AVLT: xavltree_struct                  │
│   ┌─────────────────────────────────┐  │
│   │ RootNode → AVL Tree             │  │
│   │ Node: [int64 key][user data]    │  │
│   └─────────────────────────────────┘  │
└────────────────────────────────────────┘
```

### Differences from Regular Arrays

| Feature | List | Regular Array |
|---------|------|---------------|
| **Index Type** | int64 (any integer) | uint32 (continuous) |
| **Negative Index** | ✅ Supported | ❌ Not Supported |
| **Sparse Index** | ✅ Efficient | ❌ Wastes Memory |
| **Lookup Performance** | O(log n) | O(1) |
| **Memory Usage** | Only stores used indices | Pre-allocates continuous space |

---

## Data Structures

### xlist_struct

List structure.

**Definition:**
```c
typedef struct {
    xavltree_struct AVLT;    // Internal AVL tree
} xlist_struct, *xlist;
```

**Notes:**
- Internally uses AVL tree to store key-value pairs
- Each node contains an `int64` key and user data
- Automatically manages node memory (using FSMemPool)

---

## Callback Function Types

### List_EachProc

List traversal callback function.

**Definition:**
```c
typedef bool (*List_EachProc)(int64 pKey, ptr pVal, ptr pArg);
```

**Parameters:**
- `pKey` - Current element's key (int64)
- `pVal` - Current element's value pointer
- `pArg` - User-defined parameter

**Return Value:**
- `TRUE` - Continue traversal
- `FALSE` - Interrupt traversal

**Example:**
```c
bool PrintItem(int64 key, ptr pVal, ptr pArg) {
    int* val = (int*)pVal;
    printf("Key: %" PRId64 ", Value: %d\n", key, *val);
    return TRUE;  // Continue traversal
}
```

---

## List Operations

### xrtListCreate

Create a list.

**Function Prototype:**
```c
XXAPI xlist xrtListCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Value size (bytes)

**Return Value:**
- Success: List object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtListDestroy` to free

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create a list storing int
    xlist list = xrtListCreate(sizeof(int));
    
    // Set element
    bool isNew;
    int* val = (int*)xrtListSet(list, 0, &isNew);
    *val = 100;
    
    printf("Value: %d\n", *(int*)xrtListGet(list, 0));  // 100
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListDestroy

Free a list.

**Function Prototype:**
```c
XXAPI void xrtListDestroy(xlist objList);
```

**Parameters:**
- `objList` - List object

---

### xrtListInit

Initialize a list (for stack or embedded structures).

**Function Prototype:**
```c
XXAPI void xrtListInit(xlist objList, uint32 iItemLength);
```

**Parameters:**
- `objList` - Pre-allocated list structure pointer
- `iItemLength` - Value size (bytes)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Stack allocation
    xlist_struct listData;
    xlist list = &listData;
    xrtListInit(list, sizeof(double));
    
    // Use the list
    bool isNew;
    double* val = (double*)xrtListSet(list, 1, &isNew);
    *val = 3.14159;
    
    printf("Value: %f\n", *(double*)xrtListGet(list, 1));
    
    // Must call Unit to free internal resources
    xrtListUnit(list);
    
    xrtUnit();
    return 0;
}
```

---

### xrtListUnit

Free list internal resources (does not free the structure itself).

**Function Prototype:**
```c
XXAPI void xrtListUnit(xlist objList);
```

**Aliases:**
```c
#define xrtListRemoveAll xrtListUnit
#define xrtListClear     xrtListUnit
```

---

## Element Operations

### xrtListSet

Set element (returns value pointer).

**Function Prototype:**
```c
XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet);
```

**Parameters:**
- `objList` - List object
- `iKey` - Key (int64 type, can be negative)
- `bNewRet` - Output: whether it's a new key (can be NULL)

**Return Value:**
- Success: Pointer to value storage location
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    // Insert new key
    bool isNew;
    int* val1 = (int*)xrtListSet(list, 0, &isNew);
    *val1 = 100;
    printf("Key 0: isNew=%d, value=%d\n", isNew, *val1);  // isNew=1, value=100
    
    // Update existing key
    int* val2 = (int*)xrtListSet(list, 0, &isNew);
    *val2 = 200;
    printf("Key 0: isNew=%d, value=%d\n", isNew, *val2);  // isNew=0, value=200
    
    // Support negative index
    int* val3 = (int*)xrtListSet(list, -5, &isNew);
    *val3 = 500;
    printf("Key -5: value=%d\n", *val3);  // value=500
    
    // Support sparse index
    int* val4 = (int*)xrtListSet(list, 1000, &isNew);
    *val4 = 1000;
    printf("Key 1000: value=%d\n", *val4);  // value=1000
    
    printf("Element count: %u\n", xrtListCount(list));  // 4
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListSetPtr

Set element (directly set pointer value).

**Function Prototype:**
```c
XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal);
```

**Parameters:**
- `objList` - List object
- `iKey` - Key
- `pVal` - Pointer value to set
- `ppOldVal` - Output: old value pointer (can be NULL)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(ptr));
    
    // Set pointer value
    ptr oldVal;
    xrtListSetPtr(list, 1, (ptr)"Hello", &oldVal);
    printf("Old value: %s\n", oldVal ? (char*)oldVal : "(null)");  // (null)
    
    // Update pointer value
    xrtListSetPtr(list, 1, (ptr)"World", &oldVal);
    printf("Old value: %s\n", (char*)oldVal);  // Hello
    
    // Get current value
    printf("Current value: %s\n", (char*)xrtListGetPtr(list, 1));  // World
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListGet

Get element (returns value pointer).

**Function Prototype:**
```c
XXAPI ptr xrtListGet(xlist objList, int64 iKey);
```

**Parameters:**
- `objList` - List object
- `iKey` - Key

**Return Value:**
- Exists: Value pointer
- Not exists: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    // Set elements
    bool isNew;
    *(int*)xrtListSet(list, 10, &isNew) = 100;
    *(int*)xrtListSet(list, 20, &isNew) = 200;
    
    // Get existing key
    int* val1 = (int*)xrtListGet(list, 10);
    if (val1) {
        printf("Key 10: %d\n", *val1);  // 100
    }
    
    // Get non-existing key
    int* val2 = (int*)xrtListGet(list, 15);
    if (val2 == NULL) {
        printf("Key 15 does not exist\n");
    }
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListGetPtr

Get element (directly returns stored pointer value).

**Function Prototype:**
```c
XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey);
```

**Return Value:**
- Exists: Stored pointer value
- Not exists: `NULL`

---

### xrtListRemove

Remove element.

**Function Prototype:**
```c
XXAPI bool xrtListRemove(xlist objList, int64 iKey);
```

**Return Value:**
- `TRUE` - Deletion successful
- `FALSE` - Key does not exist

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    // Add elements
    bool isNew;
    *(int*)xrtListSet(list, 1, &isNew) = 10;
    *(int*)xrtListSet(list, 2, &isNew) = 20;
    *(int*)xrtListSet(list, 3, &isNew) = 30;
    
    printf("Before deletion: %u elements\n", xrtListCount(list));  // 3
    
    // Delete existing key
    bool result1 = xrtListRemove(list, 2);
    printf("Delete key 2: %s\n", result1 ? "Success" : "Failed");  // Success
    
    // Delete non-existing key
    bool result2 = xrtListRemove(list, 100);
    printf("Delete key 100: %s\n", result2 ? "Success" : "Failed");  // Failed
    
    printf("After deletion: %u elements\n", xrtListCount(list));  // 2
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListRemovePtr

Remove element and return stored pointer value.

**Function Prototype:**
```c
XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey);
```

**Return Value:**
- Success: Removed pointer value
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(ptr));
    
    // Set pointer value
    str text = xrtCopyStr((str)"Hello World", 0);
    xrtListSetPtr(list, 1, text, NULL);
    
    // Remove and get pointer
    ptr removed = xrtListRemovePtr(list, 1);
    if (removed) {
        printf("Removed value: %s\n", (char*)removed);
        xrtFree(removed);  // Manual free
    }
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListExists

Check if key exists.

**Function Prototype:**
```c
XXAPI bool xrtListExists(xlist objList, int64 iKey);
```

**Return Value:**
- `TRUE` - Key exists
- `FALSE` - Key does not exist

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 5, &isNew) = 50;
    
    printf("Key 5 exists: %s\n", xrtListExists(list, 5) ? "Yes" : "No");   // Yes
    printf("Key 10 exists: %s\n", xrtListExists(list, 10) ? "Yes" : "No"); // No
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## Traversal

### xrtListCount

Get element count.

**Function Prototype:**
```c
XXAPI uint32 xrtListCount(xlist objList);
```

**Return Value:**
- Number of elements in the list

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, -10, &isNew) = 1;
    *(int*)xrtListSet(list, 0, &isNew) = 2;
    *(int*)xrtListSet(list, 100, &isNew) = 3;
    
    printf("Element count: %u\n", xrtListCount(list));  // 3
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListWalk

Traverse all elements in the list (in key ascending order).

**Function Prototype:**
```c
XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg);
```

**Parameters:**
- `objList` - List object
- `procEach` - Traversal callback function
- `pArg` - Custom parameter passed to callback

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

bool PrintItem(int64 key, ptr pVal, ptr pArg) {
    int* sum = (int*)pArg;
    int val = *(int*)pVal;
    printf("Key: %" PRId64 ", Value: %d\n", key, val);
    *sum += val;
    return TRUE;  // Continue traversal
}

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 10, &isNew) = 100;
    *(int*)xrtListSet(list, -5, &isNew) = 50;
    *(int*)xrtListSet(list, 20, &isNew) = 200;
    *(int*)xrtListSet(list, 0, &isNew) = 75;
    
    int sum = 0;
    printf("Traverse in ascending key order:\n");
    xrtListWalk(list, PrintItem, &sum);
    // Output order: -5, 0, 10, 20
    
    printf("Sum: %d\n", sum);  // 425
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Traversal order is ascending by key (in-order traversal of AVL tree)
- Returning `FALSE` from callback interrupts traversal

---

## Usage Scenarios

### 1. Sparse Array

```c
#include "xrt.h"
#include <stdio.h>

// Sparse array: only some indices have values
int main() {
    xrtInit();
    
    xlist sparse = xrtListCreate(sizeof(double));
    
    // Set scattered indices
    bool isNew;
    *(double*)xrtListSet(sparse, 0, &isNew) = 1.0;
    *(double*)xrtListSet(sparse, 1000000, &isNew) = 2.0;
    *(double*)xrtListSet(sparse, -500000, &isNew) = 3.0;
    
    // Only uses 3 nodes memory, not 1500001
    printf("Element count: %u\n", xrtListCount(sparse));  // 3
    
    // Get value
    double* val = (double*)xrtListGet(sparse, 1000000);
    if (val) {
        printf("Index 1000000: %f\n", *val);  // 2.0
    }
    
    xrtListDestroy(sparse);
    xrtUnit();
    return 0;
}
```

---

### 2. Dynamic Object Collection

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str name;
    int age;
} Person;

Person* CreatePerson(str name, int age) {
    Person* p = xrtMalloc(sizeof(Person));
    p->name = xrtCopyStr(name, 0);
    p->age = age;
    return p;
}

void FreePerson(Person* p) {
    if (p) {
        xrtFree(p->name);
        xrtFree(p);
    }
}

bool PrintPerson(int64 key, ptr pVal, ptr pArg) {
    Person* p = (Person*)xrtListGetPtr((xlist)pArg, key);
    printf("ID %" PRId64 ": %s, %d\n", key, p->name, p->age);
    return TRUE;
}

int main() {
    xrtInit();
    
    xlist people = xrtListCreate(sizeof(ptr));
    
    // Use ID as key
    xrtListSetPtr(people, 1001, CreatePerson((str)"Alice", 25), NULL);
    xrtListSetPtr(people, 1002, CreatePerson((str)"Bob", 30), NULL);
    xrtListSetPtr(people, 1003, CreatePerson((str)"Charlie", 35), NULL);
    
    // Traverse
    printf("All people:\n");
    xrtListWalk(people, PrintPerson, people);
    
    // Cleanup: remove and free each object
    ptr removed;
    removed = xrtListRemovePtr(people, 1001);
    FreePerson((Person*)removed);
    removed = xrtListRemovePtr(people, 1002);
    FreePerson((Person*)removed);
    removed = xrtListRemovePtr(people, 1003);
    FreePerson((Person*)removed);
    
    xrtListDestroy(people);
    xrtUnit();
    return 0;
}
```

---

### 3. Bidirectional Mapping (Key ↔ Value)

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // ID → Name
    xlist idToName = xrtListCreate(sizeof(ptr));
    // Name → ID (using Dict)
    xdict nameToId = xrtDictCreate(sizeof(int64));
    
    // Add mapping
    int64 id = 1001;
    str name = (str)"Alice";
    
    xrtListSetPtr(idToName, id, name, NULL);
    bool isNew;
    *(int64*)xrtDictSet(nameToId, name, strlen((char*)name), &isNew) = id;
    
    // Forward lookup: ID → Name
    printf("Name of ID 1001: %s\n", (char*)xrtListGetPtr(idToName, 1001));
    
    // Reverse lookup: Name → ID
    int64* foundId = (int64*)xrtDictGet(nameToId, (ptr)"Alice", 5);
    if (foundId) {
        printf("ID of Alice: %" PRId64 "\n", *foundId);
    }
    
    xrtListDestroy(idToName);
    xrtDictDestroy(nameToId);
    xrtUnit();
    return 0;
}
```

---

## Comparison with Other Data Structures

| Feature | List | Array | PtrArray | Dict |
|---------|------|-------|----------|------|
| **Key Type** | int64 | uint32 (1-based) | uint32 (1-based) | Any binary |
| **Negative Index** | ✅ | ❌ | ❌ | - |
| **Sparse Index** | ✅ | ❌ | ❌ | ✅ |
| **Sorted Traversal** | ✅ Ascending by key | ✅ By index | ✅ By index | ❌ |
| **Lookup Performance** | O(log n) | O(1) | O(1) | O(log n) |
| **Memory Efficiency** | Only stores used keys | Continuous memory | Continuous memory | Only stores used keys |
| **Use Case** | Integer key sparse storage | Continuous index storage | Pointer collection | String key storage |

### Selection Guide

- **Use List**: When you need integer keys, negative indices, sparse indices
- **Use Array**: Continuous indices, high-frequency random access
- **Use Dict**: String or complex keys

---

## Best Practices

### 1. Check If Key Exists

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 10, &isNew) = 100;
    
    // ✗ Not recommended: Use Get to check existence
    if (xrtListGet(list, 10) != NULL) {
        // ...
    }
    
    // ✓ Recommended: Use Exists
    if (xrtListExists(list, 10)) {
        printf("Key 10 exists\n");
    }
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 2. Pointer Value Memory Management

```c
#include "xrt.h"
#include <stdio.h>

bool FreeStrings(int64 key, ptr pVal, ptr pArg) {
    xlist list = (xlist)pArg;
    ptr strVal = xrtListGetPtr(list, key);
    if (strVal) {
        xrtFree(strVal);
    }
    return TRUE;
}

int main() {
    xrtInit();
    
    xlist strings = xrtListCreate(sizeof(ptr));
    
    // Store dynamically allocated strings
    xrtListSetPtr(strings, 1, xrtCopyStr((str)"Hello", 0), NULL);
    xrtListSetPtr(strings, 2, xrtCopyStr((str)"World", 0), NULL);
    
    // Free all strings before cleanup
    xrtListWalk(strings, FreeStrings, strings);
    
    xrtListDestroy(strings);
    xrtUnit();
    return 0;
}
```

---

### 3. Safe Interrupt Traversal

```c
#include "xrt.h"
#include <stdio.h>

bool FindFirst(int64 key, ptr pVal, ptr pArg) {
    int target = *(int*)pArg;
    int current = *(int*)pVal;
    
    if (current == target) {
        printf("Found target %d at key %" PRId64 "\n", target, key);
        return FALSE;  // Interrupt traversal
    }
    return TRUE;  // Continue traversal
}

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 1, &isNew) = 10;
    *(int*)xrtListSet(list, 2, &isNew) = 20;
    *(int*)xrtListSet(list, 3, &isNew) = 30;
    *(int*)xrtListSet(list, 4, &isNew) = 20;
    
    // Find first element with value 20
    int target = 20;
    xrtListWalk(list, FindFirst, &target);
    // Output: Found target 20 at key 2
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Array Struct Array](api-array.en.md)
- [PtrArray Pointer Array](api-ptrarray.en.md)
- [Dict Dictionary](api-dict.en.md)
- [AVLTree Balanced Binary Tree](api-avltree.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#list-library)

</div>
