# XID Distributed ID Library

> 192-bit distributed unique ID generator

[English](api-xid.en.md) | [中文](api-xid.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [XID Structure](#xid-structure)
- [ID Generation](#id-generation)
- [ID Conversion](#id-conversion)
- [ID Comparison](#id-comparison)
- [Use Cases](#use-cases)
- [Notes](#notes)

---

## XID Structure

### Structure Definition

```c
typedef struct {
    xtime Time;        // Current timestamp (64-bit)
    int32 Addr;        // Local IP address (32-bit)
    int32 Tick;        // CPU clock/precision timer (32-bit)
    int64 Rand;        // Random number (64-bit)
} xid_struct, *xid;
```

### Structure Diagram

```
192-bit XID Structure (24 bytes)
┌────────────────┬────────┬────────┬────────────────┐
│      Time        │  Addr  │  Tick  │      Rand       │
│    (64bit)       │ (32bit)│ (32bit)│    (64bit)      │
├────────────────┼────────┼────────┼────────────────┤
│  Year/Month/Day  │   IP   │ High   │  PCG Random     │
│  Hour/Min/Sec    │ Address│ Precision│    Number      │
└────────────────┴────────┴────────┴────────────────┘
       8 bytes         4 bytes   4 bytes      8 bytes
```

### Field Description

| Field | Type | Size | Description |
|-------|------|------|-------------|
| `Time` | `xtime` (int64) | 8 bytes | Timestamp at generation (seconds) |
| `Addr` | `int32` | 4 bytes | Local IP address (`xCore.LocalAddr`) |
| `Tick` | `int32` | 4 bytes | High-precision clock counter |
| `Rand` | `int64` | 8 bytes | PCG algorithm 64-bit random number |

### Features

- **192 bits (24 bytes)**: Longer than UUID (128 bits), lower collision probability
- **Globally unique**: Combination of time + IP + precision timer + random number guarantees uniqueness
- **Time-ordered**: Contains precise timestamp, sortable
- **Machine identification**: Contains IP address, traceable to origin
- **Distributed safe**: No central coordination needed, each node generates independently

---

## ID Generation

### xrtMakeXID

Create a new XID object.

**Function Prototype:**
```c
XXAPI xid xrtMakeXID();
```

**Return Value:**
- Success: New XID object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create XID
    xid id = xrtMakeXID();
    if (id != NULL) {
        printf("Timestamp: %" PRId64 "\n", id->Time);
        printf("IP Address: 0x%08X\n", id->Addr);
        printf("Precision Timer: %d\n", id->Tick);
        printf("Random: %" PRIu64 "\n", (uint64)id->Rand);
        xrtFree(id);
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Windows uses `QueryPerformanceCounter` for high-precision timing
- Linux uses `clock_gettime(CLOCK_MONOTONIC)` for nanosecond-level timing
- Random number generated using PCG algorithm

---

### xrtMakeXIDS

Create XID and directly return as string (convenience function).

**Function Prototype:**
```c
XXAPI str xrtMakeXIDS();
```

**Return Value:**
- Success: Base64 encoded XID string (32 characters)
- Failure: `xCore.sNull`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // One-step XID string generation
    str xidStr = xrtMakeXIDS();
    if (xidStr != xCore.sNull) {
        printf("XID: %s\n", xidStr);  // 32-character Base64 encoding
        printf("Length: %zu\n", strlen((char*)xidStr));  // 32
        xrtFree(xidStr);
    }
    
    // Generate multiple XIDs
    printf("\nGenerating 10 XIDs:\n");
    for (int i = 0; i < 10; i++) {
        str s = xrtMakeXIDS();
        printf("%d: %s\n", i + 1, s);
        xrtFree(s);
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Internally calls `xrtMakeXID` + `xrtEncodeXID`
- More concise than calling separately, no need to manually release intermediate object
- Returned string uses custom Base64 character set

---

## ID Conversion

### xrtEncodeXID

Convert XID object to string.

**Function Prototype:**
```c
XXAPI str xrtEncodeXID(xid pXID);
```

**Parameters:**
- `pXID` - XID object pointer

**Return Value:**
- Base64 encoded string (32 characters)

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xid id = xrtMakeXID();
    if (id != NULL) {
        str encoded = xrtEncodeXID(id);
        printf("Encoded: %s\n", encoded);
        printf("Characters: %zu\n", strlen((char*)encoded));  // 32
        
        xrtFree(encoded);
        xrtFree(id);
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Uses custom Base64 character set (includes `0-9`, `A-Z`, `a-z`, `-`, `_`)
- 24 bytes encoded to 32 characters (every 3 bytes encoded to 4 characters)
- Output string is URL and filename safe

---

### xrtDecodeXID

Convert string to XID object.

**Function Prototype:**
```c
XXAPI xid xrtDecodeXID(str sXID);
```

**Parameters:**
- `sXID` - XID string (32 characters)

**Return Value:**
- Success: XID object pointer
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create and encode
    str original = xrtMakeXIDS();
    printf("Original: %s\n", original);
    
    // Decode
    xid decoded = xrtDecodeXID(original);
    if (decoded != NULL) {
        printf("Decode successful:\n");
        printf("  Time: %" PRId64 "\n", decoded->Time);
        printf("  Addr: 0x%08X\n", decoded->Addr);
        printf("  Tick: %d\n", decoded->Tick);
        printf("  Rand: %" PRIu64 "\n", (uint64)decoded->Rand);
        
        // Re-encode to verify
        str reEncoded = xrtEncodeXID(decoded);
        printf("Re-encoded: %s\n", reEncoded);
        printf("Match: %s\n", 
            strcmp((char*)original, (char*)reEncoded) == 0 ? "YES" : "NO");
        
        xrtFree(reEncoded);
        xrtFree(decoded);
    }
    
    xrtFree(original);
    xrtUnit();
    return 0;
}
```

---

## ID Comparison

### xrtCompXID

Compare if two XIDs are identical.

**Function Prototype:**
```c
XXAPI bool xrtCompXID(xid pXID1, xid pXID2);
```

**Parameters:**
- `pXID1` - First XID
- `pXID2` - Second XID

**Return Value:**
- `TRUE` - Identical
- `FALSE` - Different

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create two different XIDs
    xid id1 = xrtMakeXID();
    xid id2 = xrtMakeXID();
    
    printf("Different XIDs comparison: %s\n", 
        xrtCompXID(id1, id2) ? "Same" : "Different");  // Different
    
    // Create copy through encode/decode
    str s1 = xrtEncodeXID(id1);
    xid id1_copy = xrtDecodeXID(s1);
    
    printf("Copy XID comparison: %s\n", 
        xrtCompXID(id1, id1_copy) ? "Same" : "Different");  // Same
    
    xrtFree(s1);
    xrtFree(id1_copy);
    xrtFree(id1);
    xrtFree(id2);
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Compares all four fields (Time, Addr, Tick, Rand)
- Returns TRUE only if all fields are identical

---

## Use Cases

### 1. Unique Order ID

```c
#include "xrt.h"
#include <stdio.h>

str GenerateOrderID() {
    return xrtMakeXIDS();
}

int main() {
    xrtInit();
    
    str orderId = GenerateOrderID();
    printf("Order ID: %s\n", orderId);
    // Example: "Abc123XyzDef456GhiJkl789MnoPqr0"
    xrtFree(orderId);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Distributed Tracing ID

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str trace_id;
    str span_id;
} TraceContext;

TraceContext* CreateTrace() {
    TraceContext* ctx = xrtMalloc(sizeof(TraceContext));
    ctx->trace_id = xrtMakeXIDS();
    ctx->span_id = xrtMakeXIDS();
    return ctx;
}

void FreeTrace(TraceContext* ctx) {
    if (ctx) {
        if (ctx->trace_id != xCore.sNull) xrtFree(ctx->trace_id);
        if (ctx->span_id != xCore.sNull) xrtFree(ctx->span_id);
        xrtFree(ctx);
    }
}

int main() {
    xrtInit();
    
    TraceContext* ctx = CreateTrace();
    printf("Trace ID: %s\n", ctx->trace_id);
    printf("Span ID: %s\n", ctx->span_id);
    FreeTrace(ctx);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Database Primary Key

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str id;  // XID primary key
    str name;
    int age;
} User;

User* CreateUser(str name, int age) {
    User* user = xrtMalloc(sizeof(User));
    user->id = xrtMakeXIDS();  // Auto-generate primary key
    user->name = xrtCopyStr(name, 0);
    user->age = age;
    return user;
}

void FreeUser(User* user) {
    if (user) {
        xrtFree(user->id);
        xrtFree(user->name);
        xrtFree(user);
    }
}

int main() {
    xrtInit();
    
    User* user = CreateUser((str)"Alice", 30);
    printf("User ID: %s\n", user->id);
    printf("Username: %s\n", user->name);
    printf("Age: %d\n", user->age);
    FreeUser(user);
    
    xrtUnit();
    return 0;
}
```

---

### 4. Temporary Filename

```c
#include "xrt.h"
#include <stdio.h>

str GenerateTempFileName(str extension) {
    str xid = xrtMakeXIDS();
    str filename = xrtFormat((str)"temp_%s.%s", xid, extension);
    xrtFree(xid);
    return filename;
}

int main() {
    xrtInit();
    
    str tmpFile = GenerateTempFileName((str)"txt");
    printf("Temporary file: %s\n", tmpFile);
    xrtFree(tmpFile);
    
    xrtUnit();
    return 0;
}
```

---

## Notes

### 1. Memory Management

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ Correct: xrtMakeXID return needs to be released
    xid id = xrtMakeXID();
    // ... use
    xrtFree(id);
    
    // ✅ Correct: xrtMakeXIDS return needs to be released
    str s = xrtMakeXIDS();
    // ... use
    xrtFree(s);
    
    // ✅ Correct: Both xrtEncodeXID and xrtDecodeXID need release
    xid id2 = xrtMakeXID();
    str encoded = xrtEncodeXID(id2);
    xid decoded = xrtDecodeXID(encoded);
    xrtFree(decoded);
    xrtFree(encoded);
    xrtFree(id2);
    
    xrtUnit();
    return 0;
}
```

### 2. Network Initialization

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    // xrtInit() automatically obtains local IP
    xrtInit();
    
    // When network is not connected, xCore.LocalAddr may be 0
    if (xCore.LocalAddr == 0) {
        printf("Warning: No IP address obtained, XID may not be unique\n");
    }
    
    // XID can still be generated, but Addr field will be 0
    str xid = xrtMakeXIDS();
    printf("XID: %s\n", xid);
    xrtFree(xid);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Network Functions](api-network.en.md) - IP address retrieval
- [Math Operations](api-math.en.md) - Random number generation
- [Time Processing](api-time.en.md) - Timestamp retrieval
- [String](api-string.en.md) - Base64 encoding
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#xid-distributed-id-library)

</div>
