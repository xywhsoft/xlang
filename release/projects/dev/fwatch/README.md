# fwatch - 文件监听器（轮询检测文件变化）

轮询扫描目录，对比前后两次快照，检测文件的新增 / 删除 / 修改 / 重命名。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\fwatch\fwatch.xl -- projects\dev [interval_ms] [max_scans]
xl build projects\dev\fwatch\fwatch.xl -o projects\dev\fwatch\fwatch.exe
fwatch.exe [dir] [interval_ms=1000] [max_scans=10]
```

参数：
- `dir`         监听目录（默认当前目录）
- `interval_ms` 轮询间隔毫秒（默认 1000）
- `max_scans`   扫描多少轮后退出（默认 10，Ctrl+C 可随时终止）

## 检测的事件

- `[ADDED]`     上次没有、本次出现的文件
- `[REMOVED]`   上次有、本次消失的文件
- `[MODIFIED]`  两次都在，但 size 或修改时间变化（输出旧 → 新字节数）
- `[RENAMED]`   尽力而为：同一轮里「一个 REMOVED + 一个 ADDED 且 size 相同」配对视为重命名（目录 API 不提供 inode，只能靠 size 配对）

## 输出示例

```
Watching: projects\dev (every 1000ms)
Scan 1: 15 files tracked
Scan 2: no changes
Scan 3: [MODIFIED] projects\dev\test\log.txt (1024 → 2048 bytes)
Scan 4: [ADDED] projects\dev\test\new.txt
Scan 5: [REMOVED] projects\dev\test\old.txt
Done after 10 scans.
```

## 设计要点

- `dir.files(root, false)` 取顶层文件路径列表（非递归）。
- `file.info(path)` 取每个文件的 `.size` 与 `.changeTime`，`time.toUnix()` 压成 int 便于 `==` 比较。
- 快照封装在 `Snap` 类（三条并行数组：path / size / mtime），`find` 线性查找。
- diff：遍历 curr 找 ADDED + MODIFIED，遍历 prev 找 REMOVED；再用 size 配对挑出 RENAMED。
- `time.sleep(interval_ms)` 控制轮询节奏（毫秒）。

## 已知 xlang 限制（见 dev/BUGS.md BUG-F1）

class 方法体内访问容器字段必须显式写 `self.<field>`：裸字段名当方法/索引接收方
（`paths.append`、`paths[i]`、`paths.length`）或赋值目标会在 `xl run`/`xl build`
阶段失败，而 `xl check` 却报 ok。本工具的 `Snap` 类全程使用 `self.paths.xxx`。

## 验证

- `xl run` EXIT=0；`xl build` EXIT=0，生成的 exe 运行 EXIT=0。
- 实时变更测试（500ms × 8 轮，过程中依次新增/修改/重命名/删除文件）四类事件全部正确识别，
  UTF-8 箭头 `→` 字节正确输出。
