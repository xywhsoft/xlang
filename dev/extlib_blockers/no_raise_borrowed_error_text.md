# 借用字符串 getter 接收时丢失待查询错误（M12）

## 无包最小复现

```powershell
release/xl.exe run dev/extlib_blockers/no_raise_borrowed_error_text.xl
release/xl.exe build dev/extlib_blockers/no_raise_borrowed_error_text.xl -o build/test/error-probe.exe
build/test/error-probe.exe
```

同名 C 伴随源只调用现有 XRT：设置 `XERR_IO` 和已知消息，随后通过
`const char*` getter 借用 `xrtErrorMessage(xrtGetError())`。纯 C 对照在
同一个 getter 上比较完整消息，成功后清错。语言端随后重新设错，
通过 `extern @no_raise ... as string` 调用 getter，要求收到同一完整消息。
这不是坏指针、包导入或自制的替代运行时。

2026-10-30 固定 Windows 发布物：纯 C 对照成功；JIT 和 AOT 均输出
`observed:` 后无正文，断言失败、退出 1。报告为
`build/test/zip_commit_audit_1793364258/no_raise.windows.json`。

同日 Linux 原生快照也在纯 C 对照通过后，JIT/AOT 都得到空消息并以
同一断言退出 1；见 `/var/tmp/xlang-zip-commit-audit-vTSMUrPs` 的
`no_raise_borrowed_error_text.jit.txt` / `.aot.txt`。ZIP 的两种实际
公开错误同样是描述 0/2，默认门禁分别保留 JIT 与 AOT 正例要求。

## ZIP 实际影响

`zip_native.c` 的 `xzip_last_error()` 也是上述 getter。包的错误包装先
调用布尔 `xzip_last_error_is_memory()`，再复制消息，所以错误类别可能
正确，消息却为空。Linux 已有链接的目标提取被拒绝且未向链接外写入，
但 `symlink_boundary.xl` 第一项的精确消息断言失败；不能误报为已证实
发生越界写入，也不能删除这条断言来制造通过记录。余下链接场景是否
通过仍需修复该阻断后完整重跑。

## 定位及主线验收建议

实际 cgen 的布尔 no_raise 查询直接调用 C；字符串返回却进入
`xl6ForeignCallBegin`，随后才调用 `xzip_last_error` 并复制返回值。
因此 C getter 看到的是新作用域中的空错误，而非调用前的待查询错误。
固定生成现场见该轮 `boundaries.generated.c` 中 `xlir_fn_6`。

借用返回复制本身可能 OOM，保留可失败的文本接收是正确的；但应区分
“C 被调函数是否自动抛错”和“接收复制是否可能失败”，不能仅为了
准备接收就改变 no_raise getter 能观察到的错误。主线须同时验证：

- 原错误的类别、消息、域、代码在 C getter 内仍可观察。
- 接收结果独立有效；原错误释放后不能留下悬垂借用。
- 接收复制发生真实 OOM 时仍正确传播 MemoryError，不能返回半成品。
- 无旧错/有旧错、空/非空返回、直接调用/函数值、JIT/AOT 与多线程隔离。

这是一项可单独复现的互操作边界，不能只修改 ZIP 的诊断 getter。
本任务不修改主线，也不在每个包增加私有 TLS 错误副本或更改 C ABI
来绕过；修复后重跑无包最小源和 ZIP 原有精确诊断测试。
