# json.patch: XLang 字典传入 C xvalue 后不能按对象访问

2026-10-25，当前 Windows 发布版（`release/xl.exe` / `xlc.dll` 均为当日构建）
复现。此记录属于语言容器与 C/XRT 值接口的边界，不在普通扩展包中绕过。

## 最小复现

从 `demo6/release` 运行：

```powershell
.\xl.exe run extlibs/json.patch/test/var_bridge_repro.xl
```

文件先断言 `var doc = {"a": 1}` 的 `doc["a"] == 1`，再断言
`json.patch.has(doc, "/a")`。前者通过，后者返回 `false`，最终抛
`AssertionError`。中央 `test/cases/extlib_json_patch.xl` 同样在
`has /name` 失败；默认包门禁的 C 边界、两份 C 范例和 C 单文件入口
先通过，XLang 边界首个 `get(source, "/a")` 报
`ValueError: get target does not exist`。

包内 C 测试新增正常 `xjsonpatch_has_value` / `xjsonpatch_get_value`
正例；当前严格 C 门禁通过。这排除了原生 `_value` 入口对普通
XRT `XVALUE_OBJECT` 的普遍失效。

## 边界分析

当前 `xl cgen` 对语言字典字面量生成 `xl6DictCreateTakeV1` 和
`xl6DictProjectionTakeV1`，传给包的仍是 `xvalue*`。包的 JSON 对象
实现按 XRT 公共合同检查 `xrtValueType(v) == XVALUE_OBJECT`，并通过
`xrtValueObjectGet` 读键。语言字典的投影句柄与普通 XRT 对象不是
同一表示；语言索引可读不意味着第三方 C 函数能按 XRT 对象读取。
这也是其他接收 `var` 容器的 C 扩展库需要联动审计的边界。

## 修复方向与验收

在语言到 C/XRT 的值边界统一定义并实现容器投影/导入合同：传入
`xvalue*` 的公开 C 函数必须能通过 XRT 公共值 API 观察声明支持的
容器；或者明确提供显式、无损且有所有权规则的转换入口。先确认
`array/list/set/dict` 的读、写、嵌套、null、内嵌 NUL 键和错误恢复，
再检查返回值从 C 回到语言的所有权。不要在 `json.patch` 单包中以
JSON 序列化、逐字段复制或改变公开 API 规避。

修复后重跑 `var_bridge_repro.xl`、中央用例和不带 `-SkipAot` 的
`release/extlibs/json.patch/test/check.ps1`，并用新版 Linux 发布物
跑包级门禁。当前整包发布级未签收。
