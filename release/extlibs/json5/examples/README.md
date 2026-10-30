# JSON5 双语范例

所有命令从仓库根目录运行，不需要网络、服务或配置文件。`xl run`
以内存方式运行；要验证独立程序，使用 `xl build ... -o 输出路径`。
包本身的发布阻断以[包 README](../README.md)为准，范例通过不代表
M10 数字消费问题已解决。

## 常规配置

```powershell
./release/xl.exe run release/extlibs/json5/examples/json5.xl
./release/xl.exe run release/extlibs/json5/examples/json5_c.c
```

两者展示 parse、valid、getValue、get，包含中文键、尾随逗号、数组、
整数和标量文本查询；各自断言预期值，输出逐行一致。C 程序的路径
查询返回值需要 `xrtValueRelease`，返回文本需 `xrtFree`；根对象内取出
的字段仅借用，不能独立释放。

## Unicode 转义与注释

```powershell
./release/xl.exe run release/extlibs/json5/examples/unicode_config.xl
./release/xl.exe run release/extlibs/json5/examples/unicode_config_c.c
```

Windows 命令使用 `xl.exe`，Linux 改用 `./release/xl`，其他路径相同。
两种语言提供相同配置，分别校验三个值并输出：

```text
title=hello世界
label=你
port=8080
JSON5 unicode config: OK
```

`unicode_config.expected.txt` 是门禁使用的 UTF-8 黄金输出。范例不创建
任何数据文件，也没有需要关闭的外部资源；C 失败出口统一释放已持有
的 root、label、title，XLang 由正常生命周期释放。

源码刻意明确表示不可见分隔符：XLang 用 `Buffer.fromHex(...).text()`，
C 用 UTF-8 字节转义。这里展示的是被 JSON5 解析的配置语法，不改变
XLang 自己的字符串规则：

- `title` 中反斜线后紧接实际 U+2028（LS）：续行被删除，得到 hello世界。
- `label` 中反斜线后为“你”：identity escape 保留完整字符，不只读取首字节。
- `//` 注释由实际 U+2029（PS）终止，后面的 port 字段不能被吞掉。
- 反斜线后实际 LS/PS 与六字符文本 `\u2028`/`\u2029` 不同，后者是
  Unicode 转义值；未转义 LS/PS 放在 JSON5 字符串内也会保留。

C 范例直接包含包内桥接与解析器，可由 `xl run` 单文件运行。正式包
按清单分开编译；手动编译该单文件范例时不可再链接一份相同桥接或
解析器，否则会重复定义。包门禁分别验证 GCC、TCC JIT 和独立 AOT，
同时检查断言及黄金输出，不只比较两个实现是否输出相同内容。
