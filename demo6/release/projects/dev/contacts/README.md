# contacts - 通讯录管理器

用 xlang `class + derive json` 实现的命令行通讯录 CRUD 工具，数据持久化到
`contacts.json`（与脚本/exe 同目录）。

本工具同时展示 `derive json` 对**容器内 class 实例**的直接序列化与强类型还原。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\contacts\contacts.xl -- add "张三" "13800138000" "zhangsan@test.com"
xl run projects\dev\contacts\contacts.xl -- add "李四" "13900139000" "lisi@test.com"
xl run projects\dev\contacts\contacts.xl -- list
xl run projects\dev\contacts\contacts.xl -- search "张"
xl run projects\dev\contacts\contacts.xl -- update 1 --name "赵六" --phone "13911112222"
xl run projects\dev\contacts\contacts.xl -- delete 2
xl run projects\dev\contacts\contacts.xl -- test
```

build 成 exe：

```bat
xl build projects\dev\contacts\contacts.xl -o projects\dev\contacts\contacts.exe
projects\dev\contacts\contacts.exe list
```

## 命令

| 命令 | 作用 |
| --- | --- |
| `add "<name>" "<phone>" "<email>"` | 添加联系人（id 自增） |
| `list` | 列出全部 |
| `search "<query>"` | 按姓名/电话/邮箱子串搜索 |
| `delete <id>` | 按 id 删除 |
| `update <id> --name "<n>" --phone "<p>" --email "<e>"` | 更新（各 flag 可选，未传字段保留原值） |
| `test` | 在随机临时目录中验证新增、读取、部分更新、清空字段和删除 |

## 设计

- `Contact` 用 `class + derive value + derive json` 定义 `id/name/phone/email`。
- **集合直接用 `array<Contact>` + `json.stringify(items, pretty = true)` 整体落盘**
  ：
  ```xlang
  array<Contact> items = loadContacts(path_)
  items.append(c)
  string json = json.stringify(items, pretty = true)
  file.writeText(path, json)
  ```
  序列化结果完整保留每个 Contact 的全部字段。
- 读取用 `json.parse(text)` 解析成 var 数组，逐项 `json.parse::<Contact>(json.stringify(elem))`
  还原为强类型实例。
- `array<Contact>` 作为函数参数和返回值正常传递（`loadContacts`/`saveContacts`/
  `nextId`/`findById` 全部收发 typed 容器）。
- `array<Contact>.append(c)` / `.remove(idx)` 正常工作。
- pretty 输出默认 TAB 缩进，落盘前 `string.replace(jsonText, "\t", "  ")` 统一成 2 空格。

## 持久化结论

`json.stringify(array<Contact>, pretty=true)` 正确输出
`[{"id":1,"name":"张三","phone":"...","email":"..."}, ...]`，每个容器内 class
实例的全部字段都被序列化；`json.parse` → `json.parse::<Contact>` round-trip
还原完整，跨进程调用数据持久化正确。无需任何手工 JSON 拼接。

## 边界

- pretty JSON 的字段顺序不作为数据契约；读回按字段名匹配。
- `update` 用独立的 `hasXxx` 状态区分“未传”与“显式空串”，因此
  `--email ""` 可以清空邮箱。
