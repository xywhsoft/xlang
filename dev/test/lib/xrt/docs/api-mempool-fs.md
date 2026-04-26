# FSMemPool 固定大小内存池

> Fixed-Size Memory Pool - 高性能固定大小对象内存池

[English](api-mempool-fs.en.md) | [中文](api-mempool-fs.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [内存池操作](#内存池操作)
- [内存分配](#内存分配)
- [GC回收](#gc回收)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 概述

FSMemPool（Fixed-Size Memory Pool）是基于 MemUnit 的高性能内存池，专为频繁分配和释放**固定大小**对象设计，无容量限制。

### 核心特点

- **无限容量**：自动扩展，无 256 限制
- **四链表管理**：Idle/Full/Null/Free 分类管理 MemUnit
- **O(1) 分配**：大多数情况下常数时间分配
- **智能复用**：空单元缓存，避免临界状态频繁分配
- **GC 支持**：支持标记-清除垃圾回收

### 架构图

```
FSMemPool 结构：
┌─────────────────────────────────────────────────────────────────┐
│ ItemLength │ arrMMU (BSMM) │ LL_Idle │ LL_Full │ LL_Null │ LL_Free │
└─────────────────────────────────────────────────────────────────┘
                    │
                    ▼
        ┌───────────────────────────────────┐
        │         MMU_LLNode 数组            │
        │  ┌─────┬─────┬─────┬─────┬─────┐  │
        │  │ [0] │ [1] │ [2] │ [3] │ ... │  │
        │  └──┬──┴──┬──┴──┬──┴──┬──┴─────┘  │
        └─────┼─────┼─────┼─────┼───────────┘
              ▼     ▼     ▼     ▼
           MemUnit MemUnit MemUnit MemUnit
           (256个) (256个) (256个) (256个)

四链表分类：
┌────────┬────────────────────────────────────────────────────────┐
│ LL_Idle │ 有空闲槽位的 MemUnit（优先从这里分配）                  │
├────────┼────────────────────────────────────────────────────────┤
│ LL_Full │ 已满载的 MemUnit（不再分配，等待释放）                  │
├────────┼────────────────────────────────────────────────────────┤
│ LL_Null │ 全空的 MemUnit（缓存备用，最多 1 个）                   │
├────────┼────────────────────────────────────────────────────────┤
│ LL_Free │ 已释放的 Flag 槽位（复用 Flag 避免冲突）               │
└────────┴────────────────────────────────────────────────────────┘
```

### 分配策略

1. 从 `LL_Idle` 链表头部的 MemUnit 分配
2. 如果 `LL_Idle` 为空：
   - 优先使用 `LL_Null` 缓存的备用单元
   - 其次复用 `LL_Free` 中的 Flag 槽位创建新单元
   - 最后创建全新的 MemUnit
3. 分配后检查：如果 MemUnit 满载，移动到 `LL_Full`

---

## 数据结构

### MMU_LLNode

MemUnit 链表节点结构。

**定义：**
```c
typedef struct MMU_LLNode {
    uint32 Flag;                    // 标志位前缀（用于识别所属单元）
    xmemunit objMMU;                // 指向的 MemUnit
    struct MMU_LLNode* Prev;        // 前一个节点
    struct MMU_LLNode* Next;        // 后一个节点
} MMU_LLNode;
```

**说明：**
- `Flag` 存储在每个元素的 `ItemFlag` 高位，用于释放时定位 MemUnit
- 双向链表结构支持快速插入和删除

---

### xfsmempool_struct

固定大小内存池结构。

**定义：**
```c
typedef struct {
    uint32 ItemLength;          // 成员占用内存长度
    xbsmm_struct arrMMU;        // MMU_LLNode 数组管理器
    MMU_LLNode* LL_Idle;        // 有空闲槽位的单元链表
    MMU_LLNode* LL_Full;        // 满载单元链表
    MMU_LLNode* LL_Null;        // 备用全空单元（最多1个）
    MMU_LLNode* LL_Free;        // 已释放的 Flag 槽位链表
} xfsmempool_struct, *xfsmempool;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `ItemLength` | 每个元素的数据大小 |
| `arrMMU` | 使用 BSMM 管理 MMU_LLNode 数组 |
| `LL_Idle` | 有空闲槽位的 MemUnit 链表头 |
| `LL_Full` | 已满载的 MemUnit 链表头 |
| `LL_Null` | 缓存的全空 MemUnit（避免临界状态抖动）|
| `LL_Free` | 已释放单元的 Flag 链表（复用 Flag）|

---

## 内存池操作

### xrtFSMemPoolCreate

创建固定大小内存池。

**函数原型：**
```c
XXAPI xfsmempool xrtFSMemPoolCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 每个元素的数据大小（不含头部 4 字节）

**返回值：**
- 成功：内存池指针
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} User;

int main() {
    xrtInit();
    
    // 创建 User 对象池
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(User));
    if (pool) {
        printf("内存池创建成功\n");
        printf("元素大小: %u 字节\n", pool->ItemLength);
        
        xrtFSMemPoolDestroy(pool);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFSMemPoolDestroy

销毁内存池。

**函数原型：**
```c
XXAPI void xrtFSMemPoolDestroy(xfsmempool objMM);
```

**参数：**
- `objMM` - 内存池对象

**说明：**
- 释放所有 MemUnit 和内部结构
- 销毁后所有已分配的指针都无效

---

### xrtFSMemPoolInit

初始化内存池（用于栈上或嵌入式结构）。

**函数原型：**
```c
XXAPI void xrtFSMemPoolInit(xfsmempool objMM, uint32 iItemLength);
```

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

typedef struct {
    int config;
    xfsmempool_struct itemPool;  // 嵌入的内存池
} Manager;

int main() {
    xrtInit();
    
    Manager mgr;
    mgr.config = 100;
    xrtFSMemPoolInit(&mgr.itemPool, sizeof(Item));
    
    // 使用内嵌的内存池
    Item* item = (Item*)xrtFSMemPoolAlloc(&mgr.itemPool);
    item->value = 42;
    printf("值: %d\n", item->value);
    
    xrtFSMemPoolFree(&mgr.itemPool, item);
    xrtFSMemPoolUnit(&mgr.itemPool);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFSMemPoolUnit

释放内存池内部资源（用于栈上或嵌入式结构）。

**函数原型：**
```c
XXAPI void xrtFSMemPoolUnit(xfsmempool objMM);
```

---

## 内存分配

### xrtFSMemPoolAlloc

从内存池分配一个元素。

**函数原型：**
```c
XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM);
```

**参数：**
- `objMM` - 内存池对象

**返回值：**
- 成功：元素指针
- 失败：`NULL`（内存不足）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    double value;
} Record;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Record));
    
    // 分配多个对象（无 256 限制）
    Record* records[1000];
    for (int i = 0; i < 1000; i++) {
        records[i] = (Record*)xrtFSMemPoolAlloc(pool);
        records[i]->id = i;
        records[i]->value = i * 1.5;
    }
    
    printf("成功分配 1000 个 Record\n");
    printf("Record[500]: id=%d, value=%.1f\n", 
           records[500]->id, records[500]->value);
    
    // 释放
    for (int i = 0; i < 1000; i++) {
        xrtFSMemPoolFree(pool, records[i]);
    }
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtFSMemPoolFree

释放内存池中的元素。

**函数原型：**
```c
XXAPI void xrtFSMemPoolFree(xfsmempool objMM, ptr p);
```

**参数：**
- `objMM` - 内存池对象
- `p` - 元素指针

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Item));
    
    Item* item1 = (Item*)xrtFSMemPoolAlloc(pool);
    Item* item2 = (Item*)xrtFSMemPoolAlloc(pool);
    Item* item3 = (Item*)xrtFSMemPoolAlloc(pool);
    
    item1->value = 10;
    item2->value = 20;
    item3->value = 30;
    
    // 释放中间元素
    xrtFSMemPoolFree(pool, item2);
    
    // 再分配会复用 item2 的槽位
    Item* item4 = (Item*)xrtFSMemPoolAlloc(pool);
    printf("item4 地址: %p (可能与 item2 相同: %p)\n", item4, item2);
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

**释放流程：**
1. 从指针前 4 字节读取 `ItemFlag`
2. 提取 MemUnit 索引（高位）和元素索引（低位）
3. 调用对应 MemUnit 的释放函数
4. 检查 MemUnit 状态，必要时调整链表分类

---

## GC回收

### xrtFSMemPoolGC_Mark

标记元素为 GC 使用中。

**定义（宏）：**
```c
#define xrtFSMemPoolGC_Mark xrtMemUnitGC_Mark
```

**参数：**
- 元素指针

---

### xrtFSMemPoolGC

执行一轮垃圾回收。

**函数原型：**
```c
XXAPI void xrtFSMemPoolGC(xfsmempool objMM, bool bFreeMark);
```

**参数：**
- `objMM` - 内存池对象
- `bFreeMark` - 回收模式
  - `TRUE` - 回收**被标记**的元素
  - `FALSE` - 回收**未被标记**的元素（标准 Mark-Sweep）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int refCount;
} Object;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Object));
    
    // 分配 10 个对象
    Object* objs[10];
    for (int i = 0; i < 10; i++) {
        objs[i] = (Object*)xrtFSMemPoolAlloc(pool);
        objs[i]->id = i;
        objs[i]->refCount = (i % 3 == 0) ? 1 : 0;  // 0,3,6,9 有引用
    }
    
    printf("GC 前分配了 10 个对象\n");
    
    // 标记有引用的对象
    for (int i = 0; i < 10; i++) {
        if (objs[i]->refCount > 0) {
            xrtFSMemPoolGC_Mark(objs[i]);
            printf("标记对象 %d\n", objs[i]->id);
        }
    }
    
    // 执行 GC（回收未标记的对象）
    xrtFSMemPoolGC(pool, FALSE);
    printf("GC 完成，回收了 6 个对象\n");
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

**GC 流程：**
1. 遍历 `LL_Idle` 和 `LL_Full` 中的所有 MemUnit
2. 对每个 MemUnit 调用 `xrtMemUnitGC`
3. 重新分类 MemUnit：空的移到 `LL_Null`/`LL_Free`，有空闲的移到 `LL_Idle`

---

## 使用场景

### 1. 高频对象分配

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int type;
    char data[64];
} Message;

xfsmempool g_msgPool = NULL;

Message* CreateMessage(int type, const char* data) {
    Message* msg = (Message*)xrtFSMemPoolAlloc(g_msgPool);
    if (msg) {
        msg->type = type;
        strncpy(msg->data, data, 63);
        msg->data[63] = '\0';
    }
    return msg;
}

void DestroyMessage(Message* msg) {
    xrtFSMemPoolFree(g_msgPool, msg);
}

int main() {
    xrtInit();
    
    g_msgPool = xrtFSMemPoolCreate(sizeof(Message));
    
    // 模拟高频消息处理
    for (int i = 0; i < 10000; i++) {
        Message* msg = CreateMessage(i % 10, "test message");
        // 处理消息...
        DestroyMessage(msg);
    }
    
    printf("处理了 10000 条消息\n");
    
    xrtFSMemPoolDestroy(g_msgPool);
    xrtUnit();
    return 0;
}
```

---

### 2. 链表节点池

```c
#include "xrt.h"
#include <stdio.h>

typedef struct ListNode {
    struct ListNode* next;
    int value;
} ListNode;

int main() {
    xrtInit();
    
    xfsmempool nodePool = xrtFSMemPoolCreate(sizeof(ListNode));
    ListNode* head = NULL;
    
    // 构建链表
    for (int i = 0; i < 100; i++) {
        ListNode* node = (ListNode*)xrtFSMemPoolAlloc(nodePool);
        node->value = i;
        node->next = head;
        head = node;
    }
    
    // 遍历链表
    int count = 0;
    ListNode* current = head;
    while (current) {
        count++;
        current = current->next;
    }
    printf("链表节点数: %d\n", count);
    
    // 释放链表
    current = head;
    while (current) {
        ListNode* next = current->next;
        xrtFSMemPoolFree(nodePool, current);
        current = next;
    }
    
    xrtFSMemPoolDestroy(nodePool);
    xrtUnit();
    return 0;
}
```

---

### 3. 带 GC 的对象管理

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int marked;  // 用于应用层标记
} GCObject;

xfsmempool g_heap = NULL;
GCObject* g_roots[10];  // 根对象
int g_rootCount = 0;

GCObject* AllocObject(int id) {
    GCObject* obj = (GCObject*)xrtFSMemPoolAlloc(g_heap);
    obj->id = id;
    obj->marked = 0;
    return obj;
}

void MarkPhase() {
    // 从根开始标记
    for (int i = 0; i < g_rootCount; i++) {
        if (g_roots[i]) {
            xrtFSMemPoolGC_Mark(g_roots[i]);
        }
    }
}

void SweepPhase() {
    xrtFSMemPoolGC(g_heap, FALSE);
}

int main() {
    xrtInit();
    
    g_heap = xrtFSMemPoolCreate(sizeof(GCObject));
    
    // 分配对象
    g_roots[g_rootCount++] = AllocObject(1);
    g_roots[g_rootCount++] = AllocObject(2);
    AllocObject(3);  // 无根引用
    AllocObject(4);  // 无根引用
    g_roots[g_rootCount++] = AllocObject(5);
    
    printf("分配了 5 个对象，3 个有根引用\n");
    
    // GC 周期
    MarkPhase();
    SweepPhase();
    
    printf("GC 完成，回收了 2 个无根对象\n");
    
    xrtFSMemPoolDestroy(g_heap);
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 选择合适的内存管理器

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // FSMemPool 适用于：
    // - 所有对象大小相同
    // - 对象数量可能超过 256
    // - 需要 GC 支持
    
    // 不适用：
    // - 对象大小不固定 → 使用 MemPool
    // - 对象数量固定且少 → 使用 Array
    // - 需要键值查找 → 使用 Dict
    
    printf("FSMemPool 最佳场景:\n");
    printf("1. 链表/树节点分配\n");
    printf("2. 消息/事件对象\n");
    printf("3. 临时计算对象\n");
    printf("4. 需要 GC 的对象堆\n");
    
    xrtUnit();
    return 0;
}
```

---

### 2. 避免跨池释放

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xfsmempool pool1 = xrtFSMemPoolCreate(sizeof(Item));
    xfsmempool pool2 = xrtFSMemPoolCreate(sizeof(Item));
    
    Item* item1 = (Item*)xrtFSMemPoolAlloc(pool1);
    Item* item2 = (Item*)xrtFSMemPoolAlloc(pool2);
    
    // ✓ 正确：释放到对应的池
    xrtFSMemPoolFree(pool1, item1);
    xrtFSMemPoolFree(pool2, item2);
    
    // ✗ 错误：跨池释放会导致未定义行为
    // xrtFSMemPoolFree(pool1, item2);  // 危险！
    
    xrtFSMemPoolDestroy(pool1);
    xrtFSMemPoolDestroy(pool2);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 批量操作优化

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int data[16]; } Block;

int main() {
    xrtInit();
    
    xfsmempool pool = xrtFSMemPoolCreate(sizeof(Block));
    
    // 预分配避免频繁扩展
    Block* blocks[1000];
    
    // 批量分配
    for (int i = 0; i < 1000; i++) {
        blocks[i] = (Block*)xrtFSMemPoolAlloc(pool);
    }
    printf("批量分配 1000 个块\n");
    
    // 批量释放（顺序无关紧要）
    for (int i = 999; i >= 0; i--) {
        xrtFSMemPoolFree(pool, blocks[i]);
    }
    printf("批量释放完成\n");
    
    xrtFSMemPoolDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

## 与其他内存管理器对比

| 特性 | FSMemPool | MemUnit | MemPool | malloc/free |
|------|-----------|---------|---------|-------------|
| **容量** | 无限制 | 256 | 无限制 | 无限制 |
| **元素大小** | 固定 | 固定 | 可变 | 可变 |
| **分配速度** | O(1) | O(1) | O(1) | O(n) |
| **内存碎片** | 无 | 无 | 低 | 高 |
| **GC 支持** | ✅ | ✅ | ✅ | ❌ |
| **复杂度** | 中 | 低 | 高 | - |
| **适用场景** | 固定大小对象池 | 底层组件 | 多尺寸对象 | 通用 |

---

## 相关文档

- [MemUnit 内存单元管理](api-memunit.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [MemPool 通用内存池](api-mempool.md)
- [LList 双向链表](api-llist.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#fsmempool-固定大小内存池)

</div>
