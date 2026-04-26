# MemUnit 内存单元管理库

> Memory Unit - 256元素固定页内存管理单元

[English](api-memunit.en.md) | [中文](api-memunit.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [常量定义](#常量定义)
- [数据结构](#数据结构)
- [管理器操作](#管理器操作)
- [内存分配](#内存分配)
- [GC回收](#gc回收)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 概述

MemUnit（Memory Unit）是 XRT 库的底层内存管理单元，每个单元管理 **256 个固定大小** 的元素。它是 FSMemPool 和 MemPool 的基础组件。

### 核心特点

- **固定容量**：每个单元最多存储 256 个元素
- **空闲复用**：释放的内存通过环形队列管理，优先复用
- **O(1) 操作**：分配和释放都是常数时间
- **GC 支持**：支持标记-清除垃圾回收
- **元数据头**：每个元素前置 4 字节标志位

### 内存布局

```
MemUnit 结构：
┌─────────────────────────────────────────────────────────────┐
│ FreeList[256] │ ItemLength │ Count │ FreeCount │ FreeOffset │
├─────────────────────────────────────────────────────────────┤
│                        Memory[]                              │
│  ┌──────────┬──────────┬──────────┬─────┬──────────┐        │
│  │ [Flag+0] │ [Flag+1] │ [Flag+2] │ ... │ [Flag+255]│        │
│  │  Data0   │  Data1   │  Data2   │     │  Data255  │        │
│  └──────────┴──────────┴──────────┴─────┴──────────┘        │
└─────────────────────────────────────────────────────────────┘

每个元素：
┌──────────────┬──────────────────────────────────────┐
│ ItemFlag(4B) │ 用户数据 (ItemLength - 4)             │
└──────────────┴──────────────────────────────────────┘
```

---

## 常量定义

### 标志位常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `MMU_FLAG_MASK` | `0x3FFFFFFF` | 有效 ID 区间掩码（30位） |
| `MMU_FLAG_USE` | `0x80000000` | 结构体使用状态标记（最高位） |
| `MMU_FLAG_GC` | `0x40000000` | GC 回收标记位（次高位） |
| `MMU_FLAG_EXT` | `0xBFFFFFFF` | 非内存管理器管理的内存标记 |

### 标志位布局

```
ItemFlag (32位):
┌───┬───┬──────────────────────────────┐
│ U │ G │          ID / Index          │
├───┼───┼──────────────────────────────┤
│31 │30 │          29 - 0              │
└───┴───┴──────────────────────────────┘
  U = MMU_FLAG_USE (使用中)
  G = MMU_FLAG_GC (GC标记)
```

---

## 数据结构

### MMU_Value

元素头部标志结构。

**定义：**
```c
typedef struct {
    uint32 ItemFlag;    // 标志位：使用状态 + GC标记 + 索引
} MMU_Value, *MMU_ValuePtr;
```

**说明：**
- 每个元素前置 4 字节 `ItemFlag`
- 用于追踪元素状态和在单元中的位置

---

### xmemunit_struct

内存单元管理器结构。

**定义：**
```c
typedef struct {
    uint8 FreeList[256];    // 已释放成员索引环形队列
    uint32 ItemLength;      // 成员占用内存长度（含 4 字节头）
    uint16 Count;           // 当前使用的成员数量
    uint8 FreeCount;        // 环形队列中已释放成员数量
    uint8 FreeOffset;       // 环形队列首元素偏移
    uint32 Flag;            // 值的 Flag 前缀（由上级管理器下发）
    char Memory[];          // 柔性数组，存储 256 个元素
} xmemunit_struct, *xmemunit;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `FreeList` | 环形队列，存储已释放元素的索引 |
| `ItemLength` | 每个元素的总长度（用户请求长度 + 4） |
| `Count` | 当前已分配且未释放的元素数量 |
| `FreeCount` | 可复用的已释放元素数量 |
| `FreeOffset` | 下一个可复用元素在 FreeList 中的位置 |
| `Flag` | 上级管理器传递的标志前缀 |
| `Memory` | 实际存储数据的内存区域 |

---

## 管理器操作

### xrtMemUnitCreate

创建内存管理单元。

**函数原型：**
```c
XXAPI xmemunit xrtMemUnitCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 每个元素的数据大小（不含头部 4 字节）

**返回值：**
- 成功：内存单元指针
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Record;

int main() {
    xrtInit();
    
    // 创建可存储 256 个 Record 的内存单元
    xmemunit unit = xrtMemUnitCreate(sizeof(Record));
    if (unit) {
        printf("内存单元创建成功\n");
        printf("元素长度: %u (含 4 字节头)\n", unit->ItemLength);
        printf("当前使用: %u\n", unit->Count);
        
        xrtMemUnitDestroy(unit);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `iItemLength` 会自动加 4 字节用于存储 `ItemFlag`
- 内存单元总大小 = `sizeof(xmemunit_struct) + 256 * (iItemLength + 4)`

---

### xrtMemUnitDestroy

销毁内存管理单元。

**定义（宏）：**
```c
#define xrtMemUnitDestroy xrtFree
```

**说明：**
- 直接调用 `xrtFree` 释放整个内存单元
- 不会调用各元素的析构函数
- 销毁前确保不再使用单元中的任何元素

---

## 内存分配

### xrtMemUnitAlloc

从内存单元申请一个元素。

**函数原型：**
```c
XXAPI ptr xrtMemUnitAlloc(xmemunit objUnit);
```

**参数：**
- `objUnit` - 内存单元对象

**返回值：**
- 成功：元素指针（跳过头部，直接指向用户数据）
- 失败：`NULL`（单元为空或已满）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // 分配 5 个元素
    for (int i = 0; i < 5; i++) {
        Item* item = (Item*)xrtMemUnitAlloc(unit);
        if (item) {
            item->value = i * 10;
            printf("分配元素 %d: value = %d\n", i, item->value);
        }
    }
    
    printf("当前使用: %u 个元素\n", unit->Count);
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

**分配策略：**
1. 优先从 FreeList 复用已释放的槽位
2. 如果没有可复用槽位，分配新槽位（索引 = Count）
3. 超过 256 个元素返回 `NULL`

---

### xrtMemUnitAlloc_Inline

内联版本的元素分配（高性能）。

**函数原型：**
```c
static inline ptr xrtMemUnitAlloc_Inline(xmemunit objUnit);
```

**说明：**
- 不检查 `objUnit` 是否为 `NULL`
- 不检查是否超过 256 限制
- 调用方必须确保条件满足

---

### xrtMemUnitFree

释放内存单元中的元素（通过指针）。

**函数原型：**
```c
XXAPI bool xrtMemUnitFree(xmemunit objUnit, ptr obj);
```

**参数：**
- `objUnit` - 内存单元对象
- `obj` - 元素指针

**返回值：**
- `TRUE` - 释放成功
- `FALSE` - 失败（单元为空、元素未使用或指针无效）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // 分配
    Item* item1 = (Item*)xrtMemUnitAlloc(unit);
    Item* item2 = (Item*)xrtMemUnitAlloc(unit);
    Item* item3 = (Item*)xrtMemUnitAlloc(unit);
    
    printf("分配后: %u 个元素\n", unit->Count);  // 3
    
    // 释放中间的元素
    xrtMemUnitFree(unit, item2);
    printf("释放后: %u 个元素\n", unit->Count);  // 2
    
    // 再分配会复用 item2 的槽位
    Item* item4 = (Item*)xrtMemUnitAlloc(unit);
    printf("item4 地址: %p (应与 item2 相同: %p)\n", item4, item2);
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

---

### xrtMemUnitFreeIdx

释放内存单元中的元素（通过索引）。

**函数原型：**
```c
XXAPI bool xrtMemUnitFreeIdx(xmemunit objUnit, uint8 idx);
```

**参数：**
- `objUnit` - 内存单元对象
- `idx` - 元素索引（0-255）

**返回值：**
- `TRUE` - 释放成功
- `FALSE` - 失败

**说明：**
- 当已知元素索引时，比 `xrtMemUnitFree` 更快
- 索引可从 `ItemFlag & 0xFF` 获取

---

### xrtMemUnitFree_Inline / xrtMemUnitFreeIdx_Inline

内联版本的释放函数。

**注意：**
- `xrtMemUnitFreeIdx_Inline` 不会清空 `ItemFlag`
- 调用方负责清空标志位（如果需要）

---

## GC回收

### xrtMemUnitGC_Mark

标记元素为 GC 使用中。

**定义（宏）：**
```c
#define xrtMemUnitGC_Mark(p) \
    (((MMU_ValuePtr)((void*)p - sizeof(MMU_Value)))->ItemFlag |= MMU_FLAG_GC)
```

**参数：**
- `p` - 元素指针

**说明：**
- 在 GC 周期中，标记需要保留的元素
- 设置 `MMU_FLAG_GC` 位

---

### xrtMemUnitGC

执行一轮垃圾回收。

**函数原型：**
```c
XXAPI int xrtMemUnitGC(xmemunit objUnit, bool bFreeMark);
```

**参数：**
- `objUnit` - 内存单元对象
- `bFreeMark` - 回收模式
  - `TRUE` - 回收**被标记**的元素
  - `FALSE` - 回收**未被标记**的元素（标准 Mark-Sweep）

**返回值：**
- 回收的元素数量

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // 分配 5 个元素
    Item* items[5];
    for (int i = 0; i < 5; i++) {
        items[i] = (Item*)xrtMemUnitAlloc(unit);
        items[i]->value = i;
    }
    printf("分配后: %u 个元素\n", unit->Count);  // 5
    
    // 标记需要保留的元素（0, 2, 4）
    xrtMemUnitGC_Mark(items[0]);
    xrtMemUnitGC_Mark(items[2]);
    xrtMemUnitGC_Mark(items[4]);
    
    // 回收未标记的元素（1, 3）
    int freed = xrtMemUnitGC(unit, FALSE);
    printf("回收了 %d 个元素\n", freed);  // 2
    printf("剩余: %u 个元素\n", unit->Count);  // 3
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

**GC 流程：**

1. **标记阶段**：遍历所有活跃引用，调用 `xrtMemUnitGC_Mark` 标记
2. **清除阶段**：调用 `xrtMemUnitGC(unit, FALSE)` 回收未标记元素
3. **标记清除**：`xrtMemUnitGC` 会自动清除保留元素的 GC 标记

---

## 使用场景

### 1. 底层内存池组件

MemUnit 通常作为更高级内存管理器的内部组件使用：

```c
#include "xrt.h"
#include <stdio.h>

// FSMemPool 内部使用 MemUnit 管理内存页
// 这是一个简化的示意：

typedef struct {
    xmemunit pages[4];  // 最多 4 页，共 1024 个元素
    int pageCount;
} SimplePool;

ptr SimplePoolAlloc(SimplePool* pool, size_t itemSize) {
    // 尝试从现有页分配
    for (int i = 0; i < pool->pageCount; i++) {
        if (pool->pages[i]->Count < 256) {
            return xrtMemUnitAlloc(pool->pages[i]);
        }
    }
    // 需要新页
    if (pool->pageCount < 4) {
        pool->pages[pool->pageCount] = xrtMemUnitCreate(itemSize);
        return xrtMemUnitAlloc(pool->pages[pool->pageCount++]);
    }
    return NULL;  // 池已满
}

int main() {
    xrtInit();
    printf("MemUnit 通常用作底层组件\n");
    printf("推荐直接使用 FSMemPool 或 MemPool\n");
    xrtUnit();
    return 0;
}
```

---

### 2. 高性能对象缓存

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char data[60];
} CacheEntry;

int main() {
    xrtInit();
    
    // 256 个缓存槽
    xmemunit cache = xrtMemUnitCreate(sizeof(CacheEntry));
    
    // 模拟缓存操作
    CacheEntry* entries[10];
    for (int i = 0; i < 10; i++) {
        entries[i] = (CacheEntry*)xrtMemUnitAlloc(cache);
        entries[i]->id = i;
        snprintf(entries[i]->data, 60, "Cache item %d", i);
    }
    
    printf("缓存使用: %u / 256\n", cache->Count);
    
    // 释放部分缓存
    xrtMemUnitFree(cache, entries[3]);
    xrtMemUnitFree(cache, entries[7]);
    
    printf("释放后: %u / 256\n", cache->Count);
    
    xrtMemUnitDestroy(cache);
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 优先使用高级内存管理器

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ❌ 直接使用 MemUnit（除非有特殊需求）
    // xmemunit unit = xrtMemUnitCreate(sizeof(MyStruct));
    
    // ✅ 推荐：使用 FSMemPool（无 256 限制）
    // xfsmempool pool = xrtFSMemPoolCreate(sizeof(MyStruct));
    
    // ✅ 推荐：使用 MemPool（多尺寸支持）
    // xmempool pool = xrtMemPoolCreate();
    
    printf("MemUnit 适用于:\n");
    printf("1. 实现自定义内存管理器\n");
    printf("2. 需要精确控制 256 元素页\n");
    printf("3. 底层性能优化场景\n");
    
    xrtUnit();
    return 0;
}
```

---

### 2. 容量检查

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xmemunit unit = xrtMemUnitCreate(sizeof(Item));
    
    // 分配前检查容量
    for (int i = 0; i < 300; i++) {
        if (unit->Count >= 256) {
            printf("单元已满，无法分配第 %d 个元素\n", i + 1);
            break;
        }
        Item* item = (Item*)xrtMemUnitAlloc(unit);
        item->value = i;
    }
    
    printf("最终使用: %u 个元素\n", unit->Count);  // 256
    
    xrtMemUnitDestroy(unit);
    xrtUnit();
    return 0;
}
```

---

### 3. GC 周期管理

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int id; int refCount; } Object;

// 模拟引用遍历
void MarkReachable(xmemunit unit, Object* roots[], int rootCount) {
    for (int i = 0; i < rootCount; i++) {
        if (roots[i] && roots[i]->refCount > 0) {
            xrtMemUnitGC_Mark(roots[i]);
        }
    }
}

int main() {
    xrtInit();
    
    xmemunit heap = xrtMemUnitCreate(sizeof(Object));
    
    // 分配对象
    Object* objs[10];
    for (int i = 0; i < 10; i++) {
        objs[i] = (Object*)xrtMemUnitAlloc(heap);
        objs[i]->id = i;
        objs[i]->refCount = (i % 2 == 0) ? 1 : 0;  // 偶数有引用
    }
    
    printf("GC 前: %u 个对象\n", heap->Count);
    
    // GC 周期
    MarkReachable(heap, objs, 10);
    int freed = xrtMemUnitGC(heap, FALSE);
    
    printf("GC 后: %u 个对象, 回收 %d 个\n", heap->Count, freed);
    
    xrtMemUnitDestroy(heap);
    xrtUnit();
    return 0;
}
```

---

## 与其他内存管理器对比

| 特性 | MemUnit | FSMemPool | MemPool |
|------|---------|-----------|---------|
| **容量** | 固定 256 | 无限制 | 无限制 |
| **元素大小** | 固定 | 固定 | 可变 |
| **GC 支持** | ✅ | ✅ | ✅ |
| **适用场景** | 底层组件 | 固定大小对象 | 通用对象池 |
| **复杂度** | 低 | 中 | 高 |

---

## 相关文档

- [BSMM 块结构内存管理](api-bsmm.md)
- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [MemPool 通用内存池](api-mempool.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#memunit-内存单元管理库)

</div>
