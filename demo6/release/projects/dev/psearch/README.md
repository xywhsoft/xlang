# psearch - parallel file content searcher

并行搜索多个目录中**文件内容**包含指定模式的文件。重点验证 xlang 的
`async`/`await` 与 `task.all`。

## 用法

```
psearch <pattern> <dir1> [dir2] [dir3] ...
```

- `<pattern>`：纯文本子串（走 `string.contains`，不走正则）。
- 至少 1 个目录，最多 16 个目录（见下“限制”）。
- 递归扫描每个目录下所有文件，读全文做子串匹配，命中则打印该文件完整路径。
- 末尾打印摘要：pattern / 目录数 / 命中文件数 / 耗时。

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\psearch\psearch.xl -- "func" examples projects\dev
xl build projects\dev\psearch\psearch.xl projects\dev\psearch\psearch_shim.c -o projects\dev\psearch\psearch.exe
psearch.exe "func" examples projects\dev
```

## 设计（async / task.all 核心）

每个目录一个 async 搜索任务，用 `task.all` 一次等待全部完成：

```xlang
async func searchDir(string dir, string pattern) as int
    ...
    array files = dir.files(dir, true)     // 递归列出
    for each file: file.readText + string.contains -> println + count
    return count
end

async func runAll(float startSec) as int
    array<future<int>> jobs = array[
        c > 0 ? searchDir(g_dir0, g_pattern) : noop(),
        c > 1 ? searchDir(g_dir1, g_pattern) : noop(),
        ...                                                                  // 最多 16 槽
    ]
    array<int> counts = await task.all(jobs)    // 并行等待
    ...汇总...
end
```

- `searchDir` 在 `task.all` 调度下**真正并行**执行——输出里两个目录的命中路径
  交错出现即为证据（同步串行则会先出完一个目录再出另一个）。
- `task.all` 返回 `array<int>`，`await` 一次拿到全部目录的命中数。

## 为何这样写（xlang 约束 / BUG 绕过）

| 约束 | 影响 | 绕过 |
|---|---|---|
| `await` 不能用在 `func main` 里（main 非 async） | main 无法直接 `await task.all` | 把并行逻辑放进 `async func runAll`；main 解析 argv 后用 `future.wait()` 阻塞驱动协程（已验证 `.wait()` 能驱动 async 协程到完成）|
| `task.all` 形参必须是 `array<future<T>>`（拒绝 `list`/`var`）| 不能把动态收集的 future 喂给 task.all | 固定 16 槽的 `array<future<int>>` 字面量，未用槽走三元短路取 `noop()` |
| array 定长无 append（B2）| 无法按目录数动态扩 future 数组 | 同上：固定 MAX_DIRS=16 |
| **BUG-22（新）**：在 async 函数里读**全局 `list<T>` 元素**会静默崩溃（exit 3）| 不能用 `g_dirs[i]` 全局 list 给 async 任务取目录名 | 目录名存为 16 个独立全局 **string**（`g_dir0..g_dir15`，基本类型在 async 里安全）|
| 容器不能传给自定义函数（B12）| searchDir 不能接收 list | searchDir 只收 `string`/`string` 基本类型参数 |
| 无 stdin（B18）| 不能交互输入 | 全部走 argv |
| **BUG-19**：`xl build` 对所有含 `func main` 的程序链接失败（`xl6_command_argv_to_arg` 未解析）| exe 路径不通 | 随项目附 `psearch_shim.c`（外部链接定义该符号，复刻 codegen 里死代码本应 emit 的实现），`xl build psearch.xl psearch_shim.c` 即可（同 dupfind 范式）|

## 验证

- `xl run` 与 built exe 两条路径输出**集合一致**（命中文件集合 identical，仅并行打印顺序不同）。
- 正确性对照：对 `examples` + `projects/dev` 搜 `func`，psearch 命中的全部 115 个**文本**文件与 PowerShell 独立扫描逐文件比对——**0 遗漏、0 误报**。
- 二进制文件（`.exe`/`.dll` 等）：受 BUG-21（`file.readText` 遇非法 UTF-8 静默截断）影响，仅读到前几个字节，故二进制内容里的命中会被漏掉。这是已知限制，文本文件完全正确。
- 并行 stdout：多个 async 任务并发 `println` 时行序非确定（并行证据）；多次重定向到文件结果稳定可复现（115/115）。

## 限制

- 最多 16 个目录（`MAX_DIRS`，受 task.all 必须吃固定 array 字面量所限）。
- 子串匹配（非正则）。
- 二进制文件内容命中可能漏报（BUG-21）。
