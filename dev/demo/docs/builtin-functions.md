# xlang 内置函数库参考手册

本文档详细说明 xlang 语言的内置函数库，包含功能描述、参数列表和返回值说明。

---

## 目录

1. [基础函数](#1-基础函数)
2. [类型转换函数](#2-类型转换函数)
3. [字符串函数](#3-字符串函数)
4. [数学函数](#4-数学函数)
5. [时间函数](#5-时间函数)
6. [文件函数](#6-文件函数)
7. [JSON函数](#7-json函数)
8. [编码函数](#8-编码函数)
9. [容器函数](#9-容器函数)

---

## 1. 基础函数

### print(value)

输出值到标准输出（不换行）。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型，要输出的值 |
| **返回值** | `null` |

```xlang
print("Hello")      // 输出: Hello
print(123)          // 输出: 123
print([1, 2, 3])    // 输出: [1, 2, 3]
```

---

### println(value)

输出值到标准输出（自动换行）。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型，要输出的值 |
| **返回值** | `null` |

```xlang
println("Hello World")
println(3.14)
```

---

### input(prompt)

从标准输入读取一行文本。

| 项目 | 说明 |
|------|------|
| **参数** | `prompt` - 字符串，提示信息（可选） |
| **返回值** | `string` - 用户输入的文本 |

```xlang
var name = input("请输入姓名: ")
println("你好, " + name)
```

---

### len(value)

获取值的长度。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 字符串、数组、字典或列表 |
| **返回值** | `int` - 元素个数或字符数 |

```xlang
println(len("hello"))     // 5
println(len([1, 2, 3]))   // 3
println(len({a: 1, b: 2})) // 2
```

---

### type(value)

获取值的类型名称。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型 |
| **返回值** | `string` - 类型名称 |

**类型名称对照表：**

| 类型 | 返回值 |
|------|--------|
| 空值 | `"null"` |
| 布尔 | `"bool"` |
| 整数 | `"int"` |
| 浮点数 | `"float"` |
| 字符串 | `"string"` |
| 数组 | `"array"` |
| 列表 | `"list"` |
| 字典 | `"table"` |
| 集合 | `"set"` |
| 时间 | `"time"` |
| 范围 | `"range"` |

```xlang
println(type(123))        // int
println(type("hello"))    // string
println(type([1, 2]))     // array
println(type({a: 1}))     // table
```

---

### typeof(value)

获取值的类型值（用于类型比较）。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型 |
| **返回值** | `int` - 类型标识值 |

```xlang
var t = typeof(123)
println(t)  // 类型标识值
```

---

### range(start, end, step?)

创建一个范围对象，用于 for-in 循环迭代。

| 项目 | 说明 |
|------|------|
| **参数** | `start` - 起始值（包含）<br>`end` - 结束值（不包含）<br>`step` - 步长（可选，默认自动推断） |
| **返回值** | `range` - 范围对象 |

**特性：**
- 支持 1-3 个参数
- 自动推断步长方向（正向/逆向）
- `range(n)` 等同于 `range(0, n, 1)`
- `range(a, b)` 自动推断步长为 1 或 -1

```xlang
// 单参数: 0 到 n-1
for i in range(5) {
	println(i)  // 0, 1, 2, 3, 4
}

// 双参数: start 到 end-1
for i in range(1, 6) {
	println(i)  // 1, 2, 3, 4, 5
}

// 逆向迭代（自动推断）
for i in range(5, 0) {
	println(i)  // 5, 4, 3, 2, 1
}

// 指定步长
for i in range(0, 10, 2) {
	println(i)  // 0, 2, 4, 6, 8
}
```

---

### hash(value)

计算值的哈希值。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型 |
| **返回值** | `int` - 哈希值 |

```xlang
println(hash("hello"))    // 字符串哈希
println(hash(123))        // 整数哈希
println(hash([1, 2, 3]))  // 数组哈希
```

---

### id(value)

获取值的内存地址或唯一标识。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型 |
| **返回值** | `int` - 内存地址 |

```xlang
var arr = [1, 2, 3]
println(id(arr))  // 数组的内存地址
```

---

## 2. 类型转换函数

### int(value)

将值转换为整数。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 数值、字符串或布尔值 |
| **返回值** | `int` - 整数值 |

```xlang
println(int(3.14))      // 3
println(int("123"))     // 123
println(int(true))      // 1
println(int(false))     // 0
```

---

### float(value)

将值转换为浮点数。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 数值或字符串 |
| **返回值** | `float` - 浮点数值 |

```xlang
println(float(123))       // 123.0
println(float("3.14"))    // 3.14
```

---

### string(value) / tostring(value)

将值转换为字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型 |
| **返回值** | `string` - 字符串表示 |

```xlang
println(string(123))      // "123"
println(string(3.14))     // "3.14"
println(string(true))     // "true"
println(string([1,2,3]))  // "[1, 2, 3]"
```

---

### bool(value)

将值转换为布尔值。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 任意类型 |
| **返回值** | `bool` - 布尔值 |

**转换规则：**
- `0`、`0.0`、`""`、`null`、空容器 → `false`
- 其他非零值 → `true`

```xlang
println(bool(0))       // false
println(bool(1))       // true
println(bool(""))      // false
println(bool("abc"))   // true
```

---

### chr(code)

将 Unicode 码点转换为字符。

| 项目 | 说明 |
|------|------|
| **参数** | `code` - 整数，Unicode 码点 |
| **返回值** | `string` - 单个字符 |

**特性：** 支持完整 UTF-8 编码（包括中文、emoji 等）

```xlang
println(chr(65))       // A
println(chr(20013))    // 中
println(chr(128512))   // 😀
```

---

### ord(char)

获取字符的 Unicode 码点。

| 项目 | 说明 |
|------|------|
| **参数** | `char` - 字符串（取第一个字符） |
| **返回值** | `int` - Unicode 码点 |

**特性：** 支持完整 UTF-8 解码

```xlang
println(ord("A"))      // 65
println(ord("中"))     // 20013
println(ord("😀"))     // 128512
```

---

## 3. 字符串函数

### trim(str)

去除字符串两端空白字符。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 字符串 |
| **返回值** | `string` - 去除空白后的字符串 |

```xlang
println(trim("  hello  "))  // "hello"
```

---

### ltrim(str)

去除字符串左端空白字符。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 字符串 |
| **返回值** | `string` - 去除左侧空白后的字符串 |

```xlang
println(ltrim("  hello  "))  // "hello  "
```

---

### rtrim(str)

去除字符串右端空白字符。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 字符串 |
| **返回值** | `string` - 去除右侧空白后的字符串 |

```xlang
println(rtrim("  hello  "))  // "  hello"
```

---

### upper(str)

转换为大写字母。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 字符串 |
| **返回值** | `string` - 大写字符串 |

```xlang
println(upper("hello"))  // "HELLO"
```

---

### lower(str)

转换为小写字母。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 字符串 |
| **返回值** | `string` - 小写字符串 |

```xlang
println(lower("HELLO"))  // "hello"
```

---

### replace(str, old, new)

替换字符串中的子串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`old` - 要替换的子串<br>`new` - 替换后的子串 |
| **返回值** | `string` - 替换后的字符串 |

```xlang
println(replace("hello world", "world", "xlang"))
// "hello xlang"
```

---

### split(str, sep)

按分隔符拆分字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`sep` - 分隔符 |
| **返回值** | `array` - 字符串数组 |

```xlang
var arr = split("a,b,c,d", ",")
println(arr)      // ["a", "b", "c", "d"]
println(len(arr)) // 4
```

---

### join(arr, sep)

用分隔符连接数组元素。

| 项目 | 说明 |
|------|------|
| **参数** | `arr` - 字符串数组<br>`sep` - 分隔符 |
| **返回值** | `string` - 连接后的字符串 |

```xlang
var arr = ["a", "b", "c", "d"]
println(join(arr, "-"))  // "a-b-c-d"
```

---

### find(str, sub)

查找子串位置。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`sub` - 要查找的子串 |
| **返回值** | `int` - 子串位置（从 0 开始），未找到返回 -1 |

```xlang
println(find("hello world", "world"))  // 6
println(find("hello world", "xyz"))    // -1
```

---

### contains(str, sub)

检查是否包含子串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`sub` - 要查找的子串 |
| **返回值** | `bool` - 是否包含 |

```xlang
println(contains("hello world", "world"))  // true
println(contains("hello world", "xyz"))    // false
```

---

### substr(str, start, length?)

截取子串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`start` - 起始位置（从 0 开始）<br>`length` - 截取长度（可选，默认到末尾） |
| **返回值** | `string` - 子串 |

```xlang
println(substr("hello world", 0, 5))  // "hello"
println(substr("hello world", 6))     // "world"
```

---

### startswith(str, prefix)

检查是否以指定前缀开头。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`prefix` - 前缀 |
| **返回值** | `bool` - 是否匹配 |

```xlang
println(startswith("hello", "hel"))  // true
println(startswith("hello", "abc"))  // false
```

---

### endswith(str, suffix)

检查是否以指定后缀结尾。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`suffix` - 后缀 |
| **返回值** | `bool` - 是否匹配 |

```xlang
println(endswith("hello", "llo"))  // true
println(endswith("hello", "abc"))  // false
```

---

### repeat(str, times)

重复字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串<br>`times` - 重复次数 |
| **返回值** | `string` - 重复后的字符串 |

```xlang
println(repeat("ab", 3))  // "ababab"
println(repeat("-", 10))  // "----------"
```

---

### reverse(str)

反转字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原字符串 |
| **返回值** | `string` - 反转后的字符串 |

```xlang
println(reverse("hello"))  // "olleh"
println(reverse("12345"))  // "54321"
```

---

## 4. 数学函数

### abs(value)

计算绝对值。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 数值 |
| **返回值** | `int` 或 `float` - 绝对值 |

```xlang
println(abs(-5))     // 5
println(abs(-3.14))  // 3.14
```

---

### min(a, b)

返回较小值。

| 项目 | 说明 |
|------|------|
| **参数** | `a`, `b` - 数值 |
| **返回值** | `int` 或 `float` - 较小的值 |

```xlang
println(min(3, 7))      // 3
println(min(3.5, 2.1))  // 2.1
```

---

### max(a, b)

返回较大值。

| 项目 | 说明 |
|------|------|
| **参数** | `a`, `b` - 数值 |
| **返回值** | `int` 或 `float` - 较大的值 |

```xlang
println(max(3, 7))      // 7
println(max(3.5, 2.1))  // 3.5
```

---

### round(value)

四舍五入取整。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 浮点数 |
| **返回值** | `int` - 四舍五入后的整数 |

```xlang
println(round(3.4))   // 3
println(round(3.5))   // 4
println(round(-2.5))  // -3
```

---

### floor(value)

向下取整。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 浮点数 |
| **返回值** | `int` - 向下取整后的整数 |

```xlang
println(floor(3.7))   // 3
println(floor(-2.3))  // -3
```

---

### ceil(value)

向上取整。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 浮点数 |
| **返回值** | `int` - 向上取整后的整数 |

```xlang
println(ceil(3.2))   // 4
println(ceil(-2.7))  // -2
```

---

### pow(base, exp)

计算幂。

| 项目 | 说明 |
|------|------|
| **参数** | `base` - 底数<br>`exp` - 指数 |
| **返回值** | `float` - 幂运算结果 |

```xlang
println(pow(2, 10))   // 1024
println(pow(3, 0.5))  // 1.732...
```

---

### sqrt(value)

计算平方根。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 非负数值 |
| **返回值** | `float` - 平方根 |

```xlang
println(sqrt(16))  // 4
println(sqrt(2))   // 1.414...
```

---

### log(value)

计算自然对数。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 正数 |
| **返回值** | `float` - 自然对数值 |

```xlang
println(log(2.718))  // 0.999... (约等于 1)
println(log(10))     // 2.302...
```

---

### sin(value)

计算正弦值。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 弧度值 |
| **返回值** | `float` - 正弦值 |

```xlang
println(sin(0))         // 0
println(sin(3.14159/2)) // 1 (约等于)
```

---

### cos(value)

计算余弦值。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 弧度值 |
| **返回值** | `float` - 余弦值 |

```xlang
println(cos(0))         // 1
println(cos(3.14159))   // -1 (约等于)
```

---

### tan(value)

计算正切值。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 弧度值 |
| **返回值** | `float` - 正切值 |

```xlang
println(tan(0))          // 0
println(tan(3.14159/4))  // 1 (约等于)
```

---

### rand()

生成随机整数。

| 项目 | 说明 |
|------|------|
| **参数** | 无 |
| **返回值** | `int` - 随机整数 |

```xlang
println(rand())  // 随机整数
```

---

### random()

生成 0-1 之间的随机浮点数。

| 项目 | 说明 |
|------|------|
| **参数** | 无 |
| **返回值** | `float` - 0.0 到 1.0 之间的随机数 |

```xlang
println(random())  // 例如: 0.742381
```

---

### randrange(min, max)

生成指定范围内的随机整数。

| 项目 | 说明 |
|------|------|
| **参数** | `min` - 最小值（包含）<br>`max` - 最大值（包含） |
| **返回值** | `int` - 范围内的随机整数 |

```xlang
println(randrange(1, 10))  // 1-10 之间的随机整数
println(randrange(0, 100)) // 0-100 之间的随机整数
```

---

## 5. 时间函数

### now()

获取当前时间。

| 项目 | 说明 |
|------|------|
| **参数** | 无 |
| **返回值** | `time` - 当前时间对象 |

```xlang
var t = now()
println(t)
```

---

### year(time)

获取年份。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 年份 |

```xlang
var t = now()
println(year(t))  // 例如: 2026
```

---

### month(time)

获取月份。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 月份 (1-12) |

```xlang
var t = now()
println(month(t))  // 例如: 1
```

---

### day(time)

获取日期。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 日期 (1-31) |

```xlang
var t = now()
println(day(t))  // 例如: 14
```

---

### hour(time)

获取小时。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 小时 (0-23) |

```xlang
var t = now()
println(hour(t))  // 例如: 22
```

---

### minute(time)

获取分钟。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 分钟 (0-59) |

```xlang
var t = now()
println(minute(t))  // 例如: 30
```

---

### second(time)

获取秒数。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 秒数 (0-59) |

```xlang
var t = now()
println(second(t))  // 例如: 45
```

---

### weekday(time)

获取星期几。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - 星期 (0=周日, 1=周一, ..., 6=周六) |

```xlang
var t = now()
println(weekday(t))  // 例如: 4 (周四)
```

---

### strftime(time, format)

格式化时间为字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象<br>`format` - 格式字符串 |
| **返回值** | `string` - 格式化后的时间字符串 |

**格式说明符：**

| 格式 | 说明 | 示例 |
|------|------|------|
| `yyyy` | 四位年份 | 2026 |
| `yy` | 两位年份 | 26 |
| `mm` | 两位月份 | 01 |
| `dd` | 两位日期 | 14 |
| `hh` | 两位小时 | 22 |
| `nn` | 两位分钟 | 30 |
| `ss` | 两位秒数 | 45 |

```xlang
var t = now()
println(strftime(t, "yyyy-mm-dd hh:nn:ss"))
// 输出: 2026-01-14 22:30:45
```

---

### timestamp(time)

获取 Unix 时间戳。

| 项目 | 说明 |
|------|------|
| **参数** | `time` - 时间对象 |
| **返回值** | `int` - Unix 时间戳（秒） |

```xlang
var t = now()
println(timestamp(t))  // 例如: 1768430627
```

---

### fromtimestamp(ts)

从 Unix 时间戳创建时间对象。

| 项目 | 说明 |
|------|------|
| **参数** | `ts` - Unix 时间戳（秒） |
| **返回值** | `time` - 时间对象 |

```xlang
var t = fromtimestamp(1768430627)
println(year(t))  // 2026
```

---

### sleep(ms)

暂停执行指定毫秒数。

| 项目 | 说明 |
|------|------|
| **参数** | `ms` - 毫秒数 |
| **返回值** | `null` |

```xlang
println("开始")
sleep(1000)  // 暂停 1 秒
println("结束")
```

---

### timer()

获取程序运行时间（毫秒）。

| 项目 | 说明 |
|------|------|
| **参数** | 无 |
| **返回值** | `float` - 运行时间（毫秒） |

```xlang
var start = timer()
// ... 执行代码 ...
var end = timer()
println("耗时: " + (end - start) + " ms")
```

---

## 6. 文件函数

### readfile(path)

读取文件内容。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 文件路径 |
| **返回值** | `string` - 文件内容，失败返回空字符串 |

```xlang
var content = readfile("config.txt")
println(content)
```

---

### writefile(path, content)

写入文件内容（覆盖）。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 文件路径<br>`content` - 要写入的内容 |
| **返回值** | `bool` - 是否成功 |

```xlang
var ok = writefile("output.txt", "Hello World")
println(ok)  // true
```

---

### appendfile(path, content)

追加文件内容。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 文件路径<br>`content` - 要追加的内容 |
| **返回值** | `bool` - 是否成功 |

```xlang
appendfile("log.txt", "新日志行\n")
```

---

### exists(path)

检查文件或目录是否存在。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 路径 |
| **返回值** | `bool` - 是否存在 |

```xlang
println(exists("config.txt"))  // true 或 false
println(exists("./data"))      // 检查目录
```

---

### isfile(path)

检查是否为文件。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 路径 |
| **返回值** | `bool` - 是否为文件 |

```xlang
println(isfile("config.txt"))  // true
println(isfile("./data"))      // false (目录)
```

---

### isdir(path)

检查是否为目录。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 路径 |
| **返回值** | `bool` - 是否为目录 |

```xlang
println(isdir("./data"))       // true
println(isdir("config.txt"))   // false
```

---

### filesize(path)

获取文件大小。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 文件路径 |
| **返回值** | `int` - 文件大小（字节） |

```xlang
println(filesize("data.bin"))  // 例如: 1024
```

---

### mkdir(path)

创建目录。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 目录路径 |
| **返回值** | `bool` - 是否成功 |

```xlang
var ok = mkdir("./newdir")
println(ok)  // true
```

---

### remove(path)

删除文件或目录。

| 项目 | 说明 |
|------|------|
| **参数** | `path` - 路径 |
| **返回值** | `bool` - 是否成功 |

```xlang
remove("temp.txt")
remove("./tempdir")
```

---

## 7. JSON函数

### json_parse(str)

解析 JSON 字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - JSON 字符串 |
| **返回值** | `table` / `array` - 解析后的对象 |

```xlang
var obj = json_parse('{"name": "xlang", "version": 1.0}')
println(obj["name"])     // xlang
println(obj["version"])  // 1.0
```

---

### json_stringify(value)

将值序列化为 JSON 字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `value` - 对象或数组 |
| **返回值** | `string` - JSON 字符串 |

```xlang
var obj = {"name": "xlang", "version": 1.0}
var json = json_stringify(obj)
println(json)
// {"name": "xlang", "version": 1.0}
```

---

## 8. 编码函数

### base64_encode(str)

将字符串编码为 Base64。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原始字符串 |
| **返回值** | `string` - Base64 编码字符串 |

```xlang
println(base64_encode("Hello World"))
// SGVsbG8gV29ybGQ=
```

---

### base64_decode(str)

解码 Base64 字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - Base64 字符串 |
| **返回值** | `string` - 解码后的字符串 |

```xlang
println(base64_decode("SGVsbG8gV29ybGQ="))
// Hello World
```

---

### hex_encode(str)

将字符串编码为十六进制。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 原始字符串 |
| **返回值** | `string` - 十六进制字符串 |

```xlang
println(hex_encode("ABC"))  // 414243
```

---

### hex_decode(str)

解码十六进制字符串。

| 项目 | 说明 |
|------|------|
| **参数** | `str` - 十六进制字符串 |
| **返回值** | `string` - 解码后的字符串 |

```xlang
println(hex_decode("414243"))  // ABC
```

---

## 9. 容器函数

### list(...)

创建列表（AVL 树实现的有序列表）。

| 项目 | 说明 |
|------|------|
| **参数** | `...` - 初始元素（键值对） |
| **返回值** | `list` - 列表对象 |

```xlang
var lst = list()
// 或
var lst = [1: "a", 2: "b", 3: "c"]
```

---

### set(...)

创建集合（无重复元素）。

| 项目 | 说明 |
|------|------|
| **参数** | `...` - 初始元素 |
| **返回值** | `set` - 集合对象 |

```xlang
var s = set(1, 2, 3)
// 或使用花括号
var s = {1, 2, 3}
```

---

## 附录：函数速查表

### 基础函数
| 函数 | 说明 |
|------|------|
| `print(v)` | 输出（不换行） |
| `println(v)` | 输出（换行） |
| `input(prompt?)` | 读取输入 |
| `len(v)` | 获取长度 |
| `type(v)` | 获取类型名 |
| `typeof(v)` | 获取类型值 |
| `range(s,e,step?)` | 创建范围 |
| `hash(v)` | 计算哈希 |
| `id(v)` | 获取ID |

### 类型转换
| 函数 | 说明 |
|------|------|
| `int(v)` | 转整数 |
| `float(v)` | 转浮点数 |
| `string(v)` | 转字符串 |
| `bool(v)` | 转布尔值 |
| `chr(n)` | 码点转字符 |
| `ord(c)` | 字符转码点 |

### 字符串
| 函数 | 说明 |
|------|------|
| `trim(s)` | 去两端空白 |
| `ltrim(s)` | 去左空白 |
| `rtrim(s)` | 去右空白 |
| `upper(s)` | 转大写 |
| `lower(s)` | 转小写 |
| `replace(s,o,n)` | 替换 |
| `split(s,sep)` | 拆分 |
| `join(arr,sep)` | 连接 |
| `find(s,sub)` | 查找位置 |
| `contains(s,sub)` | 是否包含 |
| `substr(s,i,n?)` | 截取子串 |
| `startswith(s,p)` | 前缀检查 |
| `endswith(s,p)` | 后缀检查 |
| `repeat(s,n)` | 重复 |
| `reverse(s)` | 反转 |

### 数学
| 函数 | 说明 |
|------|------|
| `abs(v)` | 绝对值 |
| `min(a,b)` | 最小值 |
| `max(a,b)` | 最大值 |
| `round(v)` | 四舍五入 |
| `floor(v)` | 向下取整 |
| `ceil(v)` | 向上取整 |
| `pow(b,e)` | 幂运算 |
| `sqrt(v)` | 平方根 |
| `log(v)` | 自然对数 |
| `sin(v)` | 正弦 |
| `cos(v)` | 余弦 |
| `tan(v)` | 正切 |
| `rand()` | 随机整数 |
| `random()` | 随机0-1 |
| `randrange(a,b)` | 随机范围 |

### 时间
| 函数 | 说明 |
|------|------|
| `now()` | 当前时间 |
| `year(t)` | 年 |
| `month(t)` | 月 |
| `day(t)` | 日 |
| `hour(t)` | 时 |
| `minute(t)` | 分 |
| `second(t)` | 秒 |
| `weekday(t)` | 星期 |
| `strftime(t,f)` | 格式化 |
| `timestamp(t)` | 时间戳 |
| `fromtimestamp(n)` | 从时间戳 |
| `sleep(ms)` | 暂停 |
| `timer()` | 运行时间 |

### 文件
| 函数 | 说明 |
|------|------|
| `readfile(p)` | 读文件 |
| `writefile(p,c)` | 写文件 |
| `appendfile(p,c)` | 追加文件 |
| `exists(p)` | 是否存在 |
| `isfile(p)` | 是否文件 |
| `isdir(p)` | 是否目录 |
| `filesize(p)` | 文件大小 |
| `mkdir(p)` | 创建目录 |
| `remove(p)` | 删除 |

### JSON
| 函数 | 说明 |
|------|------|
| `json_parse(s)` | 解析JSON |
| `json_stringify(v)` | 生成JSON |

### 编码
| 函数 | 说明 |
|------|------|
| `base64_encode(s)` | Base64编码 |
| `base64_decode(s)` | Base64解码 |
| `hex_encode(s)` | 十六进制编码 |
| `hex_decode(s)` | 十六进制解码 |

---

*文档版本: 1.0*
*生成日期: 2026-01-14*
