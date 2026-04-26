# MemPool 通用内存池

> 可变大小内存池，支持多种大小的内存分配和 GC 回收

[English](api-mempoll.en.md) | [中文](api-mempoll.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [内存池操作](#内存池操作)
- [内存分配](#内存分配)
- [GC回收](#gc回收)
- [使用场景](#使用场景)
- [与其他内存管理器对比](#与其他内存管理器对比)
- [最佳实践](#最佳实践)

---

## 概述

MemPool（Memory Pool）是 XRT 库的通用内存池，支持**可变大小**的内存分配。与 FSMemPool（固定大小）不同，MemPool 可以分配任意大小的内存块。

### 核心特点

- **可变大小**：支持分配不同大小的内存块
- **FSB 二叉树**：使用二叉树快速查找合适的内存块大小
- **两种预设**：小内存方案（1-512B）和大内存方案（1-4096B）
- **大内存兜底**：超出范围的大内存使用 malloc 分配
- **GC 支持**：支持标记-清除垃圾回收

### 架构图

```
MemPool 结构：
┌─────────────────────────────────────────────────────────────┐
│                        MemPool                              │
├─────────────────────────────────────────────────────────────┤
│  FSB_RootNode (二叉树根)                                    │
│       ↓                                                     │
│  ┌────────────────────────────────────────────────────┐    │
│  │            FSB 二叉树（iCustom=1 示例）            │    │
│  │                        160                          │    │
│  │              ┌──────────┴──────────┐               │    │
│  │             64                    320               │    │
│  │         ┌────┴────┐          ┌────┴────┐          │    │
│  │        32        96         224       448          │    │
│  │       ┌─┴─┐    ┌─┴─┐      ┌─┴─┐     ┌─┴─┐        │    │
│  │      16  48   80  128    192 256   384 512        │    │
│  └────────────────────────────────────────────────────┘    │
│                                                             │
│  每个 FSB_Item 管理一个大小范围的 MemUnit 链表：            │
│  ┌─────────────┐                                           │
│  │ FSB_Item    │  MinLength: 1, MaxLength: 16              │
│  │ LL_Idle ────┼──> [MMU] -> [MMU] -> NULL                 │
│  │ LL_Full ────┼──> [MMU] -> NULL                          │
│  │ LL_Null ────┼──> [MMU] (备用)                           │
│  │ LL_Free ────┼──> [已释放的 MMU 位置]                    │
│  └─────────────┘                                           │
│                                                             │
│  arrMMU (BSMM): 管理所有 MemUnit                            │
│  BigMM (BSMM): 管理所有大内存分配信息                        │
│  LL_BigFree: 已释放的大内存链表                              │
└─────────────────────────────────────────────────────────────┘
```

### 分配策略

1. **小内存分配**（在 FSB 范围内）：
   - 通过二叉树查找匹配的 FSB_Item
   - 从对应的 MemUnit 分配内存
   - O(log n) 查找 + O(1) 分配

2. **大内存分配**（超出 FSB 范围）：
   - 直接使用 malloc 分配
   - 记录分配信息到 BigMM

---

## 数据结构

### MP_MemHead

大内存头结构（用于超出 FSB 范围的分配）。

**定义：**
```c
typedef struct {
    uint32 Index;    // 在 BigMM 中的索引
    uint32 Flag;     // 符合 MemUnit 标准的 Flag
} MP_MemHead;
```

---

### MP_BigInfoLL

大内存信息链表结构。

**定义：**
```c
typedef struct MP_BigInfoLL {
    uint32 Size;                  // 申请内存的大小
    ptr Ptr;                      // 指针地址
    struct MP_BigInfoLL* Next;    // 下一个链表节点（用于释放链表）
} MP_BigInfoLL;
```

---

### FSB_Item

固定大小区块管理结构。

**定义：**
```c
typedef struct FSB_Item {
    uint32 MinLength;             // 支持最小的内存长度
    uint32 MaxLength;             // 支持最大的内存长度
    MMU_LLNode* LL_Idle;          // 空闲的 MemUnit 链表
    MMU_LLNode* LL_Full;          // 满载的 MemUnit 链表
    MMU_LLNode* LL_Null;          // 全空的 MemUnit（备用）
    MMU_LLNode* LL_Free;          // 已释放的 MemUnit 链表
    struct FSB_Item* left;        // 左子树
    struct FSB_Item* right;       // 右子树
} FSB_Item;
```

**链表说明：**

| 链表 | 说明 |
|------|------|
| `LL_Idle` | 有空闲槽位的 MemUnit，优先从这里分配 |
| `LL_Full` | 已满的 MemUnit，不分配 |
| `LL_Null` | 完全空闲的 MemUnit（最多1个），用于避免临界震荡 |
| `LL_Free` | 已销毁的 MemUnit 位置，创建新 MemUnit 时优先复用 |

---

### xmempool_struct

通用内存池结构。

**定义：**
```c
typedef struct {
    FSB_Item* FSB_Memory;       // FSB 数组内存
    FSB_Item* FSB_RootNode;     // FSB 二叉树根节点
    xbsmm_struct arrMMU;        // MemUnit 阵列管理
    xbsmm_struct BigMM;         // 大内存信息管理
    MP_BigInfoLL* LL_BigFree;   // 已释放的大内存链表
} xmempool_struct, *xmempool;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `FSB_Memory` | FSB 数组的内存指针 |
| `FSB_RootNode` | FSB 二叉树的根节点 |
| `arrMMU` | 使用 BSMM 管理所有 MemUnit |
| `BigMM` | 使用 BSMM 管理大内存分配信息 |
| `LL_BigFree` | 已释放的大内存信息链表 |

---

## 内存池操作

### xrtMemPoolCreate

创建通用内存池。

**函数原型：**
```c
XXAPI xmempool xrtMemPoolCreate(int iCustom);
```

**参数：**
- `iCustom` - 预设配置：
  - `1` - 小内存方案（4层树，1-512 字节，15个区块）
  - `2` - 大内存方案（5层树，1-4096 字节，31个区块）
  - 其他 - 不创建预设，需手动配置 FSB

**返回值：**
- 成功：内存池对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtMemPoolDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建小内存方案（1-512B）
    xmempool pool1 = xrtMemPoolCreate(1);
    printf("小内存池: 支持 1-512 字节\n");
    
    // 创建大内存方案（1-4096B）
    xmempool pool2 = xrtMemPoolCreate(2);
    printf("大内存池: 支持 1-4096 字节\n");
    
    // 分配各种大小的内存
    ptr p1 = xrtMemPoolAlloc(pool1, 32);
    ptr p2 = xrtMemPoolAlloc(pool1, 128);
    ptr p3 = xrtMemPoolAlloc(pool1, 500);
    ptr p4 = xrtMemPoolAlloc(pool1, 1024);  // 超出范围，使用 malloc
    
    printf("分配 32B: %p\n", p1);
    printf("分配 128B: %p\n", p2);
    printf("分配 500B: %p\n", p3);
    printf("分配 1024B (大内存): %p\n", p4);
    
    // 释放
    xrtMemPoolFree(pool1, p1);
    xrtMemPoolFree(pool1, p2);
    xrtMemPoolFree(pool1, p3);
    xrtMemPoolFree(pool1, p4);
    
    xrtMemPoolDestroy(pool1);
    xrtMemPoolDestroy(pool2);
    
    xrtUnit();
    return 0;
}
```

**预设区块配置：**

**iCustom = 1（小内存方案）：**

| 区块 | 范围 (字节) |
|------|-------------|
| 1 | 1-16 |
| 2 | 17-32 |
| 3 | 33-48 |
| 4 | 49-64 |
| 5 | 65-80 |
| 6 | 81-96 |
| 7 | 97-128 |
| 8 | 129-160 |
| 9 | 161-192 |
| 10 | 193-224 |
| 11 | 225-256 |
| 12 | 257-320 |
| 13 | 321-384 |
| 14 | 385-448 |
| 15 | 449-512 |

**iCustom = 2（大内存方案）：**

在小内存方案基础上增加：

| 区块 | 范围 (字节) |
|------|-------------|
| 16 | 513-640 |
| 17 | 641-768 |
| ... | ... |
| 31 | 3841-4096 |

---

### xrtMemPoolDestroy

销毁内存池。

**函数原型：**
```c
XXAPI void xrtMemPoolDestroy(xmempool objMP);
```

**参数：**
- `objMP` - 内存池对象

**说明：**
- 释放所有 MemUnit
- 释放所有大内存块
- 释放 FSB 数组
- 释放内存池结构本身

---

### xrtMemPoolInit

初始化内存池（用于栈上或嵌入式场景）。

**函数原型：**
```c
XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom);
```

**参数：**
- `objMP` - 内存池对象指针
- `iCustom` - 预设配置（同 Create）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上分配
    xmempool_struct pool;
    xrtMemPoolInit(&pool, 1);
    
    ptr p = xrtMemPoolAlloc(&pool, 64);
    printf("分配: %p\n", p);
    xrtMemPoolFree(&pool, p);
    
    xrtMemPoolUnit(&pool);  // 注意：使用 Unit 而不是 Destroy
    
    xrtUnit();
    return 0;
}
```

---

### xrtMemPoolUnit

释放内存池内部资源（不释放结构体本身）。

**函数原型：**
```c
XXAPI void xrtMemPoolUnit(xmempool objMP);
```

**说明：**
- 配合 `xrtMemPoolInit` 使用
- 释放所有内部资源，但不释放结构体本身

---

## 内存分配

### xrtMemPoolAlloc

从内存池分配内存。

**函数原型：**
```c
XXAPI ptr xrtMemPoolAlloc(xmempool objMP, uint32 iSize);
```

**参数：**
- `objMP` - 内存池对象
- `iSize` - 请求的内存大小（字节）

**返回值：**
- 成功：内存指针
- 失败：`NULL`

**分配策略：**
1. 在 FSB 二叉树中查找匹配的区块
2. 如果找到：从对应的 MemUnit 分配
3. 如果未找到：使用 malloc 分配大内存

**示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[32];
} User;

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // 分配不同大小的内存
    User* user = xrtMemPoolAlloc(pool, sizeof(User));
    user->id = 1;
    strcpy(user->name, "Alice");
    
    char* buffer = xrtMemPoolAlloc(pool, 256);
    strcpy(buffer, "Hello, World!");
    
    // 大内存分配（超出 FSB 范围）
    char* largeBuffer = xrtMemPoolAlloc(pool, 8192);
    memset(largeBuffer, 'X', 8192);
    
    printf("User: id=%d, name=%s\n", user->id, user->name);
    printf("Buffer: %s\n", buffer);
    printf("Large buffer allocated: %p\n", largeBuffer);
    
    xrtMemPoolFree(pool, user);
    xrtMemPoolFree(pool, buffer);
    xrtMemPoolFree(pool, largeBuffer);
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

### xrtMemPoolFree

释放从内存池分配的内存。

**函数原型：**
```c
XXAPI void xrtMemPoolFree(xmempool objMP, ptr ptr);
```

**参数：**
- `objMP` - 内存池对象
- `ptr` - 要释放的内存指针

**释放策略：**
1. 检查内存头标志
2. 如果是大内存：使用 free 释放，加入 LL_BigFree 复用
3. 如果是 FSB 内存：释放到对应的 MemUnit，更新链表状态

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // 分配多个内存块
    ptr blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = xrtMemPoolAlloc(pool, 32 * (i + 1));
        printf("分配 %d 字节: %p\n", 32 * (i + 1), blocks[i]);
    }
    
    // 释放所有内存块
    for (int i = 0; i < 10; i++) {
        xrtMemPoolFree(pool, blocks[i]);
        printf("释放 block[%d]\n", i);
    }
    
    // 再次分配（会复用之前的内存）
    ptr reused = xrtMemPoolAlloc(pool, 64);
    printf("复用分配: %p\n", reused);
    xrtMemPoolFree(pool, reused);
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

## GC回收

### xrtMemPoolGC_Mark

标记内存为使用中（宏定义）。

**宏定义：**
```c
#define xrtMemPoolGC_Mark  xrtMemUnitGC_Mark
```

**函数原型：**
```c
void xrtMemPoolGC_Mark(ptr pVal);
```

**参数：**
- `pVal` - 内存指针

**说明：**
- 在 GC 前标记仍在使用的内存
- 与 `xrtMemPoolGC` 配合使用

---

### xrtMemPoolGC

执行垃圾回收。

**函数原型：**
```c
XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark);
```

**参数：**
- `objMP` - 内存池对象
- `bFreeMark` - GC 模式：
  - `TRUE` - 回收**已标记**的内存
  - `FALSE` - 回收**未标记**的内存（常用）

**GC 流程（bFreeMark = FALSE）：**
1. 标记阶段：遍历所有活跃对象，调用 `xrtMemPoolGC_Mark`
2. 回收阶段：调用 `xrtMemPoolGC`，未标记的内存被回收
3. 清除阶段：GC 自动清除已标记对象的标记

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // 分配多个内存块
    ptr p1 = xrtMemPoolAlloc(pool, 32);
    ptr p2 = xrtMemPoolAlloc(pool, 64);
    ptr p3 = xrtMemPoolAlloc(pool, 128);
    ptr p4 = xrtMemPoolAlloc(pool, 256);
    
    printf("分配了 4 块内存\n");
    
    // 假设 p1 和 p3 仍在使用，p2 和 p4 不再使用
    // 标记仍在使用的内存
    xrtMemPoolGC_Mark(p1);
    xrtMemPoolGC_Mark(p3);
    
    // 执行 GC，回收未标记的内存（p2 和 p4）
    xrtMemPoolGC(pool, FALSE);
    
    printf("GC 完成，p2 和 p4 已被回收\n");
    
    // p1 和 p3 仍然有效
    // 注意：GC 后标记会被清除，下次 GC 前需要重新标记
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

**完整 GC 示例：**
```c
#include "xrt.h"
#include <stdio.h>

// 模拟对象管理
typedef struct {
    int id;
    ptr data;
    bool inUse;
} Object;

Object objects[100];
int objectCount = 0;
xmempool pool;

Object* CreateObject(int id, size_t dataSize) {
    Object* obj = &objects[objectCount++];
    obj->id = id;
    obj->data = xrtMemPoolAlloc(pool, dataSize);
    obj->inUse = TRUE;
    return obj;
}

void MarkAndSweep() {
    // 标记阶段：标记所有仍在使用的对象
    for (int i = 0; i < objectCount; i++) {
        if (objects[i].inUse && objects[i].data) {
            xrtMemPoolGC_Mark(objects[i].data);
        }
    }
    
    // 回收阶段：回收未标记的内存
    xrtMemPoolGC(pool, FALSE);
}

int main() {
    xrtInit();
    
    pool = xrtMemPoolCreate(1);
    
    // 创建一些对象
    Object* obj1 = CreateObject(1, 64);
    Object* obj2 = CreateObject(2, 128);
    Object* obj3 = CreateObject(3, 256);
    
    printf("创建了 3 个对象\n");
    
    // 模拟 obj2 不再使用
    obj2->inUse = FALSE;
    
    // 执行 GC
    MarkAndSweep();
    
    printf("GC 完成，obj2 的内存已被回收\n");
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 多类型对象分配

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct { int x, y; } Point;
typedef struct { int x, y, w, h; } Rect;
typedef struct { char name[64]; int value; } Config;

int main() {
    xrtInit();
    
    xmempool pool = xrtMemPoolCreate(1);
    
    // 分配不同类型的对象
    Point* p1 = xrtMemPoolAlloc(pool, sizeof(Point));
    p1->x = 10; p1->y = 20;
    
    Rect* r1 = xrtMemPoolAlloc(pool, sizeof(Rect));
    r1->x = 0; r1->y = 0; r1->w = 100; r1->h = 50;
    
    Config* c1 = xrtMemPoolAlloc(pool, sizeof(Config));
    strcpy(c1->name, "MaxConnections");
    c1->value = 100;
    
    printf("Point: (%d, %d)\n", p1->x, p1->y);
    printf("Rect: (%d, %d, %d, %d)\n", r1->x, r1->y, r1->w, r1->h);
    printf("Config: %s = %d\n", c1->name, c1->value);
    
    xrtMemPoolFree(pool, p1);
    xrtMemPoolFree(pool, r1);
    xrtMemPoolFree(pool, c1);
    
    xrtMemPoolDestroy(pool);
    
    xrtUnit();
    return 0;
}
```

---

### 2. JSON 解析器内存管理

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct JsonNode {
    int type;  // 0=null, 1=string, 2=number, 3=object, 4=array
    union {
        char* str;
        double num;
        struct JsonNode* children;
    } value;
    struct JsonNode* next;
} JsonNode;

xmempool jsonPool;

JsonNode* CreateStringNode(const char* value) {
    JsonNode* node = xrtMemPoolAlloc(jsonPool, sizeof(JsonNode));
    node->type = 1;
    size_t len = strlen(value);
    node->value.str = xrtMemPoolAlloc(jsonPool, len + 1);
    strcpy(node->value.str, value);
    node->next = NULL;
    return node;
}

JsonNode* CreateNumberNode(double value) {
    JsonNode* node = xrtMemPoolAlloc(jsonPool, sizeof(JsonNode));
    node->type = 2;
    node->value.num = value;
    node->next = NULL;
    return node;
}

int main() {
    xrtInit();
    
    jsonPool = xrtMemPoolCreate(1);
    
    // 模拟 JSON 解析
    JsonNode* root = CreateStringNode("Hello, JSON!");
    JsonNode* num = CreateNumberNode(3.14159);
    root->next = num;
    
    printf("String node: %s\n", root->value.str);
    printf("Number node: %f\n", num->value.num);
    
    // 销毁时一次性释放所有内存
    xrtMemPoolDestroy(jsonPool);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 带 GC 的脚本引擎

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct ScriptVar {
    int type;  // 0=int, 1=string
    union {
        int intVal;
        char* strVal;
    };
    bool marked;  // 用于 GC 标记
} ScriptVar;

xmempool varPool;
ScriptVar* variables[256];
int varCount = 0;

ScriptVar* CreateInt(int value) {
    ScriptVar* var = xrtMemPoolAlloc(varPool, sizeof(ScriptVar));
    var->type = 0;
    var->intVal = value;
    var->marked = FALSE;
    variables[varCount++] = var;
    return var;
}

ScriptVar* CreateString(const char* value) {
    ScriptVar* var = xrtMemPoolAlloc(varPool, sizeof(ScriptVar));
    var->type = 1;
    size_t len = strlen(value);
    var->strVal = xrtMemPoolAlloc(varPool, len + 1);
    strcpy(var->strVal, value);
    var->marked = FALSE;
    variables[varCount++] = var;
    return var;
}

void MarkVariable(ScriptVar* var) {
    if (!var->marked) {
        var->marked = TRUE;
        xrtMemPoolGC_Mark(var);
        if (var->type == 1 && var->strVal) {
            xrtMemPoolGC_Mark(var->strVal);
        }
    }
}

void RunGC() {
    // 标记阶段（这里简化，实际应从根集开始标记）
    for (int i = 0; i < varCount; i++) {
        if (variables[i] && variables[i]->marked) {
            MarkVariable(variables[i]);
        }
    }
    
    // 回收阶段
    xrtMemPoolGC(varPool, FALSE);
    
    // 清除标记
    for (int i = 0; i < varCount; i++) {
        if (variables[i]) {
            variables[i]->marked = FALSE;
        }
    }
}

int main() {
    xrtInit();
    
    varPool = xrtMemPoolCreate(1);
    
    // 创建变量
    ScriptVar* a = CreateInt(42);
    ScriptVar* b = CreateString("Hello");
    ScriptVar* c = CreateString("World");
    
    printf("创建了 3 个变量\n");
    
    // 模拟 b 仍在使用
    b->marked = TRUE;
    
    // 运行 GC
    RunGC();
    
    printf("GC 完成\n");
    
    xrtMemPoolDestroy(varPool);
    
    xrtUnit();
    return 0;
}
```

---

## 与其他内存管理器对比

| 特性 | MemPool | FSMemPool | MemUnit | malloc/free |
|------|---------|-----------|---------|-------------|
| **分配大小** | 可变 | 固定 | 固定 | 可变 |
| **容量** | 无限制 | 无限制 | 256 | 无限制 |
| **大内存支持** | ✅ 自动兜底 | ❌ | ❌ | ✅ |
| **GC 支持** | ✅ | ✅ | ✅ | ❌ |
| **分配速度** | O(log n) + O(1) | O(1) | O(1) | O(?) |
| **内存碎片** | 区块内无碎片 | 无碎片 | 无碎片 | 有碎片 |
| **适用场景** | 通用分配 | 固定类型对象 | 底层组件 | 通用 |

### 选择建议

- **MemPool**：需要分配不同大小内存时使用
- **FSMemPool**：只分配固定大小对象时使用（性能更好）
- **MemUnit**：作为底层组件，通常不直接使用
- **malloc/free**：简单场景或与其他库交互时使用

---

## 最佳实践

### 1. 选择合适的预设

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 小对象为主（<512B）
    xmempool smallPool = xrtMemPoolCreate(1);
    
    // 大对象为主（<4096B）
    xmempool largePool = xrtMemPoolCreate(2);
    
    // 分配示例
    ptr p1 = xrtMemPoolAlloc(smallPool, 64);   // 使用 FSB
    ptr p2 = xrtMemPoolAlloc(smallPool, 1024); // 使用 malloc（超出范围）
    
    ptr p3 = xrtMemPoolAlloc(largePool, 64);   // 使用 FSB
    ptr p4 = xrtMemPoolAlloc(largePool, 1024); // 使用 FSB（在范围内）
    ptr p5 = xrtMemPoolAlloc(largePool, 8192); // 使用 malloc（超出范围）
    
    printf("小内存池 - 64B: FSB, 1024B: malloc\n");
    printf("大内存池 - 64B: FSB, 1024B: FSB, 8192B: malloc\n");
    
    xrtMemPoolFree(smallPool, p1);
    xrtMemPoolFree(smallPool, p2);
    xrtMemPoolFree(largePool, p3);
    xrtMemPoolFree(largePool, p4);
    xrtMemPoolFree(largePool, p5);
    
    xrtMemPoolDestroy(smallPool);
    xrtMemPoolDestroy(largePool);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 按用途分离内存池

```c
#include "xrt.h"
#include <stdio.h>

// 不同用途使用不同的内存池
xmempool configPool;    // 配置对象（长期存在）
xmempool requestPool;   // 请求对象（短期存在，可 GC）

void Init() {
    configPool = xrtMemPoolCreate(1);
    requestPool = xrtMemPoolCreate(1);
}

void Cleanup() {
    xrtMemPoolDestroy(configPool);
    xrtMemPoolDestroy(requestPool);
}

void ProcessRequest() {
    // 分配请求相关的内存
    ptr req = xrtMemPoolAlloc(requestPool, 256);
    
    // 处理请求...
    
    // 处理完成后可以选择：
    // 1. 手动释放
    xrtMemPoolFree(requestPool, req);
    
    // 2. 或者使用 GC 批量回收
}

void GCRequests() {
    // 定期回收请求池中的未标记内存
    xrtMemPoolGC(requestPool, FALSE);
}

int main() {
    xrtInit();
    
    Init();
    
    // 分配配置（长期存在）
    ptr config = xrtMemPoolAlloc(configPool, 128);
    printf("配置已分配\n");
    
    // 处理多个请求
    for (int i = 0; i < 5; i++) {
        ProcessRequest();
    }
    
    // 定期 GC
    GCRequests();
    printf("请求池已 GC\n");
    
    xrtMemPoolFree(configPool, config);
    
    Cleanup();
    
    xrtUnit();
    return 0;
}
```

---

### 3. 避免跨池释放

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xmempool pool1 = xrtMemPoolCreate(1);
    xmempool pool2 = xrtMemPoolCreate(1);
    
    ptr p1 = xrtMemPoolAlloc(pool1, 64);
    ptr p2 = xrtMemPoolAlloc(pool2, 64);
    
    // ✅ 正确：从同一个池释放
    xrtMemPoolFree(pool1, p1);
    xrtMemPoolFree(pool2, p2);
    
    // ❌ 错误：跨池释放（可能导致崩溃或数据损坏）
    // xrtMemPoolFree(pool1, p2);  // 不要这样做！
    
    xrtMemPoolDestroy(pool1);
    xrtMemPoolDestroy(pool2);
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [MemUnit 内存单元管理](api-memunit.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#mempool-通用内存池)

</div>
