# AVLTree 平衡二叉树库

> 自平衡二叉搜索树，自动管理节点内存

[English](api-avltree.en.md) | [中文](api-avltree.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [回调函数类型](#回调函数类型)
- [树操作](#树操作)
- [节点操作](#节点操作)
- [遍历](#遍历)
- [使用场景](#使用场景)
- [与AVLTree Base对比](#与avltree-base对比)
- [最佳实践](#最佳实践)

---

## 概述

AVLTree 是基于 AVLTree Base 的高级 AVL 树实现，自动管理节点内存（使用 FSMemPool）。

### 核心特点

- **自动内存管理**：使用 FSMemPool 分配节点
- **节点缓存**：优化连续插入性能
- **继承树支持**：可设置父树（Parent）用于查找
- **自定义释放**：支持节点数据的自定义释放回调

### 与 AVLTree Base 的关系

```
AVLTree 架构：
┌─────────────────────────────────────┐
│           AVLTree (高级)             │
│  ┌─────────────────────────────────┐│
│  │    FSMemPool (节点内存管理)      ││
│  └─────────────────────────────────┘│
│  ┌─────────────────────────────────┐│
│  │    AVLTree Base (树结构操作)     ││
│  └─────────────────────────────────┘│
└─────────────────────────────────────┘
```

---

## 数据结构

### xavltree_struct

AVL 树结构。

**定义：**
```c
typedef struct xavltree_struct {
    xavltnode RootNode;           // 根节点
    uint32 Count;                  // 节点数量
    struct xavltree_struct* Parent; // 父树（用于继承查找）
    AVLTree_CompProc CompProc;    // 比较函数
    AVLTree_FreeProc FreeProc;    // 节点释放回调（可选）
    xfsmempool_struct objMM;      // 内置内存池
    xavltnode NodeCache;          // 节点缓存
} xavltree_struct, *xavltree;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `RootNode` | 树的根节点 |
| `Count` | 当前节点数量 |
| `Parent` | 父树指针，Search 未找到时会查找父树 |
| `CompProc` | 比较函数，用于节点排序 |
| `FreeProc` | 释放回调，删除节点时调用（可为 NULL） |
| `objMM` | 内置 FSMemPool，管理节点内存 |
| `NodeCache` | 预分配的节点缓存，优化连续插入 |

---

## 回调函数类型

### AVLTree_CompProc

比较回调函数（继承自 AVLTree Base）。

**定义：**
```c
typedef int (*AVLTree_CompProc)(ptr pNode, ptr pKey);
```

**参数：**
- `pNode` - 树中节点的数据指针
- `pKey` - 要查找/比较的键指针

**返回值：**
- `< 0` - pNode 小于 pKey
- `= 0` - 相等
- `> 0` - pNode 大于 pKey

---

### AVLTree_FreeProc

节点释放回调函数（可选）。

**定义：**
```c
typedef void (*AVLTree_FreeProc)(ptr objTree, ptr pNode);
```

**参数：**
- `objTree` - 树对象
- `pNode` - 节点数据指针

**用途：** 当节点数据中包含需要额外释放的资源时使用。

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    str name;  // 需要释放
} User;

void FreeUser(ptr tree, ptr pNode) {
    User* user = (User*)pNode;
    if (user->name) {
        xrtFree(user->name);
    }
}

int CompareUser(ptr pNode, ptr pKey) {
    return ((User*)pNode)->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(User), CompareUser);
    tree->FreeProc = FreeUser;  // 设置释放回调
    
    // 插入数据
    int id = 1;
    bool isNew;
    User* user = (User*)xrtAVLTreeInsert(tree, &id, &isNew);
    if (isNew) {
        user->id = id;
        user->name = xrtCopyStr((str)"Alice", 0);
    }
    
    // 销毁时会自动调用 FreeUser 释放 name
    xrtAVLTreeDestroy(tree);
    
    xrtUnit();
    return 0;
}
```

---

## 树操作

### xrtAVLTreeCreate

创建 AVL 树。

**函数原型：**
```c
XXAPI xavltree xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp);
```

**参数：**
- `iItemLength` - 节点数据大小（字节）
- `procComp` - 比较函数

**返回值：**
- 成功：树对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtAVLTreeDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // 插入数据
    int keys[] = {5, 3, 7, 1, 9, 4, 6};
    for (int i = 0; i < 7; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &keys[i], &isNew);
        if (isNew) {
            *data = keys[i];
        }
    }
    
    printf("节点数: %u\n", tree->Count);  // 7
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeDestroy

销毁 AVL 树。

**函数原型：**
```c
XXAPI void xrtAVLTreeDestroy(xavltree objAVLT);
```

**参数：**
- `objAVLT` - 树对象

**说明：**
- 释放所有节点和树结构
- 如果设置了 `FreeProc`，会先遍历所有节点调用释放回调

---

### xrtAVLTreeInit

初始化 AVL 树（用于栈上或嵌入式结构体）。

**函数原型：**
```c
XXAPI void xrtAVLTreeInit(xavltree objAVLT, uint32 iItemLength, AVLTree_CompProc procComp);
```

**参数：**
- `objAVLT` - 树对象指针
- `iItemLength` - 节点数据大小
- `procComp` - 比较函数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    // 栈上分配
    xavltree_struct treeData;
    xrtAVLTreeInit(&treeData, sizeof(int), CompareInt);
    
    // 使用
    int key = 42;
    bool isNew;
    int* data = (int*)xrtAVLTreeInsert(&treeData, &key, &isNew);
    *data = key;
    
    printf("节点数: %u\n", treeData.Count);
    
    // 释放内部资源（不释放结构体本身）
    xrtAVLTreeUnit(&treeData);
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeUnit

释放 AVL 树内部资源（不释放结构体本身）。

**函数原型：**
```c
XXAPI void xrtAVLTreeUnit(xavltree objAVLT);
```

**说明：**
- 与 `xrtAVLTreeInit` 配对使用
- 释放内置 FSMemPool 和所有节点

**宏别名：**
- `xrtAVLTreeRemoveAll` - 删除所有节点
- `xrtAVLTreeClear` - 清空树

---

## 节点操作

### xrtAVLTreeInsert

插入节点。

**函数原型：**
```c
XXAPI ptr xrtAVLTreeInsert(xavltree objAVLT, ptr pKey, bool* bNew);
```

**参数：**
- `objAVLT` - 树对象
- `pKey` - 键指针（用于比较）
- `bNew` - 输出参数，指示是否为新节点（可为 NULL）

**返回值：**
- 成功：节点数据指针
- 失败：`NULL`

**说明：**
- 如果键已存在，返回已有节点的数据指针，`*bNew = FALSE`
- 如果键不存在，创建新节点，`*bNew = TRUE`
- 新节点需要在返回后填充数据

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int value;
} Item;

int CompareItem(ptr pNode, ptr pKey) {
    return ((Item*)pNode)->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(Item), CompareItem);
    
    // 插入新节点
    int id1 = 100;
    bool isNew;
    Item* item1 = (Item*)xrtAVLTreeInsert(tree, &id1, &isNew);
    if (isNew) {
        item1->id = id1;
        item1->value = 1000;
        printf("插入新节点: id=%d, value=%d\n", item1->id, item1->value);
    }
    
    // 尝试插入重复键
    Item* item2 = (Item*)xrtAVLTreeInsert(tree, &id1, &isNew);
    if (!isNew) {
        printf("键已存在: id=%d, value=%d\n", item2->id, item2->value);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeSearch

查找节点。

**函数原型：**
```c
XXAPI ptr xrtAVLTreeSearch(xavltree objAVLT, ptr pKey);
```

**参数：**
- `objAVLT` - 树对象
- `pKey` - 键指针

**返回值：**
- 找到：节点数据指针
- 未找到：`NULL`

**继承查找：** 如果当前树未找到且设置了 `Parent`，会继续在父树中查找。

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // 插入数据
    for (int i = 1; i <= 10; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &i, &isNew);
        *data = i * 100;
    }
    
    // 查找
    int key = 5;
    int* found = (int*)xrtAVLTreeSearch(tree, &key);
    if (found) {
        printf("找到: key=%d, value=%d\n", key, *found);  // 500
    }
    
    // 查找不存在的键
    int notExist = 20;
    int* notFound = (int*)xrtAVLTreeSearch(tree, &notExist);
    if (!notFound) {
        printf("未找到: key=%d\n", notExist);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTreeRemove

删除节点。

**函数原型：**
```c
XXAPI bool xrtAVLTreeRemove(xavltree objAVLT, ptr pKey);
```

**参数：**
- `objAVLT` - 树对象
- `pKey` - 键指针

**返回值：**
- `TRUE` - 成功删除
- `FALSE` - 键不存在

**说明：**
- 删除后会自动释放节点内存
- 如果设置了 `FreeProc`，删除前会先调用释放回调

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // 插入数据
    for (int i = 1; i <= 5; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &i, &isNew);
        *data = i;
    }
    printf("删除前: %u 个节点\n", tree->Count);  // 5
    
    // 删除存在的键
    int key = 3;
    if (xrtAVLTreeRemove(tree, &key)) {
        printf("成功删除 key=%d\n", key);
    }
    printf("删除后: %u 个节点\n", tree->Count);  // 4
    
    // 删除不存在的键
    int notExist = 100;
    if (!xrtAVLTreeRemove(tree, &notExist)) {
        printf("删除失败: key=%d 不存在\n", notExist);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

## 遍历

### xrtAVLTreeWalk / xrtAVLTreeWalkEx

遍历树中所有节点（宏定义，使用 AVLTree Base 的实现）。

**宏定义：**
```c
#define xrtAVLTreeWalk    xrtAVLTB_Walk
#define xrtAVLTreeWalkEx  xrtAVLTB_WalkEx
```

**函数原型：**
```c
// 基本遍历
bool xrtAVLTreeWalk(xavltbase objBase, int WalkOrder, AVLTree_EachProc procEach, ptr pArg);

// 扩展遍历（支持显式终止）
bool xrtAVLTreeWalkEx(xavltbase objBase, int WalkOrder, AVLTree_EachProc procEach, ptr pArg, ptr pExit);
```

**WalkOrder 参数：**
- `AVLTREE_WALK_PREORDER` (1) - 前序遍历
- `AVLTREE_WALK_INORDER` (2) - 中序遍历（有序）
- `AVLTREE_WALK_POSTORDER` (3) - 后序遍历

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

bool PrintNode(ptr pNode, ptr pArg) {
    printf("%d ", *(int*)pNode);
    return TRUE;  // 继续遍历
}

bool SumNodes(ptr pNode, ptr pArg) {
    int* sum = (int*)pArg;
    *sum += *(int*)pNode;
    return TRUE;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // 插入数据
    int keys[] = {5, 3, 7, 1, 9, 4, 6};
    for (int i = 0; i < 7; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &keys[i], &isNew);
        *data = keys[i];
    }
    
    // 中序遍历（有序输出）
    printf("中序遍历: ");
    xrtAVLTreeWalk((xavltbase)tree, AVLTREE_WALK_INORDER, PrintNode, NULL);
    printf("\n");  // 1 3 4 5 6 7 9
    
    // 计算总和
    int sum = 0;
    xrtAVLTreeWalk((xavltbase)tree, AVLTREE_WALK_INORDER, SumNodes, &sum);
    printf("总和: %d\n", sum);  // 35
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 整数索引

```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    // 插入数据
    for (int i = 1; i <= 100; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(tree, &i, &isNew);
        *data = i;
    }
    
    // 查找
    int search = 50;
    int* found = (int*)xrtAVLTreeSearch(tree, &search);
    if (found) {
        printf("找到: %d\n", *found);
    }
    
    // 统计
    printf("节点数: %u\n", tree->Count);
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 2. 字符串键索引

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char key[32];
    int value;
} StringKeyNode;

int CompareStringKey(ptr pNode, ptr pKey) {
    return strcmp(((StringKeyNode*)pNode)->key, (char*)pKey);
}

void FreeStringNode(ptr tree, ptr pNode) {
    // 如果有额外资源需要释放，在此处理
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(StringKeyNode), CompareStringKey);
    
    // 插入
    char* keys[] = {"apple", "banana", "cherry", "date"};
    int values[] = {100, 200, 300, 400};
    
    for (int i = 0; i < 4; i++) {
        bool isNew;
        StringKeyNode* node = (StringKeyNode*)xrtAVLTreeInsert(tree, keys[i], &isNew);
        if (isNew) {
            strcpy(node->key, keys[i]);
            node->value = values[i];
        }
    }
    
    // 查找
    char* searchKey = "banana";
    StringKeyNode* found = (StringKeyNode*)xrtAVLTreeSearch(tree, searchKey);
    if (found) {
        printf("找到 %s: value=%d\n", found->key, found->value);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 3. 继承树（Parent）

```c
#include "xrt.h"
#include <stdio.h>

int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    // 创建父树（全局配置）
    xavltree parentTree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    int globalKeys[] = {100, 200, 300};
    for (int i = 0; i < 3; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(parentTree, &globalKeys[i], &isNew);
        *data = globalKeys[i];
    }
    
    // 创建子树（局部配置）
    xavltree childTree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    childTree->Parent = parentTree;  // 设置父树
    
    int localKeys[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        bool isNew;
        int* data = (int*)xrtAVLTreeInsert(childTree, &localKeys[i], &isNew);
        *data = localKeys[i];
    }
    
    // 在子树中查找
    int key1 = 2;
    int* found1 = (int*)xrtAVLTreeSearch(childTree, &key1);
    printf("在子树查找 %d: %s\n", key1, found1 ? "找到" : "未找到");
    
    // 查找子树中不存在但父树存在的键
    int key2 = 200;
    int* found2 = (int*)xrtAVLTreeSearch(childTree, &key2);
    printf("在子树查找 %d: %s\n", key2, found2 ? "找到(来自父树)" : "未找到");
    
    xrtAVLTreeDestroy(childTree);
    xrtAVLTreeDestroy(parentTree);
    xrtUnit();
    return 0;
}
```

---

## 与AVLTree Base对比

| 特性 | AVLTree | AVLTree Base |
|------|---------|-------------|
| **内存管理** | 自动（FSMemPool） | 手动 |
| **节点分配** | 自动 | 用户自行 |
| **节点缓存** | ✅ 支持 | ❌ 不支持 |
| **继承树** | ✅ Parent 支持 | ❌ 不支持 |
| **释放回调** | ✅ FreeProc | ❌ 不支持 |
| **适用场景** | 一般需求 | 自定义内存管理 |

### 选择建议

- **使用 AVLTree**：大多数场景，无需关心内存管理
- **使用 AVLTree Base**：需要自定义内存分配策略或嵌入式场景

---

## 最佳实践

### 1. 比较函数设计

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// ✅ 正确：返回正数/零/负数
int CompareInt(ptr pNode, ptr pKey) {
    int a = *(int*)pNode;
    int b = *(int*)pKey;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

// ✅ 简化版（小数值时安全）
int CompareIntSimple(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

// ❌ 错误：大数值溢出
int CompareIntWrong(ptr pNode, ptr pKey) {
    int64 a = *(int64*)pNode;
    int64 b = *(int64*)pKey;
    return (int)(a - b);  // 溢出风险！
}

// ✅ 安全的 64 位比较
int CompareInt64Safe(ptr pNode, ptr pKey) {
    int64 a = *(int64*)pNode;
    int64 b = *(int64*)pKey;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
    
    int key = 42;
    bool isNew;
    int* data = (int*)xrtAVLTreeInsert(tree, &key, &isNew);
    *data = key;
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 2. 复合键设计

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int category;
    int id;
    char name[32];
} Record;

// 多级比较：先比较 category，再比较 id
int CompareRecord(ptr pNode, ptr pKey) {
    Record* a = (Record*)pNode;
    Record* b = (Record*)pKey;
    
    // 第一级：category
    if (a->category != b->category) {
        return a->category - b->category;
    }
    // 第二级：id
    return a->id - b->id;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(Record), CompareRecord);
    
    // 插入记录
    Record records[] = {
        {1, 100, "Item A"},
        {1, 200, "Item B"},
        {2, 100, "Item C"},
        {2, 200, "Item D"}
    };
    
    for (int i = 0; i < 4; i++) {
        bool isNew;
        Record* node = (Record*)xrtAVLTreeInsert(tree, &records[i], &isNew);
        if (isNew) {
            *node = records[i];
        }
    }
    
    // 查找
    Record searchKey = {1, 200, ""};
    Record* found = (Record*)xrtAVLTreeSearch(tree, &searchKey);
    if (found) {
        printf("找到: category=%d, id=%d, name=%s\n", 
               found->category, found->id, found->name);
    }
    
    xrtAVLTreeDestroy(tree);
    xrtUnit();
    return 0;
}
```

---

### 3. 带释放回调的使用

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    str data;       // 动态分配
    ptr extra;      // 额外资源
} ComplexNode;

void FreeComplexNode(ptr tree, ptr pNode) {
    ComplexNode* node = (ComplexNode*)pNode;
    if (node->data) {
        xrtFree(node->data);
        node->data = NULL;
    }
    if (node->extra) {
        xrtFree(node->extra);
        node->extra = NULL;
    }
}

int CompareComplex(ptr pNode, ptr pKey) {
    return ((ComplexNode*)pNode)->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltree tree = xrtAVLTreeCreate(sizeof(ComplexNode), CompareComplex);
    tree->FreeProc = FreeComplexNode;  // 设置释放回调
    
    // 插入节点
    for (int i = 1; i <= 5; i++) {
        bool isNew;
        ComplexNode* node = (ComplexNode*)xrtAVLTreeInsert(tree, &i, &isNew);
        if (isNew) {
            node->id = i;
            node->data = xrtFormat((str)"Data for %d", i);
            node->extra = xrtMalloc(100);
        }
    }
    
    printf("创建了 %u 个节点\n", tree->Count);
    
    // 删除单个节点（会调用 FreeComplexNode）
    int delKey = 3;
    xrtAVLTreeRemove(tree, &delKey);
    printf("删除后剩余 %u 个节点\n", tree->Count);
    
    // 销毁树（会遍历所有节点调用 FreeComplexNode）
    xrtAVLTreeDestroy(tree);
    printf("所有资源已释放\n");
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [AVLTree Base AVL树基础](api-avltree-base.md)
- [Dict 字典](api-dict.md)
- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#avltree-平衡二叉树库)

</div>
