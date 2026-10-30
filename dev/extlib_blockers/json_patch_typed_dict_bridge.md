> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# json.patch: XLang 字典传入 C xvalue 后不能按对象访问

## 2026-10-30 根目录布局复验

默认Windows/Linux包门禁分别前37/38阶段满足预期，包含原C、新move
所有权/原子性/OOM、八条实际原型、新双语与五个独立程序，最后原中央
字面量仍报AssertionError: has /name。最小源从仓库根目录运行
`release/xl.exe run release/extlibs/json.patch/test/var_bridge_repro.xl`；
Linux用release/xl。两平台check/build成功，JIT与独立程序均断言失败
退出1，说明构建成功不等于容器投影闭环。

新relocate范例从标准json.parse读取已有JSON文本，两平台可用；它没有
先序列化语言容器，也没有新增原生工厂，不能据此关闭字面量的M1。原
中央/边界/分配/旧双语及AOT仍为必过项，未改为expected-failure。当前
最终报告及旧/新C独立参照证据见验收矩阵JSON Patch行与任务台账。

2026-10-25，当前 Windows 发布版（`release/xl.exe` / `xlc.dll` 均为当日构建）
复现。此记录属于语言容器与 C/XRT 值接口的边界，不在普通扩展包中绕过。

## 最小复现

从 `release` 运行：

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

## 2026-10-26 当前发布版复验

Windows `release/xl.exe` 与 Linux `release/xl` 从 `release` 目录运行
`extlibs/json.patch/test/var_bridge_repro.xl` 均以断言失败退出 1。
两平台运行中央 `test/cases/extlib_json_patch.xl` 均在首条
`json.patch.has(doc, "/name")` 报 `AssertionError: has /name`；
Linux 包门禁先通过 C ASan/UBSan 边界和两份 C 范例，在 XLang JIT
阶段同样失败，未执行后续 AOT。包内没有加入句柄探测或序列化绕行；
主线修复容器投影后须重新运行完整双平台门禁。
