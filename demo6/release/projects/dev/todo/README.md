# todo - 待办事项管理器

用 xlang `class + derive json` 实现的命令行待办事项工具，数据持久化到
`todos.json`（与脚本/exe 同目录）。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\todo\todo.xl -- add "买牛奶"
xl run projects\dev\todo\todo.xl -- add "写代码"
xl run projects\dev\todo\todo.xl -- list
xl run projects\dev\todo\todo.xl -- done 1
xl run projects\dev\todo\todo.xl -- remove 2
xl run projects\dev\todo\todo.xl -- clear
```

build 成 exe：

```bat
xl build projects\dev\todo\todo.xl -o projects\dev\todo\todo.exe
projects\dev\todo\todo.exe add "buy milk"
projects\dev\todo\todo.exe list
```

## 命令

| 命令              | 作用                  |
| ----------------- | --------------------- |
| `add "<text>"`    | 添加一条待办          |
| `list`            | 列出全部（带编号/状态）|
| `done <id>`       | 标记指定 id 完成       |
| `remove <id>`     | 删除指定 id            |
| `clear`           | 清空所有已完成项       |

输出示例：

```
[1] [ ] 买牛奶
[2] [x] 写代码
Total: 2
```

## 设计

- `TodoItem` 用 `class + derive value + derive json` 定义 `id/text/done/createdAt`。
- 单项序列化走 `json.stringify(item)`，反序列化走 `json.parse::<TodoItem>(text)`。
- 集合持久化以"JSON 对象数组文本"形式存盘（`[{...},{...}]`），见
  `BUGS.md` BUG-002 中关于 `derive json` 不序列化容器内 class 实例的说明及绕过方式。

## 已知限制

见 `../BUGS.md`：
- BUG-002：derive json 不序列化容器内 class 实例（本工具的主要绕过点）。
- BUG-006：build 出的 exe 接收非 ASCII argv 会乱码，含中文时请用 `xl run`。
