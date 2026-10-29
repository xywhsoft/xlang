# 扩展库 AOT 构建在 xl6_sequence.h 的类型声明处失败

状态：2026-10-13 当前发布物可复现；主线阻断，普通扩展库任务不修改 runtime/TCC。

## 复现

在仓库根目录执行：

```text
demo6/release/xl.exe build demo6/release/extlibs/plist/test/smoke.xl -o demo6/build/test/plist_smoke.exe
```

实际结果：`demo6/src/runtime/xl6_sequence.h:79: error: ',' expected (got '*')`，位置为 `const xtypedsetkeypolicy* content;`。`demo6/release/examples/extlibs/xml_dom.xl` 的 AOT 构建也在同一行失败；不导入扩展库的 `demo6/release/examples/hello.xl` 可成功构建。`plist` 的 C 测试、ASan/UBSan、XLang `check` 和 JIT `run` 已通过。

## 修复建议

检查 AOT 所选 XRT 头文件是否在解析 `xl6_sequence.h` 之前声明 `xtypedsetkeypolicy`，以及当前 runtime 源/发布物中的 XRT 版本是否一致。修复应在共享构建/头文件依赖边界完成；复跑上述 plist 与 xml.dom 对照，不在扩展包内复制类型定义或加兼容别名。并行主线工作可能使此快照诊断失效，合入后需重新复验。
