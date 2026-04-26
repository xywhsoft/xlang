# Network 网络功能库

> 网络信息获取功能（本机IP、MAC地址、主机名）

[English](api-network.en.md) | [中文](api-network.md) | [返回索引](README.md)

---

## 📑 目录

- [本机信息](#本机信息)
  - [xrtGetLocalIP](#xrtgetlocalip)
  - [xrtGetLocalRawIP](#xrtgetlocalrawip)
  - [xrtGetLocalMAC](#xrtgetlocalmac)
  - [xrtGetLocalName](#xrtgetlocalname)
- [使用场景](#使用场景)
- [平台差异](#平台差异)
- [注意事项](#注意事项)

---

## 本机信息

### xrtGetLocalIP

获取本机IP地址（字符串格式）。

**函数原型：**
```c
XXAPI str xrtGetLocalIP();
```

**返回值：**
- 成功：IP地址字符串（如 `"192.168.1.100"`）
- 失败：`xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str ip = xrtGetLocalIP();
    if (ip != xCore.sNull) {
        printf("本机IP: %s\n", ip);
        xrtFree(ip);
    } else {
        printf("获取IP失败\n");
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 使用 `gethostname` + `gethostbyname` 实现
- 返回第一个网卡的IP地址
- 多网卡环境可能返回非预期的IP

---

### xrtGetLocalRawIP

获取本机IP地址（32位整数格式）。

**函数原型：**
```c
XXAPI uint32 xrtGetLocalRawIP();
```

**返回值：**
- 成功：IP地址（32位无符号整数，高位在前）
- 失败：`0`

**IP格式说明：**
```
IP地址 192.168.1.100 的存储方式：
┌────────┬────────┬────────┬────────┐
│  192   │  168   │   1    │  100   │
│ 高8位  │        │        │ 低8位  │
└────────┴────────┴────────┴────────┘
= (192 << 24) | (168 << 16) | (1 << 8) | 100
= 0xC0A80164
```

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint32 ip = xrtGetLocalRawIP();
    if (ip != 0) {
        printf("本机IP: %u.%u.%u.%u\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF
        );
        printf("原始值: 0x%08X (%u)\n", ip, ip);
    } else {
        printf("获取IP失败\n");
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `xrtInit()` 内部自动调用此函数
- 结果存储在 `xCore.LocalAddr`
- 用于 XID 分布式ID生成中的机器标识

---

### xrtGetLocalMAC

获取本机MAC地址（十六进制字符串）。

**函数原型：**
```c
XXAPI str xrtGetLocalMAC();
```

**返回值：**
- 成功：MAC地址字符串（12位十六进制，如 `"001122AABBCC"`）
- 失败：`xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str mac = xrtGetLocalMAC();
    if (mac != xCore.sNull) {
        printf("本机MAC: %s\n", mac);
        
        // 格式化输出（添加冒号分隔）
        printf("格式化: %c%c:%c%c:%c%c:%c%c:%c%c:%c%c\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            mac[6], mac[7], mac[8], mac[9], mac[10], mac[11]);
        
        xrtFree(mac);
    } else {
        printf("获取MAC失败: %s\n", xCore.LastError);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `GetAdaptersInfo` API
- Linux 使用 `ioctl(SIOCGIFHWADDR)` 系统调用
- 返回第一个网卡的MAC地址
- 返回的是大写十六进制字符串

---

### xrtGetLocalName

获取本机主机名。

**函数原型：**
```c
XXAPI str xrtGetLocalName();
```

**返回值：**
- 成功：主机名字符串
- 失败：`xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str name = xrtGetLocalName();
    if (name != xCore.sNull) {
        printf("主机名: %s\n", name);
        xrtFree(name);
    } else {
        printf("获取主机名失败\n");
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 使用 `gethostname` 系统调用
- 最大支持 260 字符的主机名

---

## 使用场景

### 1. 分布式ID生成

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // XID 使用本机IP (xCore.LocalAddr) 作为机器标识
    // 确保在分布式环境中生成唯一ID
    str xid = xrtMakeXIDS();
    printf("分布式ID: %s\n", xid);
    printf("机器IP: 0x%08X\n", xCore.LocalAddr);
    xrtFree(xid);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 设备信息收集

```c
#include "xrt.h"
#include <stdio.h>

void PrintDeviceInfo() {
    str name = xrtGetLocalName();
    str ip = xrtGetLocalIP();
    str mac = xrtGetLocalMAC();
    
    printf("=== 设备信息 ===\n");
    
    if (name != xCore.sNull) {
        printf("主机名: %s\n", name);
        xrtFree(name);
    }
    
    if (ip != xCore.sNull) {
        printf("IP地址: %s\n", ip);
        xrtFree(ip);
    }
    
    if (mac != xCore.sNull) {
        printf("MAC地址: %s\n", mac);
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

### 3. 日志标识

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
    
    LogWithMachineId((str)"应用启动");
    LogWithMachineId((str)"处理请求中...");
    LogWithMachineId((str)"应用关闭");
    
    xrtUnit();
    return 0;
}
```

---

## 平台差异

| 功能 | Windows | Linux/macOS |
|------|---------|-------------|
| `xrtGetLocalIP` | gethostbyname | gethostbyname |
| `xrtGetLocalRawIP` | gethostbyname | gethostbyname |
| `xrtGetLocalMAC` | GetAdaptersInfo | ioctl(SIOCGIFHWADDR) |
| `xrtGetLocalName` | gethostname | gethostname |

---

## 注意事项

### 1. 多网卡环境

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 注意：多网卡环境下可能返回非预期的网卡信息
    // 如果需要指定网卡，需要自行实现枚举逻辑
    str ip = xrtGetLocalIP();
    if (ip != xCore.sNull) {
        printf("获取到的IP: %s（可能不是主要网卡）\n", ip);
        xrtFree(ip);
    }
    
    xrtUnit();
    return 0;
}
```

### 2. 网络未连接

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 网络未连接时可能返回回环地址或失败
    uint32 ip = xrtGetLocalRawIP();
    if (ip == 0) {
        printf("无法获取IP，请检查网络连接\n");
    } else if ((ip >> 24) == 127) {
        printf("只获取到回环地址 (127.x.x.x)\n");
    } else {
        printf("IP正常: %u.%u.%u.%u\n",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF);
    }
    
    xrtUnit();
    return 0;
}
```

### 3. 内存释放

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ 正确：使用后释放
    str ip = xrtGetLocalIP();
    if (ip != xCore.sNull) {
        printf("%s\n", ip);
        xrtFree(ip);  // 必须释放
    }
    
    // ✅ 正确：xrtGetLocalRawIP 返回值类型，无需释放
    uint32 rawIp = xrtGetLocalRawIP();
    printf("%u\n", rawIp);  // 无需释放
    
    // ✗ 错误：忘记释放会导致内存泄漏
    // str mac = xrtGetLocalMAC();
    // printf("%s\n", mac);
    // 缺少 xrtFree(mac);
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [XID 分布式ID](api-xid.md) - 使用 IP 作为机器标识
- [Base 基础模块](api-base.md) - xCore 全局数据结构
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#network-网络功能库)

</div>
