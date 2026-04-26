# Base Library

> XRT core foundation: library initialization, memory management, error handling

[English](api-base.en.md) | [中文](api-base.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constants](#constants)
- [Data Types](#data-types)
- [Global Variables](#global-variables)
- [Library Initialization](#library-initialization)
- [Memory Management](#memory-management)
- [Temporary Memory](#temporary-memory)
- [Error Handling](#error-handling)
- [Supplementary Functions](#supplementary-functions)

---

## Constants

### Boolean Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `TRUE` | `1` | Logical true |
| `FALSE` | `0` | Logical false |
| `null` | `0` | Null pointer/value |

**Notes:**
- These constants are used to distinguish from C standard library's `true`/`false`
- `TRUE`/`FALSE` return values indicate function success or failure
- `null` is equivalent to `NULL`, used for null pointer checking

### API Export Macro

| Macro | Description |
|-------|-------------|
| `XXAPI` | API function export identifier. Defined as `__declspec(dllexport)` when compiling as DLL, empty otherwise |

**Definition:**
```c
#ifdef BUILD_DLL
    #define XXAPI   __declspec(dllexport)
#else
    #define XXAPI
#endif
```

---

## Data Types

### xrtGlobalData

XRT global data structure, stores library runtime state and configuration.

**Definition:**
```c
typedef struct {
    // Initialization state
    int bInit;                          // Whether initialized
    
    // Global constants
    str sNull;                          // Empty string constant (read-only)
    
    // Temporary return values (for multi-return scenarios)
    str sRet;                           // String return value
    int64 iRet;                         // Integer return value
    double nRet;                        // Float return value
    
    // Error handling
    str LastError;                      // Last error message
    int __pri_FreeError;                // Internal: whether to free error string
    void (*OnError)(str sError);        // Error callback function
    
    // High-precision clock (Windows only)
    #if defined(_WIN32) || defined(_WIN64)
        uint64 Frequency;               // Clock frequency
    #endif
    
    // Network info
    uint LocalAddr;                     // Local IP address (for XID generation)
    
    // Application info
    str AppFile;                        // Application full path
    str AppPath;                        // Application directory
    
    // Ring temporary memory
    ptr TempMem[32];                    // 32 temporary memory slots
    uint32 TempMemIdx;                  // Current slot index
    
    // Memory allocation functions (customizable)
    ptr (*malloc)(size_t iSize);        // Allocate memory
    ptr (*calloc)(size_t iNum, size_t iSize);  // Allocate and zero
    ptr (*realloc)(ptr pMem, size_t iSize);    // Reallocate
    void (*free)(ptr pMem);             // Free memory
    
} xrtGlobalData;
```

**Member Description:**

| Member | Type | Description |
|--------|------|-------------|
| `bInit` | `int` | Initialization flag, `TRUE` means initialized |
| `sNull` | `str` | Global empty string, used for safe returns |
| `sRet` | `str` | Temporary string return value storage |
| `iRet` | `int64` | Temporary integer return value storage |
| `nRet` | `double` | Temporary float return value storage |
| `LastError` | `str` | Last error message |
| `OnError` | Function pointer | Callback function when error occurs |
| `AppFile` | `str` | Full path of current program |
| `AppPath` | `str` | Directory of current program |
| `TempMem` | `ptr[32]` | 32 temporary memory slots |
| `malloc/calloc/realloc/free` | Function pointers | Customizable memory allocation functions |

---

## Global Variables

### xCore

Global data instance, available after `xrtInit()`.

**Declaration:**
```c
XXAPI extern xrtGlobalData xCore;
```

**Usage Example:**
```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Access application path
    printf("App Path: %s\n", xCore.AppPath);
    
    // Set error callback
    xCore.OnError = MyErrorHandler;
    
    // Check error
    if (xCore.LastError != xCore.sNull) {
        printf("Error: %s\n", xCore.LastError);
    }
    
    xrtUnit();
    return 0;
}
```

---

## Library Initialization

### xrtInit

Initialize XRT library

**Function Prototype:**
```c
XXAPI xrtGlobalData* xrtInit();
```

**Return Value:**
- Returns global data structure pointer `&xCore`

**Notes:**
- **Must** be called before using any XRT functionality
- Multiple calls are safe (checks initialization state)
- Initialization includes:
  - Setting global constants
  - Initializing random number generator
  - Getting application path
  - Initializing network (Windows)
  - Getting local IP address

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
}

int main() {
    // Initialize library
    xrtGlobalData* pCore = xrtInit();
    
    // Set error callback (optional)
    pCore->OnError = MyErrorHandler;
    
    // Print application info
    printf("App File: %s\n", pCore->AppFile);
    printf("App Path: %s\n", pCore->AppPath);
    
    // Use library features
    str text = xrtFormat("Hello, %s!", "World");
    printf("%s\n", text);
    xrtFree(text);
    
    // Cleanup resources
    xrtUnit();
    return 0;
}
```

---

### xrtUnit

Release XRT library resources

**Function Prototype:**
```c
XXAPI void xrtUnit();
```

**Notes:**
- Releases application path
- Releases all temporary memory
- Cleans up network resources (Windows)
- Should be called before program exit

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Use XRT features
    str text = xrtCopyStr("Hello XRT", 0);
    printf("%s\n", text);
    xrtFree(text);
    
    // Cleanup before program ends
    xrtUnit();
    return 0;
}
```

---

## Memory Management

### xrtMalloc

Allocate memory

**Function Prototype:**
```c
XXAPI ptr xrtMalloc(size_t iSize);
```

**Parameters:**
- `iSize` - Number of bytes to allocate

**Return Value:**
- Success: Returns allocated memory pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // Allocate 1024 bytes
    ptr buffer = xrtMalloc(1024);
    if (buffer) {
        // Use memory
        memset(buffer, 0, 1024);
        strcpy((char*)buffer, "Hello Memory!");
        printf("%s\n", (char*)buffer);
        
        // Free memory
        xrtFree(buffer);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtCalloc

Allocate and zero memory

**Function Prototype:**
```c
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize);
```

**Parameters:**
- `iNum` - Number of elements
- `iSize` - Size of each element in bytes

**Return Value:**
- Success: Returns allocated and zeroed memory pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Notes:**
- Allocates `iNum * iSize` bytes of memory
- All bytes initialized to 0

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Allocate array of 100 int32, initialized to 0
    i32* numbers = (i32*)xrtCalloc(100, sizeof(i32));
    if (numbers) {
        // numbers[0] ~ numbers[99] are all 0
        printf("Initial value: %d\n", numbers[50]);  // Output: 0
        
        numbers[0] = 42;
        numbers[99] = 100;
        printf("numbers[0] = %d, numbers[99] = %d\n", numbers[0], numbers[99]);
        
        xrtFree(numbers);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtRealloc

Reallocate memory

**Function Prototype:**
```c
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize);
```

**Parameters:**
- `pMem` - Original memory pointer (can be `NULL`)
- `iSize` - New size in bytes

**Return Value:**
- Success: Returns new memory pointer (may differ from original)
- Failure: Returns `NULL` (original memory unchanged)

**Memory Release:** ✅ Requires `xrtFree` to release

**Notes:**
- If `pMem` is `NULL`, equivalent to `xrtMalloc(iSize)`
- If `iSize` is 0, equivalent to `xrtFree(pMem)`
- New memory may be at different address, original content is copied

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // Initial allocation
    str text = xrtMalloc(100);
    strcpy((char*)text, "Hello");
    printf("Before: %s\n", text);
    
    // Extend to 200 bytes
    text = xrtRealloc(text, 200);
    strcat((char*)text, " World!");
    printf("After: %s\n", text);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

**Safe Mode:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str old_text = xrtMalloc(100);
    str new_text = xrtRealloc(old_text, 200);
    if (new_text) {
        old_text = new_text;  // Update pointer
        printf("Realloc succeeded\n");
    } else {
        // Reallocation failed, old_text still valid
        printf("Realloc failed\n");
    }
    xrtFree(old_text);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFree

Free memory

**Function Prototype:**
```c
XXAPI void xrtFree(ptr pmem);
```

**Parameters:**
- `pmem` - Memory pointer to free

**Notes:**
- Checks if pointer is `NULL` first
- Passing `NULL` is safe (won't crash)
- Can only free memory allocated by `xrtMalloc/xrtCalloc/xrtRealloc`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = xrtMalloc(256);
    xrtFree(text);       // Normal release
    
    xrtFree(NULL);       // Safe, no error
    
    // Recommended practice to avoid double free
    str data = xrtMalloc(100);
    if (data) {
        // Use data
        xrtFree(data);
        data = NULL;     // Recommended: set to NULL after free
    }
    
    xrtUnit();
    return 0;
}
```

---

## Temporary Memory

### xrtTempMemory

Allocate temporary memory (auto-managed)

**Function Prototype:**
```c
XXAPI ptr xrtTempMemory(size_t iSize);
```

**Parameters:**
- `iSize` - Number of bytes to allocate

**Return Value:**
- Success: Returns temporary memory pointer
- Failure: Returns `NULL`

**Memory Release:** ⭕ **No manual release needed** (auto-managed)

**Notes:**
- Uses ring buffer (32 slots)
- Cyclic use, 33rd call automatically frees 1st memory
- Suitable for temporary, short-term small memory
- **Not suitable for** data that needs long-term holding

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Temporary formatted string
    str temp1 = xrtTempMemory(100);
    sprintf((char*)temp1, "Number: %d", 123);
    printf("%s\n", temp1);  // Use
    
    str temp2 = xrtTempMemory(200);
    sprintf((char*)temp2, "Value: %.2f", 3.14);
    printf("%s\n", temp2);
    
    // No need to call xrtFree
    
    xrtUnit();
    return 0;
}
```

**Cautions:**
```c
// ❌ Wrong usage: returning temporary memory
str GetTempString() {
    str temp = xrtTempMemory(100);
    strcpy((char*)temp, "data");
    return temp;  // Dangerous! May be overwritten after 32 calls
}

// ✅ Correct usage: use immediately
void PrintFormat(int value) {
    str temp = xrtTempMemory(50);
    sprintf((char*)temp, "Value: %d", value);
    printf("%s\n", temp);  // Used immediately
}
```

---

### xrtFreeTempMemory

Free all temporary memory

**Function Prototype:**
```c
XXAPI void xrtFreeTempMemory();
```

**Notes:**
- Frees all 32 temporary memory slots
- Resets ring buffer index
- Usually called automatically in `xrtUnit()`
- Can be called manually when immediate release is needed

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    for (int i = 0; i < 100; i++) {
        str temp = xrtTempMemory(1024);
        sprintf((char*)temp, "Item %d", i);
        // Use temp
        
        if (i % 32 == 31) {
            xrtFreeTempMemory();  // Clean every 32 times to avoid overwrite
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## Error Handling

### xrtSetError

Set error message (UTF-8 string)

**Function Prototype:**
```c
XXAPI void xrtSetError(str sError, bool bFree);
```

**Parameters:**
- `sError` - Error message string
- `bFree` - Whether the string needs to be freed
  - `TRUE` - XRT will free when setting new error or cleanup
  - `FALSE` - XRT won't free (constant string)

**Notes:**
- Sets `xCore.LastError`
- If `xCore.OnError` callback is set, triggers callback

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT ERROR] %s\n", sError);
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // Use constant string (no free)
    xrtSetError("File not found", FALSE);
    
    // Use dynamic string (XRT will free)
    int param = -1;
    str error = xrtFormat("Invalid parameter: %d", param);
    xrtSetError(error, TRUE);  // XRT will handle freeing
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetErrorU16

Set error message (UTF-16 string)

**Function Prototype:**
```c
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree);
```

**Parameters:**
- `sError` - UTF-16 error message
- `iSize` - String length (character count, 0 for auto-calculate)
- `bFree` - Whether to free

**Notes:**
- Automatically converts to UTF-8 before setting

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Set UTF-16 error message
    u16str error = (u16str)L"File not found";
    xrtSetErrorU16(error, 0, FALSE);
    
    // Error message converted to UTF-8
    printf("Error: %s\n", xCore.LastError);
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetErrorU32

Set error message (UTF-32 string)

**Function Prototype:**
```c
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree);
```

**Parameters:**
- `sError` - UTF-32 error message
- `iSize` - String length (character count, 0 for auto-calculate)
- `bFree` - Whether to free

**Notes:**
- Automatically converts to UTF-8 before setting

---

### xrtClearError

Clear error message

**Function Prototype:**
```c
XXAPI void xrtClearError();
```

**Notes:**
- Sets `xCore.LastError` to `xCore.sNull`
- If previous error needs freeing, frees it first

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Simulate an error
    xrtSetError("Something went wrong", FALSE);
    
    // Check and handle error
    if (xCore.LastError != xCore.sNull) {
        printf("Error: %s\n", xCore.LastError);
        xrtClearError();  // Clear error
    }
    
    // Confirm error cleared
    if (xCore.LastError == xCore.sNull) {
        printf("Error cleared.\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## Supplementary Functions

### memmem

Find sub-memory in memory (Windows only)

**Function Prototype:**
```c
#if defined(_WIN32) || defined(_WIN64)
    XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
#endif
```

**Parameters:**
- `pMem` - Memory to search in
- `iMemSize` - Size of memory to search
- `pSub` - Sub-memory to find
- `iSubSize` - Size of sub-memory

**Return Value:**
- Success: Returns pointer to found position
- Failure: Returns `NULL`

**Notes:**
- Linux/Unix systems have this function natively
- Windows systems use XRT implementation

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    #if defined(_WIN32) || defined(_WIN64)
    char data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    char pattern[] = {0x03, 0x04};
    
    ptr found = memmem(data, sizeof(data), pattern, sizeof(pattern));
    if (found) {
        size_t offset = (char*)found - data;
        printf("Found at offset: %zu\n", offset);  // Output: 2
    } else {
        printf("Pattern not found\n");
    }
    #else
    printf("memmem is a native function on Linux/Unix\n");
    #endif
    
    xrtUnit();
    return 0;
}
```

---

### u16len

Get UTF-16 string length

**Function Prototype:**
```c
XXAPI size_t u16len(u16str sText);
```

**Parameters:**
- `sText` - UTF-16 string

**Return Value:**
- String length (character count, excluding terminating `\0`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u16str text = (u16str)L"Hello";
    size_t len = u16len(text);  // Returns 5
    printf("UTF-16 length: %zu\n", len);
    
    u16str chinese = (u16str)L"Hello World";
    size_t len2 = u16len(chinese);  // Returns 11
    printf("UTF-16 length: %zu\n", len2);
    
    xrtUnit();
    return 0;
}
```

---

### u32len

Get UTF-32 string length

**Function Prototype:**
```c
XXAPI size_t u32len(u32str sText);
```

**Parameters:**
- `sText` - UTF-32 string

**Return Value:**
- String length (character count, excluding terminating `\0`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u32str text = (u32str)U"Hello";
    size_t len = u32len(text);  // Returns 5
    printf("UTF-32 length: %zu\n", len);
    
    u32str emoji = (u32str)U"Hello🌍";
    size_t len2 = u32len(emoji);  // Returns 6 (including emoji)
    printf("With emoji UTF-32 length: %zu\n", len2);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Initialization and Cleanup

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
}

int main() {
    // 1. Initialize
    xrtGlobalData* pCore = xrtInit();
    
    // 2. Set error handling
    pCore->OnError = MyErrorHandler;
    
    // 3. Use features
    printf("XRT initialized, app path: %s\n", pCore->AppPath);
    
    // 4. Cleanup
    xrtUnit();
    return 0;
}
```

### 2. Memory Management Strategy

```c
#include "xrt.h"
#include <stdio.h>

void UseIt(str s) { printf("%s\n", s); }
void LoadData(str s) { strcpy((char*)s, "Loaded data"); }

int main() {
    xrtInit();
    int value = 42;
    
    // Short-term temporary use → xrtTempMemory
    str temp = xrtTempMemory(256);
    sprintf((char*)temp, "Temp: %d", value);
    UseIt(temp);
    // No need to free
    
    // Long-term use → xrtMalloc + xrtFree
    str permanent = xrtMalloc(1024);
    LoadData(permanent);
    printf("%s\n", permanent);
    xrtFree(permanent);
    
    // Array allocation → xrtCalloc (auto-zero)
    i32* arr = (i32*)xrtCalloc(100, sizeof(i32));
    arr[0] = 1;
    arr[99] = 100;
    printf("arr[0]=%d, arr[99]=%d\n", arr[0], arr[99]);
    xrtFree(arr);
    
    xrtUnit();
    return 0;
}
```

### 3. Error Handling

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT Error] %s\n", sError);
    // Optional: write to log file
}

bool MyFunction(int param) {
    if (param < 0) {
        xrtSetError("Parameter must be positive", FALSE);
        return false;
    }
    return true;
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // Call function
    if (!MyFunction(-1)) {
        printf("Check error: %s\n", xCore.LastError);
        xrtClearError();
    }
    
    if (MyFunction(10)) {
        printf("Function succeeded\n");
    }
    
    xrtUnit();
    return 0;
}
```

### 4. Safe Memory Operations

```c
#include "xrt.h"
#include <stdio.h>

int SafeMemoryExample() {
    xrtInit();
    size_t size = 1024;
    size_t new_size = 2048;
    
    // ✅ Safe: check return value
    ptr data = xrtMalloc(size);
    if (data == NULL) {
        xrtSetError("Memory allocation failed", FALSE);
        xrtUnit();
        return -1;
    }
    printf("Allocated %zu bytes\n", size);
    
    // ✅ Safe: reallocation
    ptr new_data = xrtRealloc(data, new_size);
    if (new_data) {
        data = new_data;
        printf("Reallocated to %zu bytes\n", new_size);
    } else {
        // Reallocation failed, keep original data
        printf("Realloc failed, keeping original\n");
    }
    
    // ✅ Safe: set to NULL after free
    xrtFree(data);
    data = NULL;
    printf("Memory freed\n");
    
    xrtUnit();
    return 0;
}

int main() {
    return SafeMemoryExample();
}
```

---

## Performance Tips

### Temporary Memory Performance

```c
#include "xrt.h"
#include <stdio.h>

void Process(str s) { /* Process string */ }

int main() {
    xrtInit();
    
    // ❌ Inefficient: frequent alloc/free
    for (int i = 0; i < 100; i++) {
        str temp = xrtMalloc(256);
        sprintf((char*)temp, "Item %d", i);
        Process(temp);
        xrtFree(temp);
    }
    
    // ✅ Efficient: use temporary memory (but note 32 limit)
    for (int i = 0; i < 30; i++) {  // Less than 32 times
        str temp = xrtTempMemory(256);
        sprintf((char*)temp, "Item %d", i);
        Process(temp);
    }
    
    // ✅ Optimal: pre-allocate
    str buffer = xrtMalloc(256);
    for (int i = 0; i < 1000; i++) {
        sprintf((char*)buffer, "Item %d", i);
        Process(buffer);
    }
    xrtFree(buffer);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Type Definitions](types.en.md) - Basic types and global structures
- [Charset Conversion](api-charset.en.md) - String encoding conversion
- [Main Index](README.en.md) - Return to documentation home

---

<div align="center">

[⬆️ Back to Top](#base-library)

</div>
