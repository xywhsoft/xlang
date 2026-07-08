# config - JSON 配置文件管理器

用 `var` dict 承载配置数据、`json.parse`/`json.stringify` 做 JSON 持久化的
命令行配置管理器。配置存放在脚本/exe 同目录下的 `app.json`。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\config\config.xl -- set name "xlang"
xl run projects\dev\config\config.xl -- set version 6
xl run projects\dev\config\config.xl -- set stable true
xl run projects\dev\config\config.xl -- set ratio 0.95
xl run projects\dev\config\config.xl -- list
xl run projects\dev\config\config.xl -- get name
xl run projects\dev\config\config.xl -- delete version
xl run projects\dev\config\config.xl -- export out.json
xl run projects\dev\config\config.xl -- import in.json
xl run projects\dev\config\config.xl -- selftest
```

## 命令

| 命令                  | 作用                                            |
| --------------------- | ----------------------------------------------- |
| `get <key>`           | 读取配置值；不存在打印错误并返回 1              |
| `set <key> <value>`   | 写入配置值；自动识别 string/int/bool/float 类型  |
| `list`                | 列出所有键值对                                  |
| `delete <key>`        | 删除配置项；不存在打印错误并返回 1              |
| `import <json_file>`  | 用一个 JSON 对象文件整体替换当前配置（校验合法性）|
| `export <json_file>`  | 把当前配置 pretty 输出到 JSON 文件              |
| `selftest`            | B1 探针：验证 derive json 单实例 / 容器内实例状态 |

值类型识别规则：`true`/`false` → bool；纯数字（可带前导 `-`）→ int；含 `.` 的数字 → float；其余 → string。

## 设计

- 主路径用 `var` dict 承载配置数据（比 `class derive json` 灵活，避开 **B1**）。
- 读：`file.readText` + `json.parse` → `var` dict。
- 写：`json.stringify(data)` 得到紧凑文本，再用**文本级** `prettyCompact` 规范化为
  2 空格缩进的 pretty 输出（绕过 **B10** 的 TAB 缩进，也避免二次 `json.parse` 重排键序）。
- **delete**：dict 无法删除 key（**B4**），重建一个新 `var` dict，遍历原 dict 只复制
  非目标 key（`for k, v in data ... fresh[k] = v`），再落盘。
- 键序：`json.parse` 不保留插入序（**B17/B20**），`list` 与落盘均按 hash 序输出，
  顺序稳定但与写入序无关——这是已知限制，数据值本身正确。

### `selftest` 探测结果（B1 状态确认）

`class ConfigItem derive json`（typed 字段 `string key`/`string value`）：

| 场景 | 结果 |
| --- | --- |
| A. 单实例 `json.stringify(item)` + `json.parse::<ConfigItem>` | **正常**（`{"value":"xlang","key":"name"}`，回读字段正确）|
| B. 实例进 `var` 数组字面量 `[a,b]` 后 stringify | **B1 仍存在**：输出 `[]`（容器内实例整体丢失）|
| C. 实例作 `var` dict 值 `bag["a"]=a` 后 stringify | **B1 仍存在**：输出 `{}`（容器内实例整体丢失）|
| D. 逐项 `json.stringify(a)+","+json.stringify(b)` 手拼 | **正常**（B1 官方绕过方式）|
| E. 纯 `var` dict（string/int/bool/float）round-trip | **正常**，四种值类型全部正确保留 |

**结论**：**B1（`derive json` 不序列化容器内 class 实例）在 demo6 当前 release 仍然存在**，
且比预期更严重——容器内实例不仅字段丢失，连占位 `{}` 都没有，整个数组/对象塌缩为空 `[]`/`{}`。
单实例 derive json 与纯 var dict 路径均完全正常，故本工具主路径选 var dict。

## 新发现：`func f() as var` codegen 装箱缺陷（float/动态返回）

`func parseValue(string s) as var` 内 `return float(s)` 触发 TCC 编译期错误
`cannot convert 'double' to 'struct xvalue_struct *'`（int/bool/string 的 return
路径同样属于 var 装箱路径，float 最先暴露）。即：**返回类型声明为 `var` 的函数，
其函数体里的 return 语句不会生成把具体类型值装箱为 xvalue 的代码**。
绕过：改为 `func encodeLiteral(string s) as string` 返回 JSON 字面量文本
（bool/int/float 原样、string 用手写 `jsonQuoteString` 转义加引号），调用方再
`json.parse(literal)` 一次拿到带类型的 var 值。详见 `../BUGS.md` BUG-23。

## 新发现：直接把字面量赋给 var 位置触发同一装箱缺陷

`ConfigItem item; item.value = 2.5`（float 字面量赋给 `var` 字段）以及
`d["ratio"] = 0.95`（float 字面量赋给 var dict 值）同样在 TCC 编译期报
`cannot convert 'double' to 'struct xvalue_struct *'`。**string/int 字面量到 var
位置的赋值未复现该问题（编译通过）**，但目前无法稳定判定哪些字面量类型安全，
统一建议：**需要把带类型字面量放进 var 容器/字段时，走 `json.parse('...')` 拿到 var 值再赋**。
详见 `../BUGS.md` BUG-23。

## build 路径（BUG-19 绕过：companion .c 提供 argv runtime 符号）

`xl build` 对所有用到 `arg` 的程序都失败（**BUG-19**，已由 `radix` 工具记录）：
生成代码里 `main(...)` 调用 `xl6_command_argv_to_arg(argc, argv)`，但
`src/cgen/xl6_cgen_main.c` 中负责 emit 该 static 定义的 `xl6CgenEmitArgvArrayHelper`
**是死代码——定义了却从未被 codegen 管线调用**，导致独立 exe 链接时该符号未解析；
`xrt.o`/`xl.def`/`xlc.def` 均不导出它（`xl run` 靠 xl.exe 进程内符号救场，故 run 正常）。

本环境只命中 BUG-19 的 **argv 链接症状**（未命中其 xrt.h 编译症状），故可用与
`dupfind` 工具（`../dupfind/dupfind_shim.c`，见 `../BUGS.md` NOTE-21）**完全相同的
companion-`.c` shim 绕过**（不修改编译器源码）：`argv_runtime.c` 用 XRT C API
（`xvoCreateArray`/`xvoCreateText`/`xvoArrayAppendValue`/`xvoUnref`/`xrtMalloc`/`xrtFree`，
声明见 `release/include/xl_runtime.h`）提供 `xl6_command_argv_to_arg` 的外部链接定义，
逐字镜像 cgen 本应 emit 的死代码实现（含 Windows `CommandLineToArgvW` UTF-8 路径，
顺带让 built exe 支持 UTF-8 argv，缓解 B5）。

> 注：`xl run <file.xl>` **不会**自动收集同目录 `*.c`（已用 broken-`.c` canary
> 确证——同目录放一个语法非法的 `.c` 不影响 `xl run`；dupfind 也是把 shim 放在
> `.xl` 同目录）。shim 只在 `xl build` 显式列为参数时才参与编译。本工具曾误判为
> "xl run 自动收集"，实际是前一次失败 build 留下的 tmp 缓存污染所致，清缓存后消失。

build 命令（在 `release\` 下）：

```bat
xl build projects\dev\config\config.xl ^
    projects\dev\config\argv_runtime.c ^
    -o projects\dev\config\config.exe ^
    --link ..\..\build\tmp\xrt.o ^
    --link ..\..\res\tcc\win64\x86_64\lib\libtcc1.a ^
    --link Ws2_32 --link IPHLPAPI --link Shell32 ^
    --lib-path ..\..\res\tcc\win64\x86_64\lib
```

（`--link` 路径指向 demo6 根下的 `build\tmp\xrt.o` 与 TCC 的 `libtcc1.a`；
 这些是 toolchain env `XL6_XL_DEFAULT_LINK_ARGS` 要求的标准链接项。）

built exe 经全命令验证与 `xl run` 输出一致，且退出码透传正确（不受 **B9** 影响）。

## 已知限制

见 `../BUGS.md` 与 `../README.md`：
- **B1**：`derive json` 不序列化容器内 class 实例（本工具主路径用 var dict 绕过；selftest 复现确认仍存在）。
- **B4**：dict 无法删除 key（delete 重建新 dict 绕过）。
- **B10**：`json.stringify(pretty=true)` 用 TAB（自写文本级 prettyCompact 绕过）。
- **B17/B20**：`json.parse` 不保留键序（list/落盘按 hash 序，无法绕过）。
- **BUG-23**（本工具发现）：`func f() as var` 的 return + 字面量赋 var 位置存在 codegen 装箱缺陷。
- **BUG-19**：`xl build` 缺 argv runtime 符号（用 companion `.c` shim 绕过，与 dupfind 同范式，见 NOTE-21）。
