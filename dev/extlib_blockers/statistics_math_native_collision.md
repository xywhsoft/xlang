# Windows C 原生单元与 math.abs 的符号冲突

## 现行发布物复现（2026-11-02）

在主工作区执行：

```text
release/xl.exe check dev/extlib_blockers/statistics_math_native_collision.xl
release/xl.exe run dev/extlib_blockers/statistics_math_native_collision.xl
release/xl.exe build dev/extlib_blockers/statistics_math_native_collision.xl -o build/test/math-collision.exe
```

最小源仅导入 statistics 并断言 `math.abs(-1.0) == 1.0`，没有调用任何
统计函数，也未传入容器，预期运行退出 0。当前 Windows `check` 退出 0，
JIT/build 均退出 1，诊断：

```text
tcc: __xlvfs__/csrc_0.c:142: error: 'fabs' defined twice
```

去掉 import 的纯 math 对照退出 0。隔离目录中复制同一包、只将 native
替换成当前 Git 基线旧源，原 measurement_offsets 范例仍报同一重定义。
临时在隔离旧 native 加 `#error AUDIT_OLD_PACKAGE_SELECTED`，得到该明确
诊断，证明包搜索确实选中了隔离副本；随后移除此负控，活动包未修改。
因此不是本轮调和平均数修改引入的，也不是原 M1 的数组投影错误。

Linux 原生文件系统冻结快照上，同一最小源运行退出 0，尚不将此问题
扩展为跨平台事实。两平台 C 原生专项、GCC/TCC/AOT 数值与原型检查均过。
Windows 默认门禁在原 measurement_offsets 处提前遇到本问题；Linux 在
该处报原有 `ValueError: input is not an array`，两者必须分别登记。

## 主线修复建议

- 联查 native C 自动 ABI 头、CRT 符号注入以及 TCC 中导入/定义的归属与
  去重；`src/api/xl_api.c` 的 `xlBuildCSourceWrapperText` 与
  `xlCompileUnitCSourceFiles` 是复查入口，不把推测当成已定根因。
- 验证 import 的 C 翻译单元与语言内建 math 同用时，`fabs` 及其他共享
  CRT 符号只能有一个实际定义；保持正确的公开 C 声明及宿主函数绑定。
- 增补 Windows/Linux、JIT/AOT、纯 C、纯语言 math、二者混编及两个独立
  C 翻译单元对照，再复跑 statistics 默认门。修好后仍须独立处理 M1。
- 不在每个包删 `<math.h>`、改 CRT 函数名或手写绝对值实现来绕过；
  不把原语言正例改成预期失败，不修改 statistics 的公开 ABI。

证据与发布物指纹：`docs/reports/extlib-statistics-harmonic-20261102.json`。
本任务只记录和补复现，不修改 xlang/CRT/TCC 主线。
