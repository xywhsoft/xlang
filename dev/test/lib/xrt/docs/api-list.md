# List 列表库

> 基于 AVL 树的整数键列表，支持任意 int64 索引

[English](api-json.en.md) | [中文](api-json.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [回调函数类型](#回调函数类型)
- [列表操作](#列表操作)
- [元素操作](#元素操作)
- [遍历](#遍历)
- [使用场景](#使用场景)
- [与其他数据结构对比](#与其他数据结构对比)
- [最佳实践](#最佳实践)

---

## 概述

List 是基于 AVL 树实现的整数键列表，使用 `int64` 作为键，支持任意整数索引（包括负数和跳跃索引）。

### 核心特点

- **AVL 树实现**：保证 O(log n) 的查找、插入、删除性能
- **int64 键**：支持任意整数索引（正/负/跳跃）
- **稀疏存储**：只存储实际使用的索引
- **自动排序**：遍历时按键值升序

### 架构图

```
List 结构：
┌────────────────────────────────────────┐
│              xlist_struct              │
├────────────────────────────────────────┤
│ AVLT: xavltree_struct                  │
│   ┌─────────────────────────────────┐  │
│   │ RootNode → AVL 树              │  │
│   │ 节点: [int64 key][user data]   │  │
│   └─────────────────────────────────┘  │
└────────────────────────────────────────┘
```

### 与普通数组的区别

| 特性 | List | 普通数组 |
|------|------|----------|
| **索引类型** | int64（任意整数） | uint32（连续） |
| **负索引** | ✅ 支持 | ❌ 不支持 |
| **稀疏索引** | ✅ 高效 | ❌ 浪费内存 |
| **查找性能** | O(log n) | O(1) |
| **内存占用** | 仅存储使用的索引 | 预分配连续空间 |

---

## 数据结构

### xlist_struct

列表结构体。

**定义：**
```c
typedef struct {
    xavltree_struct AVLT;    // 内部 AVL 树
} xlist_struct, *xlist;
```

**说明：**
- 内部使用 AVL 树存储键值对
- 每个节点包含 `int64` 键和用户数据
- 自动管理节点内存（使用 FSMemPool）

---

## 回调函数类型

### List_EachProc

列表遍历回调函数。

**定义：**
```c
typedef bool (*List_EachProc)(int64 pKey, ptr pVal, ptr pArg);
```

**参数：**
- `pKey` - 当前元素的键（int64）
- `pVal` - 当前元素的值指针
- `pArg` - 用户自定义参数

**返回值：**
- `TRUE` - 继续遍历
- `FALSE` - 中断遍历

**示例：**
```c
bool PrintItem(int64 key, ptr pVal, ptr pArg) {
    int* val = (int*)pVal;
    printf("Key: %" PRId64 ", Value: %d\n", key, *val);
    return TRUE;  // 继续遍历
}
```

---

## 列表操作

### xrtListCreate

创建列表。

**函数原型：**
```c
XXAPI xlist xrtListCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 值的大小（字节）

**返回值：**
- 成功：列表对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtListDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建存储 int 的列表
    xlist list = xrtListCreate(sizeof(int));
    
    // 设置元素
    bool isNew;
    int* val = (int*)xrtListSet(list, 0, &isNew);
    *val = 100;
    
    printf("值: %d\n", *(int*)xrtListGet(list, 0));  // 100
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListDestroy

释放列表。

**函数原型：**
```c
XXAPI void xrtListDestroy(xlist objList);
```

**参数：**
- `objList` - 列表对象

---

### xrtListInit

初始化列表（用于栈上或嵌入式结构）。

**函数原型：**
```c
XXAPI void xrtListInit(xlist objList, uint32 iItemLength);
```

**参数：**
- `objList` - 预分配的列表结构指针
- `iItemLength` - 值的大小（字节）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上分配
    xlist_struct listData;
    xlist list = &listData;
    xrtListInit(list, sizeof(double));
    
    // 使用列表
    bool isNew;
    double* val = (double*)xrtListSet(list, 1, &isNew);
    *val = 3.14159;
    
    printf("值: %f\n", *(double*)xrtListGet(list, 1));
    
    // 必须调用 Unit 释放内部资源
    xrtListUnit(list);
    
    xrtUnit();
    return 0;
}
```

---

### xrtListUnit

释放列表内部资源（不释放结构体本身）。

**函数原型：**
```c
XXAPI void xrtListUnit(xlist objList);
```

**别名：**
```c
#define xrtListRemoveAll xrtListUnit
#define xrtListClear     xrtListUnit
```

---

## 元素操作

## 元素操作

### xrtListSet

设置元素（返回值指针）。

**函数原型：**
```c
XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet);
```

**参数：**
- `objList` - 列表对象
- `iKey` - 键（int64 类型，可为负数）
- `bNewRet` - 输出：是否为新键（可为 NULL）

**返回值：**
- 成功：值存储位置的指针
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    // 插入新键
    bool isNew;
    int* val1 = (int*)xrtListSet(list, 0, &isNew);
    *val1 = 100;
    printf("键 0: isNew=%d, value=%d\n", isNew, *val1);  // isNew=1, value=100
    
    // 更新现有键
    int* val2 = (int*)xrtListSet(list, 0, &isNew);
    *val2 = 200;
    printf("键 0: isNew=%d, value=%d\n", isNew, *val2);  // isNew=0, value=200
    
    // 支持负索引
    int* val3 = (int*)xrtListSet(list, -5, &isNew);
    *val3 = 500;
    printf("键 -5: value=%d\n", *val3);  // value=500
    
    // 支持跳跃索引
    int* val4 = (int*)xrtListSet(list, 1000, &isNew);
    *val4 = 1000;
    printf("键 1000: value=%d\n", *val4);  // value=1000
    
    printf("元素数量: %u\n", xrtListCount(list));  // 4
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListSetPtr

设置元素（直接设置指针值）。

**函数原型：**
```c
XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal);
```

**参数：**
- `objList` - 列表对象
- `iKey` - 键
- `pVal` - 要设置的指针值
- `ppOldVal` - 输出：旧值指针（可为 NULL）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(ptr));
    
    // 设置指针值
    ptr oldVal;
    xrtListSetPtr(list, 1, (ptr)"Hello", &oldVal);
    printf("旧值: %s\n", oldVal ? (char*)oldVal : "(null)");  // (null)
    
    // 更新指针值
    xrtListSetPtr(list, 1, (ptr)"World", &oldVal);
    printf("旧值: %s\n", (char*)oldVal);  // Hello
    
    // 获取当前值
    printf("当前值: %s\n", (char*)xrtListGetPtr(list, 1));  // World
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListGet

获取元素（返回值指针）。

**函数原型：**
```c
XXAPI ptr xrtListGet(xlist objList, int64 iKey);
```

**参数：**
- `objList` - 列表对象
- `iKey` - 键

**返回值：**
- 存在：值指针
- 不存在：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    // 设置元素
    bool isNew;
    *(int*)xrtListSet(list, 10, &isNew) = 100;
    *(int*)xrtListSet(list, 20, &isNew) = 200;
    
    // 获取存在的键
    int* val1 = (int*)xrtListGet(list, 10);
    if (val1) {
        printf("键 10: %d\n", *val1);  // 100
    }
    
    // 获取不存在的键
    int* val2 = (int*)xrtListGet(list, 15);
    if (val2 == NULL) {
        printf("键 15 不存在\n");
    }
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListGetPtr

获取元素（直接返回存储的指针值）。

**函数原型：**
```c
XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey);
```

**返回值：**
- 存在：存储的指针值
- 不存在：`NULL`

---

### xrtListRemove

删除元素。

**函数原型：**
```c
XXAPI bool xrtListRemove(xlist objList, int64 iKey);
```

**返回值：**
- `TRUE` - 删除成功
- `FALSE` - 键不存在

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    // 添加元素
    bool isNew;
    *(int*)xrtListSet(list, 1, &isNew) = 10;
    *(int*)xrtListSet(list, 2, &isNew) = 20;
    *(int*)xrtListSet(list, 3, &isNew) = 30;
    
    printf("删除前: %u 个元素\n", xrtListCount(list));  // 3
    
    // 删除存在的键
    bool result1 = xrtListRemove(list, 2);
    printf("删除键 2: %s\n", result1 ? "成功" : "失败");  // 成功
    
    // 删除不存在的键
    bool result2 = xrtListRemove(list, 100);
    printf("删除键 100: %s\n", result2 ? "成功" : "失败");  // 失败
    
    printf("删除后: %u 个元素\n", xrtListCount(list));  // 2
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListRemovePtr

删除元素并返回存储的指针值。

**函数原型：**
```c
XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey);
```

**返回值：**
- 成功：删除的指针值
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(ptr));
    
    // 设置指针值
    str text = xrtCopyStr((str)"Hello World", 0);
    xrtListSetPtr(list, 1, text, NULL);
    
    // 删除并获取指针
    ptr removed = xrtListRemovePtr(list, 1);
    if (removed) {
        printf("删除的值: %s\n", (char*)removed);
        xrtFree(removed);  // 手动释放
    }
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListExists

判断键是否存在。

**函数原型：**
```c
XXAPI bool xrtListExists(xlist objList, int64 iKey);
```

**返回值：**
- `TRUE` - 键存在
- `FALSE` - 键不存在

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 5, &isNew) = 50;
    
    printf("键 5 存在: %s\n", xrtListExists(list, 5) ? "是" : "否");   // 是
    printf("键 10 存在: %s\n", xrtListExists(list, 10) ? "是" : "否"); // 否
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## 遍历

### xrtListCount

获取元素数量。

**函数原型：**
```c
XXAPI uint32 xrtListCount(xlist objList);
```

**返回值：**
- 列表中的元素数量

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, -10, &isNew) = 1;
    *(int*)xrtListSet(list, 0, &isNew) = 2;
    *(int*)xrtListSet(list, 100, &isNew) = 3;
    
    printf("元素数量: %u\n", xrtListCount(list));  // 3
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtListWalk

遍历列表中的所有元素（按键升序）。

**函数原型：**
```c
XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg);
```

**参数：**
- `objList` - 列表对象
- `procEach` - 遍历回调函数
- `pArg` - 传递给回调的自定义参数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

bool PrintItem(int64 key, ptr pVal, ptr pArg) {
    int* sum = (int*)pArg;
    int val = *(int*)pVal;
    printf("Key: %" PRId64 ", Value: %d\n", key, val);
    *sum += val;
    return TRUE;  // 继续遍历
}

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 10, &isNew) = 100;
    *(int*)xrtListSet(list, -5, &isNew) = 50;
    *(int*)xrtListSet(list, 20, &isNew) = 200;
    *(int*)xrtListSet(list, 0, &isNew) = 75;
    
    int sum = 0;
    printf("按键升序遍历：\n");
    xrtListWalk(list, PrintItem, &sum);
    // 输出顺序: -5, 0, 10, 20
    
    printf("总和: %d\n", sum);  // 425
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 遍历顺序为键的升序（中序遍历 AVL 树）
- 回调返回 `FALSE` 可中断遍历

---

## 使用场景

### 1. 稀疏数组

```c
#include "xrt.h"
#include <stdio.h>

// 稀疏数组：只有部分索引有值
int main() {
    xrtInit();
    
    xlist sparse = xrtListCreate(sizeof(double));
    
    // 设置分散的索引
    bool isNew;
    *(double*)xrtListSet(sparse, 0, &isNew) = 1.0;
    *(double*)xrtListSet(sparse, 1000000, &isNew) = 2.0;
    *(double*)xrtListSet(sparse, -500000, &isNew) = 3.0;
    
    // 只占用 3 个节点的内存，而不是 1500001 个
    printf("元素数: %u\n", xrtListCount(sparse));  // 3
    
    // 获取值
    double* val = (double*)xrtListGet(sparse, 1000000);
    if (val) {
        printf("索引 1000000: %f\n", *val);  // 2.0
    }
    
    xrtListDestroy(sparse);
    xrtUnit();
    return 0;
}
```

---

### 2. 动态对象集合

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str name;
    int age;
} Person;

Person* CreatePerson(str name, int age) {
    Person* p = xrtMalloc(sizeof(Person));
    p->name = xrtCopyStr(name, 0);
    p->age = age;
    return p;
}

void FreePerson(Person* p) {
    if (p) {
        xrtFree(p->name);
        xrtFree(p);
    }
}

bool PrintPerson(int64 key, ptr pVal, ptr pArg) {
    Person* p = (Person*)xrtListGetPtr((xlist)pArg, key);
    printf("ID %" PRId64 ": %s, %d\n", key, p->name, p->age);
    return TRUE;
}

int main() {
    xrtInit();
    
    xlist people = xrtListCreate(sizeof(ptr));
    
    // 使用 ID 作为键
    xrtListSetPtr(people, 1001, CreatePerson((str)"Alice", 25), NULL);
    xrtListSetPtr(people, 1002, CreatePerson((str)"Bob", 30), NULL);
    xrtListSetPtr(people, 1003, CreatePerson((str)"Charlie", 35), NULL);
    
    // 遍历
    printf("所有人员：\n");
    xrtListWalk(people, PrintPerson, people);
    
    // 清理：删除并释放各个对象
    ptr removed;
    removed = xrtListRemovePtr(people, 1001);
    FreePerson((Person*)removed);
    removed = xrtListRemovePtr(people, 1002);
    FreePerson((Person*)removed);
    removed = xrtListRemovePtr(people, 1003);
    FreePerson((Person*)removed);
    
    xrtListDestroy(people);
    xrtUnit();
    return 0;
}
```

---

### 3. 双向映射（键↔值）

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // ID → 名称
    xlist idToName = xrtListCreate(sizeof(ptr));
    // 名称 → ID（使用 Dict）
    xdict nameToId = xrtDictCreate(sizeof(int64));
    
    // 添加映射
    int64 id = 1001;
    str name = (str)"Alice";
    
    xrtListSetPtr(idToName, id, name, NULL);
    bool isNew;
    *(int64*)xrtDictSet(nameToId, name, strlen((char*)name), &isNew) = id;
    
    // 正向查找：ID → 名称
    printf("ID 1001 的名称: %s\n", (char*)xrtListGetPtr(idToName, 1001));
    
    // 反向查找：名称 → ID
    int64* foundId = (int64*)xrtDictGet(nameToId, (ptr)"Alice", 5);
    if (foundId) {
        printf("Alice 的 ID: %" PRId64 "\n", *foundId);
    }
    
    xrtListDestroy(idToName);
    xrtDictDestroy(nameToId);
    xrtUnit();
    return 0;
}
```

---

## 与其他数据结构对比

| 特性 | List | Array | PtrArray | Dict |
|------|------|-------|----------|------|
| **键类型** | int64 | uint32 (1-based) | uint32 (1-based) | 任意二进制 |
| **负索引** | ✅ | ❌ | ❌ | - |
| **稀疏索引** | ✅ | ❌ | ❌ | ✅ |
| **排序遍历** | ✅ 按键升序 | ✅ 按索引 | ✅ 按索引 | ❌ |
| **查找性能** | O(log n) | O(1) | O(1) | O(log n) |
| **内存效率** | 仅存储使用的键 | 连续内存 | 连续内存 | 仅存储使用的键 |
| **适用场景** | 整数键稀疏存储 | 连续索引存储 | 指针集合 | 字符串键存储 |

### 选择建议

- **使用 List**：需要整数键、负索引、稀疏索引
- **使用 Array**：连续索引、高频随机访问
- **使用 Dict**：字符串或复杂键

---

## 最佳实践

### 1. 检查键是否存在

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 10, &isNew) = 100;
    
    // ✗ 不推荐：使用 Get 检查存在性
    if (xrtListGet(list, 10) != NULL) {
        // ...
    }
    
    // ✓ 推荐：使用 Exists
    if (xrtListExists(list, 10)) {
        printf("键 10 存在\n");
    }
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 2. 指针值内存管理

```c
#include "xrt.h"
#include <stdio.h>

bool FreeStrings(int64 key, ptr pVal, ptr pArg) {
    xlist list = (xlist)pArg;
    ptr strVal = xrtListGetPtr(list, key);
    if (strVal) {
        xrtFree(strVal);
    }
    return TRUE;
}

int main() {
    xrtInit();
    
    xlist strings = xrtListCreate(sizeof(ptr));
    
    // 存储动态分配的字符串
    xrtListSetPtr(strings, 1, xrtCopyStr((str)"Hello", 0), NULL);
    xrtListSetPtr(strings, 2, xrtCopyStr((str)"World", 0), NULL);
    
    // 清理前释放所有字符串
    xrtListWalk(strings, FreeStrings, strings);
    
    xrtListDestroy(strings);
    xrtUnit();
    return 0;
}
```

---

### 3. 安全的中断遍历

```c
#include "xrt.h"
#include <stdio.h>

bool FindFirst(int64 key, ptr pVal, ptr pArg) {
    int target = *(int*)pArg;
    int current = *(int*)pVal;
    
    if (current == target) {
        printf("找到目标 %d 在键 %" PRId64 "\n", target, key);
        return FALSE;  // 中断遍历
    }
    return TRUE;  // 继续遍历
}

int main() {
    xrtInit();
    
    xlist list = xrtListCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtListSet(list, 1, &isNew) = 10;
    *(int*)xrtListSet(list, 2, &isNew) = 20;
    *(int*)xrtListSet(list, 3, &isNew) = 30;
    *(int*)xrtListSet(list, 4, &isNew) = 20;
    
    // 查找第一个值为 20 的元素
    int target = 20;
    xrtListWalk(list, FindFirst, &target);
    // 输出: 找到目标 20 在键 2
    
    xrtListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Array 结构体数组](api-array.md)
- [PtrArray 指针数组](api-ptrarray.md)
- [Dict 字典](api-dict.md)
- [AVLTree 平衡二叉树](api-avltree.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#list-列表库)

</div>
