# Buffer Dynamic Buffer Library

> Variable-size memory buffer supporting binary data and multiple string encodings

[English](api-buffer.en.md) | [中文](api-buffer.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [Data Structures](#data-structures)
- [Buffer Operations](#buffer-operations)
- [Data Read/Write](#data-readwrite)
- [Use Cases](#use-cases)
- [Best Practices](#best-practices)

---

## Constant Definitions

### String Mode Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `XBUF_BINARY` | `0` | Binary mode, no automatic terminator |
| `XBUF_ANSI` | `1` | ANSI string, append 1 byte `\0` |
| `XBUF_UTF8` | `1` | UTF-8 string, append 1 byte `\0` |
| `XBUF_UTF16` | `2` | UTF-16 string, append 2 bytes `\0\0` |
| `XBUF_UTF32` | `4` | UTF-32 string, append 4 bytes `\0\0\0\0` |

### Memory Allocation Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `XBUFFER_ALLOC_STEP` | `0x10000` (64KB) | Default pre-allocation step |

---

## Data Structures

### xbuffer_struct

Buffer manager data structure.

**Definition:**
```c
typedef struct {
    char* Buffer;       // Memory buffer pointer
    uint32 Length;      // Current data length (bytes)
    uint32 AllocLength; // Allocated memory length (bytes)
    uint32 AllocStep;   // Pre-allocation memory step
} xbuffer_struct, *xbuffer;
```

**Member Description:**
- `Buffer` - Actual data storage pointer, can be read directly
- `Length` - Current valid data length written
- `AllocLength` - Total allocated memory size
- `AllocStep` - Increment size when expanding

---

## Buffer Operations

### xrtBufferCreate

Create buffer manager.

**Function Prototype:**
```c
XXAPI xbuffer xrtBufferCreate(uint32 iStep);
```

**Parameters:**
- `iStep` - Pre-allocation step (0 uses default 64KB)

**Return Value:**
- Success: Buffer object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtBufferDestroy` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Use default step
    xbuffer buf1 = xrtBufferCreate(0);
    printf("Default step: %u\n", buf1->AllocStep);  // 65536
    
    // Custom step (suitable for small data)
    xbuffer buf2 = xrtBufferCreate(1024);
    printf("Custom step: %u\n", buf2->AllocStep);  // 1024
    
    xrtBufferDestroy(buf1);
    xrtBufferDestroy(buf2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtBufferDestroy

Destroy buffer manager.

**Function Prototype:**
```c
XXAPI void xrtBufferDestroy(xbuffer pBuf);
```

**Parameters:**
- `pBuf` - Buffer object pointer

**Notes:**
- Releases internal buffer memory and manager struct
- Passing `NULL` won't crash (safe check)

---

### xrtBufferInit

Initialize buffer manager (for stack or embedded struct).

**Function Prototype:**
```c
XXAPI void xrtBufferInit(xbuffer pBuf, uint32 iStep);
```

**Parameters:**
- `pBuf` - Buffer struct pointer
- `iStep` - Pre-allocation step (0 uses default)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Allocate struct on stack
    xbuffer_struct buf;
    xrtBufferInit(&buf, 4096);  // 4KB step
    
    xrtBufferAppend(&buf, (ptr)"Hello", 5, XBUF_ANSI);
    printf("Data: %s\n", buf.Buffer);
    printf("Length: %u\n", buf.Length);
    
    xrtBufferUnit(&buf);  // Release internal buffer, not the struct
    
    xrtUnit();
    return 0;
}
```

---

### xrtBufferUnit

Clear buffer (release internal memory, not the struct).

**Function Prototype:**
```c
XXAPI void xrtBufferUnit(xbuffer pBuf);
```

**Parameters:**
- `pBuf` - Buffer object pointer

**Notes:**
- Releases memory pointed by `Buffer`
- Sets `Length` and `AllocLength` to 0
- Does not release `xbuffer_struct` itself

**Alias:** `xrtBufferClear`

---

### xrtBufferMalloc

Pre-allocate or adjust buffer memory size.

**Function Prototype:**
```c
XXAPI bool xrtBufferMalloc(xbuffer pBuf, uint32 iCount);
```

**Parameters:**
- `pBuf` - Buffer object
- `iCount` - Target memory size (bytes)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (memory allocation failed)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    
    // Pre-allocate 1MB memory
    if (xrtBufferMalloc(buf, 1024 * 1024)) {
        printf("Pre-allocated: %u bytes\n", buf->AllocLength);
    }
    
    // Writing data won't trigger reallocation
    for (int i = 0; i < 1000; i++) {
        xrtBufferAppend(buf, (ptr)"data", 4, XBUF_BINARY);
    }
    printf("Used: %u bytes\n", buf->Length);  // 4000
    
    // Trim memory (release excess space)
    xrtBufferMalloc(buf, buf->Length);
    printf("After trim: %u bytes\n", buf->AllocLength);  // 4000
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

## Data Read/Write

### xrtBufferInsert

Insert data at specified position.

**Function Prototype:**
```c
XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode);
```

**Parameters:**
- `pBuf` - Buffer object
- `iPos` - Insert position (byte offset)
- `pData` - Data pointer
- `iSize` - Data size (bytes, 0 means auto-calculate string length)
- `bStrMode` - String mode (`XBUF_*` constants)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

---

### xrtBufferAppend

Append data at end of buffer.

**Function Prototype:**
```c
XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode);
```

**Parameters:**
- `pBuf` - Buffer object
- `pData` - Data pointer
- `iSize` - Data size (bytes, 0 means auto-calculate)
- `bStrMode` - String mode (`XBUF_*` constants)

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(1024);
    
    // Append binary data
    uint32 header = 0x12345678;
    xrtBufferAppend(buf, &header, sizeof(header), XBUF_BINARY);
    printf("Length: %u\n", buf->Length);  // 4
    
    // Append ANSI string
    xrtBufferAppend(buf, (ptr)"Hello", 5, XBUF_ANSI);
    printf("Length: %u\n", buf->Length);  // 9
    
    // Auto-calculate length when appending
    xrtBufferAppend(buf, (ptr)" World", 0, XBUF_UTF8);
    printf("Data: %s\n", &buf->Buffer[4]);  // "Hello World"
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

### Direct Buffer Access

Buffer data can be accessed directly through struct members:

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    xrtBufferAppend(buf, (ptr)"Hello World", 0, XBUF_UTF8);
    
    // Read data directly
    char* data = buf->Buffer;       // Data pointer
    uint32 len = buf->Length;       // Valid data length
    uint32 cap = buf->AllocLength;  // Allocated capacity
    
    printf("Data: %s\n", data);
    printf("Length: %u\n", len);
    printf("Capacity: %u\n", cap);
    
    // Iterate binary data
    for (uint32 i = 0; i < len; i++) {
        printf("%02X ", (unsigned char)buf->Buffer[i]);
    }
    printf("\n");
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

## Use Cases

### 1. Dynamic Packet Building

```c
#include "xrt.h"
#include <stdio.h>

xbuffer BuildPacket(uint32 cmd, ptr payload, uint32 payloadSize) {
    xbuffer buf = xrtBufferCreate(256);
    
    // Header
    uint32 header = 0xAABBCCDD;
    xrtBufferAppend(buf, &header, 4, XBUF_BINARY);
    
    // Command code
    xrtBufferAppend(buf, &cmd, 4, XBUF_BINARY);
    
    // Payload length
    xrtBufferAppend(buf, &payloadSize, 4, XBUF_BINARY);
    
    // Payload data
    if (payloadSize > 0) {
        xrtBufferAppend(buf, payload, payloadSize, XBUF_BINARY);
    }
    
    return buf;
}

int main() {
    xrtInit();
    
    str data = (str)"Hello, World!";
    xbuffer packet = BuildPacket(0x01, data, 13);
    
    printf("Packet size: %u bytes\n", packet->Length);  // 25
    
    // Print hex
    for (uint32 i = 0; i < packet->Length; i++) {
        printf("%02X ", (unsigned char)packet->Buffer[i]);
    }
    printf("\n");
    
    xrtBufferDestroy(packet);
    xrtUnit();
    return 0;
}
```

---

### 2. String Concatenation

```c
#include "xrt.h"
#include <stdio.h>

str BuildSQL(str table, str* fields, int fieldCount) {
    xbuffer buf = xrtBufferCreate(256);
    
    xrtBufferAppend(buf, (ptr)"SELECT ", 0, XBUF_ANSI);
    
    for (int i = 0; i < fieldCount; i++) {
        if (i > 0) {
            xrtBufferAppend(buf, (ptr)", ", 2, XBUF_BINARY);
        }
        xrtBufferAppend(buf, fields[i], 0, XBUF_BINARY);
    }
    
    xrtBufferAppend(buf, (ptr)" FROM ", 6, XBUF_BINARY);
    xrtBufferAppend(buf, table, 0, XBUF_ANSI);
    
    // Copy result (because we need to destroy buffer)
    str result = xrtCopyStr((str)buf->Buffer, buf->Length);
    xrtBufferDestroy(buf);
    
    return result;
}

int main() {
    xrtInit();
    
    str fields[] = {(str)"id", (str)"name", (str)"age"};
    str sql = BuildSQL((str)"users", fields, 3);
    
    printf("SQL: %s\n", sql);  // "SELECT id, name, age FROM users"
    xrtFree(sql);
    
    xrtUnit();
    return 0;
}
```

---

### 3. File Content Caching

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer cache = xrtBufferCreate(0);
    
    // Simulate multiple reads and cache
    for (int i = 0; i < 5; i++) {
        str chunk = xrtFormat((str)"Chunk %d data. ", i);
        xrtBufferAppend(cache, chunk, 0, XBUF_BINARY);
        xrtFree(chunk);
    }
    
    // Add terminator
    xrtBufferAppend(cache, (ptr)"\0", 1, XBUF_BINARY);
    
    printf("Cache content: %s\n", cache->Buffer);
    printf("Total size: %u bytes\n", cache->Length);
    
    xrtBufferDestroy(cache);
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Pre-allocate Memory

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ Not recommended: Frequent expansion
    xbuffer buf1 = xrtBufferCreate(64);  // Small step
    for (int i = 0; i < 10000; i++) {
        xrtBufferAppend(buf1, (ptr)"data", 4, XBUF_BINARY);
    }
    printf("Method 1 complete\n");
    
    // ✓ Recommended: Pre-allocate known size
    xbuffer buf2 = xrtBufferCreate(0);
    xrtBufferMalloc(buf2, 10000 * 4);  // Pre-allocate 40KB
    for (int i = 0; i < 10000; i++) {
        xrtBufferAppend(buf2, (ptr)"data", 4, XBUF_BINARY);
    }
    printf("Method 2 complete\n");
    
    xrtBufferDestroy(buf1);
    xrtBufferDestroy(buf2);
    xrtUnit();
    return 0;
}
```

---

### 2. Embedded Buffer

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str name;
    xbuffer_struct data;  // Embedded buffer
} MyObject;

MyObject* CreateObject(str name) {
    MyObject* obj = xrtMalloc(sizeof(MyObject));
    obj->name = xrtCopyStr(name, 0);
    xrtBufferInit(&obj->data, 1024);  // Initialize embedded buffer
    return obj;
}

void DestroyObject(MyObject* obj) {
    if (obj) {
        xrtFree(obj->name);
        xrtBufferUnit(&obj->data);  // Release buffer content
        xrtFree(obj);
    }
}

int main() {
    xrtInit();
    
    MyObject* obj = CreateObject((str)"TestObject");
    
    // Use embedded buffer
    xrtBufferAppend(&obj->data, (ptr)"payload data", 0, XBUF_UTF8);
    printf("Object name: %s\n", obj->name);
    printf("Data: %s\n", obj->data.Buffer);
    
    DestroyObject(obj);
    xrtUnit();
    return 0;
}
```

---

### 3. Memory Trimming

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    
    // Write small data, but allocated large memory
    xrtBufferAppend(buf, (ptr)"Small data", 10, XBUF_ANSI);
    printf("Before - Data: %u, Alloc: %u\n", buf->Length, buf->AllocLength);
    
    // Trim excess memory
    xrtBufferMalloc(buf, buf->Length + 1);  // +1 to keep terminator
    printf("After - Data: %u, Alloc: %u\n", buf->Length, buf->AllocLength);
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Base Functions Library](api-base.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#buffer-dynamic-buffer-library)

</div>
