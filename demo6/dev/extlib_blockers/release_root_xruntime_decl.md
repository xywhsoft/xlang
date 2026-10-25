# 发布版从 demo6 根目录运行缺少运行时声明头（2026-10-25）

## 状态：已复验解除

2026-10-25 重建并更新主线的 Windows 标准发布物后，同一
`extlib_roaring.xl` 从 `demo6` 和 `demo6/release` 两个工作目录运行均
退出 0。验收结果保存在
`build/c-owned/foreign-bridge-main-acceptance-1/roaring-cwd-results.json`。
该记录不再作为当前扩展库迁移阻断；以下保留旧发布物的复现信息。

## 复现

当前 Windows 发布物中，`release/include/xruntime_decl.h` 文件存在。
在 `demo6` 目录运行：

```text
release\xl.exe run test\cases\extlib_roaring.xl
```

TCC 报：

```text
include/__xl_runtime/xl6_tls_server_request.h:3: error: include file 'include/xruntime_decl.h' not found
```

切换到 `demo6/release` 目录后运行同一用例（路径改为
`..\test\cases\extlib_roaring.xl`）可通过。`roaring` 的 C 测试与
独立 AOT 也通过。

## 判断与修复建议

这是发布版运行时头文件的查找根目录/VFS 入口问题，不是扩展库源码
缺文件。应保证工作目录无关：优先使用发布物旁的外部头文件，缺失时
回退静态 VFS，并按同一逻辑解析嵌套的 `include/` 头文件。
修复后分别从 `demo6` 和 `demo6/release` 运行相同用例，并在包级
`test/check.ps1` 中保留从 `demo6` 根目录调用的门禁。

本普通扩展库任务不修改 xlang 主线。
