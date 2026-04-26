# LList 双向链表库

> 自动内存管理的双向链表（基于 FSMemPool）

[English](api-llist.en.md) | [中文](api-llist.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [链表管理](#链表管理)
- [节点操作](#节点操作)
- [遍历](#遍历)
- [使用场景](#使用场景)
- [与LList Base对比](#与llist-base对比)
- [最佳实践](#最佳实践)

---

## 概述

LList（Linked List）是基于 FSMemPool 的双向链表实现，自动管理节点内存的分配和释放。

### 核心特点

- **自动内存管理**：使用 FSMemPool 管理节点内存
- **双向遍历**：支持正向和反向遍历
- **O(1) 插入/删除**：任意位置插入删除都是常数时间
- **GC 支持**：底层内存池支持垃圾回收

### 节点内存布局

```
节点内存结构：
┌────────────────────────────────────────┐
│           xllistnode_struct            │  ← node 指针指向这里
│   ┌──────────────┬──────────────┐      │
│   │     Prev     │     Next     │      │
│   │   (指针)     │   (指针)     │      │
│   └──────────────┴──────────────┘      │
├────────────────────────────────────────┤
│              用户数据                   │  ← &node[1] 指向这里
│         (iItemLength 字节)             │
└────────────────────────────────────────┘
```

**关键：** 用户数据位于 `&node[1]`，即节点结构之后！

---

## 数据结构

### xllist_struct

链表管理结构。

**定义：**
```c
typedef struct {
    xllistnode FirstNode;       // 首节点指针
    xllistnode LastNode;        // 尾节点指针
    uint32 Count;               // 节点数量
    xfsmempool_struct objMM;    // 内存池管理器
} xllist_struct, *xllist;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `FirstNode` | 链表首节点，空链表为 `NULL` |
| `LastNode` | 链表尾节点，空链表为 `NULL` |
| `Count` | 当前节点数量 |
| `objMM` | FSMemPool 内存池，自动管理节点内存 |

---

## 链表管理

### xrtLListCreate

创建链表。

**函数原型：**
```c
XXAPI xllist xrtLListCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 节点用户数据大小（字节）

**返回值：**
- 成功：链表对象
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtLListDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Person;

int main() {
    xrtInit();
    
    // 创建链表，节点存储 Person 结构
    xllist list = xrtLListCreate(sizeof(Person));
    printf("链表已创建，节点数: %u\n", list->Count);  // 0
    
    // 添加节点
    xllistnode node = xrtLListInsertNext(list, NULL);
    Person* p = (Person*)&node[1];  // 获取用户数据指针
    p->id = 1;
    strcpy(p->name, "Alice");
    
    printf("添加后节点数: %u\n", list->Count);  // 1
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtLListDestroy

销毁链表。

**函数原型：**
```c
XXAPI void xrtLListDestroy(xllist objLL);
```

**参数：**
- `objLL` - 链表对象

**说明：**
- 释放所有节点内存
- 释放内存池
- 释放链表结构体本身

---

### xrtLListInit

初始化链表（用于栈上或嵌入式使用）。

**函数原型：**
```c
XXAPI void xrtLListInit(xllist objLL, uint32 iItemLength);
```

**参数：**
- `objLL` - 链表指针
- `iItemLength` - 节点用户数据大小

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllist_struct taskList;
    int maxTasks;
} TaskManager;

int main() {
    xrtInit();
    
    // 链表嵌入其他结构
    TaskManager mgr;
    mgr.maxTasks = 100;
    xrtLListInit(&mgr.taskList, sizeof(int));
    
    // 添加任务
    xllistnode node = xrtLListInsertNext(&mgr.taskList, NULL);
    int* taskId = (int*)&node[1];
    *taskId = 42;
    
    printf("任务数: %u\n", mgr.taskList.Count);  // 1
    
    xrtLListUnit(&mgr.taskList);
    xrtUnit();
    return 0;
}
```

---

### xrtLListUnit

释放链表资源（不释放结构体本身）。

**函数原型：**
```c
XXAPI void xrtLListUnit(xllist objLL);
```

**参数：**
- `objLL` - 链表对象

**说明：**
- 与 `xrtLListInit` 配对使用
- 释放所有节点和内存池，但不释放 `objLL` 本身

---

### xrtLListRemoveAll / xrtLListClear

删除所有成员 / 清空链表（宏）。

**定义：**
```c
#define xrtLListRemoveAll xrtLListUnit
#define xrtLListClear xrtLListUnit
```

**说明：** 这两个宏等同于 `xrtLListUnit`，释放所有节点。

---

## 节点操作

### xrtLListInsertNext

在节点后插入新节点。

**函数原型：**
```c
XXAPI xllistnode xrtLListInsertNext(xllist objLL, xllistnode objNode);
```

**参数：**
- `objLL` - 链表对象
- `objNode` - 参考节点（`NULL` 表示插入到链表末尾）

**返回值：**
- 成功：新节点指针
- 失败：`NULL`

**行为：**
- `objNode != NULL`：在 `objNode` 后面插入
- `objNode == NULL`：插入到链表末尾（成为新的 `LastNode`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} IntData;

void PrintList(xllist list) {
    printf("链表: ");
    xllistnode node = list->FirstNode;
    while (node) {
        IntData* d = (IntData*)&node[1];
        printf("%d ", d->value);
        node = node->Next;
    }
    printf("(共 %u 个)\n", list->Count);
}

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(IntData));
    
    // 追加到末尾 (objNode = NULL)
    for (int i = 1; i <= 3; i++) {
        xllistnode node = xrtLListInsertNext(list, NULL);
        IntData* d = (IntData*)&node[1];
        d->value = i * 10;
    }
    PrintList(list);  // 链表: 10 20 30 (共 3 个)
    
    // 在首节点后插入
    xllistnode n15 = xrtLListInsertNext(list, list->FirstNode);
    ((IntData*)&n15[1])->value = 15;
    PrintList(list);  // 链表: 10 15 20 30 (共 4 个)
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtLListInsertPrev

在节点前插入新节点。

**函数原型：**
```c
XXAPI xllistnode xrtLListInsertPrev(xllist objLL, xllistnode objNode);
```

**参数：**
- `objLL` - 链表对象
- `objNode` - 参考节点（`NULL` 表示插入到链表开头）

**返回值：**
- 成功：新节点指针
- 失败：`NULL`

**行为：**
- `objNode != NULL`：在 `objNode` 前面插入
- `objNode == NULL`：插入到链表开头（成为新的 `FirstNode`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } IntData;

void PrintList(xllist list) {
    printf("链表: ");
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", ((IntData*)&n[1])->value);
        n = n->Next;
    }
    printf("\n");
}

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(IntData));
    
    // 插入到开头 (objNode = NULL)
    for (int i = 3; i >= 1; i--) {
        xllistnode n = xrtLListInsertPrev(list, NULL);
        ((IntData*)&n[1])->value = i * 10;
    }
    PrintList(list);  // 链表: 10 20 30
    
    // 在尾节点前插入
    xllistnode n25 = xrtLListInsertPrev(list, list->LastNode);
    ((IntData*)&n25[1])->value = 25;
    PrintList(list);  // 链表: 10 20 25 30
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### xrtLListRemove

删除节点。

**函数原型：**
```c
XXAPI void xrtLListRemove(xllist objLL, xllistnode objNode);
```

**参数：**
- `objLL` - 链表对象
- `objNode` - 要删除的节点

**说明：**
- 从链表中移除节点
- 自动释放节点内存（归还给内存池）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } IntData;

void PrintList(xllist list) {
    printf("链表 (共 %u 个): ", list->Count);
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", ((IntData*)&n[1])->value);
        n = n->Next;
    }
    printf("\n");
}

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(IntData));
    
    // 创建链表: 10 -> 20 -> 30 -> 40
    xllistnode nodes[4];
    for (int i = 0; i < 4; i++) {
        nodes[i] = xrtLListInsertNext(list, NULL);
        ((IntData*)&nodes[i][1])->value = (i + 1) * 10;
    }
    PrintList(list);  // 链表 (共 4 个): 10 20 30 40
    
    // 删除中间节点 (20)
    xrtLListRemove(list, nodes[1]);
    PrintList(list);  // 链表 (共 3 个): 10 30 40
    
    // 删除首节点
    xrtLListRemove(list, list->FirstNode);
    PrintList(list);  // 链表 (共 2 个): 30 40
    
    // 删除尾节点
    xrtLListRemove(list, list->LastNode);
    PrintList(list);  // 链表 (共 1 个): 30
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## 遍历

### 正向遍历

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Person;

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(Person));
    
    // 添加数据
    char* names[] = {"Alice", "Bob", "Charlie"};
    for (int i = 0; i < 3; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        Person* p = (Person*)&n[1];
        p->id = i + 1;
        strcpy(p->name, names[i]);
    }
    
    // 正向遍历（从头到尾）
    printf("正向遍历:\n");
    xllistnode node = list->FirstNode;
    while (node) {
        Person* p = (Person*)&node[1];
        printf("  ID=%d, Name=%s\n", p->id, p->name);
        node = node->Next;
    }
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 反向遍历

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(int));
    
    // 添加 1, 2, 3
    for (int i = 1; i <= 3; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        *(int*)&n[1] = i * 10;
    }
    
    // 反向遍历（从尾到头）
    printf("反向遍历: ");
    xllistnode node = list->LastNode;
    while (node) {
        printf("%d ", *(int*)&node[1]);
        node = node->Prev;
    }
    printf("\n");  // 反向遍历: 30 20 10
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 安全遍历（支持删除）

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(int));
    
    // 添加 10, 20, 30, 40, 50
    for (int i = 1; i <= 5; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        *(int*)&n[1] = i * 10;
    }
    
    // 安全遍历，删除偶数
    xllistnode current = list->FirstNode;
    while (current) {
        xllistnode next = current->Next;  // 先保存下一个
        int value = *(int*)&current[1];
        
        if (value % 20 == 0) {
            printf("删除 %d\n", value);
            xrtLListRemove(list, current);
        }
        
        current = next;  // 使用保存的下一个
    }
    
    // 打印剩余
    printf("剩余: ");
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", *(int*)&n[1]);
        n = n->Next;
    }
    printf("\n");  // 剩余: 10 30 50
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 队列实现

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllist queue;
} Queue;

Queue* CreateQueue(size_t itemSize) {
    Queue* q = xrtMalloc(sizeof(Queue));
    q->queue = xrtLListCreate(itemSize);
    return q;
}

void DestroyQueue(Queue* q) {
    xrtLListDestroy(q->queue);
    xrtFree(q);
}

void Enqueue(Queue* q, ptr item, size_t size) {
    xllistnode node = xrtLListInsertNext(q->queue, NULL);
    if (node) {
        memcpy(&node[1], item, size);
    }
}

bool Dequeue(Queue* q, ptr item, size_t size) {
    if (!q->queue->FirstNode) return FALSE;
    xllistnode node = q->queue->FirstNode;
    memcpy(item, &node[1], size);
    xrtLListRemove(q->queue, node);
    return TRUE;
}

int main() {
    xrtInit();
    
    Queue* q = CreateQueue(sizeof(int));
    
    // 入队
    for (int i = 1; i <= 5; i++) {
        Enqueue(q, &i, sizeof(int));
        printf("入队: %d\n", i);
    }
    
    // 出队
    int value;
    while (Dequeue(q, &value, sizeof(int))) {
        printf("出队: %d\n", value);
    }
    
    DestroyQueue(q);
    xrtUnit();
    return 0;
}
```

---

### 2. LRU缓存

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int key;
    int value;
} CacheItem;

int main() {
    xrtInit();
    
    xllist lru_list = xrtLListCreate(sizeof(CacheItem));
    int capacity = 3;
    
    // 添加缓存项
    for (int i = 1; i <= 4; i++) {
        // 如果超过容量，淘汰最老的（尾部）
        if (lru_list->Count >= (uint32)capacity) {
            xllistnode oldest = lru_list->LastNode;
            CacheItem* item = (CacheItem*)&oldest[1];
            printf("淘汰: key=%d, value=%d\n", item->key, item->value);
            xrtLListRemove(lru_list, oldest);
        }
        
        // 插入到头部（最近使用）
        xllistnode node = xrtLListInsertPrev(lru_list, NULL);
        CacheItem* item = (CacheItem*)&node[1];
        item->key = i;
        item->value = i * 100;
        printf("添加: key=%d, value=%d\n", item->key, item->value);
    }
    
    // 打印当前缓存
    printf("\n当前缓存 (从新到旧):\n");
    xllistnode n = lru_list->FirstNode;
    while (n) {
        CacheItem* item = (CacheItem*)&n[1];
        printf("  key=%d, value=%d\n", item->key, item->value);
        n = n->Next;
    }
    
    xrtLListDestroy(lru_list);
    xrtUnit();
    return 0;
}
```

---

### 3. 任务调度器

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int priority;  // 数值越小优先级越高
    char name[32];
} Task;

void InsertByPriority(xllist list, Task* newTask) {
    xllistnode pos = list->FirstNode;
    
    // 找到第一个优先级更低的位置
    while (pos) {
        Task* t = (Task*)&pos[1];
        if (t->priority > newTask->priority) {
            break;
        }
        pos = pos->Next;
    }
    
    xllistnode node;
    if (pos) {
        node = xrtLListInsertPrev(list, pos);
    } else {
        node = xrtLListInsertNext(list, NULL);
    }
    
    if (node) {
        memcpy(&node[1], newTask, sizeof(Task));
    }
}

int main() {
    xrtInit();
    
    xllist taskList = xrtLListCreate(sizeof(Task));
    
    // 添加任务（无序）
    Task tasks[] = {
        {1, 3, "Low Priority"},
        {2, 1, "High Priority"},
        {3, 2, "Medium Priority"},
        {4, 1, "High Priority 2"}
    };
    
    for (int i = 0; i < 4; i++) {
        InsertByPriority(taskList, &tasks[i]);
    }
    
    // 打印排序后的任务列表
    printf("任务列表 (按优先级):\n");
    xllistnode n = taskList->FirstNode;
    while (n) {
        Task* t = (Task*)&n[1];
        printf("  [%d] %s (priority=%d)\n", t->id, t->name, t->priority);
        n = n->Next;
    }
    
    xrtLListDestroy(taskList);
    xrtUnit();
    return 0;
}
```

---

## 与LList Base对比

| 特性 | LList | LList Base |
|------|-------|------------|
| **内存管理** | 自动（FSMemPool） | 手动 |
| **节点分配** | `xrtLListInsert*` 自动分配 | 用户自行分配 |
| **节点释放** | `xrtLListRemove` 自动释放 | 用户自行释放 |
| **GC 支持** | ✅ 底层 FSMemPool 支持 | ❌ 无 |
| **灵活性** | 中等 | 最高 |
| **适用场景** | 一般链表需求 | 自定义内存管理 |
| **头文件** | `llist.h` | `llist_base.h` |

### 选择建议

- **使用 LList**：大多数场景，需要自动内存管理
- **使用 LList Base**：嵌入式环境、自定义内存分配器、需要完全控制内存

---

## 最佳实践

### 1. 正确访问用户数据

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
    char name[32];
} MyData;

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(MyData));
    xllistnode node = xrtLListInsertNext(list, NULL);
    
    // ✅ 正确：使用 &node[1] 获取用户数据
    MyData* data = (MyData*)&node[1];
    data->value = 42;
    strcpy(data->name, "Test");
    
    // ❌ 错误：直接使用 node 作为数据（node 是链表节点结构）
    // MyData* wrong = (MyData*)node;  // 这会覆盖 Prev/Next 指针！
    
    printf("Value: %d, Name: %s\n", data->value, data->name);
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 2. 安全删除遍历

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(int));
    
    // 添加 1-10
    for (int i = 1; i <= 10; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        *(int*)&n[1] = i;
    }
    
    // ❌ 错误：删除后继续使用 node->Next
    /*
    xllistnode node = list->FirstNode;
    while (node) {
        if (*(int*)&node[1] % 2 == 0) {
            xrtLListRemove(list, node);  // node 已失效
        }
        node = node->Next;  // 错误！node 已被释放
    }
    */
    
    // ✅ 正确：先保存 Next
    xllistnode current = list->FirstNode;
    while (current) {
        xllistnode next = current->Next;  // 先保存
        if (*(int*)&current[1] % 2 == 0) {
            xrtLListRemove(list, current);
        }
        current = next;  // 使用保存的值
    }
    
    printf("剩余 (奇数): ");
    xllistnode n = list->FirstNode;
    while (n) {
        printf("%d ", *(int*)&n[1]);
        n = n->Next;
    }
    printf("\n");  // 1 3 5 7 9
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

### 3. 使用辅助宏简化代码

```c
#include "xrt.h"
#include <stdio.h>

// 定义辅助宏
#define LLIST_DATA(node, type) ((type*)&(node)[1])
#define LLIST_FOREACH(list, node) \
    for (xllistnode node = (list)->FirstNode; node; node = node->Next)
#define LLIST_FOREACH_REVERSE(list, node) \
    for (xllistnode node = (list)->LastNode; node; node = node->Prev)

typedef struct {
    int id;
    char name[32];
} Item;

int main() {
    xrtInit();
    
    xllist list = xrtLListCreate(sizeof(Item));
    
    // 添加数据
    char* names[] = {"Alpha", "Beta", "Gamma"};
    for (int i = 0; i < 3; i++) {
        xllistnode n = xrtLListInsertNext(list, NULL);
        Item* item = LLIST_DATA(n, Item);
        item->id = i + 1;
        strcpy(item->name, names[i]);
    }
    
    // 使用宏遍历
    printf("正向:\n");
    LLIST_FOREACH(list, n) {
        Item* item = LLIST_DATA(n, Item);
        printf("  [%d] %s\n", item->id, item->name);
    }
    
    printf("反向:\n");
    LLIST_FOREACH_REVERSE(list, n) {
        Item* item = LLIST_DATA(n, Item);
        printf("  [%d] %s\n", item->id, item->name);
    }
    
    xrtLListDestroy(list);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Stack 栈](api-stack.md)
- [List 列表](api-list.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#llist-双向链表库)

</div>
