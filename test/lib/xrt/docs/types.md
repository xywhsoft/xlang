# XRT 基础类型定义

> XRT 库的所有基础类型、宏定义和全局数据结构

[English](api-types.en.md) | [中文](api-types.md) | [返回索引](README.md)

---

## 📑 目录

- [标准库依赖](#标准库依赖)
- [保护宏定义](#保护宏定义)
- [基础类型定义](#基础类型定义)
- [字符串类型](#字符串类型)
- [整数类型](#整数类型)
- [浮点数类型](#浮点数类型)
- [指针类型](#指针类型)
- [特殊类型](#特殊类型)
- [常量定义](#常量定义)
- [全局数据结构](#全局数据结构)

---

## 标准库依赖

XRT 库依赖以下 C 标准库头文件：

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
```

| 头文件 | 用途 |
|--------|------|
| `stdio.h` | 标准输入输出 |
| `stdint.h` | 固定宽度整数类型 (`intptr_t`, `uintptr_t` 等) |
| `stdarg.h` | 可变参数支持 (`va_list` 等) |
| `stdlib.h` | 内存分配 (`malloc`, `free` 等) |
| `unistd.h` | POSIX 系统调用 |
| `string.h` | 字符串操作 (`memcpy`, `strlen` 等) |
| `ctype.h` | 字符分类 (`isalpha`, `isdigit` 等) |
| `wctype.h` | 宽字符分类 |
| `math.h` | 数学函数 |
| `time.h` | 时间处理 |
| `inttypes.h` | 整数格式化宏 (`PRId64` 等) |
| `stdbool.h` | 布尔类型 (`bool`, `true`, `false`) |

**说明：**
- `_GNU_SOURCE` 宏启用 GNU 扩展功能（如 `memmem` 函数在 Linux 上）
- XRT 零外部依赖，仅依赖 C 标准库

---

## 保护宏定义

### XXRTL_CORE

头文件保护宏，防止重复包含：

```c
#ifndef XXRTL_CORE
    #define XXRTL_CORE
    // ... 所有类型定义和函数声明 ...
#endif
```

---

## 基础类型定义

### 字符串类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `u8str` | `typedef unsigned char* u8str` | UTF-8 字符串指针 |
| `u16str` | `typedef unsigned short* u16str` | UTF-16 字符串指针 |
| `u32str` | `typedef unsigned int* u32str` | UTF-32 字符串指针 |
| `binary` | `typedef unsigned char* binary` | 二进制数据指针 |
| `str` | `typedef u8str str` | 默认字符串类型（UTF-8） |

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // UTF-8 字符串
    str text = (str)"Hello World";
    printf("UTF-8: %s\n", text);
    
    // 转换为 UTF-16
    u16str wtext = xrtUTF8to16(text, 0);
    printf("UTF-16 length: %zu\n", u16len(wtext));
    xrtFree(wtext);
    
    // 二进制数据
    binary data = (binary)"\x00\x01\x02\x03";
    printf("Binary first byte: 0x%02X\n", data[0]);
    
    xrtUnit();
    return 0;
}
```

---

### 整数类型

#### 8位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i8` | `typedef char i8` | -128 ~ 127 | 有符号8位整数 |
| `int8` | `typedef char int8` | -128 ~ 127 | 有符号8位整数（别名） |
| `u8` | `typedef unsigned char u8` | 0 ~ 255 | 无符号8位整数 |
| `uint8` | `typedef unsigned char uint8` | 0 ~ 255 | 无符号8位整数（别名） |

#### 16位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i16` | `typedef short i16` | -32,768 ~ 32,767 | 有符号16位整数 |
| `int16` | `typedef short int16` | -32,768 ~ 32,767 | 有符号16位整数（别名） |
| `u16` | `typedef unsigned short u16` | 0 ~ 65,535 | 无符号16位整数 |
| `uint16` | `typedef unsigned short uint16` | 0 ~ 65,535 | 无符号16位整数（别名） |

#### 32位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i32` | `typedef int i32` | -2^31 ~ 2^31-1 | 有符号32位整数 |
| `int32` | `typedef int int32` | -2^31 ~ 2^31-1 | 有符号32位整数（别名） |
| `u32` | `typedef unsigned int u32` | 0 ~ 2^32-1 | 无符号32位整数 |
| `uint32` | `typedef unsigned int uint32` | 0 ~ 2^32-1 | 无符号32位整数（别名） |
| `uint` | `typedef unsigned int uint` | 0 ~ 2^32-1 | 无符号整数（别名） |

#### 64位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i64` | `typedef long long i64` | -2^63 ~ 2^63-1 | 有符号64位整数 |
| `int64` | `typedef long long int64` | -2^63 ~ 2^63-1 | 有符号64位整数（别名） |
| `u64` | `typedef unsigned long long u64` | 0 ~ 2^64-1 | 无符号64位整数 |
| `uint64` | `typedef unsigned long long uint64` | 0 ~ 2^64-1 | 无符号64位整数（别名） |

#### 平台相关整数

| 类型 | 定义 | 说明 |
|------|------|------|
| `ulong` | `typedef unsigned long ulong` | 自动适配32/64位的无符号长整数 |

---

### 浮点数类型

| 类型 | 定义 | 精度 | 说明 |
|------|------|------|------|
| `f32` | `typedef float f32` | 单精度 | 32位浮点数 |
| `float32` | `typedef float float32` | 单精度 | 32位浮点数（别名） |
| `f64` | `typedef double f64` | 双精度 | 64位浮点数 |
| `float64` | `typedef double float64` | 双精度 | 64位浮点数（别名） |

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 单精度浮点数
    f32 pi = 3.14159f;
    float32 radius = 5.0f;
    printf("圆周长: %.4f\n", 2 * pi * radius);
    
    // 双精度浮点数
    f64 e = 2.718281828459045;
    float64 x = 10.0;
    printf("e^10 ≈ %.6f\n", pow(e, x));
    
    xrtUnit();
    return 0;
}
```

---

### 指针类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `ptr` | `typedef void* ptr` | 通用指针类型 |
| `intptr` | `typedef intptr_t intptr` | 指针大小的有符号整数 |
| `uintptr` | `typedef uintptr_t uintptr` | 指针大小的无符号整数 |

**使用场景：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 通用指针
    ptr data = xrtMalloc(1024);
    printf("分配内存地址: %p\n", data);
    
    // 指针转整数
    intptr addr = (intptr)data;
    printf("指针整数值: %" PRIdPTR "\n", addr);
    
    // 无符号指针整数
    uintptr uaddr = (uintptr)data;
    printf("无符号指针值: %" PRIuPTR "\n", uaddr);
    
    xrtFree(data);
    xrtUnit();
    return 0;
}
```

---

### 特殊类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `curr` | `typedef long long curr` | 货币类型（64位整数） |
| `xtime` | `typedef int64 xtime` | 时间类型（64位整数，秒为单位） |

---

## 常量定义

### 布尔常量

```c
#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef null
    #define null 0
#endif
```

**说明：**
- XRT 使用标准 C 的 `bool` 类型（需要 `<stdbool.h>`）
- `TRUE/FALSE` 用于兼容性
- `null` 作为空指针常量

### API 导出标记

```c
#ifdef BUILD_DLL
    #define XXAPI __declspec(dllexport)
#else
    #define XXAPI
#endif
```

**说明：**
- 编译 DLL 时定义 `BUILD_DLL` 宏
- `XXAPI` 标记所有导出函数

---

## 全局数据结构

### xrtGlobalData - 全局数据管理器

```c
typedef struct {
    // 初始化状态
    int bInit;                          // 是否已初始化
    
    // 全局常量数据
    str sNull;                          // 空字符串常量
    
    // 临时返回值存储
    str sRet;                           // 字符串返回值
    int64 iRet;                         // 整数返回值
    double nRet;                        // 浮点数返回值
    
    // 错误处理
    str LastError;                      // 最后错误信息
    int __pri_FreeError;                // 私有：错误信息是否需要释放
    void (*OnError)(str sError);        // 错误回调函数
    
    // 高精度时钟（仅Windows）
    #if defined(_WIN32) || defined(_WIN64)
        uint64 Frequency;               // 时钟频率
    #endif
    
    // 应用信息
    uint LocalAddr;                     // 本机IP地址（用于XID）
    str AppFile;                        // 应用程序完整路径
    str AppPath;                        // 应用程序目录
    
    // 环形临时内存（32个槽位循环使用）
    ptr TempMem[32];                    // 临时内存数组
    uint32 TempMemIdx;                  // 当前索引
    
    // 可自定义的内存函数
    ptr (*malloc)(size_t iSize);        // 内存分配函数
    ptr (*calloc)(size_t iNum, size_t iSize);  // 清零分配函数
    ptr (*realloc)(ptr pMem, size_t iSize);    // 重新分配函数
    void (*free)(ptr pMem);             // 释放函数
    
} xrtGlobalData;
```

### 全局实例

```c
XXAPI extern xrtGlobalData xCore;
```

**访问方式：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    // 初始化并获取指针
    xrtGlobalData* core = xrtInit();
    
    // 通过指针访问
    printf("App Path: %s\n", core->AppPath);
    printf("App File: %s\n", core->AppFile);
    
    // 或直接访问全局变量
    xCore.LastError = xCore.sNull;
    printf("Error: %s\n", xCore.LastError);
    
    xrtUnit();
    return 0;
}
```

---

## 字段详解

### 初始化状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `bInit` | `int` | 是否已初始化，TRUE表示已初始化 |

### 全局常量

| 字段 | 类型 | 说明 |
|------|------|------|
| `sNull` | `str` | 空字符串常量，用于比较和返回 |

### 临时返回值

某些函数使用全局返回值存储多个返回值：

| 字段 | 类型 | 说明 |
|------|------|------|
| `sRet` | `str` | 临时字符串返回值 |
| `iRet` | `int64` | 临时整数返回值 |
| `nRet` | `double` | 临时浮点数返回值 |

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 假设 xrtPathGetDir 返回路径并把长度存储在 xCore.iRet
    str dir = xrtPathGetDir((str)"C:\\test\\file.txt", 0);
    int64 len = xCore.iRet;  // 获取路径长度
    printf("目录: %s (长度: %" PRId64 ")\n", dir, len);
    xrtFree(dir);
    
    xrtUnit();
    return 0;
}
```

### 错误处理

| 字段 | 类型 | 说明 |
|------|------|------|
| `LastError` | `str` | 最后一次错误信息 |
| `OnError` | 函数指针 | 错误回调函数 |

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT ERROR] %s\n", sError);
}

int main() {
    xrtInit();
    
    // 设置错误回调
    xCore.OnError = MyErrorHandler;
    
    // 模拟错误
    xrtSetError((str)"Test error message", FALSE);
    
    // 检查错误
    if (xCore.LastError != xCore.sNull) {
        printf("当前错误: %s\n", xCore.LastError);
        xrtClearError();
    }
    
    xrtUnit();
    return 0;
}
```

### 应用信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `LocalAddr` | `uint` | 本机IP地址（用于生成XID） |
| `AppFile` | `str` | 当前程序完整路径 |
| `AppPath` | `str` | 当前程序所在目录 |

### 环形临时内存

XRT 提供32个临时内存槽位，循环使用：

| 字段 | 类型 | 说明 |
|------|------|------|
| `TempMem[32]` | `ptr` 数组 | 32个临时内存槽位 |
| `TempMemIdx` | `uint32` | 当前使用的槽位索引 |

**工作原理：**
1. 每次调用 `xrtTempMemory()` 使用下一个槽位
2. 如果槽位已有内存，先释放再分配
3. 循环到第32个后回到第1个
4. 无需手动释放，会自动管理

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 这些临时内存会自动管理，无需 xrtFree
    str temp1 = xrtTempMemory(100);
    strcpy((char*)temp1, "Temporary data 1");
    printf("temp1: %s\n", temp1);
    
    str temp2 = xrtTempMemory(200);
    strcpy((char*)temp2, "Temporary data 2");
    printf("temp2: %s\n", temp2);
    
    // 无需释放，会在循环32次后自动释放
    printf("当前临时内存索引: %u\n", xCore.TempMemIdx);
    
    xrtUnit();
    return 0;
}
```

### 自定义内存函数

可以替换默认的内存分配函数：

| 字段 | 类型 | 说明 |
|------|------|------|
| `malloc` | 函数指针 | 内存分配函数 |
| `calloc` | 函数指针 | 清零分配函数 |
| `realloc` | 函数指针 | 重新分配函数 |
| `free` | 函数指针 | 释放函数 |

**自定义示例：**
```c
#include "xrt.h"
#include <stdio.h>

static size_t g_allocCount = 0;
static size_t g_totalBytes = 0;

ptr MyMalloc(size_t size) {
    g_allocCount++;
    g_totalBytes += size;
    printf("[分配 #%zu] %zu 字节\n", g_allocCount, size);
    return malloc(size);
}

void MyFree(ptr pMem) {
    if (pMem) {
        printf("[释放] 地址 %p\n", pMem);
        free(pMem);
    }
}

int main() {
    xrtInit();
    
    // 使用自定义分配器
    xCore.malloc = MyMalloc;
    xCore.free = MyFree;
    
    // 现在所有 xrtMalloc/xrtFree 都会使用自定义函数
    ptr data = xrtMalloc(256);
    xrtFree(data);
    
    printf("\n统计: 分配 %zu 次, 共 %zu 字节\n", g_allocCount, g_totalBytes);
    
    xrtUnit();
    return 0;
}
```

---

## 平台检测宏

XRT 使用以下宏进行平台检测：

### 操作系统检测

```c
#if defined(_WIN32) || defined(_WIN64)
    // Windows 平台代码
#elif defined(__unix__) || defined(__unix) || defined(__linux__)
    // Linux/Unix 平台代码
#elif defined(__APPLE__) || defined(__MACH__)
    // macOS 平台代码
#elif defined(__ANDROID__)
    // Android 平台代码
#endif
```

### 编译器检测

```c
#if defined(__TINYC__)
    // TCC 编译器
#elif defined(__GNUC__)
    // GCC 编译器
#elif defined(__clang__)
    // Clang 编译器
#elif defined(_MSC_VER)
    // MSVC 编译器
#endif
```

### 架构检测

```c
#if defined(__x86_64__) || defined(_M_X64)
    // 64位架构
#elif defined(__i386__) || defined(_M_IX86)
    // 32位架构
#endif
```

---

## 最佳实践

### 1. 使用类型别名

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ 推荐：使用 XRT 类型别名
    i64 value = 1234567890123LL;
    str text = (str)"Hello XRT";
    u32 count = 100;
    f64 ratio = 3.14159265359;
    
    printf("整数: %" PRId64 "\n", value);
    printf("字符串: %s\n", text);
    printf("计数: %u\n", count);
    printf("比例: %.10f\n", ratio);
    
    // ❓ 不推荐（虽然可以）
    // long long value = 1234567890123LL;
    // unsigned char* text = "Hello";
    
    xrtUnit();
    return 0;
}
```

### 2. 检查初始化状态

```c
#include "xrt.h"
#include <stdio.h>

void EnsureInit() {
    if (!xCore.bInit) {
        printf("初始化 XRT 库...\n");
        xrtInit();
    } else {
        printf("XRT 已初始化\n");
    }
}

int main() {
    // 第一次调用会初始化
    EnsureInit();
    
    // 第二次调用不会重复初始化
    EnsureInit();
    
    xrtUnit();
    return 0;
}
```

### 3. 使用临时内存

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 对于临时使用的小内存，优先使用 xrtTempMemory
    str temp = xrtTempMemory(256);  // 自动管理
    sprintf((char*)temp, "临时数据: %d", 12345);
    printf("%s\n", temp);
    // 无需 xrtFree
    
    // 对于需要长期持有的内存，使用 xrtMalloc
    str permanent = xrtMalloc(1024);  // 需要 xrtFree
    strcpy((char*)permanent, "持久数据");
    printf("%s\n", permanent);
    xrtFree(permanent);  // 必须释放
    
    xrtUnit();
    return 0;
}
```

### 4. 错误处理

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
    // 可选：记录日志、发送通知等
}

bool DoSomething(int value) {
    if (value < 0) {
        xrtSetError((str)"参数不能为负数", FALSE);
        return false;
    }
    printf("处理值: %d\n", value);
    return true;
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // 测试正常情况
    if (DoSomething(10)) {
        printf("成功\n");
    }
    
    // 测试错误情况
    if (!DoSomething(-5)) {
        printf("失败: %s\n", xCore.LastError);
        xrtClearError();
    }
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Base 基础函数库](api-base.md) - 内存管理函数
- [Charset 字符集转换](api-charset.md) - 字符串类型转换
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#xrt-基础类型定义)

</div>
