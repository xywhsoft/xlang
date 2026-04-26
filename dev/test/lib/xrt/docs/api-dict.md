# Dict 字典库

> 基于 AVL 树的键值对存储，支持任意二进制键

[English](api-dict.en.md) | [中文](api-dict.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [回调函数类型](#回调函数类型)
- [字典操作](#字典操作)
- [键值操作](#键值操作)
- [遍历](#遍历)
- [使用场景](#使用场景)
- [性能特点](#性能特点)
- [最佳实践](#最佳实践)

---

## 概述

Dict 是基于 AVL 树实现的字典（键值对存储），支持任意二进制数据作为键。

### 核心特点

- **AVL 树实现**：保证 O(log n) 的查找、插入、删除性能
- **任意二进制键**：键可以是字符串、数字、结构体等任意数据
- **灵活值存储**：支持存储值或直接存储指针
- **内存池支持**：可配合 MemPool 管理键内存

### 架构图

```
Dict 结构：
┌─────────────────────────────────────────┐
│                xdict                    │
├─────────────────────────────────────────┤
│  AVLT (xavltree_struct)                 │  ← AVL 树管理节点
│  MP (xmempool)                          │  ← 可选的内存池
└─────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│             AVL 树节点                   │
├─────────────────────────────────────────┤
│  Dict_Key: Key + KeyLen + Hash          │  ← 键信息
│  UserData[iItemLength]                  │  ← 用户数据
└─────────────────────────────────────────┘
```

---

## 数据结构

### Dict_Key

字典键数据结构。

**定义：**
```c
typedef struct {
    ptr Key;          // 键数据指针
    uint32 KeyLen;    // 键长度
    uint32 Hash;      // 键的哈希值（用于快速比较）
} Dict_Key;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `Key` | 指向键数据的指针（由字典自动复制和管理）|
| `KeyLen` | 键的字节长度 |
| `Hash` | 键的哈希值（64位平台用 Hash64，32位平台用 Hash32）|

---

### xdict_struct

字典对象数据结构。

**定义：**
```c
typedef struct {
    xavltree_struct AVLT;  // 内部 AVL 树
    xmempool MP;           // 可选内存池
} xdict_struct, *xdict;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `AVLT` | 内部 AVL 树，管理所有键值对 |
| `MP` | 可选的内存池，用于分配键内存（默认 NULL，使用 xrtMalloc）|

---

## 回调函数类型

### Dict_EachProc

字典遍历回调函数。

**定义：**
```c
typedef bool (*Dict_EachProc)(Dict_Key* pKey, ptr pVal, ptr pArg);
```

**参数：**
- `pKey` - 键信息（包含 Key、KeyLen、Hash）
- `pVal` - 值指针
- `pArg` - 用户自定义参数

**返回值：**
- `TRUE` - 继续遍历
- `FALSE` - 中断遍历

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

bool PrintItem(Dict_Key* pKey, ptr pVal, ptr pArg) {
    printf("%.*s = %d\n", pKey->KeyLen, (char*)pKey->Key, *(int*)pVal);
    return TRUE;  // 继续遍历
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)"age", 3, &isNew);
    *val = 25;
    
    xrtDictWalk(dict, PrintItem, NULL);
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## 字典操作

### xrtDictCreate

创建字典。

**函数原型：**
```c
XXAPI xdict xrtDictCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 值的大小（字节）。0 表示存储指针（使用 `SetPtr`/`GetPtr`）

**返回值：**
- 成功：字典对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtDictDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 存储 int 值
    xdict intDict = xrtDictCreate(sizeof(int));
    bool isNew;
    int* val = (int*)xrtDictSet(intDict, (ptr)"count", 5, &isNew);
    if (isNew) {
        *val = 100;
    }
    printf("count = %d\n", *(int*)xrtDictGet(intDict, (ptr)"count", 5));
    xrtDictDestroy(intDict);
    
    // 存储指针
    xdict ptrDict = xrtDictCreate(0);
    str text = xrtCopyStr((str)"Hello World", 0);
    xrtDictSetPtr(ptrDict, (ptr)"greeting", 8, text, NULL);
    printf("greeting = %s\n", (char*)xrtDictGetPtr(ptrDict, (ptr)"greeting", 8));
    xrtFree(text);
    xrtDictDestroy(ptrDict);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDictDestroy

销毁字典，释放所有内存。

**函数原型：**
```c
XXAPI void xrtDictDestroy(xdict objHT);
```

**参数：**
- `objHT` - 字典对象

**说明：**
- 自动释放所有键内存
- 自动调用 `xrtDictUnit` 清理内部结构
- 如果值是指针，需要在销毁前手动释放

---

### xrtDictInit

初始化字典（用于栈上或嵌入式结构）。

**函数原型：**
```c
XXAPI void xrtDictInit(xdict objHT, uint32 iItemLength);
```

**参数：**
- `objHT` - 字典对象指针
- `iItemLength` - 值的大小

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上创建字典
    xdict_struct dictObj;
    xrtDictInit(&dictObj, sizeof(int));
    
    bool isNew;
    int* val = (int*)xrtDictSet(&dictObj, (ptr)"value", 5, &isNew);
    *val = 42;
    
    printf("value = %d\n", *(int*)xrtDictGet(&dictObj, (ptr)"value", 5));
    
    xrtDictUnit(&dictObj);  // 只清理内部结构，不释放 dictObj
    
    xrtUnit();
    return 0;
}
```

---

### xrtDictUnit

释放字典内部结构（不释放字典对象本身）。

**函数原型：**
```c
XXAPI void xrtDictUnit(xdict objHT);
```

**宏别名：**
```c
#define xrtDictRemoveAll  xrtDictUnit
#define xrtDictClear      xrtDictUnit
```

---

## 键值操作

### xrtDictSet

设置键值对（返回值指针）。

**函数原型：**
```c
XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet);
```

**参数：**
- `objHT` - 字典对象
- `sKey` - 键数据指针
- `iKeyLen` - 键长度（字节）
- `bNewRet` - 输出：是否为新键（可为 NULL）

**返回值：**
- 成功：值存储位置的指针
- 失败：`NULL`

**说明：**
- 如果键已存在，返回现有值的指针，`*bNewRet = FALSE`
- 如果键不存在，插入新节点，`*bNewRet = TRUE`，需要手动赋值

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // 插入新键
    bool isNew;
    str key = (str)"age";
    int* val = (int*)xrtDictSet(dict, key, strlen((char*)key), &isNew);
    if (isNew) {
        *val = 25;
        printf("新插入: age = %d\n", *val);
    }
    
    // 更新现有键
    val = (int*)xrtDictSet(dict, key, strlen((char*)key), &isNew);
    if (!isNew) {
        *val = 30;  // 更新值
        printf("已更新: age = %d\n", *val);
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictSetPtr

设置键值对（直接存储指针）。

**函数原型：**
```c
XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal);
```

**参数：**
- `objHT` - 字典对象
- `sKey` - 键数据指针
- `iKeyLen` - 键长度
- `pVal` - 要存储的指针值
- `ppOldVal` - 输出：原指针值（可为 NULL）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(0);  // 存储指针
    
    // 插入新值
    str key = (str)"name";
    str value1 = xrtCopyStr((str)"Alice", 0);
    ptr oldVal = NULL;
    xrtDictSetPtr(dict, key, strlen((char*)key), value1, &oldVal);
    printf("旧值: %s\n", oldVal ? (char*)oldVal : "(null)");
    
    // 更新值，获取旧值
    str value2 = xrtCopyStr((str)"Bob", 0);
    xrtDictSetPtr(dict, key, strlen((char*)key), value2, &oldVal);
    printf("旧值: %s\n", oldVal ? (char*)oldVal : "(null)");  // "Alice"
    xrtFree(oldVal);  // 释放旧值
    
    xrtFree(value2);
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictSetWithKey

使用预计算的键信息设置值（内联函数，高性能）。

**函数原型：**
```c
static inline ptr xrtDictSetWithKey(xdict objHT, Dict_Key* objKey, bool* bNewRet);
```

**参数：**
- `objHT` - 字典对象
- `objKey` - 预计算的键信息
- `bNewRet` - 输出：是否为新键

**说明：**
- 用于需要多次使用同一键的场景
- 避免重复计算哈希值

---

### xrtDictGet

获取值指针。

**函数原型：**
```c
XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**参数：**
- `objHT` - 字典对象
- `sKey` - 键数据指针
- `iKeyLen` - 键长度

**返回值：**
- 成功：值存储位置的指针
- 失败：`NULL`（键不存在）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // 插入数据
    str key = (str)"score";
    bool isNew;
    int* val = (int*)xrtDictSet(dict, key, strlen((char*)key), &isNew);
    *val = 95;
    
    // 获取数据
    int* result = (int*)xrtDictGet(dict, key, strlen((char*)key));
    if (result) {
        printf("score = %d\n", *result);  // 95
    } else {
        printf("键不存在\n");
    }
    
    // 查找不存在的键
    int* notFound = (int*)xrtDictGet(dict, (ptr)"unknown", 7);
    printf("unknown: %s\n", notFound ? "存在" : "不存在");
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictGetPtr

获取存储的指针值。

**函数原型：**
```c
XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**返回值：**
- 成功：存储的指针值
- 失败：`NULL`

**说明：**
- 与 `xrtDictGet` 的区别：`GetPtr` 返回存储在字典中的指针，而 `Get` 返回存储位置的指针

---

### xrtDictGetWithKey

使用预计算的键信息获取值（内联函数、高性能）。

**函数原型：**
```c
static inline ptr xrtDictGetWithKey(xdict objHT, Dict_Key* objKey);
```

---

### xrtDictRemove

移除键值对。

**函数原型：**
```c
XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**返回值：**
- `TRUE` - 成功移除
- `FALSE` - 键不存在

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // 插入数据
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)"item", 4, &isNew);
    *val = 100;
    
    printf("删除前数量: %u\n", xrtDictCount(dict));  // 1
    
    // 删除
    if (xrtDictRemove(dict, (ptr)"item", 4)) {
        printf("删除成功\n");
    }
    
    printf("删除后数量: %u\n", xrtDictCount(dict));  // 0
    
    // 删除不存在的键
    if (!xrtDictRemove(dict, (ptr)"notexist", 8)) {
        printf("键不存在\n");
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictRemovePtr

移除键值对，返回被删除的指针值。

**函数原型：**
```c
XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**返回值：**
- 成功：被删除的指针值
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(0);
    
    // 插入带内存的指针
    str value = xrtCopyStr((str)"need to free", 0);
    xrtDictSetPtr(dict, (ptr)"data", 4, value, NULL);
    
    // 删除并获取指针，然后释放
    ptr removed = xrtDictRemovePtr(dict, (ptr)"data", 4);
    if (removed) {
        printf("删除: %s\n", (char*)removed);
        xrtFree(removed);  // 释放内存
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictExists

检查键是否存在。

**函数原型：**
```c
XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**返回值：**
- `TRUE` - 键存在
- `FALSE` - 键不存在

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)"name", 4, &isNew);
    *val = 1;
    
    // 检查存在
    if (xrtDictExists(dict, (ptr)"name", 4)) {
        printf("name 存在\n");
    }
    
    // 检查不存在
    if (!xrtDictExists(dict, (ptr)"unknown", 7)) {
        printf("unknown 不存在\n");
    }
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## 遍历

### xrtDictCount

获取键值对数量。

**函数原型：**
```c
XXAPI uint32 xrtDictCount(xdict objHT);
```

**返回值：**
- 字典中的键值对数量

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    bool isNew;
    *(int*)xrtDictSet(dict, (ptr)"a", 1, &isNew) = 1;
    *(int*)xrtDictSet(dict, (ptr)"b", 1, &isNew) = 2;
    *(int*)xrtDictSet(dict, (ptr)"c", 1, &isNew) = 3;
    
    printf("字典大小: %u\n", xrtDictCount(dict));  // 3
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### xrtDictWalk

遍历字典中的所有键值对。

**函数原型：**
```c
XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg);
```

**参数：**
- `objHT` - 字典对象
- `procEach` - 遍历回调函数
- `pArg` - 传递给回调的自定义参数

**说明：**
- 使用中序遍历（按键的哈希值排序）
- 回调返回 `FALSE` 可中断遍历

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

bool PrintEntry(Dict_Key* pKey, ptr pVal, ptr pArg) {
    int* count = (int*)pArg;
    (*count)++;
    printf("%d: %.*s = %d\n", *count, pKey->KeyLen, (char*)pKey->Key, *(int*)pVal);
    return TRUE;  // 继续遍历
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // 插入数据
    bool isNew;
    *(int*)xrtDictSet(dict, (ptr)"apple", 5, &isNew) = 10;
    *(int*)xrtDictSet(dict, (ptr)"banana", 6, &isNew) = 20;
    *(int*)xrtDictSet(dict, (ptr)"cherry", 6, &isNew) = 30;
    
    // 遍历打印
    int count = 0;
    printf("字典内容:\n");
    xrtDictWalk(dict, PrintEntry, &count);
    printf("共 %d 项\n", count);
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 配置管理

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

xdict config = NULL;

void InitConfig() {
    config = xrtDictCreate(0);  // 存储指针
}

void SetConfig(str key, str value) {
    ptr oldVal = NULL;
    str valueCopy = xrtCopyStr(value, 0);
    xrtDictSetPtr(config, key, strlen((char*)key), valueCopy, &oldVal);
    if (oldVal) {
        xrtFree(oldVal);  // 释放旧值
    }
}

str GetConfig(str key) {
    return (str)xrtDictGetPtr(config, key, strlen((char*)key));
}

bool FreeConfigItem(Dict_Key* pKey, ptr pVal, ptr pArg) {
    xrtFree(pVal);
    return TRUE;
}

void CleanupConfig() {
    xrtDictWalk(config, FreeConfigItem, NULL);
    xrtDictDestroy(config);
}

int main() {
    xrtInit();
    
    InitConfig();
    
    SetConfig((str)"database", (str)"localhost:3306");
    SetConfig((str)"username", (str)"admin");
    SetConfig((str)"password", (str)"secret");
    
    printf("database: %s\n", GetConfig((str)"database"));
    printf("username: %s\n", GetConfig((str)"username"));
    
    CleanupConfig();
    xrtUnit();
    return 0;
}
```

---

### 2. 对象缓存

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[64];
    int age;
} User;

xdict userCache = NULL;

void InitCache() {
    userCache = xrtDictCreate(sizeof(User));
}

User* GetOrCreateUser(int id) {
    char keyBuf[16];
    int keyLen = sprintf(keyBuf, "user_%d", id);
    
    bool isNew;
    User* user = (User*)xrtDictSet(userCache, (ptr)keyBuf, keyLen, &isNew);
    
    if (isNew) {
        // 初始化新用户
        user->id = id;
        sprintf(user->name, "User%d", id);
        user->age = 20 + id;
        printf("创建新用户: %d\n", id);
    } else {
        printf("从缓存获取: %d\n", id);
    }
    
    return user;
}

int main() {
    xrtInit();
    
    InitCache();
    
    // 第一次访问，创建新用户
    User* u1 = GetOrCreateUser(1);
    printf("  name: %s, age: %d\n", u1->name, u1->age);
    
    // 第二次访问，从缓存获取
    User* u1_again = GetOrCreateUser(1);
    printf("  name: %s, age: %d\n", u1_again->name, u1_again->age);
    
    // 不同用户
    User* u2 = GetOrCreateUser(2);
    printf("  name: %s, age: %d\n", u2->name, u2->age);
    
    printf("缓存大小: %u\n", xrtDictCount(userCache));
    
    xrtDictDestroy(userCache);
    xrtUnit();
    return 0;
}
```

---

### 3. 二进制键

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x;
    int y;
} Point;

int main() {
    xrtInit();
    
    // 使用结构体作为键
    xdict pointMap = xrtDictCreate(sizeof(int));
    
    Point p1 = {10, 20};
    Point p2 = {30, 40};
    Point p3 = {10, 20};  // 与 p1 相同
    
    bool isNew;
    *(int*)xrtDictSet(pointMap, &p1, sizeof(Point), &isNew) = 100;
    *(int*)xrtDictSet(pointMap, &p2, sizeof(Point), &isNew) = 200;
    
    // 查找
    int* val1 = (int*)xrtDictGet(pointMap, &p1, sizeof(Point));
    int* val3 = (int*)xrtDictGet(pointMap, &p3, sizeof(Point));  // 与 p1 相同
    
    printf("p1: %d\n", val1 ? *val1 : -1);  // 100
    printf("p3: %d\n", val3 ? *val3 : -1);  // 100（与 p1 相同）
    
    xrtDictDestroy(pointMap);
    xrtUnit();
    return 0;
}
```

---

## 性能特点

### 时间复杂度

| 操作 | 复杂度 | 说明 |
|------|---------|------|
| 插入 | O(log n) | AVL 树平衡插入 |
| 查找 | O(log n) | AVL 树查找 |
| 删除 | O(log n) | AVL 树平衡删除 |
| 遍历 | O(n) | 中序遍历所有节点 |

### 空间复杂度

- O(n)，其中 n 为键值对数量
- 每个节点开销：`sizeof(xavltnode_struct) + sizeof(Dict_Key) + iItemLength`

### 与哈希表对比

| 特性 | Dict (AVL 树) | 哈希表 |
|------|--------------|--------|
| 查找复杂度 | O(log n) | O(1) 平均 |
| 有序遍历 | ✅ 支持 | ✖ 不支持 |
| 最坏情况 | O(log n) | O(n) |
| 内存开销 | 较小 | 较大（需要桶数组）|

---

## 最佳实践

### 1. 键长度计算

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // ✅ 正确：使用 strlen 计算字符串键长度
    str key1 = (str)"hello";
    bool isNew;
    int* val = (int*)xrtDictSet(dict, key1, strlen((char*)key1), &isNew);
    *val = 1;
    
    // ✗ 错误：不要包含终止符
    // xrtDictSet(dict, "hello", 6, &isNew);  // 包含 \0，不一致
    
    // ✅ 二进制键：使用 sizeof
    int intKey = 12345;
    val = (int*)xrtDictSet(dict, &intKey, sizeof(int), &isNew);
    *val = 2;
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

### 2. 指针值内存管理

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

bool FreeValue(Dict_Key* pKey, ptr pVal, ptr pArg) {
    xrtFree(pVal);  // 释放存储的指针
    return TRUE;
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(0);  // 存储指针
    
    // 插入带内存的值
    str data = xrtCopyStr((str)"allocated data", 0);
    xrtDictSetPtr(dict, (ptr)"key", 3, data, NULL);
    
    // 销毁前释放所有值
    xrtDictWalk(dict, FreeValue, NULL);
    xrtDictDestroy(dict);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 拍扁结构作为键

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// ✗ 不推荐：结构体内存对齐可能导致问题
typedef struct {
    char type;
    int id;
    // 可能有填充字节
} BadKey;

// ✅ 推荐：使用紧凑结构或序列化
typedef struct {
    char type;
    int id;
} __attribute__((packed)) GoodKey;  // GCC

// 或者序列化为字符串
char* MakeKey(char type, int id) {
    static char buf[32];
    sprintf(buf, "%c:%d", type, id);
    return buf;
}

int main() {
    xrtInit();
    
    xdict dict = xrtDictCreate(sizeof(int));
    
    // 使用序列化键
    char* key = MakeKey('A', 100);
    bool isNew;
    int* val = (int*)xrtDictSet(dict, (ptr)key, strlen(key), &isNew);
    *val = 42;
    
    printf("查找: %d\n", *(int*)xrtDictGet(dict, (ptr)MakeKey('A', 100), strlen(key)));
    
    xrtDictDestroy(dict);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [AVLTree 平衡二叉树](api-avltree.md)
- [Hash 哈希计算](api-hash.md)
- [List 列表](api-list.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#dict-字典库)

</div>
