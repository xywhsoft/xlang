# TOML / XLang dict 到 C var 的投影阻断

2026-10-24，更新后的 Windows 发布版 `xl.exe`。

在 `demo6` 目录运行：

```powershell
./release/xl.exe run release/extlibs/toml/test/root_argument_repro.xl
```

预期：两个 `stringify` 都接受语言层的字典。实际：`toml.parse()` 的
结果可写出；XLang 字面量赋给 `var` 后，第二次写出报
`TypeError: TOML document root must be a dict`。默认包门禁
`release/extlibs/toml/test/check.ps1` 在 XLang 边界阶段同样失败；
严格 C 语法和 C 边界先通过。

生成代码对 XLang 字典调用 `xl6DictCreateTakeV1` 和
`xl6DictProjectionTakeV1`，其结果是运行时受控句柄；TOML 的
`xtoml_stringify_text(xvalue*, ...)` 最终调用 `xrtTomlStringify`，
要求根值的 XRT tag 为 `XVALUE_OBJECT`。这是语言容器传入 C/XRT
`var` 参数时缺少明确投影的问题。不能在 TOML 包里解读私有句柄，
也不能仅把错误改名后视为通过。

建议在主线统一定义静态 `dict` 到 C/XRT `var` 的值投影及所有权，
同时验证数组、集合和嵌套容器，而非为 TOML 单独开兼容路径。
修复后重跑最小复现、Windows/Linux 包门禁及 AOT。
