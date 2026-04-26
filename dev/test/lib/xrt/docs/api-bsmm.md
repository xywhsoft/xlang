# BSMM 块结构内存管理库

> Blocks Struct Memory Management - 高效的固定大小结构体内存池

[English](api-bsmm.en.md) | [中文](api-bsmm.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [管理器操作](#管理器操作)
- [内存分配](#内存分配)
- [使用场景](#使用场景)
- [与其他内存管理器对比](#与其他内存管理器对比)
- [最佳实践](#最佳实践)

---

## 概述

BSMM（Blocks Struct Memory Management）是一个块结构内存管理器，专为频繁分配和释放**固定大小**结构体设计。

### 核心特点

- **分页管理**：每页存储 256 个元素，按需分配新页
- **空闲复用**：释放的内存通过链表管理，优先复用
- **O(1) 分配**：分配和释放操作都是常数时间
- **无碎片**：所有分配大小相同，不产生内存碎片

### 内存布局

```
内存页管理：
┌─────────┬─────────┬─────────┬─────────┐
│ Page 0  │ Page 1  │ Page 2  │  ...    │  (每页 256 个元素)
└─────────┴─────────┴─────────┴─────────┘

索引规则（0 为不存在的成员编号）：
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

---

## 数据结构

### MemPtr_LLNode

内存指针单向链表节点（内部使用）。

**定义：**
```c
typedef struct MemPtr_LLNode {
    ptr Ptr;                      // 内存指针
    struct MemPtr_LLNode* Next;   // 下一个节点
} MemPtr_LLNode;
```

---

### xbsmm_struct

块结构内存管理器数据结构。

**定义：**
```c
typedef struct {
    uint32 ItemLength;        // 成员占用内存长度
    uint32 Count;             // 管理器中存在多少成员
    xparray_struct PageMMU;   // 内存页管理器（指针数组）
    MemPtr_LLNode* LL_Free;   // 已释放的内存块链表
} xbsmm_struct, *xbsmm;
```

**成员说明：**
- `ItemLength` - 每个元素的字节大小
- `Count` - 已分配的元素总数（含已释放的）
- `PageMMU` - 内存页指针数组，每页 256 个元素
- `LL_Free` - 空闲内存块链表，用于复用已释放的内存

---

## 管理器操作

### xrtBsmmCreate

创建块结构内存管理器。

**函数原型：**
```c
XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 每个元素的大小（字节）

**返回值：**
- 成功：管理器对象
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtBsmmDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
    float score;
} Student;

int main() {
    xrtInit();
    
    // 创建学生对象池
    xbsmm pool = xrtBsmmCreate(sizeof(Student));
    if (!pool) {
        printf("创建失败\n");
        xrtUnit();
        return 1;
    }
    
    printf("元素大小: %u 字节\n", pool->ItemLength);
    printf("当前数量: %u\n", pool->Count);
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmDestroy

销毁块结构内存管理器，释放所有内存。

**函数原型：**
```c
XXAPI void xrtBsmmDestroy(xbsmm objBSMM);
```

**参数：**
- `objBSMM` - 管理器对象

**说明：**
- 释放所有内存页
- 释放空闲链表
- 释放管理器结构体本身

---

### xrtBsmmInit

初始化块结构内存管理器（用于内嵌结构体）。

**函数原型：**
```c
XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength);
```

**参数：**
- `objBSMM` - 管理器对象指针
- `iItemLength` - 每个元素的大小（字节）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

typedef struct {
    char name[32];
    xbsmm_struct itemPool;  // 内嵌管理器
} Container;

int main() {
    xrtInit();
    
    Container cont;
    strcpy(cont.name, "MyContainer");
    
    // 初始化内嵌管理器
    xrtBsmmInit(&cont.itemPool, sizeof(Item));
    
    // 使用管理器
    Item* item = (Item*)xrtBsmmAlloc(&cont.itemPool);
    item->value = 100;
    printf("值: %d\n", item->value);
    
    // 释放内嵌管理器的内存（不释放结构体本身）
    xrtBsmmUnit(&cont.itemPool);
    
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmUnit

释放块结构内存管理器的内部数据（但不释放结构体本身）。

**函数原型：**
```c
XXAPI void xrtBsmmUnit(xbsmm objBSMM);
```

**参数：**
- `objBSMM` - 管理器对象

**说明：**
- 与 `xrtBsmmInit` 配对使用
- 释放所有内存页和空闲链表
- 不释放管理器结构体本身

---

## 内存分配

### xrtBsmmAlloc

从管理器中分配一块内存。

**函数原型：**
```c
XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM);
```

**参数：**
- `objBSMM` - 管理器对象

**返回值：**
- 成功：内存指针
- 失败：`NULL`

**分配策略：**
1. 优先从空闲链表中复用已释放的内存
2. 如果没有空闲内存，从当前页分配
3. 如果当前页已满，分配新页（256 个元素）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xbsmm pool = xrtBsmmCreate(sizeof(Point));
    
    // 分配点对象
    Point* p1 = (Point*)xrtBsmmAlloc(pool);
    p1->x = 10;
    p1->y = 20;
    
    Point* p2 = (Point*)xrtBsmmAlloc(pool);
    p2->x = 30;
    p2->y = 40;
    
    printf("P1: (%d, %d)\n", p1->x, p1->y);
    printf("P2: (%d, %d)\n", p2->x, p2->y);
    printf("已分配: %u\n", pool->Count);
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmFree

释放一块内存（加入空闲链表）。

**函数原型：**
```c
XXAPI void xrtBsmmFree(xbsmm objBSMM, ptr p);
```

**参数：**
- `objBSMM` - 管理器对象
- `p` - 要释放的内存指针

**说明：**
- 内存不会真正释放，而是加入空闲链表
- 下次 `xrtBsmmAlloc` 时优先复用
- 内存实际释放要等到 `xrtBsmmDestroy`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int id; } Node;

int main() {
    xrtInit();
    
    xbsmm pool = xrtBsmmCreate(sizeof(Node));
    
    // 分配 3 个节点
    Node* n1 = (Node*)xrtBsmmAlloc(pool);
    Node* n2 = (Node*)xrtBsmmAlloc(pool);
    Node* n3 = (Node*)xrtBsmmAlloc(pool);
    
    n1->id = 1;
    n2->id = 2;
    n3->id = 3;
    
    printf("分配后: Count = %u\n", pool->Count);  // 3
    
    // 释放中间节点
    xrtBsmmFree(pool, n2);
    
    // 再分配一个，会复用 n2 的内存
    Node* n4 = (Node*)xrtBsmmAlloc(pool);
    n4->id = 4;
    
    printf("复用后: Count = %u\n", pool->Count);  // 仍然是 3
    printf("n4 地址 == n2 地址: %s\n", (n4 == n2) ? "是" : "否");  // 是
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

### xrtBsmmGetPtr_Inline

通过索引获取元素指针（内联版本）。

**函数原型：**
```c
static inline ptr xrtBsmmGetPtr_Inline(xbsmm objBSMM, uint32 iIdx)
{
    uint32 iBlock = iIdx >> 8;
    uint32 iPos = iIdx & 0xFF;
    str pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
    if ( pBlock ) {
        return &pBlock[iPos * objBSMM->ItemLength];
    } else {
        return NULL;
    }
}
```

**参数：**
- `objBSMM` - 管理器对象
- `iIdx` - 元素索引（0-based）

**返回值：**
- 成功：元素指针
- 失败：`NULL`

**警告：** 此函数不推荐常规使用，仅用于特殊需求（如遍历所有分配的元素）。

---

## 使用场景

### 1. 对象池模式

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int health;
    int x, y;
    int active;
} Enemy;

typedef struct {
    xbsmm pool;
    int nextId;
} EnemyManager;

EnemyManager* CreateEnemyManager() {
    EnemyManager* mgr = xrtMalloc(sizeof(EnemyManager));
    mgr->pool = xrtBsmmCreate(sizeof(Enemy));
    mgr->nextId = 1;
    return mgr;
}

Enemy* SpawnEnemy(EnemyManager* mgr, int x, int y) {
    Enemy* e = (Enemy*)xrtBsmmAlloc(mgr->pool);
    if (e) {
        e->id = mgr->nextId++;
        e->health = 100;
        e->x = x;
        e->y = y;
        e->active = 1;
    }
    return e;
}

void KillEnemy(EnemyManager* mgr, Enemy* e) {
    e->active = 0;
    xrtBsmmFree(mgr->pool, e);
}

void DestroyEnemyManager(EnemyManager* mgr) {
    xrtBsmmDestroy(mgr->pool);
    xrtFree(mgr);
}

int main() {
    xrtInit();
    
    EnemyManager* mgr = CreateEnemyManager();
    
    // 生成敌人
    Enemy* e1 = SpawnEnemy(mgr, 100, 200);
    Enemy* e2 = SpawnEnemy(mgr, 300, 400);
    
    printf("敌人 %d: 位置 (%d, %d)\n", e1->id, e1->x, e1->y);
    printf("敌人 %d: 位置 (%d, %d)\n", e2->id, e2->x, e2->y);
    
    // 击杀敌人
    KillEnemy(mgr, e1);
    
    // 生成新敌人（复用 e1 的内存）
    Enemy* e3 = SpawnEnemy(mgr, 500, 600);
    printf("敌人 %d: 位置 (%d, %d)\n", e3->id, e3->x, e3->y);
    
    DestroyEnemyManager(mgr);
    xrtUnit();
    return 0;
}
```

---

### 2. 链表节点分配

```c
#include "xrt.h"
#include <stdio.h>

typedef struct ListNode {
    int value;
    struct ListNode* next;
} ListNode;

typedef struct {
    xbsmm nodePool;
    ListNode* head;
} LinkedList;

void ListInit(LinkedList* list) {
    xrtBsmmInit(&list->nodePool, sizeof(ListNode));
    list->head = NULL;
}

void ListPush(LinkedList* list, int value) {
    ListNode* node = (ListNode*)xrtBsmmAlloc(&list->nodePool);
    node->value = value;
    node->next = list->head;
    list->head = node;
}

void ListPrint(LinkedList* list) {
    ListNode* cur = list->head;
    while (cur) {
        printf("%d -> ", cur->value);
        cur = cur->next;
    }
    printf("NULL\n");
}

void ListDestroy(LinkedList* list) {
    xrtBsmmUnit(&list->nodePool);
    list->head = NULL;
}

int main() {
    xrtInit();
    
    LinkedList list;
    ListInit(&list);
    
    ListPush(&list, 10);
    ListPush(&list, 20);
    ListPush(&list, 30);
    
    ListPrint(&list);  // 30 -> 20 -> 10 -> NULL
    
    ListDestroy(&list);
    xrtUnit();
    return 0;
}
```

---

### 3. 树节点分配

```c
#include "xrt.h"
#include <stdio.h>

typedef struct TreeNode {
    int value;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;

xbsmm g_nodePool;

TreeNode* CreateNode(int value) {
    TreeNode* node = (TreeNode*)xrtBsmmAlloc(g_nodePool);
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

void InOrder(TreeNode* node) {
    if (!node) return;
    InOrder(node->left);
    printf("%d ", node->value);
    InOrder(node->right);
}

int main() {
    xrtInit();
    
    g_nodePool = xrtBsmmCreate(sizeof(TreeNode));
    
    // 构建二叉树
    //       5
    //      / \
    //     3   7
    //    / \
    //   1   4
    TreeNode* root = CreateNode(5);
    root->left = CreateNode(3);
    root->right = CreateNode(7);
    root->left->left = CreateNode(1);
    root->left->right = CreateNode(4);
    
    printf("中序遍历: ");
    InOrder(root);
    printf("\n");  // 1 3 4 5 7
    
    printf("节点总数: %u\n", g_nodePool->Count);  // 5
    
    xrtBsmmDestroy(g_nodePool);
    xrtUnit();
    return 0;
}
```

---

## 与其他内存管理器对比

| 特性 | BSMM | Array | malloc/free |
|------|------|-------|-------------|
| **固定大小** | ✅ 必须 | ✅ 必须 | ❌ 任意 |
| **内存复用** | ✅ 空闲链表 | ❌ 需手动管理 | ❌ 系统管理 |
| **按索引访问** | ⚠️ 不推荐 | ✅ 高效 | ❌ 不支持 |
| **内存连续** | ⚠️ 页内连续 | ✅ 全局连续 | ❌ 不保证 |
| **碎片问题** | ✅ 无碎片 | ✅ 无碎片 | ⚠️ 可能碎片 |
| **分配速度** | ✅ O(1) | ✅ O(1) 摊销 | ⚠️ 依赖系统 |
| **适用场景** | 频繁分配释放 | 顺序访问 | 通用场景 |

---

## 最佳实践

### 1. 选择合适的场景

```c
// ✅ 适合 BSMM：频繁创建销毁的对象
xbsmm bulletPool = xrtBsmmCreate(sizeof(Bullet));
// 每帧可能创建/销毁数百个子弹

// ❌ 不适合 BSMM：需要顺序遍历的集合
// 使用 xarray 或 xparray 更合适
```

### 2. 复用而非重建

```c
// ✅ 正确：复用管理器
void GameLoop() {
    static xbsmm pool = NULL;
    if (!pool) {
        pool = xrtBsmmCreate(sizeof(Particle));
    }
    // 使用 pool...
}

// ❌ 错误：每次重建管理器
void BadGameLoop() {
    xbsmm pool = xrtBsmmCreate(sizeof(Particle));
    // 使用 pool...
    xrtBsmmDestroy(pool);  // 浪费！
}
```

### 3. 注意悬挂指针

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xbsmm pool = xrtBsmmCreate(sizeof(Item));
    
    Item* item = (Item*)xrtBsmmAlloc(pool);
    item->value = 42;
    
    xrtBsmmFree(pool, item);
    
    // ⚠️ 警告：item 现在是悬挂指针！
    // printf("%d\n", item->value);  // 未定义行为
    
    // ✅ 正确做法：释放后置空
    item = NULL;
    
    xrtBsmmDestroy(pool);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [MemUnit 内存单元管理](api-memunit.md)
- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [MemPool 通用内存池](api-mempool.md)
- [PtrArray 指针数组](api-ptrarray.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#bsmm-块结构内存管理库)

</div>
