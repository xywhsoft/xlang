# LList Base 链表基础库

> 双向链表底层操作，用户自行管理节点内存

[English](api-llist-base.en.md) | [中文](api-llist-base.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [链表管理](#链表管理)
- [节点操作](#节点操作)
- [使用场景](#使用场景)
- [与LList对比](#与llist对比)
- [最佳实践](#最佳实践)

---

## 概述

LList Base（Linked List Base）是双向链表的底层实现，提供基础的链表操作。与高级链表（LList）不同，用户需要**自行管理节点内存**。

### 核心特点

- **底层操作**：只管理链表结构，不管理节点内存
- **灵活性高**：节点可以来自任何内存分配器
- **零开销**：无额外内存管理开销
- **嵌入式友好**：适合自定义内存管理场景

### 链表结构

```
xllistbase_struct：
┌────────────┬────────────┬────────────┐
│ FirstNode  │  LastNode  │   Count    │
└─────┬──────┴─────┬──────┴────────────┘
      │            │
      ▼            ▼
┌─────────┐   ┌─────────┐   ┌─────────┐
│  Node1  │◄─►│  Node2  │◄─►│  Node3  │
│ Prev=N  │   │         │   │ Next=N  │
└─────────┘   └─────────┘   └─────────┘
      ▲                           ▲
      │                           │
  FirstNode                   LastNode
```

### 节点内存布局

用户自定义节点结构时，**必须将 `xllistnode_struct` 作为第一个成员**：

```c
// 自定义节点结构
typedef struct {
    xllistnode_struct node;  // 必须是第一个成员！
    // 用户数据...
    int id;
    char name[32];
} MyNode;
```

---

## 数据结构

### xllistnode_struct

链表节点基础结构。

**定义：**
```c
typedef struct LList_NodeBase {
    struct LList_NodeBase* Prev;  // 前驱节点指针
    struct LList_NodeBase* Next;  // 后继节点指针
} xllistnode_struct, *xllistnode;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `Prev` | 指向前一个节点，首节点为 `NULL` |
| `Next` | 指向后一个节点，尾节点为 `NULL` |

---

### xllistbase_struct

链表管理结构。

**定义：**
```c
typedef struct {
    xllistnode FirstNode;  // 首节点指针
    xllistnode LastNode;   // 尾节点指针
    uint32 Count;          // 节点数量
} xllistbase_struct, *xllistbase;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `FirstNode` | 链表首节点，空链表为 `NULL` |
| `LastNode` | 链表尾节点，空链表为 `NULL` |
| `Count` | 当前节点数量 |

---

## 链表管理

### xrtLLB_Init

初始化链表（宏）。

**定义：**
```c
#define xrtLLB_Init(o) (o)->FirstNode = NULL; (o)->LastNode = NULL; (o)->Count = 0
```

**参数：**
- `o` - 链表对象指针

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上分配链表管理器
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    printf("链表已初始化\n");
    printf("首节点: %p\n", list.FirstNode);  // (nil)
    printf("尾节点: %p\n", list.LastNode);   // (nil)
    printf("节点数: %u\n", list.Count);       // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_Unit

释放链表（宏）。

**定义：**
```c
#define xrtLLB_Unit xrtLLB_Init
```

**说明：**
- 仅重置链表状态，**不释放节点内存**
- 用户需要自行释放所有节点

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} MyNode;

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // 添加节点（用户自行分配内存）
    MyNode* n1 = xrtMalloc(sizeof(MyNode));
    n1->value = 100;
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n1);
    
    MyNode* n2 = xrtMalloc(sizeof(MyNode));
    n2->value = 200;
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n2);
    
    printf("节点数: %u\n", list.Count);  // 2
    
    // 释放所有节点（用户责任）
    xllistnode current = list.FirstNode;
    while (current) {
        xllistnode next = current->Next;
        xrtFree(current);
        current = next;
    }
    
    // 重置链表状态
    xrtLLB_Unit(&list);
    printf("释放后节点数: %u\n", list.Count);  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_RemoveAll / xrtLLB_Clear

删除所有成员 / 清空管理器（宏）。

**定义：**
```c
#define xrtLLB_RemoveAll xrtLLB_Unit
#define xrtLLB_Clear xrtLLB_Unit
```

**⚠️ 警告：** 这些宏仅重置链表状态，不释放节点内存！

---

## 节点操作

### xrtLLB_InsertPrev

在节点前插入新节点。

**函数原型：**
```c
XXAPI void xrtLLB_InsertPrev(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
```

**参数：**
- `objLLB` - 链表对象
- `objNode` - 参考节点（`NULL` 表示插入到链表最前）
- `objNewNode` - 要插入的新节点

**行为：**
- `objNode != NULL`：在 `objNode` 前面插入
- `objNode == NULL`：插入到链表最前（成为新的 `FirstNode`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

IntNode* CreateNode(int value) {
    IntNode* n = xrtMalloc(sizeof(IntNode));
    n->value = value;
    return n;
}

void PrintList(xllistbase list) {
    printf("链表: ");
    xllistnode current = list->FirstNode;
    while (current) {
        printf("%d ", ((IntNode*)current)->value);
        current = current->Next;
    }
    printf("(共 %u 个)\n", list->Count);
}

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // 在空链表插入（成为首节点）
    IntNode* n2 = CreateNode(20);
    xrtLLB_InsertPrev(&list, NULL, (xllistnode)n2);
    PrintList(&list);  // 链表: 20 (共 1 个)
    
    // 在首节点前插入
    IntNode* n1 = CreateNode(10);
    xrtLLB_InsertPrev(&list, list.FirstNode, (xllistnode)n1);
    PrintList(&list);  // 链表: 10 20 (共 2 个)
    
    // 在指定节点前插入
    IntNode* n15 = CreateNode(15);
    xrtLLB_InsertPrev(&list, (xllistnode)n2, (xllistnode)n15);
    PrintList(&list);  // 链表: 10 15 20 (共 3 个)
    
    // 清理
    xllistnode c = list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_InsertNext

在节点后插入新节点。

**函数原型：**
```c
XXAPI void xrtLLB_InsertNext(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
```

**参数：**
- `objLLB` - 链表对象
- `objNode` - 参考节点（`NULL` 表示插入到链表末尾）
- `objNewNode` - 要插入的新节点

**行为：**
- `objNode != NULL`：在 `objNode` 后面插入
- `objNode == NULL`：插入到链表末尾（成为新的 `LastNode`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

IntNode* CreateNode(int value) {
    IntNode* n = xrtMalloc(sizeof(IntNode));
    n->value = value;
    return n;
}

void PrintList(xllistbase list) {
    printf("链表: ");
    xllistnode current = list->FirstNode;
    while (current) {
        printf("%d ", ((IntNode*)current)->value);
        current = current->Next;
    }
    printf("\n");
}

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // 追加到末尾（objNode = NULL）
    xrtLLB_InsertNext(&list, NULL, (xllistnode)CreateNode(10));
    xrtLLB_InsertNext(&list, NULL, (xllistnode)CreateNode(20));
    xrtLLB_InsertNext(&list, NULL, (xllistnode)CreateNode(30));
    PrintList(&list);  // 链表: 10 20 30
    
    // 在首节点后插入
    IntNode* n15 = CreateNode(15);
    xrtLLB_InsertNext(&list, list.FirstNode, (xllistnode)n15);
    PrintList(&list);  // 链表: 10 15 20 30
    
    // 清理
    xllistnode c = list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtLLB_Remove

从链表中删除节点。

**函数原型：**
```c
XXAPI void xrtLLB_Remove(xllistbase objLLB, xllistnode objNode);
```

**参数：**
- `objLLB` - 链表对象
- `objNode` - 要删除的节点

**说明：**
- 只从链表中移除节点，**不释放节点内存**
- 用户需要自行释放删除的节点

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

IntNode* CreateNode(int value) {
    IntNode* n = xrtMalloc(sizeof(IntNode));
    n->value = value;
    return n;
}

void PrintList(xllistbase list) {
    printf("链表: ");
    xllistnode c = list->FirstNode;
    while (c) {
        printf("%d ", ((IntNode*)c)->value);
        c = c->Next;
    }
    printf("(共 %u 个)\n", list->Count);
}

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // 创建链表: 10 -> 20 -> 30 -> 40
    IntNode* n1 = CreateNode(10);
    IntNode* n2 = CreateNode(20);
    IntNode* n3 = CreateNode(30);
    IntNode* n4 = CreateNode(40);
    
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n1);
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n2);
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n3);
    xrtLLB_InsertNext(&list, NULL, (xllistnode)n4);
    PrintList(&list);  // 链表: 10 20 30 40 (共 4 个)
    
    // 删除中间节点
    xrtLLB_Remove(&list, (xllistnode)n2);
    xrtFree(n2);  // 用户释放内存
    PrintList(&list);  // 链表: 10 30 40 (共 3 个)
    
    // 删除首节点
    xrtLLB_Remove(&list, (xllistnode)n1);
    xrtFree(n1);
    PrintList(&list);  // 链表: 30 40 (共 2 个)
    
    // 删除尾节点
    xrtLLB_Remove(&list, (xllistnode)n4);
    xrtFree(n4);
    PrintList(&list);  // 链表: 30 (共 1 个)
    
    // 清理最后一个节点
    xrtFree(n3);
    
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 自定义内存管理

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int id;
    char data[128];
} DataNode;

// 使用 BSMM 管理节点内存
typedef struct {
    xllistbase_struct list;
    xbsmm_struct nodeMgr;
} CustomList;

void CustomListInit(CustomList* cl) {
    xrtLLB_Init(&cl->list);
    xrtBsmmInit(&cl->nodeMgr, sizeof(DataNode));
}

void CustomListUnit(CustomList* cl) {
    xrtLLB_Unit(&cl->list);
    xrtBsmmUnit(&cl->nodeMgr);
}

DataNode* CustomListAppend(CustomList* cl, int id, str data) {
    DataNode* n = (DataNode*)xrtBsmmAlloc(&cl->nodeMgr);
    n->id = id;
    strncpy(n->data, (char*)data, 127);
    xrtLLB_InsertNext(&cl->list, NULL, (xllistnode)n);
    return n;
}

void CustomListRemove(CustomList* cl, DataNode* n) {
    xrtLLB_Remove(&cl->list, (xllistnode)n);
    xrtBsmmFree(&cl->nodeMgr, n);
}

int main() {
    xrtInit();
    
    CustomList myList;
    CustomListInit(&myList);
    
    // 添加节点
    CustomListAppend(&myList, 1, (str)"First");
    CustomListAppend(&myList, 2, (str)"Second");
    DataNode* n3 = CustomListAppend(&myList, 3, (str)"Third");
    
    printf("节点数: %u\n", myList.list.Count);  // 3
    
    // 删除节点
    CustomListRemove(&myList, n3);
    printf("删除后: %u\n", myList.list.Count);  // 2
    
    CustomListUnit(&myList);
    xrtUnit();
    return 0;
}
```

---

### 2. 嵌入式链表（节点嵌入其他结构）

```c
#include "xrt.h"
#include <stdio.h>

// 任务结构（链表节点是其中一部分）
typedef struct {
    xllistnode_struct readyNode;   // 就绪队列节点
    xllistnode_struct waitNode;    // 等待队列节点
    int taskId;
    int priority;
} Task;

// 从节点指针获取任务指针的宏
#define TASK_FROM_READY_NODE(n) ((Task*)(n))
#define TASK_FROM_WAIT_NODE(n) ((Task*)((char*)(n) - offsetof(Task, waitNode)))

int main() {
    xrtInit();
    
    // 两个任务队列
    xllistbase_struct readyQueue;
    xllistbase_struct waitQueue;
    xrtLLB_Init(&readyQueue);
    xrtLLB_Init(&waitQueue);
    
    // 创建任务
    Task* t1 = xrtMalloc(sizeof(Task));
    t1->taskId = 1;
    t1->priority = 5;
    
    Task* t2 = xrtMalloc(sizeof(Task));
    t2->taskId = 2;
    t2->priority = 10;
    
    // t1 在就绪队列
    xrtLLB_InsertNext(&readyQueue, NULL, &t1->readyNode);
    
    // t2 同时在两个队列
    xrtLLB_InsertNext(&readyQueue, NULL, &t2->readyNode);
    xrtLLB_InsertNext(&waitQueue, NULL, &t2->waitNode);
    
    printf("就绪队列: %u 个任务\n", readyQueue.Count);  // 2
    printf("等待队列: %u 个任务\n", waitQueue.Count);   // 1
    
    // 从等待队列移除 t2
    xrtLLB_Remove(&waitQueue, &t2->waitNode);
    printf("等待队列: %u 个任务\n", waitQueue.Count);   // 0
    
    // 清理
    xrtFree(t1);
    xrtFree(t2);
    
    xrtUnit();
    return 0;
}
```

---

### 3. LRU缓存

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    xllistnode_struct node;
    char key[32];
    char value[128];
} CacheEntry;

typedef struct {
    xllistbase_struct list;
    uint32 maxSize;
} LRUCache;

void LRUInit(LRUCache* cache, uint32 maxSize) {
    xrtLLB_Init(&cache->list);
    cache->maxSize = maxSize;
}

CacheEntry* LRUFind(LRUCache* cache, str key) {
    xllistnode c = cache->list.FirstNode;
    while (c) {
        CacheEntry* e = (CacheEntry*)c;
        if (strcmp(e->key, (char*)key) == 0) {
            // 移动到头部（最近使用）
            xrtLLB_Remove(&cache->list, c);
            xrtLLB_InsertPrev(&cache->list, NULL, c);
            return e;
        }
        c = c->Next;
    }
    return NULL;
}

void LRUPut(LRUCache* cache, str key, str value) {
    // 检查是否已存在
    CacheEntry* e = LRUFind(cache, key);
    if (e) {
        strncpy(e->value, (char*)value, 127);
        return;
    }
    
    // 淘汰最旧的（尾部）
    if (cache->list.Count >= cache->maxSize) {
        xllistnode last = cache->list.LastNode;
        xrtLLB_Remove(&cache->list, last);
        printf("淘汰: %s\n", ((CacheEntry*)last)->key);
        xrtFree(last);
    }
    
    // 添加新条目到头部
    e = xrtMalloc(sizeof(CacheEntry));
    strncpy(e->key, (char*)key, 31);
    strncpy(e->value, (char*)value, 127);
    xrtLLB_InsertPrev(&cache->list, NULL, (xllistnode)e);
}

void LRUUnit(LRUCache* cache) {
    xllistnode c = cache->list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    xrtLLB_Unit(&cache->list);
}

int main() {
    xrtInit();
    
    LRUCache cache;
    LRUInit(&cache, 3);
    
    LRUPut(&cache, (str)"a", (str)"value_a");
    LRUPut(&cache, (str)"b", (str)"value_b");
    LRUPut(&cache, (str)"c", (str)"value_c");
    printf("缓存大小: %u\n", cache.list.Count);  // 3
    
    // 访问 a（移到头部）
    CacheEntry* e = LRUFind(&cache, (str)"a");
    printf("访问 a: %s\n", e->value);
    
    // 添加 d，淘汰最旧的 b
    LRUPut(&cache, (str)"d", (str)"value_d");  // 输出: 淘汰: b
    
    LRUUnit(&cache);
    xrtUnit();
    return 0;
}
```

---

## 与LList对比

| 特性 | LList Base | LList |
|------|------------|-------|
| **内存管理** | 用户自行管理 | 自动管理（FSMemPool） |
| **节点分配** | 用户分配 | `xrtLListInsert*` 自动分配 |
| **节点释放** | 用户释放 | `xrtLListRemove` 自动释放 |
| **灵活性** | 高 | 中 |
| **使用复杂度** | 高 | 低 |
| **适用场景** | 自定义内存管理、嵌入式 | 一般用途 |

### 选择建议

- **使用 LList Base**：需要自定义内存管理、节点属于多个链表、嵌入式系统
- **使用 LList**：一般用途、希望自动管理内存

---

## 最佳实践

### 1. 节点结构定义

```c
#include "xrt.h"

// ✅ 正确：xllistnode_struct 作为第一个成员
typedef struct {
    xllistnode_struct node;  // 必须是第一个！
    int id;
    char name[32];
} CorrectNode;

// ❌ 错误：xllistnode_struct 不是第一个成员
typedef struct {
    int id;
    xllistnode_struct node;  // 位置错误！
    char name[32];
} WrongNode;

int main() {
    xrtInit();
    
    CorrectNode* n = xrtMalloc(sizeof(CorrectNode));
    n->id = 1;
    
    // ✅ 正确：可以直接转换
    xllistnode nodePtr = (xllistnode)n;
    CorrectNode* back = (CorrectNode*)nodePtr;
    printf("ID: %d\n", back->id);  // 1
    
    xrtFree(n);
    xrtUnit();
    return 0;
}
```

---

### 2. 安全遍历（支持删除）

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // 添加节点
    for (int i = 1; i <= 5; i++) {
        IntNode* n = xrtMalloc(sizeof(IntNode));
        n->value = i * 10;
        xrtLLB_InsertNext(&list, NULL, (xllistnode)n);
    }
    
    // ✅ 安全遍历（可以删除）
    xllistnode current = list.FirstNode;
    while (current) {
        xllistnode next = current->Next;  // 先保存下一个
        IntNode* n = (IntNode*)current;
        
        if (n->value == 30) {
            xrtLLB_Remove(&list, current);
            xrtFree(current);
            printf("删除 30\n");
        } else {
            printf("保留 %d\n", n->value);
        }
        
        current = next;  // 使用保存的下一个
    }
    
    // 清理剩余节点
    current = list.FirstNode;
    while (current) {
        xllistnode next = current->Next;
        xrtFree(current);
        current = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

### 3. 反向遍历

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xllistnode_struct node;
    int value;
} IntNode;

int main() {
    xrtInit();
    
    xllistbase_struct list;
    xrtLLB_Init(&list);
    
    // 添加节点: 10 -> 20 -> 30
    for (int i = 1; i <= 3; i++) {
        IntNode* n = xrtMalloc(sizeof(IntNode));
        n->value = i * 10;
        xrtLLB_InsertNext(&list, NULL, (xllistnode)n);
    }
    
    // 正向遍历
    printf("正向: ");
    xllistnode c = list.FirstNode;
    while (c) {
        printf("%d ", ((IntNode*)c)->value);
        c = c->Next;
    }
    printf("\n");  // 正向: 10 20 30
    
    // 反向遍历
    printf("反向: ");
    c = list.LastNode;
    while (c) {
        printf("%d ", ((IntNode*)c)->value);
        c = c->Prev;
    }
    printf("\n");  // 反向: 30 20 10
    
    // 清理
    c = list.FirstNode;
    while (c) {
        xllistnode next = c->Next;
        xrtFree(c);
        c = next;
    }
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [LList 双向链表](api-llist.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#llist-base-链表基础库)

</div>
