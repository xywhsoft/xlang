# linalg 目录 smoke 引用了已移除的重复数学 API

2026-10-16，在 `demo6` 目录运行：

```powershell
./tools/smoke_extlib_catalog.ps1 -Package linalg
./release/xl.exe run test/cases/extlib_linalg.xl
```

包内 `test/boundaries.xl` 通过。中央用例在第 40 行
`linalg.clamp(15.0, 0.0, 10.0)` 报
`[import] import package has no such exported symbol`；第 41、42 行另调用
`linalg.deg2rad`、`linalg.rad2deg`，同属旧 API。

当前设计由标准库提供 `math.clamp`、`math.rad`、`math.deg`，`linalg`
专注二维/三维向量和 2×2 矩阵，不重复导出这些标量函数。

2026-10-18 已解决目录测试漂移：仅修改扩展库中央用例
`test/cases/extlib_linalg.xl`，删除三个非本包接口的断言；对应标量能力
原本就在 `test/cases/stdlib_math.xl` 中独立验证。当前 Windows
`smoke_extlib_catalog.ps1 -Package linalg` 为 3/3，Linux 包级脚本也纳入
修正后的中央用例。没有恢复兼容别名或修改编译器主线。完整 AOT 的
共享缺符号阻断另见 `text_snapshot_link.md`，不因目录 smoke 转绿而关闭。
