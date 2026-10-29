# kvdb - 简易 KV 数据库

用 xlang 编写的命令行 KV 数据库（简化版 Redis/LevelDB），验证 `dict` + `json` 持久化能力。

## 用法

```
kvdb set <key> <value>      设置键值（值自动识别 bool/int/float/string）
kvdb get <key>              获取值（不存在输出 (nil)）
kvdb delete <key>           删除键
kvdb list                   列出所有键
kvdb count                  键数量
kvdb clear                  清空
kvdb keys <pattern>         按通配符筛选键（如 user:*，语法同 string.like）
kvdb export <file>          导出为 pretty JSON（2 空格缩进）
kvdb import <file>          从 JSON 导入（整体替换）
kvdb incr <key>             整数自增（键不存在则从 0 开始）
kvdb search <text>          在所有值中搜索文本
```

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\kvdb\kvdb.xl -- set count 5
xl run projects\dev\kvdb\kvdb.xl -- get count
xl build projects\dev\kvdb\kvdb.xl -o projects\dev\kvdb\kvdb.exe
projects\dev\kvdb\kvdb.exe get count
```

> 注意：`xl run` 向脚本透传参数必须用 `--` 分隔（`xl run <file> -- <args...>`），
> 否则尾随参数会被 `xl` 当作自身选项（如误把 `set` 当成 `--link set`）。

## 设计

- 存储用 `var` dict，值支持 `string`/`int`/`bool`/`float` 四种基础类型。
- 落盘 `kvdb.json` 用 `json.stringify`（compact 模式），读取用 `json.parse`。
- 数据文件路径取自 `arg[0]`（脚本或 exe）所在目录，因此 `xl run` 与 build 后的 exe
  **共享同一份 `kvdb.json`**，天然验证跨调用持久化。
- `set` 的值自动识别类型：`true`/`false`→bool，纯数字→int，含 `.`/`e`→float，其余→string。
- `export` 用 `pretty=true` 并把默认 TAB 缩进替换为 2 空格，便于人读。
- `import`/`export` 的目标文件路径按用户输入解析（相对当前工作目录）。

## 验证

`xl run` 与 `xl build` 两条路径均 EXIT=0；set/get/incr/delete/import/export/count/list/keys/search
全部功能正确；数据在独立的进程调用之间正确持久化（见同级 `kvdb.json`）。

## 已知限制 / 发现的 BUG

- **键序不保插入序**：`var` dict 的迭代顺序与插入顺序不一致（对常见键表现为**逆序**），
  `json.parse` 也会把 JSON 文档键序反过来。因此 `list`/`keys`/`export` 的输出顺序
  不等于写入顺序。这与 dev/README 中 "json.parse compact 模式保留键序 ✅" 的描述不符，
  详见 `../BUGS.md` BUG-K1。功能正确性不受影响（dict 本身无序），仅影响展示顺序。
