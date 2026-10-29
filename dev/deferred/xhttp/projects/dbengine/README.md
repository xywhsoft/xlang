# dbengine - 内存 KV 数据库引擎

用 xlang 编写的内存 KV 数据库引擎，验证**复杂数据结构（嵌套 dict/list）、字段倒排索引、TTL、事务快照、JSON 持久化、HTTP API** 等综合能力。

## 用法

```
dbengine                运行内置自测（75 项，必须全部通过）
dbengine run            执行内置示例脚本（完整演示）
dbengine run <file>     执行 .dbs 脚本文件（每行一条命令）
dbengine serve [port]   HTTP API 模式（默认端口 18200）
```

## 查询语言（大小写不敏感）

| 命令 | 说明 |
|---|---|
| `GET key` | 取值（不存在返回 `(nil)`） |
| `SET key value` | 存值（value 自动识别类型） |
| `DELETE key` | 删除键（返回 `1`/`0`） |
| `EXISTS key` | 键是否存在（返回 `1`/`0`） |
| `KEYS pattern` | 通配符匹配键（`string.like` 语法：`*` `?`） |
| `FIND field=value` | 按字段索引查找键集合 |
| `COUNT` | 键总数 |
| `EXPIRE key seconds` | 设置 TTL（秒） |
| `TTL key` | 剩余秒数（`-1` 无 TTL，`-2` 不存在/已过期） |
| `INCR key` | 整数自增（不存在从 0 开始） |
| `TYPE key` | 值类型（`string/int/float/bool/list/dict/none`） |
| `BEGIN` / `COMMIT` / `ROLLBACK` | 事务（快照深拷贝） |
| `SAVE file` / `LOAD file` | JSON 持久化 |
| `CLEAR` | 清空全部 |

### value 类型识别（SET 时）

- `{...}` / `[...]` → JSON 解析为 dict / list
- `"..."` → 引号字符串（支持 `\"` `\\` `\n` `\t` `\r` 转义）
- `true` / `false` / `null` → 布尔 / null
- 纯数字 → int；含 `.` 或 `e/E` → float；其它 → string

## HTTP serve 模式

- `GET /?line=COMMAND` 或 `POST /`（body 为命令文本）执行单条命令。
- 返回 `{"ok":true,"result":"..."}`。
- 状态经**磁盘文件**（`dbengine.serve.json`）跨越 native callback 边界，
  每个请求内 `load → execute → store`，复用同一套引擎逻辑。

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\dbengine\dbengine.xl
xl run projects\dev\dbengine\dbengine.xl -- run projects\dev\dbengine\script.dbs
xl build projects\dev\dbengine\dbengine.xl -o projects\dev\dbengine\dbengine.exe
projects\dev\dbengine\dbengine.exe
```

> `xl run` 向脚本透传参数必须用 `--` 分隔（`xl run <file> -- <args...>`）。

## 设计

- **存储**：文件作用域 `var gStore`（key→value），值支持 `string/int/float/bool/list/dict`。
- **TTL**：`var gTtl`（key→过期 unix 秒，`time.toUnix(time.now())`）；每条命令前惰性清理过期键。
- **索引**：`var gIndex` 为字段倒排索引，结构 `field → valueStr → { key → true }`
  （键集合用 dict 当 set 用）。SET dict 值时自动建标量字段索引；DELETE/覆盖/ROLLBACK 自动维护。
  > 注：键集合未用 typed `set<string>`，而是用嵌套 `var` dict 模拟——因为
  > `set<string>` 作为函数参数按值拷贝、修改不回传（见 `../BUGS.md` BUG-XL1），
  > 而全局可变的嵌套索引需要可靠的就地增删，`var` dict 嵌套下标赋值/`.remove()` 均验证可靠。
- **事务**：`BEGIN` 用 `json.parse(json.stringify(data))` 深拷贝 store+ttl 为快照；
  `ROLLBACK` 还原快照并 `reindexAll()` 重建索引。
- **持久化**：`SAVE` 把 `{store, ttl}` 序列化为 compact JSON 落盘；`LOAD` 还原并重建索引。

## 验证

`xl run` 与 `xl build` 两条路径均 EXIT=0；内置自测 **75/75 全部通过**，覆盖：
基础类型读写、字段索引（建立/覆盖/删除/重建/多命中）、KEYS 通配、COUNT、INCR、
TTL（设置/查看/惰性过期/SET 清除）、事务（BEGIN/COMMIT/ROLLBACK/无事务保护/回滚后索引一致）、
SAVE/LOAD（含 LOAD 后索引重建）、HTTP serve（POST SET / GET 查询 / 跨请求计数）。

## 发现的 xlang 手感问题

开发中触发的已记录 BUG（本工具已绕过，未发现新 BUG）：

- **BUG-XB1**（文件作用域 var 容器在 HTTP 回调里元素引用丢失）：serve 模式用磁盘文件
  跨越回调边界完全绕开——回调内每次 `json.parse` 出全新的局部/全局容器并立即使用，
  不依赖跨回调的内存容器共享。自测第 8 组真实起 HTTP server 验证通过。
- **BUG-XL1**（typed 容器按值传参不回传）：索引用嵌套 `var` dict 而非 `set<string>`。
- **BUG-S1**（`splitLines` 尾部空串）：脚本执行时删掉末尾空串。
- **多返回值签名必须加括号**：`func f(...) as (string, string)`，不能写成 `as string, string`
  （后者触发 `[parse] 语法错误`，非 BUG，是语法要求，但文档速查里没明说）。
