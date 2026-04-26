# AVLTree Base AVL树基础库

> 自平衡二叉搜索树底层实现，用户自行管理节点内存

[English](api-avltree-base.en.md) | [中文](api-avltree-base.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [常量定义](#常量定义)
- [数据结构](#数据结构)
- [回调函数类型](#回调函数类型)
- [宏定义](#宏定义)
- [树操作](#树操作)
- [遍历](#遍历)
- [使用场景](#使用场景)
- [与AVLTree对比](#与avltree对比)
- [最佳实践](#最佳实践)

---

## 概述

AVLTree Base 是 AVL 树的底层实现，提供基础的自平衡二叉搜索树操作。与高级版本（AVLTree）不同，用户需要**自行管理节点内存**。

### 核心特点

- **自平衡**：插入/删除后自动平衡，保证 O(log n) 操作
- **底层操作**：只管理树结构，不管理节点内存
- **灵活性高**：节点可以来自任何内存分配器
- **最大高度 48**：支持约 2^48 个节点

### AVL 树特性

```
AVL 平衡规则：任意节点的左右子树高度差不超过 1

平衡的 AVL 树：          不平衡（会自动调整）：
       4                        4
      / \                      / \
     2   6                    2   7
    / \ / \                  /     \
   1  3 5  7                1       8
                                     \
                                      9
```

### 节点内存布局

```
节点内存结构：
┌────────────────────────────────────────┐
│           xavltnode_struct             │  ← node 指针指向这里
│   ┌──────┬──────┬──────────┐           │
│   │ left │right │  height  │           │
│   │(指针)│(指针)│  (int)   │           │
│   └──────┴──────┴──────────┘           │
├────────────────────────────────────────┤
│              用户数据                   │  ← &node[1] 指向这里
│         (iItemLength 字节)             │
└────────────────────────────────────────┘
```

---

## 常量定义

| 常量 | 值 | 说明 |
|------|-----|------|
| `AVLTree_MAX_HEIGHT` | `48` | 树的最大高度，支持约 2^48 个节点 |

---

## 数据结构

### xavltnode_struct

AVL 树节点结构。

**定义：**
```c
typedef struct xavltnode_struct {
    struct xavltnode_struct* left;   // 左子节点
    struct xavltnode_struct* right;  // 右子节点
    int height;                       // 节点高度
} xavltnode_struct, *xavltnode;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `left` | 左子节点指针，无左子节点时为 `NULL` |
| `right` | 右子节点指针，无右子节点时为 `NULL` |
| `height` | 节点高度，叶子节点高度为 1 |

---

### xavltbase_struct

AVL 树管理结构。

**定义：**
```c
typedef struct {
    xavltnode RootNode;  // 根节点指针
    uint32 Count;        // 节点数量
} xavltbase_struct, *xavltbase;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `RootNode` | 根节点，空树为 `NULL` |
| `Count` | 当前节点数量 |

---

## 回调函数类型

### AVLTree_CompProc

比较回调函数类型。

**定义：**
```c
typedef int (*AVLTree_CompProc)(ptr pNode, ptr pKey);
```

**参数：**
- `pNode` - 节点数据指针（`&node[1]`）
- `pKey` - 查找键指针

**返回值：**
- `< 0`：pNode < pKey，继续在左子树搜索
- `> 0`：pNode > pKey，继续在右子树搜索
- `= 0`：找到匹配节点

**示例：**
```c
// 整数比较
int IntCompare(ptr pNode, ptr pKey) {
    int nodeVal = *(int*)pNode;
    int keyVal = *(int*)pKey;
    if (nodeVal < keyVal) return -1;
    if (nodeVal > keyVal) return 1;
    return 0;
}

// 字符串比较
int StrCompare(ptr pNode, ptr pKey) {
    return strcmp((char*)pNode, (char*)pKey);
}
```

---

### AVLTree_EachProc

遍历回调函数类型。

**定义：**
```c
typedef bool (*AVLTree_EachProc)(ptr pNode, ptr pArg);
```

**参数：**
- `pNode` - 节点数据指针（`&node[1]`）
- `pArg` - 用户自定义参数

**返回值：**
- `FALSE`：继续遍历
- `TRUE`：中断遍历

---

## 宏定义

### xrtAVLTreeGetNodeBase

从数据指针获取节点结构指针。

**定义：**
```c
#define xrtAVLTreeGetNodeBase(p) ((xavltnode)((ptr)p - sizeof(xavltnode_struct)))
```

**参数：**
- `p` - 用户数据指针（`&node[1]`）

**返回值：**
- 节点结构指针（`xavltnode`）

---

### xrtAVLTreeGetNodeData

从节点结构获取用户数据指针。

**定义：**
```c
#define xrtAVLTreeGetNodeData(p) ((ptr)(&p[1]))
```

**参数：**
- `p` - 节点结构指针（`xavltnode`）

**返回值：**
- 用户数据指针

---

### xrtAVLTreeGetRootData

获取根节点的用户数据。

**定义：**
```c
#define xrtAVLTreeGetRootData(obj) xrtAVLTreeGetNodeData(obj->RootNode)
```

---

### xrtAVLTB_Init

初始化 AVL 树。

**定义：**
```c
#define xrtAVLTB_Init(o) (o)->RootNode = NULL; (o)->Count = 0
```

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    printf("节点数: %u\n", tree.Count);  // 0
    printf("根节点: %p\n", tree.RootNode);  // (nil)
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_Unit / xrtAVLTB_RemoveAll / xrtAVLTB_Clear

释放 AVL 树（仅重置指针，不释放节点内存）。

**定义：**
```c
#define xrtAVLTB_Unit xrtAVLTB_Init
#define xrtAVLTB_RemoveAll xrtAVLTB_Unit
#define xrtAVLTB_Clear xrtAVLTB_Unit
```

**警告：** 这些宏只重置树结构，**不会释放节点内存**！用户必须先遍历释放所有节点。

---

## 树操作

### xrtAVLTB_Insert

向树中插入节点。

**函数原型：**
```c
XXAPI xavltnode xrtAVLTB_Insert(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey, xavltnode pNewNode);
```

**参数：**
- `objAVLT` - 树对象
- `procComp` - 比较函数
- `pKey` - 键指针（用于比较和定位）
- `pNewNode` - 预分配的新节点

**返回值：**
- 成功（新插入）：返回 `pNewNode`
- 键已存在：返回已存在的节点
- 失败（超高度）：返回 `NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;  // 节点结构必须在最前面
    int key;
    char value[32];
} MyNode;

int MyCompare(ptr pNode, ptr pKey) {
    int nodeKey = *(int*)pNode;
    int searchKey = *(int*)pKey;
    return nodeKey - searchKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 分配并插入节点
    MyNode* n1 = xrtMalloc(sizeof(MyNode));
    n1->key = 10;
    strcpy(n1->value, "Ten");
    
    int key = 10;
    xavltnode result = xrtAVLTB_Insert(&tree, MyCompare, &key, (xavltnode)n1);
    
    if (result == (xavltnode)n1) {
        printf("插入成功，节点数: %u\n", tree.Count);  // 1
    }
    
    // 尝试插入重复键
    MyNode* n2 = xrtMalloc(sizeof(MyNode));
    n2->key = 10;
    strcpy(n2->value, "Duplicate");
    
    result = xrtAVLTB_Insert(&tree, MyCompare, &key, (xavltnode)n2);
    if (result != (xavltnode)n2) {
        printf("键已存在，返回现有节点\n");
        xrtFree(n2);  // 释放未使用的节点
    }
    
    // 清理
    xrtFree(n1);
    xrtAVLTB_Unit(&tree);
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_Remove

从树中删除节点。

**函数原型：**
```c
XXAPI xavltnode xrtAVLTB_Remove(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
```

**参数：**
- `objAVLT` - 树对象
- `procComp` - 比较函数
- `pKey` - 要删除的键

**返回值：**
- 成功：返回被删除的节点指针（用户需释放）
- 失败：返回 `NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int key;
} IntNode;

int IntCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 插入节点 1, 2, 3
    for (int i = 1; i <= 3; i++) {
        IntNode* n = xrtMalloc(sizeof(IntNode));
        n->key = i;
        xrtAVLTB_Insert(&tree, IntCompare, &i, (xavltnode)n);
    }
    printf("插入后节点数: %u\n", tree.Count);  // 3
    
    // 删除节点 2
    int key = 2;
    xavltnode removed = xrtAVLTB_Remove(&tree, IntCompare, &key);
    if (removed) {
        printf("删除成功，节点数: %u\n", tree.Count);  // 2
        xrtFree(removed);  // 释放被删除的节点
    }
    
    // 尝试删除不存在的节点
    key = 100;
    removed = xrtAVLTB_Remove(&tree, IntCompare, &key);
    if (!removed) {
        printf("节点不存在\n");
    }
    
    // 清理剩余节点（实际应用中需遍历释放）
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_Search

在树中查找节点。

**函数原型：**
```c
XXAPI xavltnode xrtAVLTB_Search(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
```

**参数：**
- `objAVLT` - 树对象
- `procComp` - 比较函数
- `pKey` - 要查找的键

**返回值：**
- 找到：节点指针
- 未找到：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int id;
    char name[32];
} UserNode;

int UserCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 插入用户
    UserNode* u1 = xrtMalloc(sizeof(UserNode));
    u1->id = 100;
    strcpy(u1->name, "Alice");
    int key = 100;
    xrtAVLTB_Insert(&tree, UserCompare, &key, (xavltnode)u1);
    
    // 查找用户
    key = 100;
    xavltnode found = xrtAVLTB_Search(&tree, UserCompare, &key);
    if (found) {
        UserNode* user = (UserNode*)found;
        printf("找到用户: ID=%d, Name=%s\n", user->id, user->name);
    }
    
    // 查找不存在的用户
    key = 999;
    found = xrtAVLTB_Search(&tree, UserCompare, &key);
    if (!found) {
        printf("用户 %d 不存在\n", key);
    }
    
    xrtFree(u1);
    xrtUnit();
    return 0;
}
```

---

## 遍历

### xrtAVLTB_Walk

中序遍历所有节点（升序）。

**宏定义：**
```c
#define xrtAVLTB_Walk(obj, p, a) xrtAVLTB_WalkRecuProc(obj->RootNode, (ptr)p, (ptr)a)
```

**参数：**
- `obj` - 树对象
- `p` - 遍历回调函数（`AVLTree_EachProc`）
- `a` - 用户参数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int value;
} IntNode;

int IntCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

bool PrintNode(ptr pNode, ptr pArg) {
    int value = *(int*)pNode;
    printf("%d ", value);
    return FALSE;  // 继续遍历
}

bool SumNodes(ptr pNode, ptr pArg) {
    int value = *(int*)pNode;
    int* sum = (int*)pArg;
    *sum += value;
    return FALSE;
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 插入 5, 3, 7, 1, 9
    int values[] = {5, 3, 7, 1, 9};
    IntNode* nodes[5];
    for (int i = 0; i < 5; i++) {
        nodes[i] = xrtMalloc(sizeof(IntNode));
        nodes[i]->value = values[i];
        xrtAVLTB_Insert(&tree, IntCompare, &values[i], (xavltnode)nodes[i]);
    }
    
    // 中序遍历（升序输出）
    printf("升序: ");
    xrtAVLTB_Walk(&tree, PrintNode, NULL);
    printf("\n");  // 升序: 1 3 5 7 9
    
    // 计算总和
    int sum = 0;
    xrtAVLTB_Walk(&tree, SumNodes, &sum);
    printf("总和: %d\n", sum);  // 25
    
    // 清理
    for (int i = 0; i < 5; i++) {
        xrtFree(nodes[i]);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtAVLTB_WalkEx

扩展遍历（支持前序、中序、后序回调）。

**宏定义：**
```c
#define xrtAVLTB_WalkEx(obj, p1, p2, p3, a) xrtAVLTB_WalkExRecuProc(obj->RootNode, (ptr)p1, (ptr)p2, (ptr)p3, (ptr)a)
```

**参数：**
- `obj` - 树对象
- `p1` - 前序回调（进入节点前）
- `p2` - 中序回调（左子树遍历后）
- `p3` - 后序回调（离开节点前）
- `a` - 用户参数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

bool PreOrder(ptr pNode, ptr pArg) {
    printf("Pre(%d) ", *(int*)pNode);
    return FALSE;
}

bool InOrder(ptr pNode, ptr pArg) {
    printf("In(%d) ", *(int*)pNode);
    return FALSE;
}

bool PostOrder(ptr pNode, ptr pArg) {
    printf("Post(%d) ", *(int*)pNode);
    return FALSE;
}

int main() {
    xrtInit();
    
    // ... 创建树并插入节点 ...
    
    // 扩展遍历
    xrtAVLTB_WalkEx(&tree, PreOrder, InOrder, PostOrder, NULL);
    
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
    xavltnode_struct node;
    int key;
    char data[64];
} CustomNode;

int NodeCompare(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

// 使用 BSMM 管理节点内存
int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 使用 BSMM 管理固定大小节点
    xbsmm nodeMgr = xrtBsmmCreate(sizeof(CustomNode));
    
    // 分配并插入节点
    for (int i = 0; i < 100; i++) {
        CustomNode* n = xrtBsmmAlloc(nodeMgr);
        n->key = i;
        sprintf(n->data, "Data_%d", i);
        xrtAVLTB_Insert(&tree, NodeCompare, &i, (xavltnode)n);
    }
    
    printf("节点数: %u\n", tree.Count);  // 100
    
    // 查找
    int key = 50;
    xavltnode found = xrtAVLTB_Search(&tree, NodeCompare, &key);
    if (found) {
        CustomNode* n = (CustomNode*)found;
        printf("找到: key=%d, data=%s\n", n->key, n->data);
    }
    
    // 销毁时释放整个 BSMM（所有节点一起释放）
    xrtBsmmDestroy(nodeMgr);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 嵌入式索引结构

```c
#include "xrt.h"
#include <stdio.h>

// 主数据结构（数组存储）
typedef struct {
    int id;
    char name[32];
    float score;
} Student;

// 索引节点（只包含键和索引）
typedef struct {
    xavltnode_struct node;
    int id;         // 键
    int arrayIndex; // 指向 Students 数组的索引
} StudentIndex;

Student students[100];
int studentCount = 0;

int IndexCompare(ptr pNode, ptr pKey) {
    StudentIndex* idx = (StudentIndex*)((char*)pNode - sizeof(xavltnode_struct));
    return idx->id - *(int*)pKey;
}

int main() {
    xrtInit();
    
    xavltbase_struct idIndex;
    xrtAVLTB_Init(&idIndex);
    
    // 添加学生并建立索引
    students[0] = (Student){1001, "Alice", 95.5};
    students[1] = (Student){1003, "Bob", 87.0};
    students[2] = (Student){1002, "Charlie", 92.3};
    studentCount = 3;
    
    StudentIndex* indices = xrtMalloc(sizeof(StudentIndex) * 3);
    for (int i = 0; i < 3; i++) {
        indices[i].id = students[i].id;
        indices[i].arrayIndex = i;
        xrtAVLTB_Insert(&idIndex, IndexCompare, &students[i].id, (xavltnode)&indices[i]);
    }
    
    // 通过索引快速查找
    int searchId = 1002;
    xavltnode found = xrtAVLTB_Search(&idIndex, IndexCompare, &searchId);
    if (found) {
        StudentIndex* idx = (StudentIndex*)found;
        Student* s = &students[idx->arrayIndex];
        printf("找到学生: ID=%d, Name=%s, Score=%.1f\n", 
               s->id, s->name, s->score);
    }
    
    xrtFree(indices);
    xrtUnit();
    return 0;
}
```

---

## 与AVLTree对比

| 特性 | AVLTree Base | AVLTree |
|------|--------------|---------|
| **内存管理** | 手动 | 自动（FSMemPool） |
| **节点分配** | 用户自行分配 | `xrtAVLTreeInsert` 自动分配 |
| **节点释放** | 用户自行释放 | `xrtAVLTreeRemove` 自动释放 |
| **比较函数** | 每次操作传入 | 创建时指定一次 |
| **GC 支持** | ❌ 无 | ✅ FSMemPool 支持 |
| **灵活性** | 最高 | 中等 |
| **适用场景** | 自定义内存、嵌入式 | 一般应用 |

### 选择建议

- **使用 AVLTree Base**：自定义内存分配器、嵌入式环境、需要完全控制内存
- **使用 AVLTree**：大多数场景，需要自动内存管理

---

## 最佳实践

### 1. 节点结构定义

```c
#include "xrt.h"
#include <stdio.h>

// ✅ 正确：节点结构在最前面
typedef struct {
    xavltnode_struct node;  // 必须在第一个位置
    int key;
    char value[32];
} GoodNode;

// ❌ 错误：节点结构不在最前面
typedef struct {
    int key;                // 错误！
    xavltnode_struct node;
    char value[32];
} BadNode;

int main() {
    xrtInit();
    
    // 验证内存布局
    GoodNode good;
    printf("GoodNode: node偏移=%zu, key偏移=%zu\n",
           (char*)&good.node - (char*)&good,   // 0
           (char*)&good.key - (char*)&good);   // sizeof(xavltnode_struct)
    
    xrtUnit();
    return 0;
}
```

---

### 2. 安全删除所有节点

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int key;
} IntNode;

bool FreeNodeCallback(ptr pNode, ptr pArg) {
    // pNode 指向 &node[1]，需要回退到节点开头
    xavltnode nodePtr = xrtAVLTreeGetNodeBase(pNode);
    xrtFree(nodePtr);
    return FALSE;  // 继续遍历
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 插入节点...
    
    // ✅ 正确：先遍历释放所有节点，再清空树
    xrtAVLTB_Walk(&tree, FreeNodeCallback, NULL);
    xrtAVLTB_Unit(&tree);
    
    // ❌ 错误：直接清空（内存泄漏）
    // xrtAVLTB_Unit(&tree);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 中断遍历

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xavltnode_struct node;
    int value;
} IntNode;

// 查找第一个大于目标的值
bool FindFirstGreater(ptr pNode, ptr pArg) {
    int value = *(int*)pNode;
    int* target = (int*)pArg;
    
    if (value > *target) {
        printf("找到: %d > %d\n", value, *target);
        return TRUE;  // 中断遍历
    }
    return FALSE;  // 继续
}

int main() {
    xrtInit();
    
    xavltbase_struct tree;
    xrtAVLTB_Init(&tree);
    
    // 插入 1, 3, 5, 7, 9
    int values[] = {1, 3, 5, 7, 9};
    IntNode nodes[5];
    int IntCompare(ptr pNode, ptr pKey) {
        return *(int*)pNode - *(int*)pKey;
    }
    
    for (int i = 0; i < 5; i++) {
        nodes[i].value = values[i];
        xrtAVLTB_Insert(&tree, IntCompare, &values[i], (xavltnode)&nodes[i]);
    }
    
    // 查找第一个 > 4 的值
    int target = 4;
    xrtAVLTB_Walk(&tree, FindFirstGreater, &target);  // 找到: 5 > 4
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [AVLTree AVL树](api-avltree.md)
- [Dict 字典](api-dict.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [FSMemPool 固定大小内存池](api-mempool-fs.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#avltree-base-avl树基础库)

</div>
