# Dict Dictionary Library

> AVL tree-based key-value storage supporting arbitrary binary keys

[English](api-dict.en.md) | [中文](api-dict.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Overview](#overview)
- [Data Structures](#data-structures)
- [Callback Function Types](#callback-function-types)
- [Dictionary Operations](#dictionary-operations)
- [Key-Value Operations](#key-value-operations)
- [Traversal](#traversal)
- [Use Cases](#use-cases)
- [Performance Characteristics](#performance-characteristics)
- [Best Practices](#best-practices)

---

## Overview

Dict is a dictionary (key-value storage) implemented based on AVL tree, supporting arbitrary binary data as keys.

### Core Features

- **AVL tree implementation**: Guarantees O(log n) lookup, insert, delete performance
- **Arbitrary binary keys**: Keys can be strings, numbers, structs, or any data
- **Flexible value storage**: Supports storing values or directly storing pointers
- **Memory pool support**: Can work with MemPool to manage key memory

### Architecture

```
Dict Structure:
┌─────────────────────────────────────────┐
│                xdict                    │
├─────────────────────────────────────────┤
│  AVLT (xavltree_struct)                 │  ← AVL tree manages nodes
│  MP (xmempool)                          │  ← Optional memory pool
└─────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│             AVL Tree Node               │
├─────────────────────────────────────────┤
│  Dict_Key: Key + KeyLen + Hash          │  ← Key info
│  UserData[iItemLength]                  │  ← User data
└─────────────────────────────────────────┘
```

---

## Data Structures

### Dict_Key

Dictionary key data structure.

**Definition:**
```c
typedef struct {
    ptr Key;          // Key data pointer
    uint32 KeyLen;    // Key length
    uint32 Hash;      // Key hash value (for fast comparison)
} Dict_Key;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `Key` | Pointer to key data (automatically copied and managed by dictionary) |
| `KeyLen` | Key byte length |
| `Hash` | Key hash value (Hash64 on 64-bit platforms, Hash32 on 32-bit) |

---

### xdict_struct

Dictionary object data structure.

**Definition:**
```c
typedef struct {
    xavltree_struct AVLT;  // Internal AVL tree
    xmempool MP;           // Optional memory pool
} xdict_struct, *xdict;
```

**Member Description:**

| Member | Description |
|--------|-------------|
| `AVLT` | Internal AVL tree, manages all key-value pairs |
| `MP` | Optional memory pool for allocating key memory (default NULL, uses xrtMalloc) |

---

## Callback Function Types

### Dict_EachProc

Dictionary traversal callback function.

**Definition:**
```c
typedef bool (*Dict_EachProc)(Dict_Key* pKey, ptr pVal, ptr pArg);
```

**Parameters:**
- `pKey` - Key information (includes Key, KeyLen, Hash)
- `pVal` - Value pointer
- `pArg` - User-defined parameter

**Return Value:**
- `TRUE` - Continue traversal
- `FALSE` - Stop traversal

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

bool PrintItem(Dict_Key* pKey, ptr pVal, ptr pArg) {
    printf("%.*s = %d\n", pKey->KeyLen, (char*)pKey->Key, *(int*)pVal);
    return TRUE;  // Continue traversal
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)"age", 3, &isNew);
    *val = 25;
    
    xrtDictWalk(dict, PrintItem, NULL);
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## Dictionary Operations

### xrtDictCreate

Create a dictionary.

**Function Prototype:**
```c
XXAPI xdict xrtDictCreate(uint32 iItemLength);
```

**Parameters:**
- `iItemLength` - Value size (bytes). 0 means store pointers (use `SetPtr`/`GetPtr`)

**Return Value:**
- Success: Dictionary object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtDictDestroy` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Store int values
    xdict intDict = xrtDictCreate(sizeof(int));
    bool isNew;
    int* val = (int*)xrtDictSet(intDict, (ptr)"count", 5, &isNew);
    if (isNew) {
        *val = 100;
    }
    printf("count = %d\n", *(int*)xrtDictGet(intDict, (ptr)"count", 5));
    xrtDictDestroy(intDict);
    
    // Store pointers
    xdict ptrDict = xrtDictCreate(0);
    str text = xrtCopyStr((str)"Hello World", 0);
    xrtDictSetPtr(ptrDict, (ptr)"greeting", 8, text, NULL);
    printf("greeting = %s\n", (char*)xrtDictGetPtr(ptrDict, (ptr)"greeting", 8));
    xrtFree(text);
    xrtDictDestroy(ptrDict);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDictDestroy

Destroy dictionary and release all memory.

**Function Prototype:**
```c
XXAPI void xrtDictDestroy(xdict objHT);
```

**Parameters:**
- `objHT` - Dictionary object

**Notes:**
- Automatically releases all key memory
- Automatically calls `xrtDictUnit` to clean internal structure
- If values are pointers, they need to be manually freed before destruction

---

### xrtDictInit

Initialize dictionary (for stack or embedded structures).

**Function Prototype:**
```c
XXAPI void xrtDictInit(xdict objHT, uint32 iItemLength);
```

**Parameters:**
- `objHT` - Dictionary object pointer
- `iItemLength` - Value size

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create dictionary on stack
    xdict_struct dictObj;
    xrtDictInit(&dictObj, sizeof(int));
    
    bool isNew;
    int* val = (int*)xrtDictSet(&dictObj, (ptr)"value", 5, &isNew);
    *val = 42;
    
    printf("value = %d\n", *(int*)xrtDictGet(&dictObj, (ptr)"value", 5));
    
    xrtDictUnit(&dictObj);  // Only clean internal structure, not dictObj
    
    xrtUnit();
    return 0;
}
```

---

### xrtDictUnit

Release dictionary internal structure (not the dictionary object itself).

**Function Prototype:**
```c
XXAPI void xrtDictUnit(xdict objHT);
```

**Macro Aliases:**
```c
#define xrtDictRemoveAll  xrtDictUnit
#define xrtDictClear      xrtDictUnit
```

---

## Key-Value Operations

### xrtDictSet

Set key-value pair (returns value pointer).

**Function Prototype:**
```c
XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet);
```

**Parameters:**
- `objHT` - Dictionary object
- `sKey` - Key data pointer
- `iKeyLen` - Key length (bytes)
- `bNewRet` - Output: whether it's a new key (can be NULL)

**Return Value:**
- Success: Pointer to value storage location
- Failure: `NULL`

**Notes:**
- If key exists, returns existing value pointer, `*bNewRet = FALSE`
- If key doesn't exist, inserts new node, `*bNewRet = TRUE`, value needs manual assignment

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // Insert new key
    bool isNew;
    str key = (str)"age";
    int* val = (int*)xrtDictSet(dict, key, strlen((char*)key), &isNew);
    if (isNew) {
        *val = 25;
        printf("Newly inserted: age = %d\n", *val);
    }
    
    // Update existing key
    val = (int*)xrtDictSet(dict, key, strlen((char*)key), &isNew);
    if (!isNew) {
        *val = 30;  // Update value
        printf("Updated: age = %d\n", *val);
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictSetPtr

Set key-value pair (store pointer directly).

**Function Prototype:**
```c
XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal);
```

**Parameters:**
- `objHT` - Dictionary object
- `sKey` - Key data pointer
- `iKeyLen` - Key length
- `pVal` - Pointer value to store
- `ppOldVal` - Output: old pointer value (can be NULL)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(0);  // Store pointers
    
    // Insert new value
    str key = (str)"name";
    str value1 = xrtCopyStr((str)"Alice", 0);
    ptr oldVal = NULL;
    xrtDictSetPtr(dict, key, strlen((char*)key), value1, &oldVal);
    printf("Old value: %s\n", oldVal ? (char*)oldVal : "(null)");
    
    // Update value, get old value
    str value2 = xrtCopyStr((str)"Bob", 0);
    xrtDictSetPtr(dict, key, strlen((char*)key), value2, &oldVal);
    printf("Old value: %s\n", oldVal ? (char*)oldVal : "(null)");  // "Alice"
    xrtFree(oldVal);  // Free old value
    
    xrtFree(value2);
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictSetWithKey

Set value using pre-computed key info (inline function, high performance).

**Function Prototype:**
```c
static inline ptr xrtDictSetWithKey(xdict objHT, Dict_Key* objKey, bool* bNewRet);
```

**Parameters:**
- `objHT` - Dictionary object
- `objKey` - Pre-computed key info
- `bNewRet` - Output: whether it's a new key

**Notes:**
- For scenarios requiring multiple uses of the same key
- Avoids repeated hash calculation

---

### xrtDictGet

Get value pointer.

**Function Prototype:**
```c
XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**Parameters:**
- `objHT` - Dictionary object
- `sKey` - Key data pointer
- `iKeyLen` - Key length

**Return Value:**
- Success: Pointer to value storage location
- Failure: `NULL` (key doesn't exist)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // Insert data
    str key = (str)"score";
    bool isNew;
    int* val = (int*)xrtDictSet(dict, key, strlen((char*)key), &isNew);
    *val = 95;
    
    // Get data
    int* result = (int*)xrtDictGet(dict, key, strlen((char*)key));
    if (result) {
        printf("score = %d\n", *result);  // 95
    } else {
        printf("Key doesn't exist\n");
    }
    
    // Find non-existent key
    int* notFound = (int*)xrtDictGet(dict, (ptr)"unknown", 7);
    printf("unknown: %s\n", notFound ? "exists" : "doesn't exist");
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictGetPtr

Get stored pointer value.

**Function Prototype:**
```c
XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**Return Value:**
- Success: Stored pointer value
- Failure: `NULL`

**Notes:**
- Difference from `xrtDictGet`: `GetPtr` returns the pointer stored in dictionary, while `Get` returns pointer to storage location

---

### xrtDictGetWithKey

Get value using pre-computed key info (inline function, high performance).

**Function Prototype:**
```c
static inline ptr xrtDictGetWithKey(xdict objHT, Dict_Key* objKey);
```

---

### xrtDictRemove

Remove key-value pair.

**Function Prototype:**
```c
XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**Return Value:**
- `TRUE` - Successfully removed
- `FALSE` - Key doesn't exist

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // Insert data
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)"item", 4, &isNew);
    *val = 100;
    
    printf("Count before removal: %u\n", xrtDictCount(dict));  // 1
    
    // Remove
    if (xrtDictRemove(dict, (ptr)"item", 4)) {
        printf("Removal successful\n");
    }
    
    printf("Count after removal: %u\n", xrtDictCount(dict));  // 0
    
    // Remove non-existent key
    if (!xrtDictRemove(dict, (ptr)"notexist", 8)) {
        printf("Key doesn't exist\n");
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictRemovePtr

Remove key-value pair and return deleted pointer value.

**Function Prototype:**
```c
XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**Return Value:**
- Success: Deleted pointer value
- Failure: `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(0);
    
    // Insert pointer with memory
    str value = xrtCopyStr((str)"need to free", 0);
    xrtDictSetPtr(dict, (ptr)"data", 4, value, NULL);
    
    // Remove and get pointer, then free
    ptr removed = xrtDictRemovePtr(dict, (ptr)"data", 4);
    if (removed) {
        printf("Removed: %s\n", (char*)removed);
        xrtFree(removed);  // Free memory
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictExists

Check if key exists.

**Function Prototype:**
```c
XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**Return Value:**
- `TRUE` - Key exists
- `FALSE` - Key doesn't exist

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)"name", 4, &isNew);
    *val = 1;
    
    // Check exists
    if (xrtDictExists(dict, (ptr)"name", 4)) {
        printf("name exists\n");
    }
    
    // Check doesn't exist
    if (!xrtDictExists(dict, (ptr)"unknown", 7)) {
        printf("unknown doesn't exist\n");
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## Traversal

### xrtDictCount

Get key-value pair count.

**Function Prototype:**
```c
XXAPI uint32 xrtDictCount(xdict objHT);
```

**Return Value:**
- Number of key-value pairs in dictionary

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtDictSet(dict, (ptr)"a", 1, &isNew) = 1;
    *(int*)xrtDictSet(dict, (ptr)"b", 1, &isNew) = 2;
    *(int*)xrtDictSet(dict, (ptr)"c", 1, &isNew) = 3;
    
    printf("Dictionary size: %u\n", xrtDictCount(dict));  // 3
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictWalk

Traverse all key-value pairs in dictionary.

**Function Prototype:**
```c
XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg);
```

**Parameters:**
- `objHT` - Dictionary object
- `procEach` - Traversal callback function
- `pArg` - Custom parameter passed to callback

**Notes:**
- Uses in-order traversal (sorted by key hash value)
- Callback returning `FALSE` stops traversal

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

bool PrintEntry(Dict_Key* pKey, ptr pVal, ptr pArg) {
    int* count = (int*)pArg;
    (*count)++;
    printf("%d: %.*s = %d\n", *count, pKey->KeyLen, (char*)pKey->Key, *(int*)pVal);
    return TRUE;  // Continue traversal
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // Insert data
    bool isNew;
    *(int*)xrtDictSet(dict, (ptr)"apple", 5, &isNew) = 10;
    *(int*)xrtDictSet(dict, (ptr)"banana", 6, &isNew) = 20;
    *(int*)xrtDictSet(dict, (ptr)"cherry", 6, &isNew) = 30;
    
    // Traverse and print
    int count = 0;
    printf("Dictionary contents:\n");
    xrtDictWalk(dict, PrintEntry, &count);
    printf("Total %d items\n", count);
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## Use Cases

### 1. Configuration Management

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

xdict config = NULL;

void InitConfig() {
    config = xrtDictCreate(0);  // Store pointers
}

void SetConfig(str key, str value) {
    ptr oldVal = NULL;
    str valueCopy = xrtCopyStr(value, 0);
    xrtDictSetPtr(config, key, strlen((char*)key), valueCopy, &oldVal);
    if (oldVal) {
        xrtFree(oldVal);  // Free old value
    }
}

str GetConfig(str key) {
    return (str)xrtDictGetPtr(config, key, strlen((char*)key));
}

bool FreeConfigItem(Dict_Key* pKey, ptr pVal, ptr pArg) {
    xrtFree(pVal);
    return TRUE;
}

void CleanupConfig() {
    xrtDictWalk(config, FreeConfigItem, NULL);
    xrtDictDestroy(config);
}

int main() {
    xrtInit();
    
    InitConfig();
    
    SetConfig((str)"database", (str)"localhost:3306");
    SetConfig((str)"username", (str)"admin");
    SetConfig((str)"password", (str)"secret");
    
    printf("database: %s\n", GetConfig((str)"database"));
    printf("username: %s\n", GetConfig((str)"username"));
    
    CleanupConfig();
    xrtUnit();
    return 0;
}
```

---

### 2. Object Cache

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[64];
    int age;
} User;

xdict userCache = NULL;

void InitCache() {
    userCache = xrtDictCreate(sizeof(User));
}

User* GetOrCreateUser(int id) {
    char keyBuf[16];
    int keyLen = sprintf(keyBuf, "user_%d", id);
    
    bool isNew;
    User* user = (User*)xrtDictSet(userCache, (ptr)keyBuf, keyLen, &isNew);
    
    if (isNew) {
        // Initialize new user
        user->id = id;
        sprintf(user->name, "User%d", id);
        user->age = 20 + id;
        printf("Creating new user: %d\n", id);
    } else {
        printf("Getting from cache: %d\n", id);
    }
    
    return user;
}

int main() {
    xrtInit();
    
    InitCache();
    
    // First access, create new user
    User* u1 = GetOrCreateUser(1);
    printf("  name: %s, age: %d\n", u1->name, u1->age);
    
    // Second access, get from cache
    User* u1_again = GetOrCreateUser(1);
    printf("  name: %s, age: %d\n", u1_again->name, u1_again->age);
    
    // Different user
    User* u2 = GetOrCreateUser(2);
    printf("  name: %s, age: %d\n", u2->name, u2->age);
    
    printf("Cache size: %u\n", xrtDictCount(userCache));
    
    xrtDictDestroy(userCache);
    xrtUnit();
    return 0;
}
```

---

### 3. Binary Keys

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x;
    int y;
} Point;

int main() {
    xrtInit();
    
    // Use struct as key
    xdict pointMap = xrtDictCreate(sizeof(int));
    
    Point p1 = {10, 20};
    Point p2 = {30, 40};
    Point p3 = {10, 20};  // Same as p1
    
    bool isNew;
    *(int*)xrtDictSet(pointMap, &p1, sizeof(Point), &isNew) = 100;
    *(int*)xrtDictSet(pointMap, &p2, sizeof(Point), &isNew) = 200;
    
    // Lookup
    int* val1 = (int*)xrtDictGet(pointMap, &p1, sizeof(Point));
    int* val3 = (int*)xrtDictGet(pointMap, &p3, sizeof(Point));  // Same as p1
    
    printf("p1: %d\n", val1 ? *val1 : -1);  // 100
    printf("p3: %d\n", val3 ? *val3 : -1);  // 100 (same as p1)
    
    xrtDictDestroy(pointMap);
    xrtUnit();
    return 0;
}
```

---

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Description |
|-----------|------------|-------------|
| Insert | O(log n) | AVL tree balanced insert |
| Lookup | O(log n) | AVL tree search |
| Delete | O(log n) | AVL tree balanced delete |
| Traverse | O(n) | In-order traverse all nodes |

### Space Complexity

- O(n), where n is the number of key-value pairs
- Per-node overhead: `sizeof(xavltnode_struct) + sizeof(Dict_Key) + iItemLength`

### Comparison with Hash Table

| Feature | Dict (AVL Tree) | Hash Table |
|---------|-----------------|------------|
| Lookup Complexity | O(log n) | O(1) average |
| Ordered Traversal | ✅ Supported | ✖ Not supported |
| Worst Case | O(log n) | O(n) |
| Memory Overhead | Lower | Higher (needs bucket array) |

---

## Best Practices

### 1. Key Length Calculation

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // ✅ Correct: Use strlen to calculate string key length
    str key1 = (str)"hello";
    bool isNew;
    int* val = (int*)xrtDictSet(dict, key1, strlen((char*)key1), &isNew);
    *val = 1;
    
    // ✗ Wrong: Don't include terminator
    // xrtDictSet(dict, "hello", 6, &isNew);  // Includes \0, inconsistent
    
    // ✅ Binary keys: Use sizeof
    int intKey = 12345;
    val = (int*)xrtDictSet(dict, &intKey, sizeof(int), &isNew);
    *val = 2;
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### 2. Pointer Value Memory Management

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

bool FreeValue(Dict_Key* pKey, ptr pVal, ptr pArg) {
    xrtFree(pVal);  // Free stored pointer
    return TRUE;
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(0);  // Store pointers
    
    // Insert value with memory
    str data = xrtCopyStr((str)"allocated data", 0);
    xrtDictSetPtr(dict, (ptr)"key", 3, data, NULL);
    
    // Free all values before destruction
    xrtDictWalk(dict, FreeValue, NULL);
    xrtDictDestroy(dict);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Flattening Structures as Keys

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// ✗ Not recommended: Struct memory alignment may cause issues
typedef struct {
    char type;
    int id;
    // May have padding bytes
} BadKey;

// ✅ Recommended: Use packed struct or serialization
typedef struct {
    char type;
    int id;
} __attribute__((packed)) GoodKey;  // GCC

// Or serialize to string
char* MakeKey(char type, int id) {
    static char buf[32];
    sprintf(buf, "%c:%d", type, id);
    return buf;
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // Use serialized key
    char* key = MakeKey('A', 100);
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)key, strlen(key), &isNew);
    *val = 42;
    
    printf("Lookup: %d\n", *(int*)xrtDictGet(dict, (ptr)MakeKey('A', 100), strlen(key)));
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [AVLTree Balanced Binary Tree](api-avltree.en.md)
- [Hash Calculation](api-hash.en.md)
- [List](api-list.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#dict-dictionary-library)

</div>
