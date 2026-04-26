# XID 分布式ID库

> 192位分布式唯一ID生成器

[English](api-xid.en.md) | [中文](api-xid.md) | [返回索引](README.md)

---

## 📑 目录

- [XID结构](#xid结构)
- [ID生成](#id生成)
- [ID转换](#id转换)
- [ID比较](#id比较)
- [使用场景](#使用场景)
- [注意事项](#注意事项)

---

## XID结构

### 结构定义

```c
typedef struct {
    xtime Time;        // 当前时间戳 (64位)
    int32 Addr;        // 本机IP地址 (32位)
    int32 Tick;        // CPU时钟/精度计时器 (32位)
    int64 Rand;        // 随机数 (64位)
} xid_struct, *xid;
```

### 结构图解

```
192位 XID 结构 (24字节)
┌────────────────┬────────┬────────┬────────────────┐
│      Time        │  Addr  │  Tick  │      Rand       │
│    (64bit)       │ (32bit)│ (32bit)│    (64bit)      │
├────────────────┼────────┼────────┼────────────────┤
│  年月日时分秒    │ IP地址 │ 高精度 │   PCG随机数     │
└────────────────┴────────┴────────┴────────────────┘
       8字节           4字节     4字节         8字节
```

### 字段说明

| 字段 | 类型 | 大小 | 说明 |
|------|------|------|------|
| `Time` | `xtime` (int64) | 8字节 | 生成时的时间戳（秒） |
| `Addr` | `int32` | 4字节 | 本机IP地址（`xCore.LocalAddr`） |
| `Tick` | `int32` | 4字节 | 高精度时钟计数器 |
| `Rand` | `int64` | 8字节 | PCG算法64位随机数 |

### 特点

- **192位（24字节）**：比UUID（128位）更长，碰撞概率更低
- **全局唯一**：时间+IP+精度计时+随机数组合保证
- **时间有序**：包含精确时间戳，可排序
- **机器标识**：包含IP地址，可追溯来源
- **分布式安全**：无需中心协调，各节点独立生成

---

## ID生成

### xrtMakeXID

创建新的 XID 对象。

**函数原型：**
```c
XXAPI xid xrtMakeXID();
```

**返回值：**
- 成功：新的 XID 对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建 XID
    xid id = xrtMakeXID();
    if (id != NULL) {
        printf("时间戳: %" PRId64 "\n", id->Time);
        printf("IP地址: 0x%08X\n", id->Addr);
        printf("精度计时: %d\n", id->Tick);
        printf("随机数: %" PRIu64 "\n", (uint64)id->Rand);
        xrtFree(id);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `QueryPerformanceCounter` 获取高精度计时
- Linux 使用 `clock_gettime(CLOCK_MONOTONIC)` 获取纳秒级计时
- 随机数使用 PCG 算法生成

---

### xrtMakeXIDS

创建 XID 并直接返回字符串（便捷函数）。

**函数原型：**
```c
XXAPI str xrtMakeXIDS();
```

**返回值：**
- 成功：Base64 编码的 XID 字符串（32字符）
- 失败：`xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 一步生成 XID 字符串
    str xidStr = xrtMakeXIDS();
    if (xidStr != xCore.sNull) {
        printf("XID: %s\n", xidStr);  // 32字符 Base64 编码
        printf("长度: %zu\n", strlen((char*)xidStr));  // 32
        xrtFree(xidStr);
    }
    
    // 生成多个 XID
    printf("\n生成10个XID:\n");
    for (int i = 0; i < 10; i++) {
        str s = xrtMakeXIDS();
        printf("%d: %s\n", i + 1, s);
        xrtFree(s);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部调用 `xrtMakeXID` + `xrtEncodeXID`
- 比分开调用更简洁，无需手动释放中间对象
- 返回的字符串使用自定义 Base64 字符集

---

## ID转换

### xrtEncodeXID

XID 对象转换为字符串。

**函数原型：**
```c
XXAPI str xrtEncodeXID(xid pXID);
```

**参数：**
- `pXID` - XID 对象指针

**返回值：**
- Base64 编码字符串（32字符）

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xid id = xrtMakeXID();
    if (id != NULL) {
        str encoded = xrtEncodeXID(id);
        printf("编码后: %s\n", encoded);
        printf("字符数: %zu\n", strlen((char*)encoded));  // 32
        
        xrtFree(encoded);
        xrtFree(id);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 使用自定义 Base64 字符集（包含 `0-9`, `A-Z`, `a-z`, `-`, `_`）
- 24字节编码为32字符（每3字节编码为4字符）
- 输出字符串对URL和文件名安全

---

### xrtDecodeXID

字符串转换为 XID 对象。

**函数原型：**
```c
XXAPI xid xrtDecodeXID(str sXID);
```

**参数：**
- `sXID` - XID 字符串（32字符）

**返回值：**
- 成功：XID 对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建并编码
    str original = xrtMakeXIDS();
    printf("原始: %s\n", original);
    
    // 解码
    xid decoded = xrtDecodeXID(original);
    if (decoded != NULL) {
        printf("解码成功:\n");
        printf("  Time: %" PRId64 "\n", decoded->Time);
        printf("  Addr: 0x%08X\n", decoded->Addr);
        printf("  Tick: %d\n", decoded->Tick);
        printf("  Rand: %" PRIu64 "\n", (uint64)decoded->Rand);
        
        // 重新编码验证
        str reEncoded = xrtEncodeXID(decoded);
        printf("重编码: %s\n", reEncoded);
        printf("匹配: %s\n", 
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

## ID比较

### xrtCompXID

比较两个 XID 是否相同。

**函数原型：**
```c
XXAPI bool xrtCompXID(xid pXID1, xid pXID2);
```

**参数：**
- `pXID1` - 第一个 XID
- `pXID2` - 第二个 XID

**返回值：**
- `TRUE` - 相同
- `FALSE` - 不同

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建两个不同的 XID
    xid id1 = xrtMakeXID();
    xid id2 = xrtMakeXID();
    
    printf("不同XID比较: %s\n", 
        xrtCompXID(id1, id2) ? "相同" : "不同");  // 不同
    
    // 通过编码/解码创建副本
    str s1 = xrtEncodeXID(id1);
    xid id1_copy = xrtDecodeXID(s1);
    
    printf("副本XID比较: %s\n", 
        xrtCompXID(id1, id1_copy) ? "相同" : "不同");  // 相同
    
    xrtFree(s1);
    xrtFree(id1_copy);
    xrtFree(id1);
    xrtFree(id2);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 比较所有四个字段（Time, Addr, Tick, Rand）
- 只有全部字段都相同才返回 TRUE

---

## 使用场景

### 1. 唯一订单号

```c
#include "xrt.h"
#include <stdio.h>

str GenerateOrderID() {
    return xrtMakeXIDS();
}

int main() {
    xrtInit();
    
    str orderId = GenerateOrderID();
    printf("订单号: %s\n", orderId);
    // 示例: "Abc123XyzDef456GhiJkl789MnoPqr0"
    xrtFree(orderId);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 分布式追踪ID

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
    printf("追踪ID: %s\n", ctx->trace_id);
    printf("SpanID: %s\n", ctx->span_id);
    FreeTrace(ctx);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 数据库主键

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str id;  // XID 主键
    str name;
    int age;
} User;

User* CreateUser(str name, int age) {
    User* user = xrtMalloc(sizeof(User));
    user->id = xrtMakeXIDS();  // 自动生成主键
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
    printf("用户ID: %s\n", user->id);
    printf("用户名: %s\n", user->name);
    printf("年龄: %d\n", user->age);
    FreeUser(user);
    
    xrtUnit();
    return 0;
}
```

---

### 4. 临时文件名

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
    printf("临时文件: %s\n", tmpFile);
    xrtFree(tmpFile);
    
    xrtUnit();
    return 0;
}
```

---

## 注意事项

### 1. 内存管理

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ 正确：xrtMakeXID 返回的需要释放
    xid id = xrtMakeXID();
    // ... 使用
    xrtFree(id);
    
    // ✅ 正确：xrtMakeXIDS 返回的需要释放
    str s = xrtMakeXIDS();
    // ... 使用
    xrtFree(s);
    
    // ✅ 正确：xrtEncodeXID 和 xrtDecodeXID 都需要释放
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

### 2. 网络初始化

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    // xrtInit() 会自动获取本机IP
    xrtInit();
    
    // 网络未连接时 xCore.LocalAddr 可能为 0
    if (xCore.LocalAddr == 0) {
        printf("警告: 未获取到IP地址，XID可能不唯一\n");
    }
    
    // 仍然可以生成 XID，但 Addr 字段为 0
    str xid = xrtMakeXIDS();
    printf("XID: %s\n", xid);
    xrtFree(xid);
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Network 网络功能](api-network.md) - IP地址获取
- [Math 数学运算](api-math.md) - 随机数生成
- [Time 时间处理](api-time.md) - 时间戳获取
- [String 字符串](api-string.md) - Base64编码
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#xid-分布式id库)

</div>
