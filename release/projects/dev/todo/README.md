# todo - 待办事项管理器

用 xlang `class + derive json` 实现的命令行待办事项工具，数据持久化到
`todos.json`（与脚本/exe 同目录）。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\todo\todo.xl -- add "买牛奶"
xl run projects\dev\todo\todo.xl -- add "写代码"
xl run projects\dev\todo\todo.xl -- list
xl run projects\dev\todo\todo.xl -- done 1
xl run projects\dev\todo\todo.xl -- remove 2
xl run projects\dev\todo\todo.xl -- clear
xl run projects\dev\todo\todo.xl -- test
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
| `test`            | 在随机临时目录运行完整持久化自测 |

输出示例：

```
[1] [ ] 买牛奶
[2] [x] 写代码
Total: 2
```

## 设计

- `TodoItem` 用 `class + derive value + derive json` 定义 `id/text/done/createdAt`。
- 集合直接通过 `json.stringify(array<TodoItem>, pretty = true)` 序列化，读回时
  用 `json.parse::<TodoItem>` 恢复强类型对象。
- `array<TodoItem>` 可直接作为参数和返回值使用；class 引用在容器中保持正常生命周期。

## 数据边界

- 数据文件使用 UTF-8 JSON；`xl run` 与 build 产物均支持中文参数和中文路径。
- `clear` 只删除已完成项；未完成项和原 id 保持不变。
