# Network Library

> Network information retrieval functions (Local IP, MAC address, hostname)

[English](api-network.en.md) | [中文](api-network.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Local Information](#local-information)
  - [xrtGetLocalIP](#xrtgetlocalip)
  - [xrtGetLocalRawIP](#xrtgetlocalrawip)
  - [xrtGetLocalMAC](#xrtgetlocalmac)
  - [xrtGetLocalName](#xrtgetlocalname)
- [Use Cases](#use-cases)
- [Platform Differences](#platform-differences)
- [Notes](#notes)

---

## Local Information

### xrtGetLocalIP

Get local IP address (string format).

**Function Prototype:**
```c
XXAPI str xrtGetLocalIP();
```

**Return Value:**
- Success: IP address string (e.g., `"192.168.1.100"`)
- Failure: `xCore.sNull`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str ip = xrtGetLocalIP();
    if (ip != xCore.sNull) {
        printf("Local IP: %s\n", ip);
        xrtFree(ip);
    } else {
        printf("Failed to get IP\n");
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Implemented using `gethostname` + `gethostbyname`
- Returns IP address of the first network interface
- May return unexpected IP in multi-NIC environments

---

### xrtGetLocalRawIP

Get local IP address (32-bit integer format).

**Function Prototype:**
```c
XXAPI uint32 xrtGetLocalRawIP();
```

**Return Value:**
- Success: IP address (32-bit unsigned integer, high byte first)
- Failure: `0`

**IP Format:**
```
Storage of IP address 192.168.1.100:
┌────────┬────────┬────────┬────────┐
│  192   │  168   │   1    │  100   │
│ High 8 │        │        │ Low 8  │
└────────┴────────┴────────┴────────┘
= (192 << 24) | (168 << 16) | (1 << 8) | 100
= 0xC0A80164
```

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint32 ip = xrtGetLocalRawIP();
    if (ip != 0) {
        printf("Local IP: %u.%u.%u.%u\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF
        );
        printf("Raw value: 0x%08X (%u)\n", ip, ip);
    } else {
        printf("Failed to get IP\n");
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- `xrtInit()` internally calls this function automatically
- Result is stored in `xCore.LocalAddr`
- Used for machine identification in XID distributed ID generation

---

### xrtGetLocalMAC

Get local MAC address (hexadecimal string).

**Function Prototype:**
```c
XXAPI str xrtGetLocalMAC();
```

**Return Value:**
- Success: MAC address string (12 hex digits, e.g., `"001122AABBCC"`)
- Failure: `xCore.sNull`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str mac = xrtGetLocalMAC();
    if (mac != xCore.sNull) {
        printf("Local MAC: %s\n", mac);
        
        // Formatted output (with colon separators)
        printf("Formatted: %c%c:%c%c:%c%c:%c%c:%c%c:%c%c\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            mac[6], mac[7], mac[8], mac[9], mac[10], mac[11]);
        
        xrtFree(mac);
    } else {
        printf("Failed to get MAC: %s\n", xCore.LastError);
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Windows uses `GetAdaptersInfo` API
- Linux uses `ioctl(SIOCGIFHWADDR)` system call
- Returns MAC address of the first network interface
- Returns uppercase hexadecimal string

---

### xrtGetLocalName

Get local hostname.

**Function Prototype:**
```c
XXAPI str xrtGetLocalName();
```

**Return Value:**
- Success: Hostname string
- Failure: `xCore.sNull`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str name = xrtGetLocalName();
    if (name != xCore.sNull) {
        printf("Hostname: %s\n", name);
        xrtFree(name);
    } else {
        printf("Failed to get hostname\n");
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Uses `gethostname` system call
- Supports hostname up to 260 characters

---

## Use Cases

### 1. Distributed ID Generation

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // XID uses local IP (xCore.LocalAddr) as machine identifier
    // Ensures unique ID generation in distributed environments
    str xid = xrtMakeXIDS();
    printf("Distributed ID: %s\n", xid);
    printf("Machine IP: 0x%08X\n", xCore.LocalAddr);
    xrtFree(xid);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Device Information Collection

```c
#include "xrt.h"
#include <stdio.h>

void PrintDeviceInfo() {
    str name = xrtGetLocalName();
    str ip = xrtGetLocalIP();
    str mac = xrtGetLocalMAC();
    
    printf("=== Device Information ===\n");
    
    if (name != xCore.sNull) {
        printf("Hostname: %s\n", name);
        xrtFree(name);
    }
    
    if (ip != xCore.sNull) {
        printf("IP Address: %s\n", ip);
        xrtFree(ip);
    }
    
    if (mac != xCore.sNull) {
        printf("MAC Address: %s\n", mac);
        xrtFree(mac);
    }
}

int main() {
    xrtInit();
    PrintDeviceInfo();
    xrtUnit();
    return 0;
}
```

---

### 3. Log Identification

```c
#include "xrt.h"
#include <stdio.h>

void LogWithMachineId(str message) {
    str timeStr = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
    uint32 ip = xCore.LocalAddr;
    
    printf("[%s] [%u.%u.%u.%u] %s\n",
        timeStr,
        (ip >> 24) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF,
        ip & 0xFF,
        message);
    
    xrtFree(timeStr);
}

int main() {
    xrtInit();
    
    LogWithMachineId((str)"Application started");
    LogWithMachineId((str)"Processing request...");
    LogWithMachineId((str)"Application shutdown");
    
    xrtUnit();
    return 0;
}
```

---

## Platform Differences

| Function | Windows | Linux/macOS |
|----------|---------|-------------|
| `xrtGetLocalIP` | gethostbyname | gethostbyname |
| `xrtGetLocalRawIP` | gethostbyname | gethostbyname |
| `xrtGetLocalMAC` | GetAdaptersInfo | ioctl(SIOCGIFHWADDR) |
| `xrtGetLocalName` | gethostname | gethostname |

---

## Notes

### 1. Multi-NIC Environment

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Note: In multi-NIC environments, may return unexpected NIC info
    // If specific NIC is needed, implement enumeration logic yourself
    str ip = xrtGetLocalIP();
    if (ip != xCore.sNull) {
        printf("Retrieved IP: %s (may not be primary NIC)\n", ip);
        xrtFree(ip);
    }
    
    xrtUnit();
    return 0;
}
```

### 2. Network Not Connected

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // When network is not connected, may return loopback address or fail
    uint32 ip = xrtGetLocalRawIP();
    if (ip == 0) {
        printf("Cannot get IP, please check network connection\n");
    } else if ((ip >> 24) == 127) {
        printf("Only got loopback address (127.x.x.x)\n");
    } else {
        printf("IP is normal: %u.%u.%u.%u\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF);
    }
    
    xrtUnit();
    return 0;
}
```

### 3. Memory Release

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ Correct: Release after use
    str ip = xrtGetLocalIP();
    if (ip != xCore.sNull) {
        printf("%s\n", ip);
        xrtFree(ip);  // Must release
    }
    
    // ✅ Correct: xrtGetLocalRawIP returns value type, no release needed
    uint32 rawIp = xrtGetLocalRawIP();
    printf("%u\n", rawIp);  // No release needed
    
    // ✗ Wrong: Forgetting to release causes memory leak
    // str mac = xrtGetLocalMAC();
    // printf("%s\n", mac);
    // Missing xrtFree(mac);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [XID Distributed ID](api-xid.en.md) - Uses IP as machine identifier
- [Base Module](api-base.en.md) - xCore global data structure
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#network-library)

</div>
