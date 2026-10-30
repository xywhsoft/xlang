# xlang

**一门语言。原生性能。脚本般的敏捷。两者之间没有边界。**

[English](README.md) · **中文**

当前产品版本：**0.6.0**，版本代号 **`0x00000600`**；宿主嵌入 ABI 保持版本 1。已验证的修复、验收结果与检查范围见[发布前检查报告](docs/prerelease-0.6.0.md)。

---

## xlang 是什么?

xlang 是一门静态类型、可与 C 互操作的编程语言:最终经由 C 编译为原生代码,却拥有脚本语言一般的即时运行体验。它的完整工具链——编译器、内存态原生后端、标准库、包系统、调试器、IDE——全部自包含于本仓库。

两个理念定义了 xlang:

1. **"编译程序"与"脚本"之间的墙并不存在。** xlang 程序在运行时执行 xlang 源码文本;被执行的脚本还可以继续执行下一层脚本——自顶向下都是同一门语言,没有方言、没有第二套运行时、没有生态割裂。
2. **静态的速度与动态的灵活共存于同一个对象。** 把你关心的字段固定为原生布局槽位;让可选的长尾字段以动态键的方式挂载——一个实例、一行语法,无需手搓混合类型。

---

## 为什么是 xlang

### 1 · 一门语言 —— 编译代码与脚本之间没有墙

多数生态是把脚本语言"外挂"到编译语言上:C++ 宿主嵌入 Lua,或者 VB 与 VBS 各自为政。两种语言、两套类型系统、两套工具链,到处是 FFI 胶水,"引擎"与"模组"之间隔着一道硬边界。

xlang 从构造上消除了这道边界:

- **编译模式**(`xl build`):提前编译为独立原生 `.exe` / `.dll`。
- **即时模式**(`xl run`):同一份源码在内存中编译为机器码并立即执行——不落临时文件、没有解释器、不会因为是"脚本"而变慢。
- **运行时执行**:一个**正在运行的** xlang 程序可以随时编译并执行 xlang 源码文本:

```xlang
import xlang

// 在一个已编译的 xlang 程序里 —— 现在就执行一段 xlang 源码。
// 它走的是完全相同的管线:词法 → 语法 → 语义类型检查 → C 生成 → 内存态原生代码。
assert xlang.run("println(\"hello from a script\")\n")

// 从磁盘加载一个模组……
assert xlang.runFile("mods/boss_ai.xl")

// ……或者持有一个带独立状态的长期脚本上下文。
XLangContext ctx = xlang.Context()
if not ctx.run(modSource) then
    println(ctx.lastError)
end
```

这段脚本自己还可以再调用 `xlang.run(...)`——嵌套没有尽头,而且**每一层都经过类型检查并编译为原生代码**,不是解释执行。

**为什么这对游戏(以及一切"引擎 + 模组"架构)至关重要:** 引擎用 xlang 写成 `.exe`,游戏模块以 `.xl` 文本文件发行,两半是同一门语言——同样的类型、同样的标准库、同样的包、同样的调试器和 IDE。模组不是外来运行时里的客人,而是同一个运行时的一等公民。

**从 C 嵌入同样是头等公民。** `xl.dll`(运行时)与 `xlc.dll`(编译器)暴露一套小而稳定的 C API:

```c
#include "xl.h"

XL_ContextOptions opts = xlDefaultContextOptions();
XL_Context* ctx = xlCreateContext(&opts);

// 把引擎函数暴露给脚本;xlang 侧用
// `extern "host_spawn_particle" func ...` 声明即可调用。
xlRegisterCFunction(ctx, "host_spawn_particle", &spawn_particle);

// 运行脚本文件:类型检查、进程内编译为原生代码。
xlRunScriptFile(ctx, "scripts/main.xl");

// 或者让模块常驻并调用其导出:
//   XL_Module* m = xlCompileScriptFileToModule(ctx, "mods/game.xl");
//   xlActivateModule(ctx, m, arg);
//   void* fn = xlModuleGetSymbol(m, "mod_update");
// (xlc.dll)或者直接编译成独立 exe:
//   xlCompileScriptFileToExe(ctx, "scripts/main.xl", "game.exe");
```

标准库甚至提供了 `tcc.*` 模块,xlang 程序在运行时还能编译并执行 **C 源码**——适合 shader 式片段、插件和 JIT 式扩展点。

### 2 · 静态的速度,动态的灵活 —— 在同一个对象里

本机语言给你一个快速但字段写死的结构体——而真实领域(游戏实体、配置、来自外部的 JSON)充满事先未知的可选字段。脚本语言给你灵活的对象——但每次字段访问都是一次哈希查找,每个值都是装箱的。

xlang 让一个实例同时承载两半,**由你决定**每个字段活在哪个世界:

```xlang
class Player extends object
    derive value

    int    id    = 0            // 静态字段 —— 固定槽位,原生访问速度
    string name  = ""
    Stats  stats = Stats()      // 嵌套静态对象
    var    tags  = []           // 动态值槽位
end

auto p = Player()
p.id            = 7             // 静态写入:普通内存访问
p.stats.hp      = 120           // 嵌套静态访问
p.rank          = 9             // 动态字段:按需出现在实例上
p["title"]      = "knight"      // 动态键访问

// 序列化同时看到两半,并带着类型往返:
auto jsonText = json.stringify(p)
auto q        = json.parse::<Player>(jsonText)
println(q.name)                 // 静态字段,有类型
println(q.rank)                 // 动态字段,被保留
```

唯一的仪式就是 `extends object`——不需要建造者类、不需要 variant 访问器、不需要反射框架。C++ 和 C# *可以*手工造出等价物;xlang 让它成为对象的默认形态,像脚本语言一样简单,而静态那一半依旧编译为朴素的 C。

同样的哲学贯穿整个类型系统:

| 能力 | 写法 |
|---|---|
| 需要时用动态值 | `var x = ...` —— 标量、容器、函数、对象皆可 |
| 可空 + 流类型收窄 | `T?`、`?.`、`?[...]`、`??`;`if v != null` 自动收窄类型 |
| 运行时类型判断 | `v is T`;`v as T`(失败抛错)/ `v as T?`(失败返回 null) |
| 有类型的容器 | `array<T>`、`list<T>`、`set<T>`、`dict<T>` |
| 自定义类型的运算符协议 | `__add__`、`__call__`、`__iter__`、`__contains__`、`__eq__` 等 |
| 派生行为 | `derive string / hash / equals / value` |
| 协议作为泛型约束 | `func cloneAll<T is cloneable>(array<T>) as array<T>` |
| 受管弱引用 | `weak<T>`,`lock()` 返回 `T?` |

---

## 语言速览

```xlang
// 源码为 UTF-8;中文标识符就是普通标识符。
func 斐波那契(int n) as int
    if n < 2 then
        return n
    end
    return 斐波那契(n - 1) + 斐波那契(n - 2)
end

// 带数据的 enum:静态 tag + union payload 布局。
enum FetchResult<T>
    pending
    ready(T value)
    failed(int code, string message)
end

func formatResult(FetchResult<int> result) as string
    return match result
    case pending then "pending"
    case ready(value) then f"ready:{value}"
    case failed(code, message) then f"failed:{code}:{message}"
    end
end

func main(...) as int
    auto name = "xlang"
    println(f"hello {name}, fib(10) = {斐波那契(10)}")
    println(formatResult(FetchResult::<int>.ready(42)))
    return 0
end
```

- **语法**:换行或 `;` 分隔语句;`/* */` 注释可嵌套;f-string / raw / 多行字符串;time 字面量;数组 / 列表 / 字典 / 集合字面量。
- **函数**:一等公民,带类型签名,多返回值(`as (int, string)`),默认参数与命名参数,`...` 数组 / `***` 字典可变参数。
- **OOP**:类继承、`virtual` / `override` / `final`、访问控制、静态成员、属性(`get` / `let`)、包装 C 句柄的 native-backed class。
- **协议与泛型**:用户自定义 protocol,结构化满足(无需书写 `implements`),turbofish 实例化 `max::<int>(a, b)`。
- **错误**:`try` / `catch` / `finally` / `throw`,分类错误模型(如 `ZeroDivisionError`、`TypeError`、`ArithmeticError`)。
- **并发**:`async` / `await` / `yield`,线程、mutex / cond / semaphore、`channel<T>`、generator、task API(`spawn` / `all` / `race` / `cancel`)。
- **模块**:`import geometry`、`import geometry as geo`、`from geometry import Point, distance as dist`;`export` 公开符号;`.xl` 源码是公开接口的唯一事实来源。
- **文本**:字符串默认 UTF-8(`len` 按 codepoint,`byteLen` 按字节);`Buffer` 承载二进制。

## 编译为 C,处处原生

管线固定且全程可见:

```
源码 (.xl) ─▶ 词法 ─▶ 语法 ─▶ 语义 / typed HIR (XLIR) ─▶ C 生成 ─▶ 内嵌 TCC ─▶ 原生代码
```

- **C 互操作是主干而非桥梁**:`.xl` 与 `.c` 在同一次 `xl build` 里混合编译;C 函数经 `extern "symbol"` 声明进入 xlang;xlang 通过 ABI 元数据与自动生成的头文件导出回 C;回调双向可用。
- **内存态后端**:`xl run` 默认全程驻留内存(不产生 `tmp/` 垃圾);内置 VFS(动态 → 磁盘 → 静态)服务内嵌源码。
- **随你选择交付方式**:立即运行、编译成 `.exe` / `.dll`,或在程序运行期再编译。

## 标准库与包生态

- **核心标准库**:500+ 公开函数、数十个模块——`net`(TCP / UDP / TLS / 解析器 / 代理)、`crypto`(AES-GCM、ChaCha20-Poly1305、SHA 家族、Ed25519、X25519、RSA 等)、`json` / `xson`、`regex`、`template`、`fs` / `dir` / `file`(同步 + 异步)、`process`、`os`、`time`、`encoding`、`codec`、`log`、`math`、`random`、`signal`、`task` 等——构建在共享运行时基座 [XRT](docs/XRT集成原则.md) 之上。
- **214 个现成扩展包**(`release/extlibs/`),每个仅一份 `xlang.package.json` 清单,支持多版本共存:`sqlite`、`zeromq`、`imgui`、`webview`、`glad`、`freetype`、`harfbuzz`、`stb_image`、`zip` / `zstd` / `brotli` / `lz4`、`yaml` / `toml` / `xml` / `csv`、`jwt`、`etcd`、`docker`、`kubernetes`,还有 AI 一角(`ggml`、`whisper`、`faiss`、`hnswlib`、`tokenizers`),以及完整的 `xge` 游戏栈与 `xui` / `ui.auto` UI 栈。
- **真实游戏作为范例**(`release/examples/games/`):九个完整游戏——贪吃蛇、俄罗斯方块、2048、Flappy、Pong、打砖块、扫雷、小行星、射击——全部用 xlang 基于 XGE 扩展库(窗口、输入、图形、字体、纹理、音频)编写。

```bat
release\xl.exe run release\examples\games\snake.xl
```

## 工具链

```bat
xl run main.xl                  :: 内存态编译并运行,无临时文件
xl build main.xl                :: 原生 exe(加 -shared 生成 dll)
xl check main.xl                :: 完整语义检查
xl lex / parse / ir / cgen / abi :: 检视管线每个阶段
xl impdef module.dll -o m.def   :: 从现有 DLL 导入 C 符号
```

- Windows 资源(版本信息、图标、清单)通过 `--res file.json` 注入。
- 专用 **xlang IDE**(`ide/`,基于 XGE)提供工程管理、C/xlang 双语法编辑、语义补全与导航、构建运行——边界两侧,同一件工具。

## 平台与构建

- **平台**:Windows x64(主平台)与 Linux x86_64。
- **自带完整工具链**:编译器以 C11 编写,用内嵌 TCC 自举构建,无需外部 SDK。

```bat
build.bat          :: Windows
test.bat           :: 主回归(4700+ 用例文件)
```

```bash
bash build.sh && bash test_linux.sh
```

## 仓库结构

| 路径 | 内容 |
|---|---|
| `src/` | 编译器、运行时、标准库、测试、文档、范例 |
| `docs/` | 设计文档(中文)——设计决策的唯一口径 |
| `release/examples/` | 450+ 范例程序,含游戏与应用 |
| `test/` | 回归资产:用例、套件、fixture、interop 测试 |
| `ide/` | xlang IDE 源码、构建与便携发布 |
| `DEMO6说明.md` | 当前编译器功能清单(中文) |

## 状态

xlang 处于发布候选压实阶段:四阶段管线、模块/包系统、DLL 嵌入 API 与大型回归套件(4700+ 用例,由 `rc0_guard` 守门)已经就位;网络数据通路拥有权模型与扩展库生态仍在推进。设计文档目前以中文为主,位于 `docs/`。

xlang 原创部分采用 [MIT 许可证](LICENSE)。第三方及其衍生代码保留各自许可，详见[第三方许可说明](THIRD_PARTY_NOTICES.md)。

开发与构建入口见 [DEVELOPMENT.md](DEVELOPMENT.md)。
