# psearch - 并行文件内容搜索

`psearch` 使用 `async`、`future<T>` 与 `task.all` 并行扫描多个目录，搜索正文中包含指定文本的文件。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\psearch\psearch.xl -- "func" examples projects\dev
xl build projects\dev\psearch\psearch.xl -o projects\dev\psearch\psearch.exe
projects\dev\psearch\psearch.exe "func" examples projects\dev
```

搜索词按普通 UTF-8 文本子串匹配，不作为正则表达式。每个目录会创建一个异步任务，读取失败的单个文件会被跳过，不影响其他任务。

## 核心写法

```xlang
async func runAll(array<string> directories, string pattern) as int
    array<future<int>> jobs = array[]
    for directory in directories
        jobs.append(searchDir(directory, pattern))
    end

    array<int> counts = await task.all(jobs)
    // 汇总每个目录的命中数
end
```

这个实现刻意使用局部的 typed container 传递参数和收集任务，不依赖全局状态、固定任务槽或额外 C shim。目录数量仅受可用资源限制。

## 行为边界

- 递归枚举普通文件，并用 `file.readText` 读取文本。
- 无法解码或无法访问的文件会被跳过。
- 多个任务并发输出时，命中路径的打印顺序不固定；最终命中总数稳定。
- `main` 通过 `future.wait()` 驱动异步入口完成，失败时输出 future 错误并返回非零退出码。
