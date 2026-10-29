# xprof - xlang 性能分析器

用 xlang 编写、用 `xl parse --format json` 取 AST 做函数计时插桩 + 采样调用图 + ASCII 火焰图。
**用 xlang 分析 xlang 自身代码**。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run  projects\dev\xprof\xprof.xl instrument <file.xl> <output.xl>
xl run  projects\dev\xprof\xprof.xl analyze <profile.json>
xl run  projects\dev\xprof\xprof.xl flame <profile.json>
xl run  projects\dev\xprof\xprof.xl test
xl build projects\dev\xprof\xprof.xl -o projects\dev\xprof\xprof.exe
```

> `xl run <script.xl>` 传参需用 `--`：`xl run xprof.xl -- instrument app.xl app.inst.xl`。

## 命令

| 命令 | 说明 |
|---|---|
| `instrument <file.xl> <output.xl>` | 解析 AST，在每个函数入口/返回路径插入计时探针，输出可运行的插桩源码 |
| `analyze <profile.json>` | 热点表（按 selfTime 排序）+ 调用图（caller → callee 采样次数） |
| `flame <profile.json>` | ASCII 火焰图，盒宽 ~ 采样占比 |
| `test` | 内置自测（findFuncEnd / 插桩 / analyze / flame） |

## 工作流

```
xprof instrument app.xl app.inst.xl     # app.inst.xl 含 __xprof 运行时
xl run app.inst.xl                       # 运行结束写出 xprof_profile.json
xprof analyze xprof_profile.json         # 看热点 + 调用图
xprof flame   xprof_profile.json         # 看火焰图
```

## 实现要点

- **AST 插桩**：`xl parse <abs> --format json` → `json.parse` 成 `var` 树；遍历收集 `func` / `async_func` /
  `export_func` / `create`（含类内方法，名作 `Class.method`）。对每个函数：
  - 函数首语句前插 `__xprof_enter("name")`；
  - 每条 `return` 前 + `end` 前插 `__xprof_exit("name")`（贯穿路径由 end 前的兜底覆盖）；
  - `main` 的退出路径额外插 `__xprof_dump()`；无 main 的脚本在文件末尾追加 dump。
  - 只增行不改行，缩进取自被插行；匹配 `end` 用「与 func 同缩进的第一个 end」（缩进良好的代码）。
- **运行时**（triple-quoted 字符串整段注入被测文件头）：
  - 计时用 `time.timer()`，enter/exit 各取一次，差值换算 elapsed/self（self = elapsed - 子时间）。
  - 调用栈用三组并行原生数组（`__xp_names` / `__xp_starts` / `__xp_childTimes`）承载，避免递归热路径分配
    class/dict（规避 BUG-LP1：递归中分配 var dict 触发 O(n²) GC 劣化）。
  - 后台采样线程（`thread(__xp_sampler)`）每 2ms 在 mutex 保护下快照调用栈，用于火焰图与调用图。
  - 程序结束（main 返回）时 `__xprof_dump()` 写出 `{functions, samples}` 的 JSON。
- **调用图**由采样栈重建（相邻帧即一条边），非每次调用的边计数 —— 进一步消除热路径字符串拼接。
- **热点/调用次数**是精确的（每次 exit 累计），只有调用图的“次数”是采样近似。

## profile.json 格式

```json
{
  "functions": [{"name":"foo","calls":10,"totalTime":0.123,"selfTime":0.012}],
  "samples":   [["main","foo","bar"], ["main","foo"]]
}
```
`samples[*][0]` = 最外层帧。

## 已知限制

- 插桩基于 AST + 缩进匹配 `end`：单行函数、签名与首语句同行的函数会被跳过（避免破坏源码）。
- 行注释 `//` 在 `xl parse` 不保留，但插桩不改原行，注释原样保留在输出里。
- 调用图为采样近似：极短/冷函数可能不在调用图与火焰图中（计时与调用次数仍精确）。
- 被测程序若递归极深，探针的锁与数组操作仍会带来可观开销（instrumentation profiler 固有）。
- 注入符号前缀 `__xp_` / `__xprof_` / 类名 `XStat`：与被测代码重名会冲突。
