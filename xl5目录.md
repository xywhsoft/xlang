# x-lang demo5 语言参考文档

---

## 一、语法写法总览

### 1. 程序结构

```
# 程序由一系列语句组成，以换行或分号分隔
# 无需 main 函数，顶层语句直接执行
语句1
语句2
```

### 2. 注释

```python
# 单行注释
// 单行注释（C 风格）

/* 
   多行注释 
   块注释 
*/
```

### 3. 字面量（Literals）

| 类型 | 写法 | 示例 |
|------|------|------|
| 十进制整数 | `123`, `1_000` | `42`, `1_000_000` |
| 十六进制整数 | `0xFF`, `&hFF` | `0x1A`, `&h0F` |
| 八进制整数 | `&o77` | `&o377` |
| 二进制整数 | `0b1010`, `&b1010` | `0b11110000` |
| 浮点数 | `3.14`, `1e10` | `2.5`, `1.5e-3` |
| 字符串（双引号，支持转义） | `"hello\n"` | `"world"` |
| 原始字符串（单引号，不转义） | `'raw string'` | `'C:\path'` |
| 模板字符串（反引号，支持转义） | `` `template` `` | `` `hello {name}` `` |
| 时间字面量 | `#2024-01-01#` | `#2024-12-25T10:30:00#` |
| 布尔值 | `true`, `false` | |
| 空值 | `null` | |

### 4. 变量声明

```python
# 动态类型
var x = 10
var s = "hello"

# 带类型声明
var int x = 10
var string s = "hello"
var array<int> nums = [1, 2, 3]

# 简写类型声明（类型在前，名字在后，可省略 var）
int x = 10
string s = "hello"
float pi = 3.14
bool flag = true
array<int> nums = [1, 2, 3]

# auto 类型（需要初始化表达式）
auto x = 10  # 自动推导为 int

# 解构声明（仅支持 auto）
auto a, b, c = some_func()
```

### 5. 常量声明

```python
const VERSION = 5
const int MAX = 100
const float PI = 3.14159
const string NAME = "xlang"
```

### 6. 赋值与复合赋值

```python
x = 10
x += 5
x -= 3
x *= 2
x /= 4
x %= 3
x **= 2       # 幂赋值
x ?= default  # null 合并赋值（仅在 x 为 null 时赋值）
```

### 7. 函数声明

```python
# 普通函数
func add(int a, int b) as int
    return a + b
end

# 无返回值
func greet(string name) as void
    print("Hello, " + name)
end

# 多返回值
func divmod(int a, int b) as int, int
    return a / b, a % b
end

# 带默认参数
func greet(string name, string prefix = "Hello")
    ...
end

# 可变参数
func sum(...args)          # ... 展开为数组参数
func kwargs(**kwargs)      # ** 展开为字典参数

# 异步函数
async func fetch(string url) as string
    ...
end

# 泛型函数
func foo<T>(T a, T b) as T
    ...
end

# 箭头表达式（单行函数体）
func double(int x) as int => x * 2

# 函数表达式（匿名函数 / lambda）
var adder = func(int a, int b) as int => a + b
var adder2 = func(int a, int b) as int
    return a + b
end

# 运算符优先级（从低到高）：
# ??（null 合并） > 三元 ? : > or > and > | > ^ > & > == != ~= > < <= >= > in > << >> > + - > * / % > ** > 一元 > 后缀
```

### 8. 类声明

```python
class Animal
    var string name
    var int age
    
    create(string name, int age)
        self.name = name
        self.age = age
    end
    
    func speak() as string
        return "..."
    end
    
    get name_str() as string
        return self.name
    end
    
    let name_str(string val)
        self.name = val
    end
    
    destroy()
        # 清理资源
    end
    
    nocopy  # 禁止拷贝
end

# 继承
class Dog extends Animal
    ...
end

# 自定义类型（custom class）
custom MyBuffer
    ...
end

# 联合类型（union）
class Config
    union Value
        var int int_val
        var string str_val
    end
end
```

### 9. 类成员

```python
class MyClass
    # 字段声明
    var field1 as string
    var field2 as int = 0
    var field3 as array<string>
    var field4 as function<string()>
    
    # 构造器
    create(string f1)
        self.field1 = f1
    end
    
    # 析构器
    destroy()
        ...
    end
    
    # 方法
    func method() as void
        ...
    end
    
    # 异步方法
    async func async_method() as string
        ...
    end
    
    # 属性 getter
    get prop() as string
        return self.field1
    end
    
    # 属性 setter
    let prop(string value)
        self.field1 = value
    end
    
    # 外部方法（绑定到 C 函数）
    extern func native_method() as void
    
    # 禁止拷贝
    nocopy
    
    # 联合体
    union Data
        var int ival
        var string sval
    end
end
```

### 10. 控制流

#### if / elseif / else

```python
# 多行 if
if x > 0
    print("positive")
elseif x == 0
    print("zero")
else
    print("negative")
end

# 单行 if（then 关键字）
if x > 0 then print("positive")

# 三元条件表达式
var result = if condition then true_val else false_val
```

#### while / do-while

```python
# while 循环
while i < 10 do
    i += 1
end

# do-while 循环
do
    i += 1
while i < 10 end
```

#### for-in 循环

```python
for item in collection
    print(item)
end

# 多变量绑定
for key, value in dict_items
    ...
end
```

#### break / continue

```python
break       # 跳出当前循环
break 2     # 跳出 2 层循环
continue    # 跳过当前迭代
continue 2  # 跳过 2 层当前迭代
```

#### try / catch / finally

```python
try
    ...
catch e
    print(e.message)
finally
    # 清理代码
end

try
    ...
catch e
    ...
finally
    ...
end

try
    ...
finally
    ...
end
```

#### throw

```python
throw Error("something went wrong")
```

#### assert

```python
assert x > 0
assert x > 0, "x must be positive"
```

#### yield

```python
yield 1
yield 2
yield 3
```

#### return

```python
return
return value
return a, b, c    # 多返回值
```

### 11. 导入与导出

```python
# 导入模块
import std.io
import std.math
import std.net.tcp as tcp

# 导入自定义模块
import mymodule
import "path/to/module"

# 导出
export my_function
export MY_CONST
export func my_exported_func() as void
    ...
end
```

### 12. 外部声明（extern）

```python
# 外部函数
extern func printf(string fmt) as void
extern "real_name" func my_func() as void

# 外部方法
extern func TypeName.method_name() as void

# 外部变量
extern var int errno

# 外部类型别名
extern type MyInt = int
```

### 13. 类型别名（typedef）

```python
typedef Age = int
typedef StringList = array<string>
typedef Callback = function<void(string)>
```

### 14. 表达式

```python
# 算术运算
+  -  *  /  %  **     # 加 减 乘 除 取模 幂

# 比较运算
==  !=  <  <=  >  >=   # 等于 不等于 小于 小于等于 大于 大于等于
~=                     # 近似等于

# 逻辑运算
and  or  not           # 与 或 非

# 位运算
&  |  ^  <<  >>        # 与 或 异或 左移 右移

# 其他运算
!                      # 逻辑非
??                     # null 合并
..                     # 范围
...                    # 展开运算符

# 成员访问
obj.field              # 成员访问
obj?.field             # 安全成员访问
obj[index]             # 下标访问
obj?[index]            # 安全下标访问

# 函数调用
func_name(arg1, arg2)
func_name(named_arg = value)   # 命名参数
func_name(...spread_array)     # 展开参数
func_name(**spread_dict)       # 展开字典参数

# new / delete
var obj = new MyClass(args)
delete obj

# await
var result = await async_func()

# 类型函数调用
List<int>()           # 泛型构造
function<int(int)>(callback)
```

### 15. 容器字面量

```python
# 数组
[]                    # 空数组
[1, 2, 3]             # 带值数组

# 列表
list{}
list{1, 2, 3}

# 集合
{}                    # 空字典（当空花括号时默认为 dict）
{1, 2, 3}             # 空花括号+值 = 集合
set{}                 # 空集合
set{1, 2, 3}         # 带值集合

# 字典
{key: value}          # 花括号 + 冒号 = 字典
dict{}
dict{key: value, key2: value2}

# 带索引的列表
[index: value]        # 等价于列表字面量
```

---

## 二、数据类型

### 1. 内置基础类型

| 类型 | 说明 | C 映射 | 默认值 |
|------|------|--------|--------|
| `void` | 无返回值 | `void` | - |
| `bool` | 布尔 | `int` | `false` |
| `int` | 整数（64位） | `int64_t` | `0` |
| `i8` | 8位有符号整数 | `int8_t` | `0` |
| `u8` | 8位无符号整数 | `uint8_t` | `0` |
| `i16` | 16位有符号整数 | `int16_t` | `0` |
| `u16` | 16位无符号整数 | `uint16_t` | `0` |
| `i32` | 32位有符号整数 | `int32_t` | `0` |
| `u32` | 32位无符号整数 | `uint32_t` | `0` |
| `i64` | 64位有符号整数 | `int64_t` | `0` |
| `u64` | 64位无符号整数 | `uint64_t` | `0` |
| `float` | 浮点数（64位双精度） | `double` | `0.0` |
| `f32` | 32位浮点数 | `float` | `0.0` |
| `f64` | 64位浮点数 | `double` | `0.0` |
| `string` | 字符串 | `const char*` | `""` |
| `time` | 时间戳 | `xtime` | - |
| `point` | 原始指针 | `void*` | `null` |
| `var` | 动态类型（any/variant） | `xvalue` | `null` |
| `auto` | 自动类型推导 | - | （需要初始化） |
| `type` | 类型反射 | - | - |
| `null` | 空值类型 | - | `null` |

### 2. 容器类型

| 类型 | 说明 | C 映射 | 特性 |
|------|------|--------|------|
| `array<T>` | 动态数组 | `xvalue` | INDEXABLE, ITERABLE |
| `list` | 链表 | `xvalue` | INDEXABLE, ITERABLE |
| `set` | 集合（AVL树，唯一值） | `xvalue` | ITERABLE |
| `dict` | 字典（键值对） | `xvalue` | INDEXABLE, ITERABLE |
| `range` | 范围 | `xvalue` | ITERABLE |

### 3. 函数类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `function` | 函数 | `function<int(string, int)>` |
| `function<R(P1,P2,...)>` | 带类型的函数 | `function<void(int)>` |

### 4. 并发类型

| 类型 | 说明 | 特性 |
|------|------|------|
| `future` | 异步 Future | AWAITABLE |
| `task` | 异步任务 | AWAITABLE |
| `channel` | 通道（协程间通信） | - |
| `generator` | 生成器 | ITERABLE, ITERATOR, GENERATOR |

### 5. 对象/类类型

| 类型 | 说明 |
|------|------|
| `object` | 所有用户定义类的基类 |
| `error` | 错误类型 |
| `module` | 模块类型 |

### 6. 错误类型字段

`error` 类型具有以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `message` | `string` | 错误消息（必需） |
| `code` | `int` | 错误代码 |
| `data` | `string` | 附加数据 |
| `stack` | `string` | 堆栈跟踪 |
| `cause` | `error` | 原始错误 |

### 7. 协议（Protocol）

类型可以实现以下协议：

| 协议 | 说明 | 实现类型 |
|------|------|----------|
| `CALLABLE` | 可调用 | `function` |
| `INDEXABLE` | 可索引 | `array`, `list`, `dict` |
| `ITERABLE` | 可迭代 | `array`, `list`, `dict`, `set`, `range`, `generator` |
| `ITERATOR` | 迭代器 | `generator` |
| `AWAITABLE` | 可等待 | `future`, `task` |
| `GENERATOR` | 生成器 | `generator` |

---

## 三、标准库

所有标准库模块通过 `import` 语句导入，例如 `import std.io`。

### 1. std.core — 核心函数

| 函数签名 | 说明 |
|----------|------|
| `func identity(int x) as int` | 返回输入值 |
| `func print(int x) as void` | 打印整数 |
| `func print(float x) as void` | 打印浮点数 |
| `func print(string x) as void` | 打印字符串 |
| `func print(bool x) as void` | 打印布尔值 |
| `func println() as void` | 打印空行 |
| `func println(int x) as void` | 打印整数+换行 |
| `func println(float x) as void` | 打印浮点数+换行 |
| `func println(string x) as void` | 打印字符串+换行 |
| `func println(bool x) as void` | 打印布尔值+换行 |
| `func len(string x) as int` | 字符串长度 |
| `func len(array x) as int` | 数组长度 |
| `func len(list x) as int` | 列表长度 |
| `func len(set x) as int` | 集合大小 |
| `func len(dict x) as int` | 字典大小 |

**常量：**
| 名称 | 类型 | 值 |
|------|------|----|
| `version` | `int` | `5` |

---

### 2. std.io — 输入输出

| 函数签名 | 说明 |
|----------|------|
| `func print(int/float/string/bool x) as void` | 打印到 stdout |
| `func println(.../void) as void` | 打印+换行 |
| `func eprint(int/float/string/bool x) as void` | 打印到 stderr |
| `func eprintln(.../void) as void` | stderr+换行 |
| `func read_line() as string` | 从 stdin 读一行 |
| `func flush() as void` | 刷新输出 |

---

### 3. std.str — 字符串操作

| 函数签名 | 说明 |
|----------|------|
| `func empty(string text) as bool` | 是否为空 |
| `func byte_len(string text) as int` | 字节长度 |
| `func contains(string text, string part) as bool` | 是否包含子串 |
| `func index_of(string text, string part) as int` | 查找子串位置 |
| `func last_index_of(string text, string part) as int` | 最后出现位置 |
| `func slice(string text, int start, int count) as string` | 截取子串 |
| `func repeat(string text, int count) as string` | 重复字符串 |
| `func split(string text, string part) as list` | 按分隔符拆分 |
| `func split_lines(string text) as list` | 按行拆分 |
| `func starts_with(string text, string part) as bool` | 是否以指定前缀开头 |
| `func ends_with(string text, string part) as bool` | 是否以指定后缀结尾 |
| `func trim(string text) as string` | 去除两端空白 |
| `func trim_left(string text) as string` | 去除左端空白 |
| `func trim_right(string text) as string` | 去除右端空白 |
| `func lower(string text) as string` | 转小写 |
| `func upper(string text) as string` | 转大写 |
| `func replace(string text, string part, string value) as string` | 替换 |
| `func reverse(string text) as string` | 反转字符串 |
| `func pad_left(string text, int width, string fill) as string` | 左填充 |
| `func pad_right(string text, int width, string fill) as string` | 右填充 |

---

### 4. std.math — 数学函数

| 函数签名 | 说明 |
|----------|------|
| `func abs(int value) as int` | 绝对值（整数） |
| `func abs(float value) as float` | 绝对值（浮点） |
| `func min(int/float left, int/float right) as int/float` | 最小值 |
| `func max(int/float left, int/float right) as int/float` | 最大值 |
| `func clamp(int/float value, int/float low, int/float high) as int/float` | 夹值 |
| `func floor(float value) as float` | 向下取整 |
| `func ceil(float value) as float` | 向上取整 |
| `func round(float value) as float` | 四舍五入 |
| `func trunc(float value) as float` | 截断取整 |
| `func sqrt(int/float value) as int/float` | 平方根 |
| `func pow(int/float value, int/float exp) as int/float` | 幂运算 |
| `func sin(float value) as float` | 正弦 |
| `func cos(float value) as float` | 余弦 |
| `func tan(float value) as float` | 正切 |
| `func exp(float value) as float` | 自然指数 |
| `func log(float value) as float` | 自然对数 |
| `func log10(float value) as float` | 常用对数 |

**常量：**
| 名称 | 类型 | 值 |
|------|------|----|
| `pi` | `float` | `3.141592653589793` |
| `e` | `float` | `2.718281828459045` |

---

### 5. std.array — 数组操作

| 函数签名 | 说明 |
|----------|------|
| `func len(array values) as int` | 获取长度 |
| `func append(array values, var item) as void` | 追加元素 |
| `func join(array values, string sep) as string` | 连接为字符串 |
| `func clear(array values) as void` | 清空 |
| `func remove(array values, int index) as bool` | 按索引移除 |

---

### 6. std.list — 链表操作

| 函数签名 | 说明 |
|----------|------|
| `func len(list values) as int` | 获取长度 |
| `func append(list values, var item) as void` | 追加元素 |
| `func join(list values, string sep) as string` | 连接为字符串 |
| `func clear(list values) as void` | 清空 |
| `func remove(list values, int index) as bool` | 按索引移除 |

---

### 7. std.dict — 字典操作

| 函数签名 | 说明 |
|----------|------|
| `func len(dict values) as int` | 获取大小 |
| `func has(dict values, string key) as bool` | 是否包含键 |
| `func clear(dict values) as void` | 清空 |
| `func remove(dict values, string key) as bool` | 移除键 |

---

### 8. std.set — 集合操作

| 函数签名 | 说明 |
|----------|------|
| `func len(set values) as int` | 获取大小 |
| `func has(set values, var item) as bool` | 是否包含元素 |
| `func clear(set values) as void` | 清空 |
| `func remove(set values, var item) as bool` | 移除元素 |

---

### 9. std.iter — 迭代器工具

| 函数签名 | 说明 |
|----------|------|
| `func count(array/list/set/dict values) as int` | 计数 |
| `func keys(dict values) as array<string>` | 获取所有键 |
| `func values(dict values) as array<var>` | 获取所有值 |
| `func items(set values) as array<var>` | 获取所有元素 |

---

### 10. std.json — JSON 编解码

| 函数签名 | 说明 |
|----------|------|
| `func encode(var value) as string` | 编码为 JSON 字符串 |
| `func decode(string text) as var` | 解码 JSON 字符串 |

---

### 11. std.regex — 正则表达式

| 函数签名 | 说明 |
|----------|------|
| `func is_match(string pattern, string text) as bool` | 是否匹配 |
| `func find(string pattern, string text) as string` | 查找匹配 |
| `func captures(string pattern, string text) as list<string>` | 获取捕获组 |
| `func capture_count(string pattern) as int` | 捕获组数量 |
| `func replace_first(string pattern, string text, string value) as string` | 替换第一个 |
| `func replace_all(string pattern, string text, string value) as string` | 替换全部 |

---

### 12. std.fs — 文件系统

| 函数签名 | 说明 |
|----------|------|
| `func exists(string path) as bool` | 路径是否存在 |
| `func is_file(string path) as bool` | 是否为文件 |
| `func is_dir(string path) as bool` | 是否为目录 |
| `func size(string path) as int` | 文件大小 |
| `func set_size(string path, int size) as bool` | 截断/扩展文件 |
| `func access_time(string path) as time` | 访问时间 |
| `func change_time(string path) as time` | 修改时间 |
| `func read_all(string path) as string` | 读取全部内容 |
| `func write_all(string path, string text) as bool` | 写入全部内容 |
| `func append_all(string path, string text) as bool` | 追加内容 |
| `func mkdir_all(string path) as bool` | 递归创建目录 |
| `func copy(string src, string dst) as bool` | 复制文件 |
| `func move(string src, string dst) as bool` | 移动文件 |
| `func delete(string path) as bool` | 删除文件 |

---

### 13. std.dir — 目录操作

| 函数签名 | 说明 |
|----------|------|
| `func create(string path) as bool` | 创建目录 |
| `func list(string path) as string` | 列出内容（换行分隔） |
| `func files(string path) as string` | 列出文件（换行分隔） |
| `func dirs(string path) as string` | 列出子目录（换行分隔） |
| `func tree(string path) as string` | 递归列出（换行分隔） |
| `func copy(string src, string dst) as bool` | 复制目录 |
| `func move(string src, string dst) as bool` | 移动目录 |
| `func delete(string path) as bool` | 删除目录 |

---

### 14. std.path — 路径操作

| 函数签名 | 说明 |
|----------|------|
| `func join(string left, string right) as string` | 拼接路径 |
| `func name_ext(string value) as string` | 文件名（含扩展名） |
| `func name(string value) as string` | 文件名（不含扩展名） |
| `func ext(string value) as string` | 扩展名 |
| `func dir(string value) as string` | 父目录 |
| `func exists(string value) as bool` | 路径是否存在 |
| `func is_abs(string value) as bool` | 是否为绝对路径 |
| `func random(string head, string foot, int len) as string` | 生成随机路径名 |

---

### 15. std.env — 环境变量

| 函数签名 | 说明 |
|----------|------|
| `func get(string name) as string` | 获取环境变量 |
| `func exists(string name) as bool` | 环境变量是否存在 |
| `func set(string name, string value) as bool` | 设置环境变量 |
| `func cwd() as string` | 当前工作目录 |
| `func temp() as string` | 临时目录 |
| `func home() as string` | 用户主目录 |

---

### 16. std.os — 操作系统信息

| 函数签名 | 说明 |
|----------|------|
| `func name() as string` | 操作系统名称 |
| `func arch() as string` | 系统架构 |
| `func user() as string` | 当前用户 |
| `func host_name() as string` | 主机名 |
| `func executable() as string` | 可执行文件路径 |

---

### 17. std.host — 主机信息

| 函数签名 | 说明 |
|----------|------|
| `func name() as string` | 主机名 |
| `func ip() as string` | IP 地址 |
| `func user() as string` | 当前用户 |
| `func os() as string` | 操作系统 |
| `func arch() as string` | 系统架构 |
| `func executable() as string` | 可执行文件路径 |

---

### 18. std.time — 时间操作

| 函数签名 | 说明 |
|----------|------|
| `func now() as time` | 当前时间 |
| `func to_string(time value) as string` | 时间转字符串 |
| `func now_text() as string` | 当前时间文本 |
| `func date_text() as string` | 当前日期文本 |
| `func time_text() as string` | 当前时间文本 |
| `func relative(time value, time base) as string` | 相对时间 |
| `func parse(string value) as time` | 解析时间 |
| `func format(time value, string pattern) as string` | 格式化时间 |
| `func parse_format(string value, string pattern) as time` | 按格式解析时间 |
| `func sleep(int ms) as void` | 休眠（毫秒） |
| `func is_leap_year(int year) as bool` | 是否闰年 |

**常量：** `millis_per_second = 1000`, `seconds_per_day = 86400`

---

### 19. std.clock — 时钟

| 函数签名 | 说明 |
|----------|------|
| `func now() as time` | 当前时间 |
| `func text() as string` | 当前时间文本 |
| `func date() as string` | 当前日期文本 |
| `func time_of_day() as string` | 时间部分 |
| `func sleep(int ms) as void` | 休眠 |

**常量：** `millis_per_second = 1000`, `seconds_per_day = 86400`

---

### 20. std.fmt — 格式化

| 函数签名 | 说明 |
|----------|------|
| `func int_text(int value, string pattern) as string` | 整数格式化 |
| `func float_text(float value, string pattern) as string` | 浮点格式化 |
| `func bool_text(bool value) as string` | 布尔格式化 |

---

### 21. std.hash — 哈希函数

| 函数签名 | 说明 |
|----------|------|
| `func hash32(string text) as int` | 32位哈希 |
| `func hash64(string text) as string` | 64位哈希 |
| `func hash64_micro(string text) as string` | 微秒级64位哈希 |
| `func hash64_nano(string text) as string` | 纳秒级64位哈希 |
| `func sha256(string text) as string` | SHA-256 |
| `func sha512(string text) as string` | SHA-512 |

---

### 22. std.crypto — 加密

| 函数签名 | 说明 |
|----------|------|
| `func hmac_sha256(string key, string text) as string` | HMAC-SHA256 |
| `func hmac_sha512(string key, string text) as string` | HMAC-SHA512 |

---

### 23. std.encode — 编解码

| 函数签名 | 说明 |
|----------|------|
| `func hex_encode(string text) as string` | HEX 编码 |
| `func hex_decode(string text) as string` | HEX 解码 |
| `func base64_encode(string text) as string` | Base64 编码 |
| `func base64_decode(string text) as string` | Base64 解码 |

---

### 24. std.log — 日志

| 函数签名 | 说明 |
|----------|------|
| `func debug(string text) as void` | DEBUG 级别日志 |
| `func info(string text) as void` | INFO 级别日志 |
| `func warn(string text) as void` | WARN 级别日志 |
| `func error(string text) as void` | ERROR 级别日志 |
| `func error(error value) as void` | ERROR 级别（错误对象） |

---

### 25. std.assert — 断言

| 函数签名 | 说明 |
|----------|------|
| `func that(bool cond) as void` | 断言条件为真 |
| `func that(bool cond, string message) as void` | 断言条件为真（带消息） |
| `func fail() as void` | 断言失败 |
| `func fail(string message) as void` | 断言失败（带消息） |
| `func equal(int/float/string/bool left, int/float/string/bool right, string message) as void` | 断言相等 |

---

### 26. std.error — 错误处理

| 函数签名 | 说明 |
|----------|------|
| `func make(string message) as error` | 创建错误 |
| `func make(string message, int code) as error` | 创建错误（带代码） |
| `func make(string message, int code, string data) as error` | 创建错误（完整） |
| `func type_error(string message) as error` | TypeError |
| `func value_error(string message) as error` | ValueError |
| `func io_error(string message) as error` | IOError |
| `func timeout_error(string message) as error` | TimeoutError |
| `func cancelled_error(string message) as error` | CancelledError |
| `func not_implemented_error(string message) as error` | NotImplementedError |
| `func native_error(string message) as error` | NativeError |
| `func is_null(error value) as bool` | 是否为空 |
| `func kind(error value) as string` | 错误类型名 |
| `func message(error value) as string` | 错误消息 |
| `func code(error value) as int` | 错误代码 |
| `func data(error value) as string` | 错误数据 |
| `func stack(error value) as string` | 错误堆栈 |
| `func cause(error value) as error` | 原始错误 |
| `func text(error value) as string` | 错误文本 |
| `func is_type(error value, string type_name) as bool` | 是否指定类型 |
| `func is_native(error value) as bool` | 是否原生错误 |

---

### 27. std.atomic — 原子操作

| 函数签名 | 说明 |
|----------|------|
| `func make(int value) as point` | 创建原子变量 |
| `func free(point value) as bool` | 释放 |
| `func load(point value) as int` | 原子读取 |
| `func store(point value, int item) as int` | 原子存储 |
| `func exchange(point value, int item) as int` | 原子交换 |
| `func compare_exchange(point value, int expected, int desired) as int` | CAS |
| `func add(point value, int delta) as int` | 原子加 |

---

### 28. std.sync — 同步原语

**互斥锁（Mutex）：**

| 函数签名 | 说明 |
|----------|------|
| `func mutex() as point` | 创建互斥锁 |
| `func mutex_free(point value) as bool` | 销毁互斥锁 |
| `func mutex_lock(point value) as bool` | 加锁 |
| `func mutex_try_lock(point value) as bool` | 尝试加锁 |
| `func mutex_unlock(point value) as bool` | 解锁 |

**条件变量（Cond）：**

| 函数签名 | 说明 |
|----------|------|
| `func cond() as point` | 创建条件变量 |
| `func cond_free(point value) as bool` | 销毁 |
| `func cond_wait(point value, point mutex_value) as bool` | 等待 |
| `func cond_wait_timeout(point value, point mutex_value, int ms) as bool` | 超时等待 |
| `func cond_signal(point value) as bool` | 唤醒一个 |
| `func cond_broadcast(point value) as bool` | 唤醒全部 |

**读写锁（RWLock）：**

| 函数签名 | 说明 |
|----------|------|
| `func rwlock() as point` | 创建读写锁 |
| `func rwlock_free(point value) as bool` | 销毁 |
| `func rwlock_read_lock(point value) as bool` | 读锁 |
| `func rwlock_try_read_lock(point value) as bool` | 尝试读锁 |
| `func rwlock_read_unlock(point value) as bool` | 读解锁 |
| `func rwlock_write_lock(point value) as bool` | 写锁 |
| `func rwlock_try_write_lock(point value) as bool` | 尝试写锁 |
| `func rwlock_write_unlock(point value) as bool` | 写解锁 |
| `func rwlock_downgrade(point value) as bool` | 写锁降级为读锁 |
| `func rwlock_upgrade(point value) as bool` | 读锁升级为写锁 |

---

### 29. std.thread — 线程

| 函数签名 | 说明 |
|----------|------|
| `func current_id() as int` | 当前线程ID |
| `func sleep(int ms) as void` | 休眠 |
| `func yield_now() as void` | 让出时间片 |

---

### 30. std.future — Future 操作

| 函数签名 | 说明 |
|----------|------|
| `async func ready(int/float/string/bool/error value) as ...` | 立即完成的 Future |
| `async func wait(future<int/float/string/bool/error> value) as ...` | 等待 Future 完成 |

---

### 31. std.task — 异步任务

| 函数签名 | 说明 |
|----------|------|
| `async func spawn(future<...> value) as ...` | 启动异步任务 |
| `async func all(future<...> left, future<...> right) as ...` | 等待全部完成 |
| `async func race(future<...> left, future<...> right) as ...` | 等待任意完成 |
| `func cancel(future<...> value) as int` | 取消任务 |
| `async func delay(int ms) as int` | 延迟 |

**常量：** `ready = 1`, `done = 2`

---

### 32. std.channel — 通道

| 函数签名 | 说明 |
|----------|------|
| `func make() as channel` | 创建无缓冲通道 |
| `func make(int capacity) as channel` | 创建有缓冲通道 |
| `func send(channel value, var/int/float/string/bool/error item) as bool` | 发送 |
| `func recv(channel value) as array<var>` | 接收 |
| `func len(channel value) as int` | 缓冲区中元素数 |
| `func capacity(channel value) as int` | 缓冲区容量 |
| `func closed(channel value) as bool` | 是否已关闭 |
| `func close(channel value) as bool` | 关闭通道 |

---

### 33. std.module — 模块反射

| 函数签名 | 说明 |
|----------|------|
| `func key(module value) as string` | 模块键名 |
| `func abi_id(module value) as string` | ABI 标识 |
| `func init_symbol(module value) as string` | 初始化符号 |
| `func exports_symbol(module value) as string` | 导出符号 |
| `func descriptor_symbol(module value) as string` | 描述符符号 |
| `func export_count(module value) as int` | 导出数量 |
| `func exports(module value) as array<var>` | 所有导出 |
| `func has_export(module value, string name) as bool` | 是否有导出 |
| `func export_symbol(module value, string name) as string` | 导出符号名 |

---

### 34. std.object — 对象反射

| 函数签名 | 说明 |
|----------|------|
| `func field_names(var value) as array<string>` | 字段名列表 |
| `func field_count(var value) as int` | 字段数量 |
| `func field_name_at(var value, int index, string default_value) as string` | 按索引获取字段名 |
| `func has_field(var value, string field_name) as bool` | 是否有字段 |
| `func method_names(var value) as array<string>` | 方法名列表 |
| `func method_count(var value) as int` | 方法数量 |
| `func method_name_at(var value, int index, string default_value) as string` | 按索引获取方法名 |
| `func has_method(var value, string method_name) as bool` | 是否有方法 |
| `func weak(var value) as point` | 创建弱引用 |
| `func alive(point value) as bool` | 弱引用是否存活 |
| `func lock(point value) as var` | 锁定弱引用 |
| `func release(point value) as bool` | 释放引用 |
| `func refcount(var value) as int` | 引用计数 |
| `func unique(var value) as bool` | 是否唯一引用 |
| `func collect() as int` | 循环回收 |

---

### 35. std.types — 类型反射与动态类型

**类定义：**
```
class DynamicTypeDesc extends object
    var name as string
    var base as type

class DynamicObject extends object
    var desc as DynamicTypeDesc
```

| 函数签名 | 说明 |
|----------|------|
| `func of(var value) as type` | 获取值的类型 |
| `func name(type value) as string` | 类型名称 |
| `func same(type left, type right) as bool` | 类型是否相同 |
| `func dynamic(string type_name, type base_type) as DynamicTypeDesc` | 创建动态类型描述 |
| `func is_dynamic(DynamicTypeDesc desc) as bool` | 是否动态类型 |
| `func dynamic_name(DynamicTypeDesc desc) as string` | 动态类型名称 |
| `func dynamic_base(DynamicTypeDesc desc) as type` | 动态类型基类 |
| `func add_field(DynamicTypeDesc desc, string field_name, type field_type) as DynamicTypeDesc` | 添加字段描述 |
| `func add_method(DynamicTypeDesc desc, string method_name) as DynamicTypeDesc` | 添加方法描述 |
| `func bind_method(DynamicTypeDesc desc, string method_name, var method_value) as DynamicTypeDesc` | 绑定方法实现 |
| `func method_impl(DynamicTypeDesc desc, string method_name, var default_value) as var` | 获取方法实现 |
| `func has_bound_method(DynamicTypeDesc desc, string method_name) as bool` | 是否有绑定方法 |
| `func bound_method_names(DynamicTypeDesc desc) as array<string>` | 绑定方法名列表 |
| `func bound_method_count(DynamicTypeDesc desc) as int` | 绑定方法数量 |
| `func bound_method_name_at(DynamicTypeDesc desc, int index, string default_value) as string` | 按索引获取绑定方法名 |
| `func has_declared_field(DynamicTypeDesc desc, string field_name) as bool` | 是否声明字段 |
| `func has_declared_method(DynamicTypeDesc desc, string method_name) as bool` | 是否声明方法 |
| `func field_names(DynamicTypeDesc desc) as array<string>` | 字段名列表 |
| `func method_names(DynamicTypeDesc desc) as array<string>` | 方法名列表 |
| `func field_count(DynamicTypeDesc desc) as int` | 字段数量 |
| `func method_count(DynamicTypeDesc desc) as int` | 方法数量 |
| `func field_name_at(DynamicTypeDesc desc, int index, string default_value) as string` | 按索引获取字段名 |
| `func field_type_name(DynamicTypeDesc desc, string field_name, string default_value) as string` | 字段类型名 |
| `func field_type_name_at(DynamicTypeDesc desc, int index, string default_value) as string` | 按索引获取字段类型名 |
| `func method_name_at(DynamicTypeDesc desc, int index, string default_value) as string` | 按索引获取方法名 |
| `func new_instance(DynamicTypeDesc desc) as DynamicObject` | 创建动态对象实例 |
| `func is_instance(var value) as bool` | 是否动态对象实例 |
| `func instance_name(DynamicObject value) as string` | 实例类型名 |
| `func instance_base(DynamicObject value) as type` | 实例基类型 |
| `func instance_desc(DynamicObject value) as DynamicTypeDesc` | 实例类型描述 |
| `func instance_has_field(DynamicObject value, string field_name) as bool` | 实例是否有字段 |
| `func instance_field_names(DynamicObject value) as array<string>` | 实例字段名 |
| `func instance_field_count(DynamicObject value) as int` | 实例字段数量 |
| `func instance_has_method(DynamicObject value, string method_name) as bool` | 实例是否有方法 |
| `func instance_method_names(DynamicObject value) as array<string>` | 实例方法名 |
| `func instance_method_count(DynamicObject value) as int` | 实例方法数量 |
| `func instance_method_bound(DynamicObject value, string method_name) as bool` | 实例方法是否绑定 |
| `func instance_bound_method_names(DynamicObject value) as array<string>` | 实例绑定方法名 |
| `func instance_bound_method_count(DynamicObject value) as int` | 实例绑定方法数量 |
| `func instance_bound_method_name_at(DynamicObject value, int index, string default_value) as string` | 按索引获取绑定方法名 |
| `func instance_method(DynamicObject value, string method_name, var default_value) as var` | 获取实例方法 |
| `func instance_field_name_at(DynamicObject value, int index, string default_value) as string` | 按索引获取字段名 |
| `func instance_field_type_name(DynamicObject value, string field_name, string default_value) as string` | 实例字段类型名 |
| `func instance_field_type_name_at(DynamicObject value, int index, string default_value) as string` | 按索引获取字段类型名 |
| `func instance_method_name_at(DynamicObject value, int index, string default_value) as string` | 按索引获取方法名 |
| `func set_field(DynamicObject value, string field_name, var field_value) as DynamicObject` | 设置实例字段 |
| `func get_field(DynamicObject value, string field_name, var default_value) as var` | 获取实例字段 |

---

### 36. std.rand — 随机数

| 函数签名 | 说明 |
|----------|------|
| `func int32() as int` | 随机32位整数 |
| `func bool() as bool` | 随机布尔值 |
| `func range(int min, int max) as int` | 范围随机整数 |
| `func text(int len) as string` | 随机文本 |
| `func text_from(string template, int len) as string` | 从模板生成随机文本 |

---

### 37. std.pack — 二进制打包

| 函数签名 | 说明 |
|----------|------|
| `func write_u8(int value) as string` | 写入 u8 |
| `func write_u16_le(int value) as string` | 写入 u16 小端 |
| `func write_u16_be(int value) as string` | 写入 u16 大端 |
| `func write_u32_le(int value) as string` | 写入 u32 小端 |
| `func write_u32_be(int value) as string` | 写入 u32 大端 |
| `func read_u8(string data, int offset) as int` | 读取 u8 |
| `func read_u16_le(string data, int offset) as int` | 读取 u16 小端 |
| `func read_u16_be(string data, int offset) as int` | 读取 u16 大端 |
| `func read_u32_le(string data, int offset) as int` | 读取 u32 小端 |
| `func read_u32_be(string data, int offset) as int` | 读取 u32 大端 |

---

### 38. std.process — 进程

| 函数签名 | 说明 |
|----------|------|
| `func exec(string command) as int` | 执行命令（返回退出码） |
| `func capture(string command) as string` | 执行命令（捕获输出） |
| `func exec_in(string command, string workdir) as int` | 指定工作目录执行 |
| `func capture_in(string command, string workdir) as string` | 指定工作目录捕获 |
| `func terminal_supported() as bool` | 终端是否支持 |

---

### 39. std.range — 范围生成

| 函数签名 | 说明 |
|----------|------|
| `func collect(int stop) as array<int>` | 生成 [0, stop) |
| `func collect(int start, int stop) as array<int>` | 生成 [start, stop) |
| `func collect(int start, int stop, int step) as array<int>` | 生成 [start, stop) 步长 step |

---

### 40. std.query — URL 查询字符串

| 函数签名 | 说明 |
|----------|------|
| `func encode(dict<string> values) as string` | 编码查询字符串 |
| `func decode(string text) as dict<string>` | 解码查询字符串 |

---

### 41. std.template — 模板渲染

| 函数签名 | 说明 |
|----------|------|
| `func render(string template, dict<string> values) as string` | 渲染模板（`{{key}}` 语法） |

---

### 42. std.debug — 调试（仅 DEBUG 模式）

| 函数签名 | 说明 |
|----------|------|
| `func text(error/var value) as string` | 调试文本 |
| `func print(error/var value) as void` | 调试打印 |
| `func println(error/var value) as void` | 调试打印+换行 |

---

### 43. std.trace — 追踪（仅 DEBUG 模式）

| 函数签名 | 说明 |
|----------|------|
| `func write(string text) as void` | 写追踪信息 |
| `func error(error value) as void` | 追踪错误 |

---

### 44. 网络 - std.net

| 函数签名 | 说明 |
|----------|------|
| `func resolve(string host) as string` | DNS 解析 |

### 45. 网络 - std.net.tcp

| 函数签名 | 说明 |
|----------|------|
| `func listen(string host, int port) as point` | 监听 TCP |
| `func accept(point listener, int timeout_ms) as point` | 接受连接 |
| `func connect(string host, int port) as point` | 连接 |
| `func send(point socket, string text) as int` | 发送数据 |
| `func recv(point socket, int size, int timeout_ms) as string` | 接收数据 |
| `func local_port(point socket) as int` | 本地端口 |
| `func close(point socket) as bool` | 关闭连接 |

### 46. 网络 - std.net.udp

| 函数签名 | 说明 |
|----------|------|
| `func bind(string host, int port) as point` | 绑定 UDP |
| `func send_to(point socket, string host, int port, string text) as int` | 发送数据 |
| `func recv_from(point socket, int size, int timeout_ms) as dict<string>` | 接收数据 |
| `func local_port(point socket) as int` | 本地端口 |
| `func close(point socket) as bool` | 关闭 |

### 47. 网络 - std.net.dns

| 函数签名 | 说明 |
|----------|------|
| `func resolve(string host) as string` | DNS 解析 |
| `func resolve_all(string host) as array<string>` | 解析所有地址 |
| `func reverse(string ip) as string` | 反向 DNS |

### 48. 网络 - std.net.ip

| 函数签名 | 说明 |
|----------|------|
| `func parse(string text) as dict<string>` | 解析 IP 地址 |
| `func build(dict<string> value) as string` | 构建 IP 地址 |
| `func normalize(string text) as string` | 规范化 IP |
| `func is_ip(string text) as bool` | 是否为 IP |
| `func is_ipv4(string text) as bool` | 是否为 IPv4 |
| `func is_ipv6(string text) as bool` | 是否为 IPv6 |

### 49. 网络 - std.net.url

| 函数签名 | 说明 |
|----------|------|
| `func encode_query(dict<string> values) as string` | 编码查询参数 |
| `func decode_query(string text) as dict<string>` | 解码查询参数 |
| `func parse(string text) as dict<string>` | 解析 URL |
| `func build(dict<string> parts) as string` | 构建 URL |

### 50. 网络 - std.net.tls

| 函数签名 | 说明 |
|----------|------|
| `func versions() as array<string>` | 支持的 TLS 版本 |
| `func is_tls_url(string url) as bool` | 是否 TLS URL |
| `func default_port(string scheme) as int` | 默认端口 |
| `func sni_host(string url) as string` | SNI 主机名 |
| `func fingerprint_sha256(string data) as string` | SHA256 指纹 |
| `func client_config(string host, bool verify) as dict<string>` | 客户端配置 |

### 51. 网络 - std.net.proxy

| 函数签名 | 说明 |
|----------|------|
| `func parse(string url) as dict<string>` | 解析代理 URL |
| `func basic_auth(string user, string pass) as string` | Basic 认证 |
| `func http_connect(string host, int port, string auth) as string` | HTTP CONNECT |
| `func socks5_greeting_hex(bool auth) as string` | SOCKS5 握手 |
| `func socks5_connect_hex(string host, int port) as string` | SOCKS5 连接 |

### 52. 网络 - std.net.ws

| 函数签名 | 说明 |
|----------|------|
| `func key() as string` | 生成 WebSocket Key |
| `func accept(string key) as string` | 生成 Accept |
| `func request(string host, string path, string key) as string` | 构建请求 |
| `func response(string key) as string` | 构建响应 |
| `func frame_text(string text, bool mask) as string` | 封装文本帧 |
| `func unframe(string frame) as dict<string>` | 解帧 |

### 53. 网络 - std.net.http

| 函数签名 | 说明 |
|----------|------|
| `func headers(dict<string> values) as string` | 构建请求头 |
| `func request(string method, string target, dict<string> headers, string body) as string` | 构建 HTTP 请求 |
| `func response(int status, string reason, dict<string> headers, string body) as string` | 构建 HTTP 响应 |
| `func parse_headers(string text) as dict<string>` | 解析请求头 |

### 54. 网络 - std.net.httpd

| 函数签名 | 说明 |
|----------|------|
| `func request(string text) as dict<string>` | 解析 HTTP 请求 |
| `func match(string pattern, string path) as dict<string>` | 路由匹配 |
| `func response(int status, dict<string> headers, string body) as string` | 构建响应 |
| `func text(int status, string body) as string` | 文本响应 |

### 55. 网络 - std.net.multipart

| 函数签名 | 说明 |
|----------|------|
| `func boundary(string content_type) as string` | 提取 boundary |
| `func parse(string text, string boundary) as array<dict<string>>` | 解析 multipart |
| `func build(array<dict> parts, string boundary) as string` | 构建 multipart |

### 56. 网络 - std.net.mime

| 函数签名 | 说明 |
|----------|------|
| `func by_ext(string ext) as string` | 按扩展名获取 MIME |
| `func by_path(string path) as string` | 按路径获取 MIME |
| `func is_text(string mime) as bool` | 是否文本 MIME |
| `func parse(string text) as dict<string>` | 解析 MIME |
| `func build(dict<string> value) as string` | 构建 MIME |

---

### 57. 系统底层 - std.sys.env

（与 `std.env` 相同，委托调用）

### 58. 系统底层 - std.sys.file

（与 `std.fs` 相同，委托调用）

### 59. 系统底层 - std.sys.lib

| 函数签名 | 说明 |
|----------|------|
| `func loadlib(string path) as var` | 加载动态库 |
| `func dlopen(string path) as var` | 打开动态库 |
| `func dlsym(var self, string name) as point` | 获取符号地址 |
| `func dlclose(var self) as bool` | 关闭动态库 |
| `func proc(var self, string name) as point` | 获取函数指针 |
| `func close(var self) as bool` | 关闭 |
| `func path(var self) as string` | 库路径 |
| `func loaded(var self) as bool` | 是否已加载 |

### 60. 系统底层 - std.sys.mem

| 函数签名 | 说明 |
|----------|------|
| `func alloc(int size) as point` | 分配内存 |
| `func free(point value) as bool` | 释放内存 |
| `func is_null(point value) as bool` | 是否空指针 |
| `func fill(point value, int byte_value, int size) as bool` | 填充内存 |
| `func copy(point dst, point src, int size) as bool` | 复制内存 |
| `func write_string(point value, string text) as int` | 写入字符串到内存 |
| `func read_string(point value, int size) as string` | 从内存读取字符串 |

### 61. 系统底层 - std.sys.net

| 函数签名 | 说明 |
|----------|------|
| `func resolve(string host) as string` | DNS 解析 |

### 62. 系统底层 - std.sys.process

（与 `std.process` 相同，委托调用）

### 63. 系统底层 - std.sys.wait

| 函数签名 | 说明 |
|----------|------|
| `func sleep(int ms) as void` | 休眠 |
| `async func delay(int ms) as int` | 异步延迟 |
| `async func wait(future<int/float/string/bool/error> value) as ...` | 等待 Future |

---

## 四、内部隐藏函数

解析器在处理字面量语法时会生成以下隐藏函数调用：

| 隐藏函数 | 说明 | 触发语法 |
|----------|------|----------|
| `__array_literal` | 创建数组 | `[1, 2, 3]` |
| `__list_literal` | 创建列表 | `list{1, 2, 3}` |
| `__list_literal_indexed` | 创建索引列表 | `[k: v, ...]` |
| `__dict_literal` | 创建字典 | `{k: v}` / `dict{}` |
| `__set_literal` | 创建集合 | `{1, 2, 3}` / `set{1, 2}` |

---

## 五、关键字列表

| 关键字 | 说明 |
|--------|------|
| `null` | 空值 |
| `true` / `false` | 布尔值 |
| `func` | 函数声明 |
| `function` | 函数类型 |
| `const` | 常量声明 |
| `var` | 变量声明 |
| `typedef` | 类型别名 |
| `as` | 类型标注 / 类型转换 |
| `if` / `then` / `else` / `elseif` / `end` | 条件语句 |
| `for` / `in` | for-in 循环 |
| `while` / `do` | while 循环 |
| `break` / `continue` | 循环控制 |
| `return` | 返回 |
| `async` / `await` | 异步 |
| `new` / `delete` | 对象创建/销毁 |
| `yield` | 生成器产出 |
| `try` / `catch` / `finally` | 异常处理 |
| `throw` | 抛出异常 |
| `assert` | 断言 |
| `import` / `export` | 导入/导出 |
| `extern` / `include` | 外部声明 |
| `class` / `custom` / `extends` | 类声明 |
| `union` | 联合体 |
| `create` / `destroy` | 构造器/析构器 |
| `get` / `let` | 属性 getter/setter |
| `nocopy` | 禁止拷贝 |
| `and` / `or` / `not` | 逻辑运算 |

---

## 六、运算符列表

| 运算符 | 说明 |
|--------|------|
| `+` `-` `*` `/` `%` `**` | 算术 |
| `==` `!=` `<` `<=` `>` `>=` | 比较 |
| `~=` | 近似等于 |
| `and` `or` `not` `!` | 逻辑 |
| `&` `\|` `^` `<<` `>>` | 位运算 |
| `??` | null 合并 |
| `?` `:` | 三元条件 |
| `.` `?.` | 成员访问/安全成员访问 |
| `[]` `?[]` | 下标/安全下标 |
| `=` `+=` `-=` `*=` `/=` `%=` `**=` | 赋值 |
| `?=` | null 合并赋值 |
| `=>` | 箭头（单行函数体） |
| `..` | 范围 |
| `...` | 展开运算符 |
| `:` | 键值分隔 |

---

## 七、标准库模块总览

| 模块 | 说明 | 函数数 | 常量 |
|------|------|--------|------|
| std.core | 核心打印/长度 | 16 | 1 |
| std.io | 输入输出 | 20 | - |
| std.str | 字符串操作 | 20 | - |
| std.math | 数学函数 | 22 | 2 |
| std.array | 数组操作 | 5 | - |
| std.list | 链表操作 | 5 | - |
| std.dict | 字典操作 | 4 | - |
| std.set | 集合操作 | 4 | - |
| std.iter | 迭代器工具 | 7 | - |
| std.json | JSON 编解码 | 2 | - |
| std.regex | 正则表达式 | 6 | - |
| std.fs | 文件系统 | 14 | - |
| std.dir | 目录操作 | 8 | - |
| std.path | 路径操作 | 8 | - |
| std.env | 环境变量 | 6 | - |
| std.os | 操作系统信息 | 5 | - |
| std.host | 主机信息 | 6 | - |
| std.time | 时间操作 | 11 | 2 |
| std.clock | 时钟 | 5 | 2 |
| std.fmt | 格式化 | 3 | - |
| std.hash | 哈希函数 | 6 | - |
| std.crypto | 加密 | 2 | - |
| std.encode | 编解码 | 4 | - |
| std.log | 日志 | 5 | - |
| std.assert | 断言 | 8 | - |
| std.error | 错误处理 | 20 | - |
| std.atomic | 原子操作 | 7 | - |
| std.sync | 同步原语 | 21 | - |
| std.thread | 线程 | 3 | - |
| std.future | Future | 10 | - |
| std.task | 异步任务 | 16 | 2 |
| std.channel | 通道 | 13 | - |
| std.module | 模块反射 | 9 | - |
| std.object | 对象反射 | 15 | - |
| std.types | 类型反射 | 47 | - |
| std.rand | 随机数 | 5 | - |
| std.pack | 二进制打包 | 10 | - |
| std.process | 进程 | 5 | - |
| std.range | 范围生成 | 3 | - |
| std.query | URL 查询 | 2 | - |
| std.template | 模板渲染 | 1 | - |
| std.debug | 调试 | 6 | - |
| std.trace | 追踪 | 2 | - |
| std.net | 网络 | 1 | - |
| std.net.tcp | TCP | 7 | - |
| std.net.udp | UDP | 5 | - |
| std.net.dns | DNS | 3 | - |
| std.net.ip | IP 地址 | 6 | - |
| std.net.url | URL | 4 | - |
| std.net.tls | TLS | 6 | - |
| std.net.proxy | 代理 | 5 | - |
| std.net.ws | WebSocket | 6 | - |
| std.net.http | HTTP 客户端 | 4 | - |
| std.net.httpd | HTTP 服务端 | 4 | - |
| std.net.multipart | Multipart | 3 | - |
| std.net.mime | MIME | 5 | - |
| std.sys.env | 系统环境变量 | 6 | - |
| std.sys.file | 系统文件 | 8 | - |
| std.sys.lib | 动态库 | 8 | - |
| std.sys.mem | 内存操作 | 7 | - |
| std.sys.net | 系统网络 | 1 | - |
| std.sys.process | 系统进程 | 5 | - |
| std.sys.wait | 系统等待 | 7 | - |
