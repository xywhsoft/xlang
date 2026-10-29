# config - JSON 配置文件管理器

一个小型命令行配置工具，展示动态字典、JSON、文件与路径标准库的组合使用。配置保存在程序所在目录的 `app.json`。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\config\config.xl -- set name xlang
xl run projects\dev\config\config.xl -- set version 6
xl run projects\dev\config\config.xl -- set stable true
xl run projects\dev\config\config.xl -- set options "{\"color\":true}"
xl run projects\dev\config\config.xl -- list
xl run projects\dev\config\config.xl -- get name
xl run projects\dev\config\config.xl -- delete version
xl run projects\dev\config\config.xl -- export out.json
xl run projects\dev\config\config.xl -- import in.json
xl run projects\dev\config\config.xl -- selftest
```

| 命令 | 作用 |
|---|---|
| `get <key>` | 读取配置值 |
| `set <key> <value>` | 写入配置值；合法 JSON 保留类型，其他文本保存为字符串 |
| `list` | 列出所有键值对 |
| `delete <key>` | 删除配置项 |
| `import <json_file>` | 用 JSON 对象替换当前配置 |
| `export <json_file>` | pretty 输出当前配置 |
| `selftest` | 验证类型解析、JSON round-trip 与字典删除 |

## 设计要点

- `var` 字典自然承载 JSON 的 null、bool、数字、字符串、数组和对象。
- `json.valid` 与 `json.parse` 负责值类型识别，不重复实现数字解析器。
- `dict.remove` 原地删除配置项，不复制整个字典。
- `json.stringify(data, pretty = true)` 负责格式化，避免手写文本处理破坏字符串内容。
- 写入失败通过返回码向调用方传播。

## 构建与验证

```bat
xl build projects\dev\config\config.xl -o projects\dev\config\config.exe
projects\dev\config\config.exe selftest
```

自测成功时输出 `config selftest ok` 并返回 0。
