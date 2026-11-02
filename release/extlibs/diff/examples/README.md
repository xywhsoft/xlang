# 双语范例

在仓库根目录运行；不需要网络、文件或额外数据。

```powershell
release/xl.exe run release/extlibs/diff/examples/review.xl
release/xl.exe run release/extlibs/diff/examples/review_c.c
release/xl.exe build release/extlibs/diff/examples/review.xl -o build/review.exe
release/xl.exe build release/extlibs/diff/examples/review_c.c -o build/review-c.exe
```

Linux 将 `release/xl.exe` 换成 `./release/xl`，输出名称无需 `.exe`。
两份源码输出都必须与 `review.expected.txt` 完全一致。配置的 CRLF 与 LF
视为相同行尾；改名只有一行删除和一行新增。末尾 LF 不多出一行空行。
重复行的匹配顺序固定，不能把两个最短结果当成同一个输出。

`review()` 返回的结果不依赖局部输入继续存活：C 明确释放输入，
最后分别 `xrtFree` 释放两个结果；XLang 自动管理输入和拥有型结果。
范例 C 的输入副本用于直观展示该生命周期，不是库内隐藏的正文复制。
输出只是逐行前缀文本，不是能直接交给 `patch` 的 unified patch。

原始 `diff_c.c` 与 `release/examples/extlibs/diff.xl` 保留为函数变化示例，
同样进入默认门禁；完整测试会严格分离编译 C 实现与示例，并与
TCC 直跑、C AOT、XLang JIT/AOT 核对固定输出。
