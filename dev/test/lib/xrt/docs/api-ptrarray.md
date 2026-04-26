# PtrArray 指针数组库

> 动态指针数组，支持插入、删除、排序等操作

[English](api-ptrarray.en.md) | [中文](api-ptrarray.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [数据结构](#数据结构)
- [数组操作](#数组操作)
- [元素访问](#元素访问)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 常量定义

| 常量 | 值 | 说明 |
|------|-----|------|
| `XPARRAY_PREASSIGNSTEP` | `256` | 默认预分配步长（元素个数） |

### 索引规则

```
成员编号规则（0为不存在的成员编号）：
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

**重要：** 索引从 **1** 开始，不是从 0 开始！0 表示不存在的元素。

---

## 数据结构

### xparray_struct

指针数组管理器结构体。

**定义：**
```c
typedef struct {
    ptr* Memory;        // 指针数组内存
    uint32 Count;       // 当前元素数量
    uint32 AllocCount;  // 已分配容量
    uint32 AllocStep;   // 预分配步长
} xparray_struct, *xparray;
```

**成员说明：**
- `Memory` - 存储指针的数组
- `Count` - 当前元素数量（可直接读取）
- `AllocCount` - 已分配的最大容量
- `AllocStep` - 扩容时的增量

---

## 数组操作

### xrtPtrArrayCreate

创建指针数组管理器。

**函数原型：**
```c
XXAPI xparray xrtPtrArrayCreate();
```

**返回值：**
- 成功：指针数组对象
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtPtrArrayDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    printf("步长: %u\n", arr->AllocStep);  // 256
    
    xrtPtrArrayAppend(arr, (ptr)"item1");
    xrtPtrArrayAppend(arr, (ptr)"item2");
    printf("元素数: %u\n", arr->Count);  // 2
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayDestroy

销毁指针数组管理器。

**函数原型：**
```c
XXAPI void xrtPtrArrayDestroy(xparray pObject);
```

**说明：**
- 释放内部内存和管理器结构体
- 不会释放数组中存储的指针指向的内存

---

### xrtPtrArrayInit

初始化指针数组（用于栈上或嵌入式结构体）。

**函数原型：**
```c
XXAPI void xrtPtrArrayInit(xparray pObject);
```

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上分配
    xparray_struct arr;
    xrtPtrArrayInit(&arr);
    
    xrtPtrArrayAppend(&arr, (ptr)"data1");
    xrtPtrArrayAppend(&arr, (ptr)"data2");
    printf("元素数: %u\n", arr.Count);
    
    xrtPtrArrayUnit(&arr);  // 释放内部内存
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayUnit

释放数组内部内存（不释放结构体）。

**函数原型：**
```c
XXAPI void xrtPtrArrayUnit(xparray pObject);
```

**别名：**
- `xrtPtrArrayRemoveAll` - 删除所有成员
- `xrtPtrArrayClear` - 清空管理器

---

### xrtPtrArrayMalloc

预分配或调整数组容量。

**函数原型：**
```c
XXAPI bool xrtPtrArrayMalloc(xparray pObject, uint32 iCount);
```

**参数：**
- `pObject` - 数组对象
- `iCount` - 目标容量（元素个数）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    // 预分配 1000 个元素的容量
    xrtPtrArrayMalloc(arr, 1000);
    printf("已分配: %u\n", arr->AllocCount);  // 1000
    
    // 添加元素不会触发重新分配
    for (int i = 0; i < 500; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    printf("元素数: %u\n", arr->Count);  // 500
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayInsert

在指定位置插入元素。

**函数原型：**
```c
XXAPI uint32 xrtPtrArrayInsert(xparray pObject, uint32 iPos, ptr pVal);
```

**参数：**
- `pObject` - 数组对象
- `iPos` - 插入位置（0 = 头部插入，Count = 末尾插入）
- `pVal` - 要插入的指针

**返回值：**
- 成功：新元素的索引（1-based）
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    xrtPtrArrayAppend(arr, (ptr)"B");
    xrtPtrArrayAppend(arr, (ptr)"D");
    
    // 在头部插入
    xrtPtrArrayInsert(arr, 0, (ptr)"A");
    
    // 在中间插入（位置2后面）
    xrtPtrArrayInsert(arr, 2, (ptr)"C");
    
    // 打印: A B C D
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%s ", (char*)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayAppend

在数组末尾追加元素。

**函数原型：**
```c
XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal);
```

**参数：**
- `pObject` - 数组对象
- `pVal` - 要追加的指针

**返回值：**
- 成功：新元素的索引（1-based）
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    uint32 idx1 = xrtPtrArrayAppend(arr, (ptr)"first");
    uint32 idx2 = xrtPtrArrayAppend(arr, (ptr)"second");
    uint32 idx3 = xrtPtrArrayAppend(arr, (ptr)"third");
    
    printf("索引: %u, %u, %u\n", idx1, idx2, idx3);  // 1, 2, 3
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayAddAlt

添加元素，自动查找空隙（替换 NULL 值）。

**函数原型：**
```c
XXAPI uint32 xrtPtrArrayAddAlt(xparray pObject, ptr pVal);
```

**参数：**
- `pObject` - 数组对象
- `pVal` - 要添加的指针

**返回值：**
- 成功：元素索引（1-based）
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    xrtPtrArrayAppend(arr, (ptr)"A");  // 索引 1
    xrtPtrArrayAppend(arr, (ptr)"B");  // 索引 2
    xrtPtrArrayAppend(arr, (ptr)"C");  // 索引 3
    
    // 将索引 2 设为 NULL
    xrtPtrArraySet(arr, 2, NULL);
    
    // AddAlt 会找到空隙并填充
    uint32 idx = xrtPtrArrayAddAlt(arr, (ptr)"D");
    printf("填充到索引: %u\n", idx);  // 2
    
    // 打印: A D C
    for (uint32 i = 1; i <= arr->Count; i++) {
        ptr p = xrtPtrArrayGet(arr, i);
        printf("%s ", p ? (char*)p : "(null)");
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 优先复用已删除的槽位（设为 NULL 的位置）
- 无空隙时在末尾追加

---

### xrtPtrArraySwap

交换两个元素的位置。

**函数原型：**
```c
XXAPI bool xrtPtrArraySwap(xparray pObject, uint32 iPosA, uint32 iPosB);
```

**参数：**
- `pObject` - 数组对象
- `iPosA` - 第一个元素索引（1-based）
- `iPosB` - 第二个元素索引（1-based）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（索引越界）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"A");
    xrtPtrArrayAppend(arr, (ptr)"B");
    xrtPtrArrayAppend(arr, (ptr)"C");
    
    // 交换第1和第3个元素
    xrtPtrArraySwap(arr, 1, 3);
    
    // 打印: C B A
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%s ", (char*)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayRemove

删除元素。

**函数原型：**
```c
XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount);
```

**参数：**
- `pObject` - 数组对象
- `iPos` - 起始索引（1-based）
- `iCount` - 删除数量

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（索引越界或数量为 0）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    for (int i = 0; i < 5; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)(i + 1));
    }
    printf("删除前: %u 个元素\n", arr->Count);  // 5
    
    // 删除第2个开始的2个元素
    xrtPtrArrayRemove(arr, 2, 2);
    printf("删除后: %u 个元素\n", arr->Count);  // 3
    
    // 剩余: 1, 4, 5
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%zu ", (size_t)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArraySort

对数组元素进行排序。

**函数原型：**
```c
XXAPI bool xrtPtrArraySort(xparray pObject, ptr procCompar);
```

**参数：**
- `pObject` - 数组对象
- `procCompar` - 比较函数指针（qsort 格式）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// 字符串比较函数
int CompareStr(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    return strcmp(sa, sb);
}

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"cherry");
    xrtPtrArrayAppend(arr, (ptr)"apple");
    xrtPtrArrayAppend(arr, (ptr)"banana");
    
    // 排序
    xrtPtrArraySort(arr, (ptr)CompareStr);
    
    // 打印: apple banana cherry
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%s ", (char*)xrtPtrArrayGet(arr, i));
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## 元素访问

### xrtPtrArrayGet

获取元素指针（安全版）。

**函数原型：**
```c
XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos);
```

**参数：**
- `pObject` - 数组对象
- `iPos` - 元素索引（1-based）

**返回值：**
- 成功：元素指针
- 失败：`NULL`（索引越界）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"first");
    xrtPtrArrayAppend(arr, (ptr)"second");
    
    // 注意：索引从 1 开始
    ptr item1 = xrtPtrArrayGet(arr, 1);
    ptr item2 = xrtPtrArrayGet(arr, 2);
    ptr item3 = xrtPtrArrayGet(arr, 3);  // 越界，返回 NULL
    
    printf("1: %s\n", item1 ? (char*)item1 : "(null)");
    printf("2: %s\n", item2 ? (char*)item2 : "(null)");
    printf("3: %s\n", item3 ? "exists" : "(null)");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArrayGet_Unsafe / xrtPtrArrayGet_Inline

获取元素指针（不安全/内联版）。

**函数原型：**
```c
XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos);
static inline ptr xrtPtrArrayGet_Inline(xparray pObject, uint32 iPos);
```

**说明：**
- 不进行边界检查，性能更高
- 调用者必须确保索引有效

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    for (int i = 0; i < 1000; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    
    // 高性能遍历（确保索引有效）
    size_t sum = 0;
    for (uint32 i = 1; i <= arr->Count; i++) {
        sum += (size_t)xrtPtrArrayGet_Inline(arr, i);
    }
    printf("总和: %zu\n", sum);  // 499500
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArraySet

设置元素指针（安全版）。

**函数原型：**
```c
XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal);
```

**参数：**
- `pObject` - 数组对象
- `iPos` - 元素索引（1-based）
- `pVal` - 新指针值

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（索引越界）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"old1");
    xrtPtrArrayAppend(arr, (ptr)"old2");
    
    // 修改第二个元素
    xrtPtrArraySet(arr, 2, (ptr)"new2");
    
    printf("%s %s\n", 
        (char*)xrtPtrArrayGet(arr, 1),
        (char*)xrtPtrArrayGet(arr, 2));  // old1 new2
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtPtrArraySet_Unsafe / xrtPtrArraySet_Inline

设置元素指针（不安全/内联版）。

**函数原型：**
```c
XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal);
static inline void xrtPtrArraySet_Inline(xparray pObject, uint32 iPos, ptr pVal);
```

**说明：**
- 不进行边界检查，性能更高
- 调用者必须确保索引有效

---

### 直接访问结构体

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    xrtPtrArrayAppend(arr, (ptr)"a");
    xrtPtrArrayAppend(arr, (ptr)"b");
    
    // 直接访问结构体字段
    uint32 count = arr->Count;        // 元素数量
    uint32 cap = arr->AllocCount;     // 已分配容量
    ptr* memory = arr->Memory;        // 内部指针数组
    
    printf("元素数: %u, 容量: %u\n", count, cap);
    
    // 直接访问内部数组（索引从 0 开始）
    for (uint32 i = 0; i < count; i++) {
        printf("%s ", (char*)memory[i]);
    }
    printf("\n");
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 对象集合管理

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    str name;
} User;

User* CreateUser(int id, str name) {
    User* u = xrtMalloc(sizeof(User));
    u->id = id;
    u->name = xrtCopyStr(name, 0);
    return u;
}

void FreeUser(User* u) {
    if (u) {
        xrtFree(u->name);
        xrtFree(u);
    }
}

int main() {
    xrtInit();
    
    xparray users = xrtPtrArrayCreate();
    
    // 添加用户
    xrtPtrArrayAppend(users, CreateUser(1, (str)"Alice"));
    xrtPtrArrayAppend(users, CreateUser(2, (str)"Bob"));
    xrtPtrArrayAppend(users, CreateUser(3, (str)"Charlie"));
    
    // 遍历
    for (uint32 i = 1; i <= users->Count; i++) {
        User* u = xrtPtrArrayGet(users, i);
        printf("User %d: %s\n", u->id, u->name);
    }
    
    // 释放所有用户
    for (uint32 i = 1; i <= users->Count; i++) {
        FreeUser(xrtPtrArrayGet(users, i));
    }
    xrtPtrArrayDestroy(users);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 字符串列表

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray lines = xrtPtrArrayCreate();
    
    // 添加字符串（复制）
    xrtPtrArrayAppend(lines, xrtCopyStr((str)"Line 1", 0));
    xrtPtrArrayAppend(lines, xrtCopyStr((str)"Line 2", 0));
    xrtPtrArrayAppend(lines, xrtCopyStr((str)"Line 3", 0));
    
    // 打印
    for (uint32 i = 1; i <= lines->Count; i++) {
        printf("%u: %s\n", i, (char*)xrtPtrArrayGet(lines, i));
    }
    
    // 释放字符串
    for (uint32 i = 1; i <= lines->Count; i++) {
        xrtFree(xrtPtrArrayGet(lines, i));
    }
    xrtPtrArrayDestroy(lines);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 对象池（使用 AddAlt）

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    bool active;
} Connection;

int main() {
    xrtInit();
    
    xparray pool = xrtPtrArrayCreate();
    
    // 创建连接
    for (int i = 0; i < 5; i++) {
        Connection* conn = xrtMalloc(sizeof(Connection));
        conn->id = i;
        conn->active = TRUE;
        xrtPtrArrayAppend(pool, conn);
    }
    printf("初始连接数: %u\n", pool->Count);  // 5
    
    // “删除”连接 2 和 4（置为 NULL）
    xrtFree(xrtPtrArrayGet(pool, 2));
    xrtPtrArraySet(pool, 2, NULL);
    xrtFree(xrtPtrArrayGet(pool, 4));
    xrtPtrArraySet(pool, 4, NULL);
    
    // 添加新连接（会复用空槽）
    Connection* newConn = xrtMalloc(sizeof(Connection));
    newConn->id = 100;
    newConn->active = TRUE;
    uint32 idx = xrtPtrArrayAddAlt(pool, newConn);
    printf("新连接位置: %u\n", idx);  // 2（复用了空槽）
    
    // 清理
    for (uint32 i = 1; i <= pool->Count; i++) {
        ptr p = xrtPtrArrayGet(pool, i);
        if (p) xrtFree(p);
    }
    xrtPtrArrayDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 预分配容量

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    // 预分配 10000 个元素的容量
    xrtPtrArrayMalloc(arr, 10000);
    
    // 添加元素不会触发重新分配
    for (int i = 0; i < 10000; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    
    printf("元素数: %u\n", arr->Count);
    printf("容量: %u\n", arr->AllocCount);
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### 2. 正确释放对象内存

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    
    // 添加动态分配的字符串
    xrtPtrArrayAppend(arr, xrtCopyStr((str)"str1", 0));
    xrtPtrArrayAppend(arr, xrtCopyStr((str)"str2", 0));
    
    // ✗ 错误: 直接销毁数组会泄漏字符串内存
    // xrtPtrArrayDestroy(arr);
    
    // ✓ 正确: 先释放每个元素
    for (uint32 i = 1; i <= arr->Count; i++) {
        xrtFree(xrtPtrArrayGet(arr, i));
    }
    xrtPtrArrayDestroy(arr);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 使用 Inline 版本提高性能

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xparray arr = xrtPtrArrayCreate();
    for (int i = 0; i < 1000000; i++) {
        xrtPtrArrayAppend(arr, (ptr)(size_t)i);
    }
    
    // 高性能遍历（已知索引有效）
    size_t sum = 0;
    for (uint32 i = 1; i <= arr->Count; i++) {
        sum += (size_t)xrtPtrArrayGet_Inline(arr, i);
    }
    printf("总和: %zu\n", sum);
    
    xrtPtrArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Array 结构体数组](api-array.md)
- [List 列表](api-list.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#ptrarray-指针数组库)

</div>
