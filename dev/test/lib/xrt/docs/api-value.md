# Value 动态类型系统库

> 16种动态数据类型，引用计数自动管理，支持容器嵌套

[English](api-value.en.md) | [中文](api-value.md) | [返回索引](README.md)

---

## 📑 目录

- [类型系统](#类型系统)
- [引用管理](#引用管理)
- [创建Value](#创建value)
- [读取值](#读取值)
- [类型判断](#类型判断)
- [数组操作](#数组操作)
- [列表操作](#列表操作)
- [集合操作](#集合操作)
- [表操作](#表操作)
- [拷贝操作](#拷贝操作)
- [调试函数](#调试函数)

---

## 类型系统

### Value类型常量

```c
#define XVO_DT_EMPTY    0   // 不存在的数据
#define XVO_DT_NULL     1   // null
#define XVO_DT_BOOL     2   // bool : true | false
#define XVO_DT_INT      3   // 整数（int64）
#define XVO_DT_FLOAT    4   // 浮点数（double）
#define XVO_DT_TEXT     5   // 字符串
#define XVO_DT_TIME     6   // 时间
#define XVO_DT_POINT    7   // 指针
#define XVO_DT_FUNC     8   // 函数
#define XVO_DT_ARRAY    9   // 数组
#define XVO_DT_LIST     10  // 列表
#define XVO_DT_COLL     11  // 集合
#define XVO_DT_TABLE    12  // 表
#define XVO_DT_STRUCT   13  // 结构体
#define XVO_DT_OBJECT   14  // 对象
#define XVO_DT_CUSTOM   15  // 自定义
```

### Value结构 [16字节]

```c
typedef struct xvalue_struct {
    uint32 Type:4;          // 类型（4位）
    uint32 Reserve:1;       // 保留（1位）
    uint32 IsStatic:1;      // 是否静态（1位）
    uint32 RefCount:26;     // 引用计数（26位，最大67108863）
    uint32 Size;            // 数据大小
    union {
        bool vBool;         // 布尔值
        int64 vInt;         // 整数值
        double vFloat;      // 浮点值
        str vText;          // 字符串指针
        xtime vTime;        // 时间值
        ptr vPoint;         // 指针
        xfunction vFunc;    // 函数指针
        xparray vArray;     // 数组
        xlist vList;        // 列表
        xavltree vColl;     // 集合
        xdict vTable;       // 表
        ptr vStruct;        // 结构体
        ptr vObject;        // 对象
        ptr vCustom;        // 自定义数据
    };
} xvalue_struct, *xvalue;

// 函数指针类型
typedef xvalue (*xfunction)(xvalue pENV, xvalue arrParam);
```

---

## 引用管理

### xvoAddRef

增加引用计数

**函数原型：**
```c
XXAPI void xvoAddRef(xvalue pVal);

// 内联版本（性能更好）
static inline void xvoAddRef_Inline(xvalue pVal);
```

**说明：**
- 引用计数+1
- 当引用计数达到最大值(0x3FFFFFF)时，自动转为静态值

---

### xvoUnref

减少引用计数

**函数原型：**
```c
XXAPI void xvoUnref(xvalue pVal);
```

**说明：**
- 引用计数-1
- 计数为0时自动释放值
- 容器类型会递归释放所有子元素
- 静态值(IsStatic=1)不会被释放

**示例：**
```c
xvalue v = xvoCreateInt(100);
xvoAddRef(v);   // RefCount = 2
xvoUnref(v);    // RefCount = 1
xvoUnref(v);    // RefCount = 0，释放
```

---

## 创建Value

### xvoCreateNull

创建 null 值（返回静态单例）

**函数原型：**
```c
XXAPI xvalue xvoCreateNull();
```

**返回值：**
- null类型的静态 Value

**说明：** null/true/false 使用静态单例，无需释放

---

### xvoCreateBool

创建布尔值（返回静态单例）

**函数原型：**
```c
XXAPI xvalue xvoCreateBool(bool bVal);
```

**说明：** 返回 TRUE/FALSE 的静态单例

---

### xvoCreateInt

创建整数

**函数原型：**
```c
XXAPI xvalue xvoCreateInt(int64 iVal);
```

**释放：** 🔄 使用 `xvoUnref` 释放

---

### xvoCreateFloat

创建浮点数

**函数原型：**
```c
XXAPI xvalue xvoCreateFloat(double fVal);
```

---

### xvoCreateText

创建字符串

**函数原型：**
```c
XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `sVal` | 字符串指针 |
| `iSize` | 长度（0表示自动计算） |
| `bColloc` | 托管模式 |

**托管模式说明：**
- `TRUE` - 直接托管字符串指针，不复制，释放时不释放字符串
- `FALSE` - 复制字符串内容，释放时释放复制的字符串

**示例：**
```c
// 复制字符串（安全模式）
xvalue v1 = xvoCreateText("Hello", 0, FALSE);

// 托管静态字符串（高效模式）
xvalue v2 = xvoCreateText("Static", 0, TRUE);
```

---

### xvoCreateTime

创建时间值

**函数原型：**
```c
XXAPI xvalue xvoCreateTime(xtime tVal);
```

---

### xvoCreateTimeSerial

通过日期时间分量创建时间值

**函数原型：**
```c
XXAPI xvalue xvoCreateTimeSerial(
    int64 iYear,    // 年
    int iMonth,     // 月
    int iDay,       // 日
    int iHour,      // 时
    int iMinute,    // 分
    int iSecond     // 秒
);
```

---

### xvoCreatePoint

创建指针

**函数原型：**
```c
XXAPI xvalue xvoCreatePoint(ptr point);
```

---

### xvoCreateFunc

创建函数引用

**函数原型：**
```c
XXAPI xvalue xvoCreateFunc(xfunction pFunc);
```

**说明：** 用于存储函数指针，支持回调

---

### xvoCreateArray

创建空数组

**函数原型：**
```c
XXAPI xvalue xvoCreateArray();
```

**说明：** 基于 `xparray` 实现，支持动态扩容

---

### xvoCreateList

创建空列表

**函数原型：**
```c
XXAPI xvalue xvoCreateList();
```

**说明：** 基于 `xlist` 实现，键为 int64 类型

---

### xvoCreateColl

创建空集合

**函数原型：**
```c
XXAPI xvalue xvoCreateColl();
```

**说明：** 基于 AVL树 实现，元素自动去重排序

---

### xvoCreateTable

创建空表（字典）

**函数原型：**
```c
XXAPI xvalue xvoCreateTable();
```

**说明：** 基于 `xdict` 实现，字符串键

---

### xvoCreateStruct

创建结构体容器

**函数原型：**
```c
XXAPI xvalue xvoCreateStruct(uint32 iSize);
```

**参数：**
- `iSize` - 结构体大小（字节），不能为0

**返回值：**
- 成功返回 xvalue，失败返回 NULL

---

### xvoCreateObject

创建对象容器

**函数原型：**
```c
XXAPI xvalue xvoCreateObject(uint32 iSize);
```

---

### xvoCreateCustom

创建自定义类型

**函数原型：**
```c
XXAPI xvalue xvoCreateCustom(ptr pObj);
```

---

## 读取值

### xvoGetBool

获取布尔值（支持自动类型转换）

**函数原型：**
```c
XXAPI bool xvoGetBool(xvalue pVal);
```

**转换规则：**
- NULL/EMPTY → FALSE
- BOOL → 返回原值
- INT → 非0为TRUE
- FLOAT → 非0.0为TRUE
- 其他类型 → TRUE

---

### xvoGetInt

获取整数值（支持自动类型转换）

**函数原型：**
```c
XXAPI int64 xvoGetInt(xvalue pVal);
```

**转换规则：**
- NULL/EMPTY → 0
- BOOL → 1 或 0
- INT → 返回原值
- FLOAT → 截断为整数
- TEXT → 解析字符串
- 其他 → 0

---

### xvoGetFloat

获取浮点数（支持自动类型转换）

**函数原型：**
```c
XXAPI double xvoGetFloat(xvalue pVal);
```

---

### xvoGetText

获取字符串（支持自动类型转换）

**函数原型：**
```c
XXAPI str xvoGetText(xvalue pVal);
```

**说明：** 非 TEXT 类型返回临时字符串，不需要释放

---

### xvoGetTime

获取时间值

**函数原型：**
```c
XXAPI xtime xvoGetTime(xvalue pVal);
```

---

### xvoGetPoint

获取指针

**函数原型：**
```c
XXAPI ptr xvoGetPoint(xvalue pVal);
```

---

### xvoGetFunc

获取函数指针

**函数原型：**
```c
XXAPI xfunction xvoGetFunc(xvalue pVal);
```

---

### 容器获取函数

获取容器的底层数据结构

**函数原型：**
```c
XXAPI xparray xvoGetArray(xvalue pVal);    // 获取数组
XXAPI xlist xvoGetList(xvalue pVal);       // 获取列表
XXAPI xavltree xvoGetColl(xvalue pVal);    // 获取集合
XXAPI xdict xvoGetTable(xvalue pVal);      // 获取表
XXAPI ptr xvoGetStruct(xvalue pVal);       // 获取结构体
XXAPI ptr xvoGetObject(xvalue pVal);       // 获取对象
XXAPI ptr xvoGetCustom(xvalue pVal);       // 获取自定义数据
```

---

## 类型判断

### xvoIsNull

检查是否为 NULL 或 EMPTY

**函数原型：**
```c
XXAPI bool xvoIsNull(xvalue pVal);
```

**说明：** 当 pVal==NULL 或 Type==NULL 或 Type==EMPTY 时返回 TRUE

---

### xvoType

获取值的类型

**函数原型：**
```c
XXAPI int xvoType(xvalue pVal);
```

**返回值：** XVO_DT_* 类型常量

**示例：**
```c
xvalue v = xvoCreateInt(123);
if (xvoType(v) == XVO_DT_INT) {
    printf("It's an integer\n");
}
xvoUnref(v);
```

---

### xvoGetSize

获取数据大小

**函数原型：**
```c
XXAPI uint32 xvoGetSize(xvalue pVal);
```

**说明：**
- TEXT 返回字符串长度
- STRUCT/OBJECT 返回结构体大小
- 其他类型返回固定大小

---

### 容器元素类型判断宏

```c
// 获取容器元素类型
#define xvoArrayItemType(pArr, index)        xvoType(xvoArrayGetValue(pArr, index))
#define xvoListItemType(pList, index)        xvoType(xvoListGetValue(pList, index))
#define xvoTableItemType(pTbl, key, kl)      xvoType(xvoTableGetValue(pTbl, key, kl))

// 获取容器元素大小
#define xvoArrayItemSize(pArr, index)        xvoGetSize(xvoArrayGetValue(pArr, index))
#define xvoListItemSize(pList, index)        xvoGetSize(xvoListGetValue(pList, index))
#define xvoTableItemSize(pTbl, key, kl)      xvoGetSize(xvoTableGetValue(pTbl, key, kl))
```

---

## 数组操作

### xvoArrayGetValue

获取数组元素

**函数原型：**
```c
XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `pArr` | 数组 Value |
| `index` | 索引（从0开始） |

**返回值：** 元素 Value，不存在则返回 EMPTY 静态值

**便捷宏：**
```c
#define xvoArrayGetBool(pArr, index)    xvoGetBool(xvoArrayGetValue(pArr, index))
#define xvoArrayGetInt(pArr, index)     xvoGetInt(xvoArrayGetValue(pArr, index))
#define xvoArrayGetFloat(pArr, index)   xvoGetFloat(xvoArrayGetValue(pArr, index))
#define xvoArrayGetText(pArr, index)    xvoGetText(xvoArrayGetValue(pArr, index))
#define xvoArrayGetTime(pArr, index)    xvoGetTime(xvoArrayGetValue(pArr, index))
#define xvoArrayGetPoint(pArr, index)   xvoGetPoint(xvoArrayGetValue(pArr, index))
#define xvoArrayGetFunc(pArr, index)    xvoGetFunc(xvoArrayGetValue(pArr, index))
#define xvoArrayGetArray(pArr, index)   xvoGetArray(xvoArrayGetValue(pArr, index))
#define xvoArrayGetList(pArr, index)    xvoGetList(xvoArrayGetValue(pArr, index))
#define xvoArrayGetColl(pArr, index)    xvoGetColl(xvoArrayGetValue(pArr, index))
#define xvoArrayGetTable(pArr, index)   xvoGetTable(xvoArrayGetValue(pArr, index))
#define xvoArrayGetStruct(pArr, index)  xvoGetStruct(xvoArrayGetValue(pArr, index))
#define xvoArrayGetObject(pArr, index)  xvoGetObject(xvoArrayGetValue(pArr, index))
#define xvoArrayGetCustom(pArr, index)  xvoGetCustom(xvoArrayGetValue(pArr, index))
```

---

### xvoArrayAppendValue

追加元素到数组末尾

**函数原型：**
```c
XXAPI bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `pArr` | 数组 Value |
| `pVal` | 要追加的值 |
| `bColloc` | TRUE=托管引用，FALSE=增加引用计数 |

**便捷宏：**
```c
#define xvoArrayAppendNull(pArr)                 xvoArrayAppendValue(pArr, xvoCreateNull(), TRUE)
#define xvoArrayAppendBool(pArr, bVal)           xvoArrayAppendValue(pArr, xvoCreateBool(bVal), TRUE)
#define xvoArrayAppendInt(pArr, iVal)            xvoArrayAppendValue(pArr, xvoCreateInt(iVal), TRUE)
#define xvoArrayAppendFloat(pArr, fVal)          xvoArrayAppendValue(pArr, xvoCreateFloat(fVal), TRUE)
#define xvoArrayAppendText(pArr, sVal, iSize, bColloc)  xvoArrayAppendValue(pArr, xvoCreateText(sVal, iSize, bColloc), TRUE)
#define xvoArrayAppendTime(pArr, tVal)           xvoArrayAppendValue(pArr, xvoCreateTime(tVal), TRUE)
#define xvoArrayAppendPoint(pArr, pVal)          xvoArrayAppendValue(pArr, xvoCreatePoint(pVal), TRUE)
#define xvoArrayAppendFunc(pArr, func)           xvoArrayAppendValue(pArr, xvoCreateFunc(func), TRUE)
#define xvoArrayAppendArray(pArr)                xvoArrayAppendValue(pArr, xvoCreateArray(), TRUE)
#define xvoArrayAppendList(pArr)                 xvoArrayAppendValue(pArr, xvoCreateList(), TRUE)
#define xvoArrayAppendColl(pArr)                 xvoArrayAppendValue(pArr, xvoCreateColl(), TRUE)
#define xvoArrayAppendTable(pArr)                xvoArrayAppendValue(pArr, xvoCreateTable(), TRUE)
#define xvoArrayAppendStruct(pArr, size)         xvoArrayAppendValue(pArr, xvoCreateStruct(size), TRUE)
#define xvoArrayAppendObject(pArr, size)         xvoArrayAppendValue(pArr, xvoCreateObject(size), TRUE)
#define xvoArrayAppendCustom(pArr, point)        xvoArrayAppendValue(pArr, xvoCreateCustom(point), TRUE)
```

**示例：**
```c
xvalue arr = xvoCreateArray();
xvoArrayAppendInt(arr, 1);
xvoArrayAppendInt(arr, 2);
xvoArrayAppendText(arr, "hello", 0, TRUE);
xvoUnref(arr);  // 自动释放所有子元素
```

---

### xvoArrayInsertValue

在指定位置插入元素

**函数原型：**
```c
XXAPI bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
```

**便捷宏：**
```c
#define xvoArrayInsertNull(pArr, idx)            xvoArrayInsertValue(pArr, idx, xvoCreateNull(), TRUE)
#define xvoArrayInsertBool(pArr, idx, bVal)      xvoArrayInsertValue(pArr, idx, xvoCreateBool(bVal), TRUE)
#define xvoArrayInsertInt(pArr, idx, iVal)       xvoArrayInsertValue(pArr, idx, xvoCreateInt(iVal), TRUE)
// ... 其他类型类似
```

---

### xvoArraySetValue

修改指定位置的元素

**函数原型：**
```c
XXAPI bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
```

**说明：** 会自动释放旧值

**便捷宏：**
```c
#define xvoArraySetNull(pArr, idx)               xvoArraySetValue(pArr, idx, xvoCreateNull(), TRUE)
#define xvoArraySetBool(pArr, idx, bVal)         xvoArraySetValue(pArr, idx, xvoCreateBool(bVal), TRUE)
#define xvoArraySetInt(pArr, idx, iVal)          xvoArraySetValue(pArr, idx, xvoCreateInt(iVal), TRUE)
// ... 其他类型类似
```

---

### 数组其他操作

```c
// 合并数组（将 pArr2 元素追加到 pArr1）
XXAPI bool xvoArrayMerge(xvalue pArr1, xvalue pArr2);

// 交换元素位置
XXAPI bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2);

// 删除元素
XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);

// 获取元素数量
XXAPI uint32 xvoArrayItemCount(xvalue pArr);

// 清空数组
XXAPI bool xvoArrayClear(xvalue pArr);

// 预分配容量
XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count);

// 排序
XXAPI bool xvoArraySort(xvalue pArr, ptr proc);
```

**示例：**
```c
xvalue arr = xvoCreateArray();
xvoArrayAppendInt(arr, 3);
xvoArrayAppendInt(arr, 1);
xvoArrayAppendInt(arr, 2);

uint32 count = xvoArrayItemCount(arr);  // 3
int64 val = xvoArrayGetInt(arr, 0);     // 3

xvoArraySwap(arr, 0, 2);  // 交换第1和第3个元素
xvoArrayRemove(arr, 1, 1); // 删除第2个元素

xvoUnref(arr);
```

---

## 列表操作

### xvoListGetValue

获取列表元素

**函数原型：**
```c
XXAPI xvalue xvoListGetValue(xvalue pList, int64 index);
```

**说明：** 索引可以是任意 int64 值（类似稀疏数组）

**便捷宏：**
```c
#define xvoListGetBool(pList, index)    xvoGetBool(xvoListGetValue(pList, index))
#define xvoListGetInt(pList, index)     xvoGetInt(xvoListGetValue(pList, index))
#define xvoListGetFloat(pList, index)   xvoGetFloat(xvoListGetValue(pList, index))
// ... 其他类型类似
```

---

### xvoListSetValue

设置列表元素

**函数原型：**
```c
XXAPI bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc);
```

**便捷宏：**
```c
#define xvoListSetNull(pList, idx)               xvoListSetValue(pList, idx, xvoCreateNull(), TRUE)
#define xvoListSetBool(pList, idx, bVal)         xvoListSetValue(pList, idx, xvoCreateBool(bVal), TRUE)
#define xvoListSetInt(pList, idx, iVal)          xvoListSetValue(pList, idx, xvoCreateInt(iVal), TRUE)
// ... 其他类型类似
```

---

### 列表其他操作

```c
// 合并列表
XXAPI bool xvoListMerge(xvalue pList1, xvalue pList2, bool bReWrite);

// 检查元素是否存在
XXAPI bool xvoListExists(xvalue pList, int64 index);

// 删除元素
XXAPI bool xvoListRemove(xvalue pList, int64 index);

// 获取元素数量
XXAPI uint32 xvoListItemCount(xvalue pList);

// 清空列表
XXAPI bool xvoListClear(xvalue pList);

// 设置父列表（用于继承查找）
XXAPI bool xvoListSetParent(xvalue pList, xvalue pParentList);
```

**示例：**
```c
xvalue list = xvoCreateList();
xvoListSetInt(list, 100, 1);     // 键100 = 1
xvoListSetInt(list, 200, 2);     // 键200 = 2
xvoListSetInt(list, -50, 3);     // 键-50 = 3

if (xvoListExists(list, 100)) {
    int64 val = xvoListGetInt(list, 100);  // 1
}

xvoUnref(list);
```

---

## 集合操作

集合（Coll）基于 AVL树 实现，元素自动去重和排序。

### xvoCollSetValue

添加元素到集合

**函数原型：**
```c
XXAPI bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc);
```

**便捷宏：**
```c
#define xvoCollSetNull(pColl)                   xvoCollSetValue(pColl, xvoCreateNull(), TRUE)
#define xvoCollSetBool(pColl, bVal)             xvoCollSetValue(pColl, xvoCreateBool(bVal), TRUE)
#define xvoCollSetInt(pColl, iVal)              xvoCollSetValue(pColl, xvoCreateInt(iVal), TRUE)
#define xvoCollSetFloat(pColl, fVal)            xvoCollSetValue(pColl, xvoCreateFloat(fVal), TRUE)
#define xvoCollSetText(pColl, sVal, iSize, bColloc)  xvoCollSetValue(pColl, xvoCreateText(sVal, iSize, bColloc), TRUE)
#define xvoCollSetTime(pColl, tVal)             xvoCollSetValue(pColl, xvoCreateTime(tVal), TRUE)
#define xvoCollSetPoint(pColl, pVal)            xvoCollSetValue(pColl, xvoCreatePoint(pVal), TRUE)
#define xvoCollSetFunc(pColl, func)             xvoCollSetValue(pColl, xvoCreateFunc(func), TRUE)
#define xvoCollSetCustom(pColl, point)          xvoCollSetValue(pColl, xvoCreateCustom(point), TRUE)
```

---

### 集合运算

```c
// 获取差集 [ pSelf 中有但 pColl 中没有的元素 ]
XXAPI xvalue xvoCollDifference(xvalue pSelf, xvalue pColl);

// 获取对称差集 [ 两个集合中不重复的元素 ]
XXAPI xvalue xvoCollSymmetricDifference(xvalue pSelf, xvalue pColl);

// 获取交集 [ 两个集合都存在的元素 ]
XXAPI xvalue xvoCollIntersection(xvalue pSelf, xvalue pColl);

// 获取并集 [ 合并两个集合，返回新集合 ]
XXAPI xvalue xvoCollUnion(xvalue pSelf, xvalue pColl);

// 合并集合 [ 将 pColl 元素并入 pSelf ]
XXAPI bool xvoCollMerge(xvalue pSelf, xvalue pColl);
```

**说明：** 差集、对称差集、交集、并集都返回新创建的集合，需要释放

---

### 集合其他操作

```c
// 检查元素是否存在
XXAPI bool xvoCollExists(xvalue pColl, xvalue pVal);

// 删除元素
XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal);

// 获取元素数量
XXAPI uint32 xvoCollItemCount(xvalue pColl);

// 清空集合
XXAPI bool xvoCollClear(xvalue pColl);

// 设置父集合
XXAPI bool xvoCollSetParent(xvalue pColl, xvalue pParentColl);
```

**示例：**
```c
// 创建两个集合
xvalue setA = xvoCreateColl();
xvoCollSetInt(setA, 1);
xvoCollSetInt(setA, 2);
xvoCollSetInt(setA, 3);

xvalue setB = xvoCreateColl();
xvoCollSetInt(setB, 2);
xvoCollSetInt(setB, 3);
xvoCollSetInt(setB, 4);

// 集合运算
xvalue diff = xvoCollDifference(setA, setB);      // {1}
xvalue inter = xvoCollIntersection(setA, setB);   // {2, 3}
xvalue uni = xvoCollUnion(setA, setB);            // {1, 2, 3, 4}
xvalue symDiff = xvoCollSymmetricDifference(setA, setB);  // {1, 4}

// 清理
xvoUnref(setA);
xvoUnref(setB);
xvoUnref(diff);
xvoUnref(inter);
xvoUnref(uni);
xvoUnref(symDiff);
```

---

## 表操作

表（Table）基于 `xdict` 实现，使用字符串作为键。

### xvoTableGetValue

获取表元素

**函数原型：**
```c
XXAPI xvalue xvoTableGetValue(xvalue pTbl, str key, uint32 kl);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `pTbl` | 表 Value |
| `key` | 字符串键 |
| `kl` | 键长度（0表示自动计算） |

**便捷宏：**
```c
#define xvoTableGetBool(pTbl, key, kl)    xvoGetBool(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetInt(pTbl, key, kl)     xvoGetInt(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetFloat(pTbl, key, kl)   xvoGetFloat(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetText(pTbl, key, kl)    xvoGetText(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetTime(pTbl, key, kl)    xvoGetTime(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetPoint(pTbl, key, kl)   xvoGetPoint(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetFunc(pTbl, key, kl)    xvoGetFunc(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetArray(pTbl, key, kl)   xvoGetArray(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetList(pTbl, key, kl)    xvoGetList(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetColl(pTbl, key, kl)    xvoGetColl(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetTable(pTbl, key, kl)   xvoGetTable(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetStruct(pTbl, key, kl)  xvoGetStruct(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetObject(pTbl, key, kl)  xvoGetObject(xvoTableGetValue(pTbl, key, kl))
#define xvoTableGetCustom(pTbl, key, kl)  xvoGetCustom(xvoTableGetValue(pTbl, key, kl))
```

---

### xvoTableSetValue

设置表元素

**函数原型：**
```c
XXAPI bool xvoTableSetValue(xvalue pTbl, str key, uint32 kl, xvalue pVal, bool bColloc);
```

**便捷宏：**
```c
#define xvoTableSetNull(pTbl, key, kl)           xvoTableSetValue(pTbl, key, kl, xvoCreateNull(), TRUE)
#define xvoTableSetBool(pTbl, key, kl, bVal)     xvoTableSetValue(pTbl, key, kl, xvoCreateBool(bVal), TRUE)
#define xvoTableSetInt(pTbl, key, kl, iVal)      xvoTableSetValue(pTbl, key, kl, xvoCreateInt(iVal), TRUE)
#define xvoTableSetFloat(pTbl, key, kl, fVal)    xvoTableSetValue(pTbl, key, kl, xvoCreateFloat(fVal), TRUE)
#define xvoTableSetText(pTbl, key, kl, sVal, iSize, bColloc)  xvoTableSetValue(pTbl, key, kl, xvoCreateText(sVal, iSize, bColloc), TRUE)
#define xvoTableSetTime(pTbl, key, kl, tVal)     xvoTableSetValue(pTbl, key, kl, xvoCreateTime(tVal), TRUE)
#define xvoTableSetPoint(pTbl, key, kl, pVal)    xvoTableSetValue(pTbl, key, kl, xvoCreatePoint(pVal), TRUE)
#define xvoTableSetFunc(pTbl, key, kl, func)     xvoTableSetValue(pTbl, key, kl, xvoCreateFunc(func), TRUE)
#define xvoTableSetArray(pTbl, key, kl)          xvoTableSetValue(pTbl, key, kl, xvoCreateArray(), TRUE)
#define xvoTableSetList(pTbl, key, kl)           xvoTableSetValue(pTbl, key, kl, xvoCreateList(), TRUE)
#define xvoTableSetColl(pTbl, key, kl)           xvoTableSetValue(pTbl, key, kl, xvoCreateColl(), TRUE)
#define xvoTableSetTable(pTbl, key, kl)          xvoTableSetValue(pTbl, key, kl, xvoCreateTable(), TRUE)
#define xvoTableSetStruct(pTbl, key, kl, size)   xvoTableSetValue(pTbl, key, kl, xvoCreateStruct(size), TRUE)
#define xvoTableSetObject(pTbl, key, kl, size)   xvoTableSetValue(pTbl, key, kl, xvoCreateObject(size), TRUE)
#define xvoTableSetCustom(pTbl, key, kl, point)  xvoTableSetValue(pTbl, key, kl, xvoCreateCustom(point), TRUE)
```

---

### 表其他操作

```c
// 合并表
XXAPI bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite);

// 检查键是否存在
XXAPI bool xvoTableExists(xvalue pTbl, str key, uint32 kl);

// 删除键
XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl);

// 获取元素数量
XXAPI uint32 xvoTableItemCount(xvalue pTbl);

// 清空表
XXAPI bool xvoTableClear(xvalue pTbl);

// 设置父表（用于继承查找）
XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable);
```

**示例：**
```c
xvalue user = xvoCreateTable();
xvoTableSetInt(user, "id", 0, 1001);
xvoTableSetText(user, "name", 0, "Alice", 0, TRUE);
xvoTableSetBool(user, "active", 0, TRUE);

// 读取
if (xvoTableExists(user, "name", 0)) {
    str name = xvoTableGetText(user, "name", 0);  // "Alice"
    int64 id = xvoTableGetInt(user, "id", 0);     // 1001
}

// 嵌套表
xvoTableSetTable(user, "profile", 0);
xvalue profile = xvoTableGetValue(user, "profile", 0);
xvoTableSetInt(profile, "age", 0, 25);
xvoTableSetText(profile, "city", 0, "Beijing", 0, TRUE);

xvoUnref(user);  // 自动释放所有子元素
```

---

## 拷贝操作

### xvoCopy

浅拷贝

**函数原型：**
```c
XXAPI xvalue xvoCopy(xvalue pVal);
```

**说明：**
- 基础类型（BOOL/INT/FLOAT/TEXT 等）：创建新值
- 复杂类型（ARRAY/LIST/COLL/TABLE）：复制容器结构，子元素增加引用
- NULL/BOOL 返回静态单例
- STRUCT/OBJECT/CUSTOM 返回 NULL

---

### xvoDeepCopy

深拷贝

**函数原型：**
```c
XXAPI xvalue xvoDeepCopy(xvalue pVal);
```

**说明：**
- 递归复制所有子元素
- 完全独立的副本

**示例：**
```c
xvalue original = xvoCreateTable();
xvoTableSetInt(original, "id", 0, 100);
xvoTableSetText(original, "name", 0, "test", 0, TRUE);

// 浅拷贝
xvalue shallow = xvoCopy(original);

// 深拷贝
xvalue deep = xvoDeepCopy(original);

xvoUnref(original);
xvoUnref(shallow);
xvoUnref(deep);
```

---

## 调试函数

### xvoPrintValue

打印 Value 的结构和值

**函数原型：**
```c
XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `objVal` | 要打印的值 |
| `iLevel` | 缩进级别（通常传0） |
| `iMode` | 0=直接输出，1=数组元素，2=表元素 |
| `iKey` | 数组索引（iMode=1时有效） |
| `sKey` | 表键名（iMode=2时有效） |

**说明：** 递归打印所有子元素，用于调试

**示例：**
```c
xvalue data = xvoCreateTable();
xvoTableSetInt(data, "id", 0, 100);
xvoTableSetText(data, "name", 0, "test", 0, TRUE);

// 打印结构
xvoPrintValue(data, 0, 0, 0, NULL);

/* 输出类似：
(table) [0x12345678] (table), count : 2
    ( int ) [0x12345680] "id" = 100
    (text ) [0x12345688] "name" = "test"
*/

xvoUnref(data);
```

---

## 使用场景

### 1. JSON风格数据

```c
// 创建用户对象
xvalue user = xvoCreateTable();
xvoTableSetInt(user, "id", 0, 1001);
xvoTableSetText(user, "name", 0, "John", 0, TRUE);
xvoTableSetText(user, "email", 0, "john@example.com", 0, TRUE);

// 创建用户列表
xvalue users = xvoCreateArray();
xvoArrayAppendValue(users, user, TRUE);

// 访问数据
xvalue first_user = xvoArrayGetValue(users, 0);
str name = xvoTableGetText(first_user, "name", 0);
printf("Name: %s\n", name);  // "John"

xvoUnref(users);
```

---

### 2. 配置系统

```c
xvalue config = xvoCreateTable();
xvoTableSetInt(config, "port", 0, 8080);
xvoTableSetText(config, "host", 0, "0.0.0.0", 0, TRUE);
xvoTableSetBool(config, "debug", 0, TRUE);

// 读取配置
int64 port = xvoTableGetInt(config, "port", 0);    // 8080
str host = xvoTableGetText(config, "host", 0);     // "0.0.0.0"
bool debug = xvoTableGetBool(config, "debug", 0);  // TRUE

xvoUnref(config);
```

---

### 3. 动态数据容器

```c
typedef struct {
    xvalue data;  // 可以存储任意类型
} DynamicContainer;

void SetValue(DynamicContainer* c, xvalue val) {
    if (c->data) {
        xvoUnref(c->data);
    }
    c->data = val;
    xvoAddRef(val);  // 增加引用
}

xvalue GetValue(DynamicContainer* c) {
    return c->data;
}

void FreeContainer(DynamicContainer* c) {
    if (c->data) {
        xvoUnref(c->data);
        c->data = NULL;
    }
}
```

---

## 最佳实践

### 1. 引用计数管理

```c
// ✅ 正确：创建后使用完释放
xvalue v = xvoCreateInt(100);
UseValue(v);
xvoUnref(v);

// ✅ 正确：bColloc=TRUE 表示托管，不需要单独释放子元素
xvalue arr = xvoCreateArray();
xvoArrayAppendInt(arr, 100);  // 内部使用 bColloc=TRUE
xvoUnref(arr);  // 容器释放时自动释放子元素

// ✅ 正确：bColloc=FALSE 时会增加引用计数
xvalue item = xvoCreateInt(200);
xvoArrayAppendValue(arr, item, FALSE);  // item 引用计数+1
xvoUnref(item);  // 原始引用-1，数组仍持有引用
```

---

### 2. 避免循环引用

```c
// ❌ 危险：循环引用导致内存泄漏
xvalue a = xvoCreateTable();
xvalue b = xvoCreateTable();
xvoTableSetValue(a, "b", 0, b, FALSE);  // a 引用 b
xvoTableSetValue(b, "a", 0, a, FALSE);  // b 引用 a
// 两者都无法释放！
```

---

### 3. 性能优化

```c
// ✅ 高效：常量字符串使用托管模式
xvalue v = xvoCreateText("constant", 0, TRUE);

// ❌ 低效：常量字符串也复制
xvalue v = xvoCreateText("constant", 0, FALSE);

// ✅ 高效：预分配容量
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 1000);  // 预分配1000个元素的容量
for (int i = 0; i < 1000; i++) {
    xvoArrayAppendInt(arr, i);
}
xvoUnref(arr);
```

---

## 相关文档

- [JSON 处理](api-json.md) - JSON与Value互转
- [Dict 字典](api-dict.md) - 底层字典实现
- [List 列表](api-list.md) - 底层列表实现
- [AVL树](api-avltree.md) - 集合底层实现
- [指针数组](api-ptrarray.md) - 数组底层实现
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#value-动态类型系统库)

</div>
