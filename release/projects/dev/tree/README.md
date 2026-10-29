# tree - 目录树打印工具

类似 Windows `tree` / Linux `tree`，用 xlang 打印目录树形结构。

## 用法

```
tree [path] [-d]
```

- `path`  省略时默认 `.`（当前目录）。
- `-d`    只显示目录，隐藏文件。
- `-h`    简短帮助。

## 输出

- 目录优先、同类按名称大小写不敏感升序。
- 文件名后跟 `(size)`，用 `file.info(path).size` 取值，自动格式化为 `B / KB / MB`。
- 树形分支字符：`├── └── │   `（UTF-8），通过递归函数 `walk` 用 `prefix` 字符串传递缩进上下文。

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\tree\tree.xl -- examples
xl run projects\dev\tree\tree.xl -- . -d
xl build projects\dev\tree\tree.xl -o projects\dev\tree\tree.exe
projects\dev\tree\tree.exe examples -d
```

## 实现要点

- `dir.entries(path, false)` 非递归取一层 var 条目，字段 `name / isDir / size / path`。
- `sortEntries` 在原数组上做选择排序：目录优先，同类按 `string.lower(name)` 比较。
- `walk(basePath, prefix, dirsOnly)` 递归，先数可见条目数以判定 `isLast`，再据此选 `└──`/`├──` 与子层 `    `/`│   `。

## 已知限制

- 见 `../BUGS.md` **BUG-003**：`xl build` 产物的 argv 不支持 UTF-8，中文路径作命令行参数时 `built exe` 找不到路径（`xl run` 正常）。ASCII 路径参数 + 程序内部 `dir.entries` 发现中文文件名则完全正常。
