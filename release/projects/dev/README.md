# dev 实战验证项目群

本目录通过编写真实可用的小工具，持续验证 xlang 语言能力、发现 BUG 和手感问题。

## 目标

- 用真实项目压测 xlang 各项特性（语法、类型、标准库、并发、持久化、C 混编）。
- 发现 BUG 和手感不正确的地方后，优先修复根因，并同步更新范例与回归测试。
- 工具本身要能跑、能用，不是玩具占位。

## 核心工具批次

当前目录包含 155 个项目目录、189 个 xlang 源文件；其中 151 个是可运行应用，
`xanalyze` 是分析器夹具，`netstack`/`xtrade` 是预留目录。下表记录最初按批次建立的核心工具，
其余项目用于网络、扩展库、嵌入式运行和边界回归等专项验证。

| 批次 | 工具 | 覆盖领域 |
|---|---|---|
| 1 | utf8count, jsonfmt, calc, primes, tree, passgen, ts, todo | string/json/math/generator/file/random/time/derive json |
| 2 | duptop, jq, csvjson, matrix, codec, stats | dir/CSV/泛型class/codec/statistics |
| 3 | psearch, portscan, sudoku, dupfind, game24, radix, config | async/TCP/回溯/hash/combinatorics/bitops/持久化 |
| 4 | guess, contacts, rename, cal, prodcon, bench, life | random/derive json/file/time/channel/benchmark/2D array |
| 5 | hashcalc, genid, envinfo, sorter, convert, geo, tpl, mdtoc, diff, bigint, timer, tictactoe, fwatch, kvdb | hash/id/env/sort/unit-convert/geometry/template/markdown/diff/bigint/timer/AI/file-watch/KV-store |
| 6 | cmix, gstack, vops, deepclone, props, overload, unionbits, dynobj, shapes, fstr, encconv, syncdemo, racedemo, hexdump, abidemo, pkgdemo, docgen | C混编/泛型/运算符协议/derive/属性/重载/union/动态字段/继承/f-string/encoding/sync原语/task高级/Buffer-File/ABI/import-package/文档注释 |
| 7 | raytrace | 继承多态+运算符协议+递归性能（光线追踪渲染器） |
| 8 | chip8 | 二进制/指令解码/模拟循环（CHIP-8 虚拟机） |
| 9 | xtest | 测试框架（断言库/测试发现/运行/setup-teardown/报告/并发） |
| 10 | lisp | 递归解析器+递归求值器+词法闭包+深递归（Lisp/Scheme 解释器） |

## 目录约定

```
dev/<tool_name>/
    <tool_name>.xl      主源码
    README.md           说明（可选）
    *.c                 C companion（如需 C 混编）
```

## 运行与验证

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\<tool_name>\<tool_name>.xl
xl build projects\dev\<tool_name>\<tool_name>.xl -o projects\dev\<tool_name>\<tool_name>.exe
```

每个工具通过 `xl run` 和 `xl build` 两条路径验证。

完整历史目录审计、改写范围、验证分级和迁出前 175 个目录的归类见
[项目审计报告.md](项目审计报告.md)。可使用以下脚本重复验证：

```powershell
powershell -ExecutionPolicy Bypass -File ..\..\..\tools\check_dev_projects.ps1 -Mode all -KeepGoing
powershell -ExecutionPolicy Bypass -File ..\..\..\tools\run_dev_selftests.ps1 -KeepGoing
```

## xlang 关键语法速查

- 输出：`print(args...)` 不加换行；`println(args...)` 加换行。参数间自动加空格。
- 函数返回类型用 `as`，不是 `->`：`func f(int a) as int`。
- 语句分隔：换行或 `;`。`()`/`[]`/`{}` 内可换行。
- 控制流：`if ... then ... elseif ... else ... end`；`for x in ... ... end`；`while cond ... end`；`do ... while cond end`。
- 类型：`var` = 动态类型；`auto` = 类型推断；无 `void`。
- 容器：`[1,2,3]` = array；`{}` = dict；`array<int>[1,2,3]`/`set<int>{1,2}`/`list[0: 1]`/`dict<int>{"a":1}` 显式构造。
- array 方法：`append/push/pop/insert/remove/removeValue/contains/indexOf/clear/reverse`，`array + array` 拼接。
- dict 方法：`d.remove(key)` / `dict.remove(d, key)`。
- 转换：`int(expr)`、`float(expr)`、`string(expr)` 等构造函数。`uint64` 格式化用 `int.format(v, format)`。
- `as` 是运行时类型检查向下转换，不是数值转换。
- 字符串拼接用 `+`。f-string：`f"hello {name}"`，`rf"c:\{name}"`。
- 标准 API 入口参考：`D:\GIT\x-lang\DEMO6功能清单.md` 第 14 节。
- 范例参考：`D:\GIT\x-lang\demo6\release\examples\` 下的 `.xl` 文件。

## 当前设计限制

以下条目是当前明确保留的设计边界，不是兼容旧实现的临时绕行：

| 限制 | 说明 | 绕过方式 |
|---|---|---|
| **无 stdin API** | 不能从控制台读输入 | 用命令行参数驱动 |
| **有 destroy 的 native-backed class 不按普通 class 值语义复制** | native 资源遵循 owned/borrowed/move 生命周期，禁止隐式复制同一句柄 | 传递借用参数，或返回 `T()` 新构造/明确可移动的资源值 |
| **json pretty 用 TAB 缩进** | XRT 底层未提供配置机制 | `string.replace(s, "\t", "  ")` |
| **`auto` 无法从 `pop()` 推断** | 用显式类型 | `int p = a.pop()` |
| **list 无 append** | int64 稀疏存储 | `li[index] = value` |
| **`#` 不是注释** | `//` 才是注释 | `#` 开启时间字面量 |

## 正向验证通过的能力

以下能力已经过当前 dev 项目群、分类测试套件和发布物闭包的联合验证：

- **语法**：f-string/rf-string、多行字符串、raw string、时间字面量、`#doc/#end` 文档注释
- **类型系统**：泛型函数/泛型 class/泛型约束/泛型迭代协议、union 类型、typedef、is/as 类型判断
- **容器**：array 全套方法 + `+` 拼接、list 稀疏索引、set/dict（含 remove）、`Type<T>[...]` 构造、**负索引 `arr[-1]`**、**`for _,_ in dict`**
- **OOP**：class 继承链、base 调用、public/private/protected、static、virtual/override/final、vtable 动态派发、**隐式向上转型**、var 装箱 vtable 保留、属性 get/let（零维/多维/virtual）、方法重载/构造重载、**`+=` 复合赋值派发运算符协议**
- **运算符协议**：`__add__/__sub__/__mul__/__div__/__mod__/__pow__`、`__neg__/__pos__/__not__/__bitNot__`、`__eq__/__ne__/__lt__/__le__/__gt__/__ge__/__approx__`、`__item__/__setItem__`（多维）、`__contains__/__call__/__len__/__iter__/__next__/__field__/__setField__`
- **derive**：copy/clone/moveOnly（冲突检查）/value/json/xson/string/hash/equals + 字段注解 @json/@persist/omit
- **持久化**：json/xson stringify/parse（含容器内 class）、字段级注解
- **并发**：async/await、future、task.all/race/spawn/cancel、generator/yield、channel（send/recvInto/tryRecvInto/close）、thread、mutex/sem/cond/rwlock
- **共享全局状态**：文件作用域 `var`/typed container/class 拥有字段可在初始化或重绑定后进入 thread、task 和 native callback；共享容器写入嵌套复合值时由 XRT 完成共享图发布
- **C 混编**：extern 函数/变量、export C ABI、native-backed class、.xl+.c 混合构建、xl abi dump、import/package/扩展库
- **标准库**：string/math/time/random/path/file/dir/json/xson/codec/hash/crypto/process/log/template/encoding/net/tcp/udp；高级 HTTP 与 Regex 项目已迁到 `dev/deferred/xhttp` 和 `dev/deferred/xregex`。
- **try/catch/finally**（含 catch 内 return）、throw、assert、多返回值+解构
- **计算密集型**：raytrace（光线追踪）渲染 100×75×5 抗锯齿递归图像 < 0.4s（约 10 万根光线/秒，每像素递归 25 层上限）
- **其他**：UTF-8 全链路（文件名/argv/输出）、xl run 精确返回码、build exe UTF-8 argv、**class 按引用传参**、**命名函数可作函数值传递**、**union `string(type(u))` 返回正确类型名**、**自引用 class 字段**

## 质量记录

历史问题不作为当前推荐写法保留在项目代码中。根因、修复边界与验收证据见
`docs/三轮问题统一修复任务.md` 和 `docs/质量压实长期任务.md`；当前行为以 demo6
设计文档、`DEMO6功能清单.md`、分类测试套件和本目录最新范例为准。
