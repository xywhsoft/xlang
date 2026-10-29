# 主线阻断：函数值不能进入 var

- 最小复现：在 `release` 运行
  `xl check ../dev/extlib_blockers/function_into_var.xl`。
- 期望：函数是一等值；`var` 可保存函数对象，随后由 XRT `xrtValueInvoke`
  按签名调用。用户设计要求 C/XLang 双向回调互通。
- 实际：`var boxed = callback` 报
  `[sem] 初始化表达式类型和声明类型不匹配: target var, value function`。
  `release/extlibs/tar/test/test_tar.xl` 第 62 行的匿名选择器直接传入
  `TarReader.extractWhere(..., var selector)` 也报 `no matching callable class member`。
- 影响：`tar.extractWhere` 和 `extractWithProgress` 的 XLang 回调路径无法签收；
  其他接收 `var` callable 的 C 扩展库也可能受影响。
- 建议：在语义/降低层统一实现 function 到 var 的装箱，保持签名、闭包环境与
  所有权，由现有 XRT 函数对象承载；随后覆盖赋值、传参、返回、再取出及
  `xrtValueInvoke` 的异常和生命周期。不要仅在 TAR 包里做临时回调封装。
- 约束：本扩展库任务不修改主线；修复后先跑上述最小复现，再复跑
  `extlibs/tar/test/test_tar.xl`。

包内 Windows 权限恢复问题已按平台能力收口；当前
`release/extlibs/tar/test/check.ps1` 的 C/XLang 聚焦门禁通过。
独立 C 测试现为 `release/extlibs/tar/test/test_tar_c.c`，
与 `test_tar.xl` 不再同名自动混编；回调装箱阻断仍需主线修复。
2026-10-21 复验：从 `demo6` 根目录运行
`release/xl.exe run release/extlibs/tar/test/test_tar.xl`，仍在第 62 行
`extractWhere` 报同一 `[sem] no matching callable class member`，测试未进入执行。
该测试已将固定 `build/tmp/tar_xlang_test` 的递归删除改为独占临时目录与
`finally` 清理；路径改动须在主线修复后随整份综合测试重新运行验证。
