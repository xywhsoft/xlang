# Stack 静态栈库

> 固定深度栈数据结构，支持结构体和指针两种模式

[English](api-stack.en.md) | [中文](api-stack.md) | [返回索引](README.md)

---

## 📑 目录

- [数据结构](#数据结构)
- [栈操作](#栈操作)
- [压栈操作](#压栈操作)
- [出栈操作](#出栈操作)
- [栈访问](#栈访问)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 数据结构

### xstack_struct

静态栈结构。

**定义：**
```c
typedef struct {
    union {
        char* Memory;       // 栈数据内存 - 结构体模式
        ptr* PtrMem;        // 栈数据内存 - 指针模式
    };
    uint32 ItemLength;      // 栈成员占用内存长度
    uint32 MaxCount;        // 栈最大容量（栈深度）
    uint32 Count;           // 当前栈中元素数量（栈顶位置）
} xstack_struct, *xstack;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `Memory` / `PtrMem` | 联合体，根据模式使用不同访问方式 |
| `ItemLength` | 每个元素的大小（字节）|
| `MaxCount` | 栈的最大容量 |
| `Count` | 当前元素数量 |

### 栈模式

| 模式 | ItemLength | 说明 |
|------|------------|------|
| **结构体栈** | `sizeof(T)` | 存储任意结构体，元素直接存储在栈内存中 |
| **指针栈** | `sizeof(ptr)` | 存储指针，适合管理外部对象 |

---

## 栈操作

### xrtStackCreate

创建固定深度栈。

**函数原型：**
```c
XXAPI xstack xrtStackCreate(uint32 iMaxCount, uint32 iItemLength);
```

**参数：**
- `iMaxCount` - 最大深度（栈容量）
- `iItemLength` - 每个元素的字节数

**返回值：**
- 成功：栈对象指针
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Item;

int main() {
    xrtInit();
    
    // 创建结构体栈（最多100个Item）
    xstack stk = xrtStackCreate(100, sizeof(Item));
    printf("栈容量: %u\n", stk->MaxCount);  // 100
    printf("当前数量: %u\n", stk->Count);    // 0
    
    xrtStackDestroy(stk);
    
    // 创建指针栈
    xstack ptrStk = xrtStackCreate(50, sizeof(ptr));
    printf("指针栈容量: %u\n", ptrStk->MaxCount);  // 50
    
    xrtStackDestroy(ptrStk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackDestroy

销毁栈。

**定义（宏）：**
```c
#define xrtStackDestroy xrtFree
```

**说明：**
- 直接调用 `xrtFree` 释放栈内存
- 栈内存是连续分配的，一次释放即可

---

## 压栈操作

### xrtStackPush

压入结构体元素（分配空间）。

**函数原型：**
```c
XXAPI ptr xrtStackPush(xstack objSTK);
```

**参数：**
- `objSTK` - 栈对象

**返回值：**
- 成功：新元素的内存指针
- 失败：`NULL`（栈已满）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // 压入元素并设置值
    Item* item1 = (Item*)xrtStackPush(stk);
    item1->value = 100;
    
    Item* item2 = (Item*)xrtStackPush(stk);
    item2->value = 200;
    
    Item* item3 = (Item*)xrtStackPush(stk);
    item3->value = 300;
    
    printf("栈中元素: %u\n", stk->Count);  // 3
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackPushData

压入结构体元素（复制数据）。

**函数原型：**
```c
XXAPI uint32 xrtStackPushData(xstack objSTK, ptr pData);
```

**参数：**
- `objSTK` - 栈对象
- `pData` - 要复制的数据指针

**返回值：**
- 成功：新元素的位置（1-based）
- 失败：`0`（栈已满）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int x; int y; } Point;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Point));
    
    Point p1 = {10, 20};
    Point p2 = {30, 40};
    Point p3 = {50, 60};
    
    uint32 pos1 = xrtStackPushData(stk, &p1);
    uint32 pos2 = xrtStackPushData(stk, &p2);
    uint32 pos3 = xrtStackPushData(stk, &p3);
    
    printf("位置: %u, %u, %u\n", pos1, pos2, pos3);  // 1, 2, 3
    
    // 验证数据
    Point* top = (Point*)xrtStackTop(stk);
    printf("栈顶: (%d, %d)\n", top->x, top->y);  // (50, 60)
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackPushPtr

压入指针元素。

**函数原型：**
```c
XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal);
```

**参数：**
- `objSTK` - 栈对象
- `pVal` - 要存储的指针

**返回值：**
- 成功：新元素的位置（1-based）
- 失败：`0`（栈已满）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(ptr));
    
    str s1 = (str)"First";
    str s2 = (str)"Second";
    str s3 = (str)"Third";
    
    xrtStackPushPtr(stk, s1);
    xrtStackPushPtr(stk, s2);
    xrtStackPushPtr(stk, s3);
    
    printf("栈中指针数: %u\n", stk->Count);  // 3
    
    // 弹出并打印
    while (stk->Count > 0) {
        str s = (str)xrtStackPopPtr(stk);
        printf("弹出: %s\n", s);
    }
    // 输出: Third, Second, First
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

## 出栈操作

### xrtStackPop

弹出结构体元素。

**函数原型：**
```c
XXAPI ptr xrtStackPop(xstack objSTK);
```

**参数：**
- `objSTK` - 栈对象

**返回值：**
- 成功：弹出元素的指针（仍有效，直到下次 Push）
- 失败：`NULL`（栈为空）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // 压入
    for (int i = 1; i <= 5; i++) {
        Item* item = (Item*)xrtStackPush(stk);
        item->value = i * 10;
    }
    
    // 弹出
    printf("弹出顺序: ");
    Item* item;
    while ((item = (Item*)xrtStackPop(stk)) != NULL) {
        printf("%d ", item->value);
    }
    printf("\n");  // 50 40 30 20 10
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

**注意：** 返回的指针在下次 Push 前有效，之后可能被覆盖。

---

### xrtStackPopPtr

弹出指针元素。

**函数原型：**
```c
XXAPI ptr xrtStackPopPtr(xstack objSTK);
```

**参数：**
- `objSTK` - 栈对象

**返回值：**
- 成功：存储的指针值
- 失败：`NULL`（栈为空）

---

## 栈访问

### xrtStackTop / xrtStackTopPtr

获取栈顶元素（不弹出）。

**函数原型：**
```c
XXAPI ptr xrtStackTop(xstack objSTK);      // 结构体栈
XXAPI ptr xrtStackTopPtr(xstack objSTK);   // 指针栈
```

**返回值：**
- 成功：栈顶元素指针/指针值
- 失败：`NULL`（栈为空）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // 压入 3 个元素
    ((Item*)xrtStackPush(stk))->value = 10;
    ((Item*)xrtStackPush(stk))->value = 20;
    ((Item*)xrtStackPush(stk))->value = 30;
    
    // 查看栈顶（不弹出）
    Item* top = (Item*)xrtStackTop(stk);
    printf("栈顶: %d\n", top->value);  // 30
    printf("栈深度: %u\n", stk->Count);  // 3（未改变）
    
    // 修改栈顶
    top->value = 99;
    
    // 弹出验证
    Item* popped = (Item*)xrtStackPop(stk);
    printf("弹出: %d\n", popped->value);  // 99
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackGetPos / xrtStackGetPos_Unsafe

获取任意位置的结构体元素。

**函数原型：**
```c
XXAPI ptr xrtStackGetPos(xstack objSTK, uint32 iPos);         // 安全版
XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint32 iPos);  // 不安全版
```

**参数：**
- `objSTK` - 栈对象
- `iPos` - 元素位置（**1-based**，1 = 栈底）

**返回值：**
- 成功：元素指针
- 失败：`NULL`（越界，仅安全版）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    // 压入 5 个元素
    for (int i = 1; i <= 5; i++) {
        ((Item*)xrtStackPush(stk))->value = i * 10;
    }
    // 栈内容: [10, 20, 30, 40, 50]
    //         pos=1  2   3   4   5
    
    // 安全版访问
    Item* item = (Item*)xrtStackGetPos(stk, 3);
    printf("位置3: %d\n", item->value);  // 30
    
    // 越界访问（安全版返回 NULL）
    Item* invalid = (Item*)xrtStackGetPos(stk, 10);
    printf("越界: %s\n", invalid ? "有效" : "NULL");  // NULL
    
    // 遍历所有元素（从栈底到栈顶）
    printf("所有元素: ");
    for (uint32 i = 1; i <= stk->Count; i++) {
        Item* it = (Item*)xrtStackGetPos(stk, i);
        printf("%d ", it->value);
    }
    printf("\n");  // 10 20 30 40 50
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### xrtStackGetPosPtr / xrtStackGetPosPtr_Unsafe

获取任意位置的指针元素。

**函数原型：**
```c
XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint32 iPos);         // 安全版
XXAPI ptr xrtStackGetPosPtr_Unsafe(xstack objSTK, uint32 iPos);  // 不安全版
```

**参数：**
- `objSTK` - 栈对象
- `iPos` - 元素位置（**1-based**）

**返回值：**
- 成功：存储的指针值
- 失败：`NULL`（越界，仅安全版）

---

## 使用场景

### 1. 表达式求值

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xstack numStack = xrtStackCreate(100, sizeof(int));
    
    // 模拟计算 3 + 4 * 2
    // 数字入栈
    *(int*)xrtStackPush(numStack) = 3;
    *(int*)xrtStackPush(numStack) = 4;
    *(int*)xrtStackPush(numStack) = 2;
    
    // 计算 4 * 2
    int b = *(int*)xrtStackPop(numStack);  // 2
    int a = *(int*)xrtStackPop(numStack);  // 4
    *(int*)xrtStackPush(numStack) = a * b;  // 8
    
    // 计算 3 + 8
    b = *(int*)xrtStackPop(numStack);  // 8
    a = *(int*)xrtStackPop(numStack);  // 3
    int result = a + b;  // 11
    
    printf("3 + 4 * 2 = %d\n", result);
    
    xrtStackDestroy(numStack);
    xrtUnit();
    return 0;
}
```

---

### 2. 括号匹配

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

bool CheckBrackets(const char* expr) {
    xstack stk = xrtStackCreate(strlen(expr), sizeof(char));
    bool valid = TRUE;
    
    for (int i = 0; expr[i] && valid; i++) {
        char c = expr[i];
        if (c == '(' || c == '[' || c == '{') {
            *(char*)xrtStackPush(stk) = c;
        } else if (c == ')' || c == ']' || c == '}') {
            if (stk->Count == 0) {
                valid = FALSE;
            } else {
                char open = *(char*)xrtStackPop(stk);
                if ((c == ')' && open != '(') ||
                    (c == ']' && open != '[') ||
                    (c == '}' && open != '{')) {
                    valid = FALSE;
                }
            }
        }
    }
    
    valid = valid && (stk->Count == 0);
    xrtStackDestroy(stk);
    return valid;
}

int main() {
    xrtInit();
    
    printf("({[]}) : %s\n", CheckBrackets("({[]})") ? "有效" : "无效");  // 有效
    printf("({[}]) : %s\n", CheckBrackets("({[}])") ? "有效" : "无效");  // 无效
    printf("((())  : %s\n", CheckBrackets("((())") ? "有效" : "无效");   // 无效
    
    xrtUnit();
    return 0;
}
```

---

### 3. 路径回溯

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Position;

int main() {
    xrtInit();
    
    xstack path = xrtStackCreate(100, sizeof(Position));
    
    // 模拟路径记录
    Position moves[] = {{0,0}, {1,0}, {1,1}, {2,1}, {2,2}};
    for (int i = 0; i < 5; i++) {
        xrtStackPushData(path, &moves[i]);
    }
    
    printf("路径长度: %u\n", path->Count);
    
    // 回溯到起点
    printf("回溯路径: ");
    Position* pos;
    while ((pos = (Position*)xrtStackPop(path)) != NULL) {
        printf("(%d,%d) ", pos->x, pos->y);
    }
    printf("\n");  // (2,2) (2,1) (1,1) (1,0) (0,0)
    
    xrtStackDestroy(path);
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 选择合适的栈类型

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 已知最大深度 → 静态栈（更快）
    xstack fixedStack = xrtStackCreate(100, sizeof(int));
    
    // 深度不确定 → 动态栈（见 api-dynstack.md）
    // xdynstack dynStack = xrtDynStackCreate(sizeof(int));
    
    printf("静态栈: 固定容量，内存连续，性能最优\n");
    printf("动态栈: 自动扩展，无限深度\n");
    
    xrtStackDestroy(fixedStack);
    xrtUnit();
    return 0;
}
```

---

### 2. 栈满检查

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(3, sizeof(Item));
    
    for (int i = 0; i < 5; i++) {
        // 检查是否还有空间
        if (stk->Count >= stk->MaxCount) {
            printf("栈已满，无法压入第 %d 个元素\n", i + 1);
            break;
        }
        
        Item* item = (Item*)xrtStackPush(stk);
        item->value = i;
        printf("压入: %d\n", i);
    }
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

### 3. 避免悬挂指针

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xstack stk = xrtStackCreate(10, sizeof(Item));
    
    ((Item*)xrtStackPush(stk))->value = 100;
    
    // Pop 返回的指针在下次 Push 前有效
    Item* popped = (Item*)xrtStackPop(stk);
    printf("刚弹出: %d\n", popped->value);  // 100 ✓
    
    // Push 后原指针可能被覆盖
    ((Item*)xrtStackPush(stk))->value = 200;
    // printf("%d\n", popped->value);  // 危险！可能是 200
    
    // ✓ 正确做法：立即使用或复制
    Item copy = *popped;  // 复制数据
    
    xrtStackDestroy(stk);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [DynStack 动态栈](api-dynstack.md)
- [Array 结构体数组](api-array.md)
- [PtrArray 指针数组](api-ptrarray.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#stack-静态栈库)

</div>
