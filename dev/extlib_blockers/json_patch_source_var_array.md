# json.patch 与当前源码版 array/var ABI 不一致

## 复现

最小输入：[json_patch_source_var_array.xl](json_patch_source_var_array.xl)。

从 `demo6` 目录执行：

```powershell
./release/xl.exe run dev/extlib_blockers/json_patch_source_var_array.xl
$env:XLANG_PACKAGE_PATH = (Resolve-Path release/extlibs).Path
./build/c-owned/extlib-source-20261023184435/public_source.exe dev/extlib_blockers/json_patch_source_var_array.xl run build/c-owned/extlib-json-patch-min-repro
```

旧发布版退出 0；2026-10-23 构建的源码版公开驱动退出 1：

```text
uncaught xlang error: ValueError: apply requires a document and a patch array
```

同一源码驱动生成的独立 EXE 编译成功，运行也报相同错误，故不是只发生在 JIT。
这份驱动是一个有时间戳的源码构建产物；若主线又有修改，请先重建驱动重跑。

## 接口边界

包声明为 `extern "xjsonpatch_apply" func ...(var doc, var patch) as var`。
原生 `xjsonpatch_apply` 在 `json_patch_native.c` 中用
`xrtValueType(patch) == XVALUE_ARRAY` 验证第二参数。
源码版生成 C 对数组字面量先调用 `xl6ArrayProjectionTakeV1`，之后保留同一个
`xvalue*` 传给 `xjsonpatch_apply`。本轮没有修改编译器或包逻辑，
尚需确认投影值的实际 XRT tag 以及公开 `var` ABI 的规范表示。

## 建议修复

先在源码版的包调用边界验证 `xrtValueType(patch)`、投影布局与元素读取，
再确定唯一正规化点：要么在进入传统 `xvalue*` 原生接口前投影为 XRT array，
要么提供明确的序列读取协议并将包统一迁移。不要仅把 `json.patch`
的一次调用特判成数组；同样使用 `var`/数组的扩展库可能有同面风险。
修复后应对本复现、`json.patch` 的 C/XLang 测试和 JIT/AOT 同时回归。
