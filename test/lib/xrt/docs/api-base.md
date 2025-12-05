# Base 基础函数库

> XRT 核心基础功能：库初始化、内存管理、错误处理

[English](api-base.en.md) | [中文](api-base.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [数据类型](#数据类型)
- [全局变量](#全局变量)
- [库初始化](#库初始化)
- [内存管理](#内存管理)
- [临时内存](#临时内存)
- [错误处理](#错误处理)
- [补充函数](#补充函数)

---

## 常量定义

### 布尔常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `TRUE` | `1` | 逻辑真值 |
| `FALSE` | `0` | 逻辑假值 |
| `null` | `0` | 空指针/空值 |

**说明：**
- 这些常量用于与 C 标准库的 `true`/`false` 区分
- `TRUE`/`FALSE` 返回值用于表示函数执行成功或失败
- `null` 等同于 `NULL`，用于空指针判断

### API 导出宏

| 宏 | 说明 |
|------|------|
| `XXAPI` | API 函数导出标识。编译为 DLL 时定义为 `__declspec(dllexport)`，否则为空 |

**定义：**
```c
#ifdef BUILD_DLL
    #define XXAPI   __declspec(dllexport)
#else
    #define XXAPI
#endif
```

---

## 数据类型

### xrtGlobalData

XRT 全局数据结构体，保存库的运行状态和配置。

**定义：**
```c
typedef struct {
    // 初始化状态
    int bInit;                          // 是否已初始化
    
    // 全局常量
    str sNull;                          // 空字符串常量（只读）
    
    // 临时返回值（用于多返回值场景）
    str sRet;                           // 字符串返回值
    int64 iRet;                         // 整数返回值
    double nRet;                        // 浮点数返回值
    
    // 错误处理
    str LastError;                      // 最后一次错误信息
    int __pri_FreeError;                // 内部：是否需要释放错误字符串
    void (*OnError)(str sError);        // 错误回调函数
    
    // 高精度时钟（仅Windows）
    #if defined(_WIN32) || defined(_WIN64)
        uint64 Frequency;               // 时钟频率
    #endif
    
    // 网络信息
    uint LocalAddr;                     // 本机IP地址（用于XID生成）
    
    // 应用信息
    str AppFile;                        // 应用程序完整路径
    str AppPath;                        // 应用程序目录
    
    // 环形临时内存
    ptr TempMem[32];                    // 32个临时内存槽位
    uint32 TempMemIdx;                  // 当前槽位索引
    
    // 内存分配函数（可自定义）
    ptr (*malloc)(size_t iSize);        // 分配内存
    ptr (*calloc)(size_t iNum, size_t iSize);  // 分配并清零
    ptr (*realloc)(ptr pMem, size_t iSize);    // 重新分配
    void (*free)(ptr pMem);             // 释放内存
    
} xrtGlobalData;
```

**成员说明：**

| 成员 | 类型 | 说明 |
|------|------|------|
| `bInit` | `int` | 初始化标志，`TRUE` 表示已初始化 |
| `sNull` | `str` | 全局空字符串，用于安全返回 |
| `sRet` | `str` | 临时字符串返回值存储 |
| `iRet` | `int64` | 临时整数返回值存储 |
| `nRet` | `double` | 临时浮点数返回值存储 |
| `LastError` | `str` | 最后一次错误信息 |
| `OnError` | 函数指针 | 错误发生时的回调函数 |
| `AppFile` | `str` | 当前程序的完整路径 |
| `AppPath` | `str` | 当前程序所在目录 |
| `TempMem` | `ptr[32]` | 32个临时内存槽位 |
| `malloc/calloc/realloc/free` | 函数指针 | 可自定义的内存分配函数 |

---

## 全局变量

### xCore

全局数据实例，在 `xrtInit()` 后可用。

**声明：**
```c
XXAPI extern xrtGlobalData xCore;
```

**使用示例：**
```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 访问应用路径
    printf("App Path: %s\n", xCore.AppPath);
    
    // 设置错误回调
    xCore.OnError = MyErrorHandler;
    
    // 检查错误
    if (xCore.LastError != xCore.sNull) {
        printf("Error: %s\n", xCore.LastError);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 库初始化

### xrtInit

初始化 XRT 库

**函数原型：**
```c
XXAPI xrtGlobalData* xrtInit();
```

**返回值：**
- 返回全局数据结构指针 `&xCore`

**说明：**
- **必须**在使用任何 XRT 功能前调用
- 多次调用是安全的（会检查初始化状态）
- 初始化内容：
  - 设置全局常量
  - 初始化随机数生成器
  - 获取应用程序路径
  - 初始化网络（Windows）
  - 获取本机IP地址

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
}

int main() {
    // 初始化库
    xrtGlobalData* pCore = xrtInit();
    
    // 设置错误回调（可选）
    pCore->OnError = MyErrorHandler;
    
    // 打印应用信息
    printf("App File: %s\n", pCore->AppFile);
    printf("App Path: %s\n", pCore->AppPath);
    
    // 使用库功能
    str text = xrtFormat("Hello, %s!", "World");
    printf("%s\n", text);
    xrtFree(text);
    
    // 清理资源
    xrtUnit();
    return 0;
}
```

---

### xrtUnit

释放 XRT 库资源

**函数原型：**
```c
XXAPI void xrtUnit();
```

**说明：**
- 释放应用程序路径
- 释放所有临时内存
- 清理网络资源（Windows）
- 程序退出前应调用

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 使用 XRT 功能
    str text = xrtCopyStr("Hello XRT", 0);
    printf("%s\n", text);
    xrtFree(text);
    
    // 程序结束前清理
    xrtUnit();
    return 0;
}
```

---

## 内存管理

### xrtMalloc

分配内存

**函数原型：**
```c
XXAPI ptr xrtMalloc(size_t iSize);
```

**参数：**
- `iSize` - 要分配的字节数

**返回值：**
- 成功：返回分配的内存指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 分配 1024 字节内存
    ptr buffer = xrtMalloc(1024);
    if (buffer) {
        // 使用内存
        memset(buffer, 0, 1024);
        strcpy((char*)buffer, "Hello Memory!");
        printf("%s\n", (char*)buffer);
        
        // 释放内存
        xrtFree(buffer);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtCalloc

分配并清零内存

**函数原型：**
```c
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize);
```

**参数：**
- `iNum` - 元素数量
- `iSize` - 每个元素的字节数

**返回值：**
- 成功：返回分配并清零的内存指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**说明：**
- 分配 `iNum * iSize` 字节内存
- 所有字节初始化为 0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 分配100个int32的数组，并初始化为0
    i32* numbers = (i32*)xrtCalloc(100, sizeof(i32));
    if (numbers) {
        // numbers[0] ~ numbers[99] 都是 0
        printf("Initial value: %d\n", numbers[50]);  // 输出: 0
        
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

重新分配内存

**函数原型：**
```c
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize);
```

**参数：**
- `pMem` - 原内存指针（可以为 `NULL`）
- `iSize` - 新的字节数

**返回值：**
- 成功：返回新内存指针（可能与原指针不同）
- 失败：返回 `NULL`（原内存保持不变）

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**说明：**
- 如果 `pMem` 为 `NULL`，等同于 `xrtMalloc(iSize)`
- 如果 `iSize` 为 0，等同于 `xrtFree(pMem)`
- 新内存可能在不同地址，原内存内容会被复制

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 初始分配
    str text = xrtMalloc(100);
    strcpy((char*)text, "Hello");
    printf("Before: %s\n", text);
    
    // 扩展到200字节
    text = xrtRealloc(text, 200);
    strcat((char*)text, " World!");
    printf("After: %s\n", text);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

**安全模式：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str old_text = xrtMalloc(100);
    str new_text = xrtRealloc(old_text, 200);
    if (new_text) {
        old_text = new_text;  // 更新指针
        printf("Realloc succeeded\n");
    } else {
        // 重新分配失败，old_text 仍然有效
        printf("Realloc failed\n");
    }
    xrtFree(old_text);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFree

释放内存

**函数原型：**
```c
XXAPI void xrtFree(ptr pmem);
```

**参数：**
- `pmem` - 要释放的内存指针

**说明：**
- 会先检查指针是否为 `NULL`
- 传入 `NULL` 是安全的（不会崩溃）
- 只能释放 `xrtMalloc/xrtCalloc/xrtRealloc` 分配的内存

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = xrtMalloc(256);
    xrtFree(text);       // 正常释放
    
    xrtFree(NULL);       // 安全，不会出错
    
    // 避免重复释放的推荐做法
    str data = xrtMalloc(100);
    if (data) {
        // 使用 data
        xrtFree(data);
        data = NULL;     // 推荐：释放后置空
    }
    
    xrtUnit();
    return 0;
}
```

---

## 临时内存

### xrtTempMemory

分配临时内存（自动管理）

**函数原型：**
```c
XXAPI ptr xrtTempMemory(size_t iSize);
```

**参数：**
- `iSize` - 要分配的字节数

**返回值：**
- 成功：返回临时内存指针
- 失败：返回 `NULL`

**内存释放：** ⭕ **无需手动释放**（自动管理）

**说明：**
- 使用环形缓冲区（32个槽位）
- 循环使用，第33次调用会自动释放第1次的内存
- 适用于临时、短期使用的小内存
- **不适用于**需要长期持有的数据

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 临时格式化字符串
    str temp1 = xrtTempMemory(100);
    sprintf((char*)temp1, "Number: %d", 123);
    printf("%s\n", temp1);  // 使用
    
    str temp2 = xrtTempMemory(200);
    sprintf((char*)temp2, "Value: %.2f", 3.14);
    printf("%s\n", temp2);
    
    // 无需调用 xrtFree
    
    xrtUnit();
    return 0;
}
```

**注意事项：**
```c
// ❌ 错误用法：返回临时内存
str GetTempString() {
    str temp = xrtTempMemory(100);
    strcpy((char*)temp, "data");
    return temp;  // 危险！可能在32次后被覆盖
}

// ✅ 正确用法：立即使用
void PrintFormat(int value) {
    str temp = xrtTempMemory(50);
    sprintf((char*)temp, "Value: %d", value);
    printf("%s\n", temp);  // 立即使用完毕
}
```

---

### xrtFreeTempMemory

释放所有临时内存

**函数原型：**
```c
XXAPI void xrtFreeTempMemory();
```

**说明：**
- 释放所有32个临时内存槽位
- 重置环形缓冲区索引
- 通常在 `xrtUnit()` 中自动调用
- 可在需要立即释放临时内存时手动调用

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    for (int i = 0; i < 100; i++) {
        str temp = xrtTempMemory(1024);
        sprintf((char*)temp, "Item %d", i);
        // 使用 temp
        
        if (i % 32 == 31) {
            xrtFreeTempMemory();  // 每32次清理一次，避免覆盖
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## 错误处理

### xrtSetError

设置错误信息（UTF-8字符串）

**函数原型：**
```c
XXAPI void xrtSetError(str sError, bool bFree);
```

**参数：**
- `sError` - 错误信息字符串
- `bFree` - 是否需要释放该字符串
  - `TRUE` - XRT会在设置新错误或清理时释放
  - `FALSE` - XRT不会释放（常量字符串）

**说明：**
- 设置 `xCore.LastError`
- 如果设置了 `xCore.OnError` 回调，会触发回调

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT ERROR] %s\n", sError);
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // 使用常量字符串（不释放）
    xrtSetError("File not found", FALSE);
    
    // 使用动态字符串（XRT会释放）
    int param = -1;
    str error = xrtFormat("Invalid parameter: %d", param);
    xrtSetError(error, TRUE);  // XRT会负责释放
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetErrorU16

设置错误信息（UTF-16字符串）

**函数原型：**
```c
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree);
```

**参数：**
- `sError` - UTF-16 错误信息
- `iSize` - 字符串长度（字符数，0表示自动计算）
- `bFree` - 是否需要释放

**说明：**
- 会自动转换为 UTF-8 后设置

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 设置 UTF-16 错误信息
    u16str error = (u16str)L"文件未找到";
    xrtSetErrorU16(error, 0, FALSE);
    
    // 错误信息已转换为 UTF-8
    printf("Error: %s\n", xCore.LastError);
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetErrorU32

设置错误信息（UTF-32字符串）

**函数原型：**
```c
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree);
```

**参数：**
- `sError` - UTF-32 错误信息
- `iSize` - 字符串长度（字符数，0表示自动计算）
- `bFree` - 是否需要释放

**说明：**
- 会自动转换为 UTF-8 后设置

---

### xrtClearError

清除错误信息

**函数原型：**
```c
XXAPI void xrtClearError();
```

**说明：**
- 将 `xCore.LastError` 设置为 `xCore.sNull`
- 如果之前的错误需要释放，会先释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 模拟一个错误
    xrtSetError("Something went wrong", FALSE);
    
    // 检查并处理错误
    if (xCore.LastError != xCore.sNull) {
        printf("Error: %s\n", xCore.LastError);
        xrtClearError();  // 清除错误
    }
    
    // 确认错误已清除
    if (xCore.LastError == xCore.sNull) {
        printf("Error cleared.\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## 补充函数

### memmem

在内存中查找子内存（仅 Windows）

**函数原型：**
```c
#if defined(_WIN32) || defined(_WIN64)
    XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
#endif
```

**参数：**
- `pMem` - 被搜索的内存
- `iMemSize` - 被搜索内存的大小
- `pSub` - 要查找的子内存
- `iSubSize` - 子内存的大小

**返回值：**
- 成功：返回找到的位置指针
- 失败：返回 `NULL`

**说明：**
- Linux/Unix 系统自带此函数
- Windows 系统由 XRT 提供实现

**示例：**
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
        printf("Found at offset: %zu\n", offset);  // 输出: 2
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

获取 UTF-16 字符串长度

**函数原型：**
```c
XXAPI size_t u16len(u16str sText);
```

**参数：**
- `sText` - UTF-16 字符串

**返回值：**
- 字符串长度（字符数，不包括结尾的 `\0`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u16str text = (u16str)L"Hello";
    size_t len = u16len(text);  // 返回 5
    printf("UTF-16 length: %zu\n", len);
    
    u16str chinese = (u16str)L"你好世界";
    size_t len2 = u16len(chinese);  // 返回 4
    printf("Chinese UTF-16 length: %zu\n", len2);
    
    xrtUnit();
    return 0;
}
```

---

### u32len

获取 UTF-32 字符串长度

**函数原型：**
```c
XXAPI size_t u32len(u32str sText);
```

**参数：**
- `sText` - UTF-32 字符串

**返回值：**
- 字符串长度（字符数，不包括结尾的 `\0`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u32str text = (u32str)U"Hello";
    size_t len = u32len(text);  // 返回 5
    printf("UTF-32 length: %zu\n", len);
    
    u32str emoji = (u32str)U"Hello🌍";
    size_t len2 = u32len(emoji);  // 返回 6（含emoji）
    printf("With emoji UTF-32 length: %zu\n", len2);
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 初始化与清理

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
}

int main() {
    // 1. 初始化
    xrtGlobalData* pCore = xrtInit();
    
    // 2. 设置错误处理
    pCore->OnError = MyErrorHandler;
    
    // 3. 使用功能
    printf("XRT initialized, app path: %s\n", pCore->AppPath);
    
    // 4. 清理
    xrtUnit();
    return 0;
}
```

### 2. 内存管理策略

```c
#include "xrt.h"
#include <stdio.h>

void UseIt(str s) { printf("%s\n", s); }
void LoadData(str s) { strcpy((char*)s, "Loaded data"); }

int main() {
    xrtInit();
    int value = 42;
    
    // 短期临时使用 → xrtTempMemory
    str temp = xrtTempMemory(256);
    sprintf((char*)temp, "Temp: %d", value);
    UseIt(temp);
    // 无需释放
    
    // 长期使用 → xrtMalloc + xrtFree
    str permanent = xrtMalloc(1024);
    LoadData(permanent);
    printf("%s\n", permanent);
    xrtFree(permanent);
    
    // 数组分配 → xrtCalloc（自动清零）
    i32* arr = (i32*)xrtCalloc(100, sizeof(i32));
    arr[0] = 1;
    arr[99] = 100;
    printf("arr[0]=%d, arr[99]=%d\n", arr[0], arr[99]);
    xrtFree(arr);
    
    xrtUnit();
    return 0;
}
```

### 3. 错误处理

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT Error] %s\n", sError);
    // 可选：写入日志文件
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
    
    // 调用函数
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

### 4. 安全的内存操作

```c
#include "xrt.h"
#include <stdio.h>

int SafeMemoryExample() {
    xrtInit();
    size_t size = 1024;
    size_t new_size = 2048;
    
    // ✅ 安全：检查返回值
    ptr data = xrtMalloc(size);
    if (data == NULL) {
        xrtSetError("Memory allocation failed", FALSE);
        xrtUnit();
        return -1;
    }
    printf("Allocated %zu bytes\n", size);
    
    // ✅ 安全：重新分配
    ptr new_data = xrtRealloc(data, new_size);
    if (new_data) {
        data = new_data;
        printf("Reallocated to %zu bytes\n", new_size);
    } else {
        // 重新分配失败，保留原数据
        printf("Realloc failed, keeping original\n");
    }
    
    // ✅ 安全：释放后置空
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

## 性能提示

### 临时内存性能

```c
#include "xrt.h"
#include <stdio.h>

void Process(str s) { /* 处理字符串 */ }

int main() {
    xrtInit();
    
    // ❌ 低效：频繁分配释放
    for (int i = 0; i < 100; i++) {
        str temp = xrtMalloc(256);
        sprintf((char*)temp, "Item %d", i);
        Process(temp);
        xrtFree(temp);
    }
    
    // ✅ 高效：使用临时内存（但注意32次限制）
    for (int i = 0; i < 30; i++) {  // 少于32次
        str temp = xrtTempMemory(256);
        sprintf((char*)temp, "Item %d", i);
        Process(temp);
    }
    
    // ✅ 最优：预分配
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

## 相关文档

- [类型定义](types.md) - 基础类型和全局结构
- [Charset 字符集转换](api-charset.md) - 字符串编码转换
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#base-基础函数库)

</div>
