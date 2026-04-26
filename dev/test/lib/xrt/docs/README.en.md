# XRT API Complete Documentation

> XRT (X Runtime Library) API Reference Manual

---

## 📚 Documentation Navigation

### 🔹 Basic Type Definitions
- [Basic Type Definitions](types.en.md) - All basic data types, macro definitions and global structures

### 🔹 Core Modules

| Module | Documentation Link | Description |
|--------|-------------------|-------------|
| **Base** | [Base Functions](api-base.en.md) | Memory management, error handling |
| **Charset** | [Character Set Conversion](api-charset.en.md) | UTF-8/16/32 encoding conversion |
| **OS** | [Operating System](api-os.en.md) | Process management, system calls |
| **Math** | [Math Operations](api-math.en.md) | Random number generation |
| **String** | [String Processing](api-string.en.md) | String operations, encoding/decoding |
| **Path** | [Path Processing](api-path.en.md) | File path operations |
| **Time** | [Time Processing](api-time.en.md) | Date and time calculations |
| **File** | [File Operations](api-file.en.md) | File read/write, directory management |
| **Thread** | [Thread Management](api-thread.en.md) | Multi-threading support |
| **Hash** | [Hash Computation](api-hash.en.md) | 32/64-bit hash functions |
| **Network** | [Network Functions](api-network.en.md) | Local network information |
| **XID** | [Distributed ID](api-xid.en.md) | Unique ID generator |

### 🔹 Data Structure Modules

| Module | Documentation Link | Description |
|--------|-------------------|-------------|
| **Buffer** | [Dynamic Buffer](api-buffer.en.md) | Memory buffer management |
| **Array** | [Struct Array](api-array.en.md) | Dynamic array |
| **PtrArray** | [Pointer Array](api-ptrarray.en.md) | Pointer dynamic array |
| **BSMM** | [Block Structured Memory](api-bsmm.en.md) | Block memory manager |
| **MemUnit** | [Memory Unit](api-memunit.en.md) | 256-byte page management |
| **FSMemPool** | [Fixed Size Memory Pool](api-mempool-fs.en.md) | Fixed size memory pool |
| **Stack** | [Static Stack](api-stack.en.md) | Fixed depth stack |
| **DynStack** | [Dynamic Stack](api-dynstack.en.md) | Variable depth stack |
| **LList** | [Doubly Linked List](api-llist.en.md) | Linked list data structure |
| **LList-Base** | [Linked List Base Operations](api-llist-base.en.md) | Low-level linked list operations |
| **AVLTree** | [AVL Tree](api-avltree.en.md) | Balanced binary tree |
| **AVLTree-Base** | [AVL Tree Base Operations](api-avltree-base.en.md) | Low-level AVL tree operations |
| **MemPool** | [General Memory Pool](api-mempool.en.md) | Multi-level memory pool |
| **Dict** | [Dictionary](api-dict.en.md) | Key-value storage |
| **List** | [List](api-list.en.md) | Integer-indexed list |

### 🔹 Advanced Modules

| Module | Documentation Link | Description |
|--------|-------------------|-------------|
| **Value** | [Dynamic Type System](api-value.en.md) | 15 dynamic data types |
| **JNUM** | [Number Conversion](api-jnum.en.md) | Number-string conversion |
| **JSON** | [JSON Processing](api-json.en.md) | JSON parsing and generation |
| **Template** | [Template Engine](api-template.en.md) | String template processing |

---

## 📖 Quick Reference

### Type Definition Quick Reference

#### Basic Types
```c
typedef unsigned char* str;        // String
typedef char i8;                    // 8-bit integer
typedef unsigned char u8;           // Unsigned 8-bit integer
typedef short i16;                  // 16-bit integer
typedef unsigned short u16;         // Unsigned 16-bit integer
typedef int i32;                    // 32-bit integer
typedef unsigned int u32;           // Unsigned 32-bit integer
typedef long long i64;              // 64-bit integer
typedef unsigned long long u64;     // Unsigned 64-bit integer
typedef float f32;                  // 32-bit floating point
typedef double f64;                 // 64-bit floating point
typedef void* ptr;                  // Generic pointer
typedef int64 xtime;                // Time type
```

#### Data Structure Types
```c
typedef struct xbuffer_struct* xbuffer;       // Buffer
typedef struct xparray_struct* xparray;       // Pointer array
typedef struct xarray_struct* xarray;         // Struct array
typedef struct xbsmm_struct* xbsmm;           // Block memory manager
typedef struct xmemunit_struct* xmemunit;     // Memory unit
typedef struct xfsmempool_struct* xfsmempool; // Fixed size memory pool
typedef struct xstack_struct* xstack;         // Static stack
typedef struct xdynstack_struct* xdynstack;   // Dynamic stack
typedef struct xllist_struct* xllist;         // Doubly linked list
typedef struct xavltree_struct* xavltree;     // AVL tree
typedef struct xmempool_struct* xmempool;     // General memory pool
typedef struct xdict_struct* xdict;           // Dictionary
typedef struct xlist_struct* xlist;           // List
typedef struct xvalue_struct* xvalue;         // Dynamic type
typedef struct xfile_struct* xfile;           // File object
typedef struct xthread_struct* xthread;       // Thread object
typedef struct xid_struct* xid;               // Distributed ID
```

### Common Functions Quick Reference

#### Initialization and Cleanup
```c
xrtGlobalData* xrtInit();           // Initialize library
void xrtUnit();                      // Release library resources
```

#### Memory Management
```c
ptr xrtMalloc(size_t iSize);                 // Allocate memory
ptr xrtCalloc(size_t iNum, size_t iSize);   // Allocate and zero-fill
ptr xrtRealloc(ptr pMem, size_t iSize);     // Reallocate
void xrtFree(ptr pmem);                      // Free memory
ptr xrtTempMemory(size_t iSize);            // Temporary memory (auto-released)
```

#### String Operations
```c
str xrtCopyStr(str sText, size_t iSize);                     // Copy string
str xrtFindStr(str sText, size_t iSize, str sSubText, ...); // Find string
str xrtReplace(str sText, ..., str sRepText, ...);          // Replace string
str* xrtSplit(str sText, ..., str sSepText, ...);           // Split string
str xrtFormat(str sFormat, ...);                             // Format string
```

#### File Operations
```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);  // Open file
void xrtClose(xfile objFile);                            // Close file
str xrtFileReadAll(str sPath, int iCharset);            // Read all content
bool xrtFileWriteAll(str sPath, str sText, ...);        // Write all content
bool xrtFileExists(str sPath);                           // Check if file exists
```

#### Time Processing
```c
xtime xrtNow();                                          // Current time
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay); // Build date
str xrtTimeToStr(xtime iTime, int iFormat);             // Time to string
xtime xrtDateAdd(int interval, int64 iValue, xtime);    // Add/subtract time
```

#### Value Dynamic Type
```c
xvalue xvoCreateNull();                                  // Create null
xvalue xvoCreateInt(int64 iVal);                        // Create integer
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool);     // Create string
xvalue xvoCreateArray();                                 // Create array
xvalue xvoCreateTable();                                 // Create table
void xvoUnref(xvalue pVal);                             // Release reference
```

---

## 🎯 Usage Guide

### 1. Library Initialization

Before using any XRT functions, you must initialize:

```c
#include "xrt.h"

int main() {
    // Initialize XRT library
    xrtGlobalData* xCore = xrtInit();
    
    // Use XRT functions
    // ...
    
    // Clean up resources
    xrtUnit();
    return 0;
}
```

### 2. Memory Management Guidelines

**Functions requiring release:**
- All functions annotated with "requires xrtFree to release"
- Memory allocated with `xrtMalloc/xrtCalloc/xrtRealloc`

**Functions not requiring release:**
- `xrtTempMemory` - Uses circular temporary memory, automatically managed
- Functions returning constant strings

### 3. Character Set Handling

XRT supports multiple character set encodings:

```c
// Character set constants
XRT_CP_AUTO     // Auto-detect
XRT_CP_BINARY   // Binary
XRT_CP_OEM      // Native encoding
XRT_CP_UTF8     // UTF-8
XRT_CP_UTF16    // UTF-16
XRT_CP_UTF32    // UTF-32
```

### 4. Error Handling

```c
// Set error callback
void OnError(str sError) {
    printf("Error: %s\n", sError);
}

xCore->OnError = OnError;

// Check for errors
if (xCore->LastError != xCore->sNull) {
    printf("Last Error: %s\n", xCore->LastError);
}
```

---

## 📊 API Statistics

| Category | Count |
|----------|-------|
| Basic Type Definitions | 30+ |
| Macro Definitions | 150+ |
| Data Structures | 25+ |
| Function Declarations | 300+ |
| Inline Functions | 20+ |

---

## 📝 Documentation Conventions

### Function Prototype Format

```c
XXAPI ReturnType FunctionName(ParamType paramName, ...);
```

- `XXAPI` - Export marker (used for DLL compilation)
- `size_t iSize` parameter - When value is 0, string length is calculated automatically
- `bool bSrcRevise` - TRUE modifies source data, FALSE returns newly allocated memory

### Memory Release Labels

- ✅ **Requires Release** - Must be released with `xrtFree`
- ⭕ **Auto-managed** - Managed automatically by the library
- 🔄 **Reference Counted** - Release with `xvoUnref`

---

## 🔗 Related Links

- [Project Home](../README.md)
- [Quick Start](../README.md#quick-start)
- [Example Code](../test/)
- [License](../README.md#license)

---

<div align="center">

**XRT API Documentation** | Version 1.0 | Last Updated: 2025-01

</div>
