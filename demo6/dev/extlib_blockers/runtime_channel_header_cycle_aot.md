# AOT 运行时头文件循环依赖

状态：2026-10-18 当前 `release/xl.exe` 可复现；属于共享运行时头文件，不在普通扩展库任务中修改。

## 最小复现

从 `demo6` 目录运行：

```powershell
./release/xl.exe build release/examples/extlibs/dotenv.xl -o build/test/dotenv-example-aot.exe
```

当前报错：`src/runtime/xl6_channel_language_abi.h:7: error: identifier expected`。同一包的 C 边界、XLang JIT、无输出中央 AOT 与 Windows/Linux 包门禁通过，只有完整范例 AOT 停在该头文件。

## 根因方向

`xl6_sequence.c` 首先包含 `xl6_sequence.h`；该头在声明 `XL6_SequenceLayoutV1` 前包含 `xl6_text_value.h`。后者又包含 `xl6_channel_language_abi.h`，而它第 7 行已经把 `XL6_SequenceLayoutV1` 作为参数类型使用。回到 `xl6_sequence.h` 时 include guard 阻止了尚未到达的布局声明，因此类型在该位置不可见。此处是按当前包含顺序作出的判断，需由主线维护者用预处理输出和多编译器验证。

## 修复建议

把共享布局类型放进不依赖 `xl6_text_value.h` 的基础 ABI 头，或调整头文件职责使语言 channel ABI 在布局定义之后才引入；不要在扩展包里复制 typedef 或增加兼容宏。修复后至少复跑上面的完整 AOT、`dotenv/test/check.ps1` 和以 `xl6_sequence.h` 为首个包含项的 C 构建。
