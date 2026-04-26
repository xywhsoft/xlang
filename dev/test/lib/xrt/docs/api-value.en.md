# Value Dynamic Type System Library

> 16 dynamic data types, automatic reference counting management, supports container nesting

[English](api-value.en.md) | [中文](api-value.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Type System](#type-system)
- [Reference Management](#reference-management)
- [Creating Values](#creating-values)
- [Reading Values](#reading-values)
- [Type Checking](#type-checking)
- [Array Operations](#array-operations)
- [List Operations](#list-operations)
- [Collection Operations](#collection-operations)
- [Table Operations](#table-operations)
- [Copy Operations](#copy-operations)
- [Debug Functions](#debug-functions)

---

## Type System

### Value Type Constants

```c
#define XVO_DT_EMPTY    0   // Non-existent data
#define XVO_DT_NULL     1   // null
#define XVO_DT_BOOL     2   // bool : true | false
#define XVO_DT_INT      3   // Integer (int64)
#define XVO_DT_FLOAT    4   // Floating point (double)
#define XVO_DT_TEXT     5   // String
#define XVO_DT_TIME     6   // Time
#define XVO_DT_POINT    7   // Pointer
#define XVO_DT_FUNC     8   // Function
#define XVO_DT_ARRAY    9   // Array
#define XVO_DT_LIST     10  // List
#define XVO_DT_COLL     11  // Collection
#define XVO_DT_TABLE    12  // Table
#define XVO_DT_STRUCT   13  // Struct
#define XVO_DT_OBJECT   14  // Object
#define XVO_DT_CUSTOM   15  // Custom
```

### Value Structure [16 bytes]

```c
typedef struct xvalue_struct {
    uint32 Type:4;          // Type (4 bits)
    uint32 Reserve:1;       // Reserved (1 bit)
    uint32 IsStatic:1;      // Is static (1 bit)
    uint32 RefCount:26;     // Reference count (26 bits, max 67108863)
    uint32 Size;            // Data size
    union {
        bool vBool;         // Boolean value
        int64 vInt;         // Integer value
        double vFloat;      // Floating point value
        str vText;          // String pointer
        xtime vTime;        // Time value
        ptr vPoint;         // Pointer
        xfunction vFunc;    // Function pointer
        xparray vArray;     // Array
        xlist vList;        // List
        xavltree vColl;     // Collection
        xdict vTable;       // Table
        ptr vStruct;        // Struct
        ptr vObject;        // Object
        ptr vCustom;        // Custom data
    };
} xvalue_struct, *xvalue;

// Function pointer type
typedef xvalue (*xfunction)(xvalue pENV, xvalue arrParam);
```

---

## Reference Management

### xvoAddRef

Increase reference count

**Prototype:**
```c
XXAPI void xvoAddRef(xvalue pVal);

// Inline version (better performance)
static inline void xvoAddRef_Inline(xvalue pVal);
```

**Description:**
- Increments reference count by 1
- When reference count reaches maximum (0x3FFFFFF), automatically converts to static value

---

### xvoUnref

Decrease reference count

**Prototype:**
```c
XXAPI void xvoUnref(xvalue pVal);
```

**Description:**
- Decrements reference count by 1
- When count reaches 0, automatically releases the value
- Container types recursively release all child elements
- Static values (IsStatic=1) are not released

**Example:**
```c
xvalue v = xvoCreateInt(100);
xvoAddRef(v);   // RefCount = 2
xvoUnref(v);    // RefCount = 1
xvoUnref(v);    // RefCount = 0, released
```

---

## Creating Values

### xvoCreateNull

Create null value (returns static singleton)

**Prototype:**
```c
XXAPI xvalue xvoCreateNull();
```

**Return Value:**
- Static Value of null type

**Description:** null/true/false use static singletons, no need to release

---

### xvoCreateBool

Create boolean value (returns static singleton)

**Prototype:**
```c
XXAPI xvalue xvoCreateBool(bool bVal);
```

**Description:** Returns static singleton for TRUE/FALSE

---

### xvoCreateInt

Create integer

**Prototype:**
```c
XXAPI xvalue xvoCreateInt(int64 iVal);
```

**Release:** 🔄 Use `xvoUnref` to release

---

### xvoCreateFloat

Create floating point number

**Prototype:**
```c
XXAPI xvalue xvoCreateFloat(double fVal);
```

---

### xvoCreateText

Create string

**Prototype:**
```c
XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `sVal` | String pointer |
| `iSize` | Length (0 for auto-calculate) |
| `bColloc` | Collocation mode |

**Collocation Mode Description:**
- `TRUE` - Directly manages string pointer, no copy, string not released on cleanup
- `FALSE` - Copies string content, copied string released on cleanup

**Example:**
```c
// Copy string (safe mode)
xvalue v1 = xvoCreateText("Hello", 0, FALSE);

// Collocate static string (efficient mode)
xvalue v2 = xvoCreateText("Static", 0, TRUE);
```

---

### xvoCreateTime

Create time value

**Prototype:**
```c
XXAPI xvalue xvoCreateTime(xtime tVal);
```

---

### xvoCreateTimeSerial

Create time value from date/time components

**Prototype:**
```c
XXAPI xvalue xvoCreateTimeSerial(
    int64 iYear,    // Year
    int iMonth,     // Month
    int iDay,       // Day
    int iHour,      // Hour
    int iMinute,    // Minute
    int iSecond     // Second
);
```

---

### xvoCreatePoint

Create pointer

**Prototype:**
```c
XXAPI xvalue xvoCreatePoint(ptr point);
```

---

### xvoCreateFunc

Create function reference

**Prototype:**
```c
XXAPI xvalue xvoCreateFunc(xfunction pFunc);
```

**Description:** Used to store function pointers, supports callbacks

---

### xvoCreateArray

Create empty array

**Prototype:**
```c
XXAPI xvalue xvoCreateArray();
```

**Description:** Based on `xparray` implementation, supports dynamic resizing

---

### xvoCreateList

Create empty list

**Prototype:**
```c
XXAPI xvalue xvoCreateList();
```

**Description:** Based on `xlist` implementation, key is int64 type

---

### xvoCreateColl

Create empty collection

**Prototype:**
```c
XXAPI xvalue xvoCreateColl();
```

**Description:** Based on AVL tree implementation, elements are automatically deduplicated and sorted

---

### xvoCreateTable

Create empty table (dictionary)

**Prototype:**
```c
XXAPI xvalue xvoCreateTable();
```

**Description:** Based on `xdict` implementation, string keys

---

### xvoCreateStruct

Create struct container

**Prototype:**
```c
XXAPI xvalue xvoCreateStruct(uint32 iSize);
```

**Parameters:**
- `iSize` - Struct size (bytes), cannot be 0

**Return Value:**
- Returns xvalue on success, NULL on failure

---

### xvoCreateObject

Create object container

**Prototype:**
```c
XXAPI xvalue xvoCreateObject(uint32 iSize);
```

---

### xvoCreateCustom

Create custom type

**Prototype:**
```c
XXAPI xvalue xvoCreateCustom(ptr pObj);
```

---

## Reading Values

### xvoGetBool

Get boolean value (supports automatic type conversion)

**Prototype:**
```c
XXAPI bool xvoGetBool(xvalue pVal);
```

**Conversion Rules:**
- NULL/EMPTY → FALSE
- BOOL → Returns original value
- INT → Non-zero is TRUE
- FLOAT → Non-zero is TRUE
- Other types → TRUE

---

### xvoGetInt

Get integer value (supports automatic type conversion)

**Prototype:**
```c
XXAPI int64 xvoGetInt(xvalue pVal);
```

**Conversion Rules:**
- NULL/EMPTY → 0
- BOOL → 1 or 0
- INT → Returns original value
- FLOAT → Truncated to integer
- TEXT → Parses string
- Others → 0

---

### xvoGetFloat

Get floating point number (supports automatic type conversion)

**Prototype:**
```c
XXAPI double xvoGetFloat(xvalue pVal);
```

---

### xvoGetText

Get string (supports automatic type conversion)

**Prototype:**
```c
XXAPI str xvoGetText(xvalue pVal);
```

**Description:** Non-TEXT types return temporary string, no need to release

---

### xvoGetTime

Get time value

**Prototype:**
```c
XXAPI xtime xvoGetTime(xvalue pVal);
```

---

### xvoGetPoint

Get pointer

**Prototype:**
```c
XXAPI ptr xvoGetPoint(xvalue pVal);
```

---

### xvoGetFunc

Get function pointer

**Prototype:**
```c
XXAPI xfunction xvoGetFunc(xvalue pVal);
```

---

### Container Getter Functions

Get underlying data structures of containers

**Prototypes:**
```c
XXAPI xparray xvoGetArray(xvalue pVal);    // Get array
XXAPI xlist xvoGetList(xvalue pVal);       // Get list
XXAPI xavltree xvoGetColl(xvalue pVal);    // Get collection
XXAPI xdict xvoGetTable(xvalue pVal);      // Get table
XXAPI ptr xvoGetStruct(xvalue pVal);       // Get struct
XXAPI ptr xvoGetObject(xvalue pVal);       // Get object
XXAPI ptr xvoGetCustom(xvalue pVal);       // Get custom data
```

---

## Type Checking

### xvoIsNull

Check if NULL or EMPTY

**Prototype:**
```c
XXAPI bool xvoIsNull(xvalue pVal);
```

**Description:** Returns TRUE when pVal==NULL or Type==NULL or Type==EMPTY

---

### xvoType

Get value type

**Prototype:**
```c
XXAPI int xvoType(xvalue pVal);
```

**Return Value:** XVO_DT_* type constant

**Example:**
```c
xvalue v = xvoCreateInt(123);
if (xvoType(v) == XVO_DT_INT) {
    printf("It's an integer\n");
}
xvoUnref(v);
```

---

### xvoGetSize

Get data size

**Prototype:**
```c
XXAPI uint32 xvoGetSize(xvalue pVal);
```

**Description:**
- TEXT returns string length
- STRUCT/OBJECT returns struct size
- Other types return fixed size

---

### Container Element Type Checking Macros

```c
// Get container element type
#define xvoArrayItemType(pArr, index)        xvoType(xvoArrayGetValue(pArr, index))
#define xvoListItemType(pList, index)        xvoType(xvoListGetValue(pList, index))
#define xvoTableItemType(pTbl, key, kl)      xvoType(xvoTableGetValue(pTbl, key, kl))

// Get container element size
#define xvoArrayItemSize(pArr, index)        xvoGetSize(xvoArrayGetValue(pArr, index))
#define xvoListItemSize(pList, index)        xvoGetSize(xvoListGetValue(pList, index))
#define xvoTableItemSize(pTbl, key, kl)      xvoGetSize(xvoTableGetValue(pTbl, key, kl))
```

---

## Array Operations

### xvoArrayGetValue

Get array element

**Prototype:**
```c
XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index);
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `pArr` | Array Value |
| `index` | Index (0-based) |

**Return Value:** Element Value, returns EMPTY static value if not exists

**Convenience Macros:**
```c
#define xvoArrayGetBool(pArr, index)    xvoGetBool(xvoArrayGetValue(pArr, index))
#define xvoArrayGetInt(pArr, index)     xvoGetInt(xvoArrayGetValue(pArr, index))
#define xvoArrayGetFloat(pArr, index)   xvoGetFloat(xvoArrayGetValue(pArr, index))
#define xvoArrayGetText(pArr, index)    xvoGetText(xvoArrayGetValue(pArr, index))
#define xvoArrayGetTime(pArr, index)    xvoGetTime(xvoArrayGetValue(pArr, index))
#define xvoArrayGetPoint(pArr, index)   xvoGetPoint(xvoArrayGetValue(pArr, index))
#define xvoArrayGetFunc(pArr, index)    xvoGetFunc(xvoArrayGetValue(pArr, index))
#define xvoArrayGetArray(pArr, index)   xvoGetArray(xvoArrayGetValue(pArr, index))
#define xvoArrayGetList(pArr, index)    xvoGetList(xvoArrayGetValue(pArr, index))
#define xvoArrayGetColl(pArr, index)    xvoGetColl(xvoArrayGetValue(pArr, index))
#define xvoArrayGetTable(pArr, index)   xvoGetTable(xvoArrayGetValue(pArr, index))
#define xvoArrayGetStruct(pArr, index)  xvoGetStruct(xvoArrayGetValue(pArr, index))
#define xvoArrayGetObject(pArr, index)  xvoGetObject(xvoArrayGetValue(pArr, index))
#define xvoArrayGetCustom(pArr, index)  xvoGetCustom(xvoArrayGetValue(pArr, index))
```

---

### xvoArrayAppendValue

Append element to array end

**Prototype:**
```c
XXAPI bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `pArr` | Array Value |
| `pVal` | Value to append |
| `bColloc` | TRUE=collocate reference, FALSE=increment reference count |

**Convenience Macros:**
```c
#define xvoArrayAppendNull(pArr)                 xvoArrayAppendValue(pArr, xvoCreateNull(), TRUE)
#define xvoArrayAppendBool(pArr, bVal)           xvoArrayAppendValue(pArr, xvoCreateBool(bVal), TRUE)
#define xvoArrayAppendInt(pArr, iVal)            xvoArrayAppendValue(pArr, xvoCreateInt(iVal), TRUE)
#define xvoArrayAppendFloat(pArr, fVal)          xvoArrayAppendValue(pArr, xvoCreateFloat(fVal), TRUE)
#define xvoArrayAppendText(pArr, sVal, iSize, bColloc)  xvoArrayAppendValue(pArr, xvoCreateText(sVal, iSize, bColloc), TRUE)
#define xvoArrayAppendTime(pArr, tVal)           xvoArrayAppendValue(pArr, xvoCreateTime(tVal), TRUE)
#define xvoArrayAppendPoint(pArr, pVal)          xvoArrayAppendValue(pArr, xvoCreatePoint(pVal), TRUE)
#define xvoArrayAppendFunc(pArr, func)           xvoArrayAppendValue(pArr, xvoCreateFunc(func), TRUE)
#define xvoArrayAppendArray(pArr)                xvoArrayAppendValue(pArr, xvoCreateArray(), TRUE)
#define xvoArrayAppendList(pArr)                 xvoArrayAppendValue(pArr, xvoCreateList(), TRUE)
#define xvoArrayAppendColl(pArr)                 xvoArrayAppendValue(pArr, xvoCreateColl(), TRUE)
#define xvoArrayAppendTable(pArr)                xvoArrayAppendValue(pArr, xvoCreateTable(), TRUE)
#define xvoArrayAppendStruct(pArr, size)         xvoArrayAppendValue(pArr, xvoCreateStruct(size), TRUE)
#define xvoArrayAppendObject(pArr, size)         xvoArrayAppendValue(pArr, xvoCreateObject(size), TRUE)
#define xvoArrayAppendCustom(pArr, point)        xvoArrayAppendValue(pArr, xvoCreateCustom(point), TRUE)
```

**Example:**
```c
xvalue arr = xvoCreateArray();
xvoArrayAppendInt(arr, 1);
xvoArrayAppendInt(arr, 2);
xvoArrayAppendText(arr, "hello", 0, TRUE);
xvoUnref(arr);  // Automatically releases all child elements
```

---

### xvoArrayInsertValue

Insert element at specified position

**Prototype:**
```c
XXAPI bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
```

**Convenience Macros:**
```c
#define xvoArrayInsertNull(pArr, idx)            xvoArrayInsertValue(pArr, idx, xvoCreateNull(), TRUE)
#define xvoArrayInsertBool(pArr, idx, bVal)      xvoArrayInsertValue(pArr, idx, xvoCreateBool(bVal), TRUE)
#define xvoArrayInsertInt(pArr, idx, iVal)       xvoArrayInsertValue(pArr, idx, xvoCreateInt(iVal), TRUE)
// ... other types similar
```

---

### xvoArraySetValue

Modify element at specified position

**Prototype:**
```c
XXAPI bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
```

**Description:** Automatically releases old value

**Convenience Macros:**
```c
#define xvoArraySetNull(pArr, idx)               xvoArraySetValue(pArr, idx, xvoCreateNull(), TRUE)
#define xvoArraySetBool(pArr, idx, bVal)         xvoArraySetValue(pArr, idx, xvoCreateBool(bVal), TRUE)
#define xvoArraySetInt(pArr, idx, iVal)          xvoArraySetValue(pArr, idx, xvoCreateInt(iVal), TRUE)
// ... other types similar
```

---

### Other Array Operations

```c
// Merge arrays (append pArr2 elements to pArr1)
XXAPI bool xvoArrayMerge(xvalue pArr1, xvalue pArr2);

// Swap element positions
XXAPI bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2);

// Remove elements
XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);

// Get element count
XXAPI uint32 xvoArrayItemCount(xvalue pArr);

// Clear array
XXAPI bool xvoArrayClear(xvalue pArr);

// Pre-allocate capacity
XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count);

// Sort
XXAPI bool xvoArraySort(xvalue pArr, ptr proc);
```

**Example:**
```c
xvalue arr = xvoCreateArray();
xvoArrayAppendInt(arr, 3);
xvoArrayAppendInt(arr, 1);
xvoArrayAppendInt(arr, 2);

uint32 count = xvoArrayItemCount(arr);  // 3
int64 val = xvoArrayGetInt(arr, 0);     // 3

xvoArraySwap(arr, 0, 2);  // Swap 1st and 3rd elements
xvoArrayRemove(arr, 1, 1); // Remove 2nd element

xvoUnref(arr);
```

---

## List Operations

### xvoListGetValue

Get list element

**Prototype:**
```c
XXAPI xvalue xvoListGetValue(xvalue pList, int64 index);
```

**Description:** Index can be any int64 value (like sparse array)

**Convenience Macros:**
```c
#define xvoListGetBool(pList, index)    xvoGetBool(xvoListGetValue(pList, index))
#define xvoListGetInt(pList, index)     xvoGetInt(xvoListGetValue(pList, index))
#define xvoListGetFloat(pList, index)   xvoGetFloat(xvoListGetValue(pList, index))
// ... other types similar
```

---

### xvoListSetValue

Set list element

**Prototype:**
```c
XXAPI bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc);
```

**Convenience Macros:**
```c
#define xvoListSetNull(pList, idx)               xvoListSetValue(pList, idx, xvoCreateNull(), TRUE)
#define xvoListSetBool(pList, idx, bVal)         xvoListSetValue(pList, idx, xvoCreateBool(bVal), TRUE)
#define xvoListSetInt(pList, idx, iVal)          xvoListSetValue(pList, idx, xvoCreateInt(iVal), TRUE)
// ... other types similar
```

---

### Other List Operations

```c
// Merge lists
XXAPI bool xvoListMerge(xvalue pList1, xvalue pList2, bool bReWrite);

// Check if element exists
XXAPI bool xvoListExists(xvalue pList, int64 index);

// Remove element
XXAPI bool xvoListRemove(xvalue pList, int64 index);

// Get element count
XXAPI uint32 xvoListItemCount(xvalue pList);

// Clear list
XXAPI bool xvoListClear(xvalue pList);

// Set parent list (for inheritance lookup)
XXAPI bool xvoListSetParent(xvalue pList, xvalue pParentList);
```

**Example:**
```c
xvalue list = xvoCreateList();
xvoListSetInt(list, 100, 1);     // Key 100 = 1
xvoListSetInt(list, 200, 2);     // Key 200 = 2
xvoListSetInt(list, -50, 3);     // Key -50 = 3

if (xvoListExists(list, 100)) {
    int64 val = xvoListGetInt(list, 100);  // 1
}

xvoUnref(list);
```

---

## Collection Operations

Collection (Coll) is based on AVL tree implementation, elements are automatically deduplicated and sorted.

### xvoCollSetValue

Add element to collection

**Prototype:**
```c
XXAPI bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc);
```

**Convenience Macros:**
```c
#define xvoCollSetNull(pColl)                   xvoCollSetValue(pColl, xvoCreateNull(), TRUE)
#define xvoCollSetBool(pColl, bVal)             xvoCollSetValue(pColl, xvoCreateBool(bVal), TRUE)
#define xvoCollSetInt(pColl, iVal)              xvoCollSetValue(pColl, xvoCreateInt(iVal), TRUE)
#define xvoCollSetFloat(pColl, fVal)            xvoCollSetValue(pColl, xvoCreateFloat(fVal), TRUE)
#define xvoCollSetText(pColl, sVal, iSize, bColloc)  xvoCollSetValue(pColl, xvoCreateText(sVal, iSize, bColloc), TRUE)
#define xvoCollSetTime(pColl, tVal)             xvoCollSetValue(pColl, xvoCreateTime(tVal), TRUE)
#define xvoCollSetPoint(pColl, pVal)            xvoCollSetValue(pColl, xvoCreatePoint(pVal), TRUE)
#define xvoCollSetFunc(pColl, func)             xvoCollSetValue(pColl, xvoCreateFunc(func), TRUE)
#define xvoCollSetCustom(pColl, point)          xvoCollSetValue(pColl, xvoCreateCustom(point), TRUE)
```

---

### Set Operations

```c
// Get difference [ elements in pSelf but not in pColl ]
XXAPI xvalue xvoCollDifference(xvalue pSelf, xvalue pColl);

// Get symmetric difference [ non-overlapping elements from both sets ]
XXAPI xvalue xvoCollSymmetricDifference(xvalue pSelf, xvalue pColl);

// Get intersection [ elements existing in both sets ]
XXAPI xvalue xvoCollIntersection(xvalue pSelf, xvalue pColl);

// Get union [ merge both sets, returns new collection ]
XXAPI xvalue xvoCollUnion(xvalue pSelf, xvalue pColl);

// Merge collection [ merge pColl elements into pSelf ]
XXAPI bool xvoCollMerge(xvalue pSelf, xvalue pColl);
```

**Description:** Difference, symmetric difference, intersection, and union all return newly created collections that need to be released

---

### Other Collection Operations

```c
// Check if element exists
XXAPI bool xvoCollExists(xvalue pColl, xvalue pVal);

// Remove element
XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal);

// Get element count
XXAPI uint32 xvoCollItemCount(xvalue pColl);

// Clear collection
XXAPI bool xvoCollClear(xvalue pColl);

// Set parent collection
XXAPI bool xvoCollSetParent(xvalue pColl, xvalue pParentColl);
```

**Example:**
```c
// Create two collections
xvalue setA = xvoCreateColl();
xvoCollSetInt(setA, 1);
xvoCollSetInt(setA, 2);
xvoCollSetInt(setA, 3);

xvalue setB = xvoCreateColl();
xvoCollSetInt(setB, 2);
xvoCollSetInt(setB, 3);
xvoCollSetInt(setB, 4);

// Set operations
xvalue diff = xvoCollDifference(setA, setB);      // {1}
xvalue inter = xvoCollIntersection(setA, setB);   // {2, 3}
xvalue uni = xvoCollUnion(setA, setB);            // {1, 2, 3, 4}
xvalue symDiff = xvoCollSymmetricDifference(setA, setB);  // {1, 4}

// Cleanup
xvoUnref(setA);
xvoUnref(setB);
xvoUnref(diff);
xvoUnref(inter);
xvoUnref(uni);
xvoUnref(symDiff);
```

---

## Table Operations

Table is based on `xdict` implementation, using strings as keys.

### xvoTableGetValue

Get table element

**Prototype:**
```c
XXAPI xvalue xvoTableGetValue(xvalue pTbl, str key, uint32 kl);
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `pTbl` | Table Value |
| `key` | String key |
| `kl` | Key length (0 for auto-calculate) |

**Convenience Macros:**
```c
#define xvoTableGetBool(pTbl, key, kl)    xvoGetBool(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetInt(pTbl, key, kl)     xvoGetInt(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetFloat(pTbl, key, kl)   xvoGetFloat(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetText(pTbl, key, kl)    xvoGetText(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetTime(pTbl, key, kl)    xvoGetTime(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetPoint(pTbl, key, kl)   xvoGetPoint(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetFunc(pTbl, key, kl)    xvoGetFunc(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetArray(pTbl, key, kl)   xvoGetArray(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetList(pTbl, key, kl)    xvoGetList(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetColl(pTbl, key, kl)    xvoGetColl(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetTable(pTbl, key, kl)   xvoGetTable(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetStruct(pTbl, key, kl)  xvoGetStruct(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetObject(pTbl, key, kl)  xvoGetObject(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetCustom(pTbl, key, kl)  xvoGetCustom(xvoTableGetValue(pTbl, key, kl))
```

---

### xvoTableSetValue

Set table element

**Prototype:**
```c
XXAPI bool xvoTableSetValue(xvalue pTbl, str key, uint32 kl, xvalue pVal, bool bColloc);
```

**Convenience Macros:**
```c
#define xvoTableSetNull(pTbl, key, kl)           xvoTableSetValue(pTbl, key, kl, xvoCreateNull(), TRUE)
#define xvoTableSetBool(pTbl, key, kl, bVal)     xvoTableSetValue(pTbl, key, kl, xvoCreateBool(bVal), TRUE)
#define xvoTableSetInt(pTbl, key, kl, iVal)      xvoTableSetValue(pTbl, key, kl, xvoCreateInt(iVal), TRUE)
#define xvoTableSetFloat(pTbl, key, kl, fVal)    xvoTableSetValue(pTbl, key, kl, xvoCreateFloat(fVal), TRUE)
#define xvoTableSetText(pTbl, key, kl, sVal, iSize, bColloc)  xvoTableSetValue(pTbl, key, kl, xvoCreateText(sVal, iSize, bColloc), TRUE)
#define xvoTableSetTime(pTbl, key, kl, tVal)     xvoTableSetValue(pTbl, key, kl, xvoCreateTime(tVal), TRUE)
#define xvoTableSetPoint(pTbl, key, kl, pVal)    xvoTableSetValue(pTbl, key, kl, xvoCreatePoint(pVal), TRUE)
#define xvoTableSetFunc(pTbl, key, kl, func)     xvoTableSetValue(pTbl, key, kl, xvoCreateFunc(func), TRUE)
#define xvoTableSetArray(pTbl, key, kl)          xvoTableSetValue(pTbl, key, kl, xvoCreateArray(), TRUE)
#define xvoTableSetList(pTbl, key, kl)           xvoTableSetValue(pTbl, key, kl, xvoCreateList(), TRUE)
#define xvoTableSetColl(pTbl, key, kl)           xvoTableSetValue(pTbl, key, kl, xvoCreateColl(), TRUE)
#define xvoTableSetTable(pTbl, key, kl)          xvoTableSetValue(pTbl, key, kl, xvoCreateTable(), TRUE)
#define xvoTableSetStruct(pTbl, key, kl, size)   xvoTableSetValue(pTbl, key, kl, xvoCreateStruct(size), TRUE)
#define xvoTableSetObject(pTbl, key, kl, size)   xvoTableSetValue(pTbl, key, kl, xvoCreateObject(size), TRUE)
#define xvoTableSetCustom(pTbl, key, kl, point)  xvoTableSetValue(pTbl, key, kl, xvoCreateCustom(point), TRUE)
```

---

### Other Table Operations

```c
// Merge tables
XXAPI bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite);

// Check if key exists
XXAPI bool xvoTableExists(xvalue pTbl, str key, uint32 kl);

// Remove key
XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl);

// Get element count
XXAPI uint32 xvoTableItemCount(xvalue pTbl);

// Clear table
XXAPI bool xvoTableClear(xvalue pTbl);

// Set parent table (for inheritance lookup)
XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable);
```

**Example:**
```c
xvalue user = xvoCreateTable();
xvoTableSetInt(user, "id", 0, 1001);
xvoTableSetText(user, "name", 0, "Alice", 0, TRUE);
xvoTableSetBool(user, "active", 0, TRUE);

// Read
if (xvoTableExists(user, "name", 0)) {
    str name = xvoTableGetText(user, "name", 0);  // "Alice"
    int64 id = xvoTableGetInt(user, "id", 0);     // 1001
}

// Nested table
xvoTableSetTable(user, "profile", 0);
xvalue profile = xvoTableGetValue(user, "profile", 0);
xvoTableSetInt(profile, "age", 0, 25);
xvoTableSetText(profile, "city", 0, "Beijing", 0, TRUE);

xvoUnref(user);  // Automatically releases all child elements
```

---

## Copy Operations

### xvoCopy

Shallow copy

**Prototype:**
```c
XXAPI xvalue xvoCopy(xvalue pVal);
```

**Description:**
- Basic types (BOOL/INT/FLOAT/TEXT etc.): Creates new value
- Complex types (ARRAY/LIST/COLL/TABLE): Copies container structure, child elements get reference incremented
- NULL/BOOL returns static singleton
- STRUCT/OBJECT/CUSTOM returns NULL

---

### xvoDeepCopy

Deep copy

**Prototype:**
```c
XXAPI xvalue xvoDeepCopy(xvalue pVal);
```

**Description:**
- Recursively copies all child elements
- Completely independent copy

**Example:**
```c
xvalue original = xvoCreateTable();
xvoTableSetInt(original, "id", 0, 100);
xvoTableSetText(original, "name", 0, "test", 0, TRUE);

// Shallow copy
xvalue shallow = xvoCopy(original);

// Deep copy
xvalue deep = xvoDeepCopy(original);

xvoUnref(original);
xvoUnref(shallow);
xvoUnref(deep);
```

---

## Debug Functions

### xvoPrintValue

Print Value structure and values

**Prototype:**
```c
XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey);
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `objVal` | Value to print |
| `iLevel` | Indent level (usually pass 0) |
| `iMode` | 0=direct output, 1=array element, 2=table element |
| `iKey` | Array index (valid when iMode=1) |
| `sKey` | Table key name (valid when iMode=2) |

**Description:** Recursively prints all child elements, for debugging

**Example:**
```c
xvalue data = xvoCreateTable();
xvoTableSetInt(data, "id", 0, 100);
xvoTableSetText(data, "name", 0, "test", 0, TRUE);

// Print structure
xvoPrintValue(data, 0, 0, 0, NULL);

/* Output similar to:
(table) [0x12345678] (table), count : 2
    ( int ) [0x12345680] "id" = 100
    (text ) [0x12345688] "name" = "test"
*/

xvoUnref(data);
```

---

## Use Cases

### 1. JSON-style Data

```c
// Create user object
xvalue user = xvoCreateTable();
xvoTableSetInt(user, "id", 0, 1001);
xvoTableSetText(user, "name", 0, "John", 0, TRUE);
xvoTableSetText(user, "email", 0, "john@example.com", 0, TRUE);

// Create user list
xvalue users = xvoCreateArray();
xvoArrayAppendValue(users, user, TRUE);

// Access data
xvalue first_user = xvoArrayGetValue(users, 0);
str name = xvoTableGetText(first_user, "name", 0);
printf("Name: %s\n", name);  // "John"

xvoUnref(users);
```

---

### 2. Configuration System

```c
xvalue config = xvoCreateTable();
xvoTableSetInt(config, "port", 0, 8080);
xvoTableSetText(config, "host", 0, "0.0.0.0", 0, TRUE);
xvoTableSetBool(config, "debug", 0, TRUE);

// Read configuration
int64 port = xvoTableGetInt(config, "port", 0);    // 8080
str host = xvoTableGetText(config, "host", 0);     // "0.0.0.0"
bool debug = xvoTableGetBool(config, "debug", 0);  // TRUE

xvoUnref(config);
```

---

### 3. Dynamic Data Container

```c
typedef struct {
    xvalue data;  // Can store any type
} DynamicContainer;

void SetValue(DynamicContainer* c, xvalue val) {
    if (c->data) {
        xvoUnref(c->data);
    }
    c->data = val;
    xvoAddRef(val);  // Increment reference
}

xvalue GetValue(DynamicContainer* c) {
    return c->data;
}

void FreeContainer(DynamicContainer* c) {
    if (c->data) {
        xvoUnref(c->data);
        c->data = NULL;
    }
}
```

---

## Best Practices

### 1. Reference Counting Management

```c
// ✅ Correct: Release after use
xvalue v = xvoCreateInt(100);
UseValue(v);
xvoUnref(v);

// ✅ Correct: bColloc=TRUE means collocated, no need to release child elements separately
xvalue arr = xvoCreateArray();
xvoArrayAppendInt(arr, 100);  // Internally uses bColloc=TRUE
xvoUnref(arr);  // Container automatically releases child elements when freed

// ✅ Correct: bColloc=FALSE increments reference count
xvalue item = xvoCreateInt(200);
xvoArrayAppendValue(arr, item, FALSE);  // item reference count +1
xvoUnref(item);  // Original reference -1, array still holds reference
```

---

### 2. Avoid Circular References

```c
// ❌ Dangerous: Circular reference causes memory leak
xvalue a = xvoCreateTable();
xvalue b = xvoCreateTable();
xvoTableSetValue(a, "b", 0, b, FALSE);  // a references b
xvoTableSetValue(b, "a", 0, a, FALSE);  // b references a
// Neither can be released!
```

---

### 3. Performance Optimization

```c
// ✅ Efficient: Use collocation mode for constant strings
xvalue v = xvoCreateText("constant", 0, TRUE);

// ❌ Inefficient: Copy constant strings too
xvalue v = xvoCreateText("constant", 0, FALSE);

// ✅ Efficient: Pre-allocate capacity
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 1000);  // Pre-allocate capacity for 1000 elements
for (int i = 0; i < 1000; i++) {
    xvoArrayAppendInt(arr, i);
}
xvoUnref(arr);
```

---

## Related Documents

- [JSON Processing](api-json.en.md) - JSON and Value conversion
- [Dict Dictionary](api-dict.en.md) - Underlying dictionary implementation
- [List](api-list.en.md) - Underlying list implementation
- [AVL Tree](api-avltree.en.md) - Collection underlying implementation
- [Pointer Array](api-ptrarray.en.md) - Array underlying implementation
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#value-dynamic-type-system-library)

</div>
