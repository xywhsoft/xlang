# dev 工具群 BUG 与手感问题记录

本文件只记录**当前未修复**的 BUG 和设计限制。已修复的 BUG 已从此文件移除。

---

## 未修复 BUG

### BUG-S1: string.splitLines 把末尾换行当分隔符，多出尾部空串
- 工具：sorter, diff, mdtoc 等
- 复现：`array a = string.splitLines("a\nb\n")`
- 现象：`a.length == 3`，元素为 `["a", "b", ""]`
- 期望：末尾换行作为行终结符，应得到 2 行
- 绕过：若末元素为 `""` 则 remove 一个
- 严重度：中
- 状态：设计行为，暂不修复

### BUG-M1: int() 对非数字字符串静默返回 0 而不抛异常
- 工具：mdtoc 等
- 复现：`int n = int("abc")` → n == 0，不抛异常
- 期望：应抛异常或有 tryParse
- 绕过：自行逐字符校验
- 严重度：低

### BUG-GS2: `hashable` 非法泛型约束名
- 工具：gstack
- 复现：`func f<T is hashable>(T x)` → `[sem] unknown generic constraint`
- 现象：`hashable` 不是合法约束名；合法名集合未文档化
- 绕过：使用 `comparable`/`cloneable`/`json` 或 class 继承约束
- 严重度：低

### BUG-V1: 复合赋值 `+=` `-=` 等不派发到 class 运算符协议
- 工具：vops
- 复现：`a += b`（a/b 为自定义 class）→ `[sem] numeric compound assignment operands must be numeric`
- 期望：应派发到 `__add__` 协议
- 绕过：展开为 `a = a + b`
- 严重度：中

### BUG-P1: 复合赋值以 class 属性为左值被拒
- 工具：props
- 复现：`m.hp += 10` → `[sem] class property assignment only supports =`
- 期望：应支持复合赋值
- 绕过：`m.hp = m.hp + 10`
- 严重度：低（与 BUG-V1 同根因）

### BUG-O1: `var` 实参无法传给重载函数
- 工具：overload
- 复现：重载 `func f(int)` / `func f(float)`，传 var 值 → `[sem] 没有匹配的函数重载`
- 绕过：先 `int(v)` 或 `float(v)` 显式转换
- 严重度：中

### BUG-O2: `__item__` 重载返回 `array<int>` 运行期崩溃
- 工具：overload
- 复现：`func __item__(int row) as array<int>` → 0xC0000005 硬崩溃
- 绕过：避免协议方法返回 typed 容器
- 严重度：高

### BUG-O3: get 属性零维+一维重载不工作
- 工具：props, overload
- 复现：`get hp() as int` + `get hp(int) as int`，访问 `m.hp` → `[checked] 找不到 class 方法`
- 绕过：使用不同属性名
- 严重度：中

### BUG-D2: `derive string/hash/equals` 未实现
- 工具：deepclone
- 复现：`class X { derive string; }` → `[sem] reserved but not implemented in demo6`
- 现象：编译期明确拒绝（诊断已改进，不再静默通过）
- 绕过：手写 `__eq__`/`__ne__`/`__toString__`/`hashByFields`
- 严重度：中（功能缺失，但诊断清晰）

### BUG-U1: union 实例 `string(type(u))` 返回 "unknown"
- 工具：unionbits
- 复现：`string(type(unionInstance))` → `"unknown"`（class 实例正常返回类名）
- 绕过：用 `type(u) == T` 或 `u is T`，不依赖字符串化
- 严重度：低

### BUG-E1: `encoding.toBytes` 加 BOM flag 返回空 Buffer
- 工具：encconv
- 复现：`encoding.toBytes("text", encoding.utf8 + encoding.bom)` → `buf.len() == 0`
- 绕过：不加 BOM flag，需要 BOM 时手工拼接
- 严重度：中

### BUG-HD1: native-backed class 不能从已有具名值 return
- 工具：hexdump
- 复现：`Buffer b = ...; return b` → `[sem] 不可复制类型不能从已有值返回`
- 注意：作函数参数传入 OK；`return Buffer()` 临时构造 OK；仅 return 已有具名值被拒
- 绕过：操作 Buffer 的函数接收参数而非返回
- 严重度：中

### BUG-HD2: `string.formatInt(n, "c")` 不按字符格式输出
- 工具：hexdump
- 复现：`string.formatInt(65, "c")` → `"65"`（期望 `"A"`）
- 绕过：手写 ASCII 映射
- 严重度：低

### BUG-K1: json pretty 模式键序可能 hash 重排
- 工具：kvdb 等
- 复现：`json.stringify(json.parse('{"a":1,"b":2}'), pretty=true)` → 键序可能变化
- 注意：compact 模式保留插入序；pretty 模式依赖 hash 表实现
- 绕过：需要保序时手工拼接 JSON 文本
- 严重度：低

## 设计限制（不予修复）

| 限制 | 说明 |
|---|---|
| `~=` 是近似相等 | 不是"不等于"；不等判断用 `!=` |
| 无 stdin API | 交互式工具用命令行参数驱动 |
| native-backed class 不可按值传参 | 带 destroy 的不可复制资源（设计决策） |
| json pretty 用 TAB 缩进 | XRT 底层未提供缩进配置 |
| list 无 append | int64 稀疏存储，用 `li[index] = value` |
| `#` 不是注释 | `//` 才是注释；`#` 开启时间字面量 |
| `auto` 无法从 `pop()` 推断 | 用显式类型 `int p = a.pop()` |
