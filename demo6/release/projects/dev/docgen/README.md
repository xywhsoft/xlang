# docgen - 文档注释 (`#doc ... #end`) 验证工具

验证 xlang 的 `#doc` 文档块系统：合法字段、附加目标、缩进收集、example 代码围栏文本化、
语义校验，以及编译器对各类非法用例的正确拒绝。

## 运行

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\docgen\docgen.xl
xl build projects\dev\docgen\docgen.xl -o projects\dev\docgen\docgen.exe
docgen.exe
```

三条路径都必须 EXIT=0。也可以先 `xl check projects\dev\docgen\docgen.xl` 验证语义校验通过。

## 覆盖范围（28 条断言，全部 PASS）

### 合法 #doc 能力（18 条运行时断言）

| 类别 | 被测写法 | 说明 |
|---|---|---|
| 附加到 var | `#doc ... #end int g_seed = 100` | 文档块附加到全局变量 |
| 附加到 const | `#doc ... #end const int G_LIMIT = 1024` | 文档块附加到全局常量 |
| 函数全字段 | `add(int,int) as int` 上 `desc/param a/param b/return/note/example` | 任务规格示例原样验证 |
| 多行 desc | `add` 的 desc 跨段落仍按缩进归属 | 多行字段体收集 |
| 描述+参数+返回 | `sub(int,int) as int` 上 `desc/param/return` | 最小完整组合 |
| desc + return | `pickMax(int,int) as int` 仅 desc + return | 字段可缺省 |
| 无返回函数 | `greet(string)` 上 `desc/param/note/example`（**无** `return`） | 验证 void 函数 doc 不允许 return |
| 泛型函数 | `clamp<T>(T,T,T) as T` 上 `param/return/example` | 文档块附加到泛型函数 |
| 附加到 class | `class Point` 上 `desc/note/example`（**无** `param/return`） | 类级 doc 不允许 param/return |
| 类方法全字段 | `Point.length2() as int` 上 `desc/return/note/example` | 方法 doc 完整支持 |
| 类方法 desc+param | `Point.scale(int)` 上 `desc/param` | 方法 doc 最小组合 |
| derive 类 + 多 ctor | `class Counter { derive value; create() / create(int) }` | doc 不影响 derive 与重载 ctor |
| doc 不影响序列化 | `json.stringify(Counter)` / `json.parse::<Counter>` 往返 | doc 块完全透明 |

### 非法 #doc 用例（10 条子进程断言）

通过 `process.run("xl check cases/<file>.xl")` 验证编译器对每种非法写法**正确拒绝**：

| 用例 | 期望阶段 | 期望诊断子串 |
|---|---|---|
| `bad_unknown_field.xl` | sem | `[sem] #doc` |
| `bad_naked_text.xl` | sem | `[sem]` |
| `bad_dup_param.xl` | sem | `[sem] #doc` |
| `bad_dup_return.xl` | sem | `[sem] #doc` |
| `bad_param_mismatch.xl` | sem | `[sem] #doc param` |
| `bad_return_on_void.xl` | sem | `[sem] #doc return` |
| `bad_param_on_class.xl` | sem | `[sem] #doc param` |
| `bad_return_on_class.xl` | sem | `[sem] #doc return` |
| `bad_unclosed.xl` | parse | `[parse]` |
| `bad_orphan.xl` | parse | `[parse]` |

## 发现的 #doc 校验规则（DEMO6功能清单 §12 未明示）

通过编写本工具的探针测试，确认了以下 `#doc` 语义校验规则（功能清单仅说"doc 字段进入语义校验"，
未列具体规则）：

| 规则 | 说明 |
|---|---|
| `desc:` 不可重复 | 同一 #doc 块内出现两个 `desc:` → `[sem] #doc desc 字段不能重复` |
| `param X:` 不可重复同名 | 同名 param → `[sem] #doc param/return 字段不能重复描述同一个名称` |
| `return X:` 不可重复同名 | 同名 return → 同上 |
| `param X:` 名字必须匹配函数签名 | 形如 `func f(int a)` 内 `param b:` 会被拒 |
| `param X:` 在 class 上不允许 | 即使 `create(int x)` 存在，类级 doc 也不能写 `param x:` |
| `return X:` 只能用于有返回值的函数 | void 函数、class、var、const 上写 `return` 会被拒 |
| `note:` 可重复 | 多条 note 允许叠加 |
| `example:` 可重复 | 多条 example 允许叠加 |
| 字段顺序任意 | `return` 在 `param` 之前也合法 |
| 字段体外裸文本不允许 | 必须 `field:` 开头 |
| 未知字段名不允许 | 仅 `desc / param X / return X / note / example` 五类合法 |
| `#doc` 必须配对 `#end` | 未闭合 → `[parse] 未闭合的 #doc 文档块` |
| `#doc` 必须紧跟一个声明 | 后面没有 func/class/var/const 等 → `[parse] 语法错误` |
| `#doc` 不能附加到 `typedef` | `#doc ... #end typedef int N` → `[parse] 语法错误` |
| 同一声明前不能堆叠多个 `#doc` | 连续两个 `#doc` 块再跟一个声明 → `[parse] 语法错误` |
| example 内的代码围栏（包括 ```` ```xlang ```` ）作为纯文本 | 不参与词法/语法/语义解析；围栏内的非法 xlang 也不报错 |

## 关键代码

- `findXl()` (`docgen.xl:64`) — 三段回退定位 xl.exe：cwd → appDir → appDir/../../../。
- `negativeCheck(fileName, expectedDiag)` (`docgen.xl:360`) — 用 `process.run("xl check ...")`
  对 cases 子目录中的非法 .xl 文件做子进程验证，检查 exitCode != 0 且 stderr 含期望诊断子串。
- `add` 函数 (`docgen.xl:121`) — 任务规格示例原样落地，完整 5 字段文档块。
- `clamp<T>` 函数 (`docgen.xl:185`) — 验证文档块附加到泛型函数不影响编译。

## 设计要点

- 主程序只包含合法 #doc 块；非法用例放在 `cases/` 子目录，由 `main` 通过 `process.run("xl check ...")` 调用验证。
- `main(...)` 始终返回 0，保证 `xl run` / `xl build` 双路径 EXIT=0；测试失败只计 `[FAIL]` 不中断。
- 工具自身运行时还会**实际调用**每一个被文档化的函数 / 类 / 字段，证明文档块完全不影响运行行为
  （这是 #doc 系统最关键的不变量：注释是注释，代码是代码）。
- `cases/` 路径双回退：优先 cwd（`xl run` 调用目录），其次 appDir（exe 同目录的 cases 子目录），
  因此无论从 release 目录还是从 docgen 目录执行构建产物都能找到 cases。

## BUG 清单

无。`#doc` 系统在本次验证的全部合法与非法路径上行为正确。
