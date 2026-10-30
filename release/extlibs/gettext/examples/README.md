# gettext 双语范例

从仓库根目录运行。Windows 使用 `release/xl.exe`，Linux 使用 `release/xl`。

## 同一目录的 PO/MO 读取

`layout.xl` 与 `layout_c.c` 读取相同的目录，展示普通消息、菜单/工具栏/
空上下文以及单复数。共享输入 `test/po_layout.po` 现在也示范续行：

```po
msgid "plain"
msgstr ""
"常"
"规"
```

续行不会插入额外换行，结果是 `常规`；需要正文换行时在引号内使用
`\n`。PO 源文件仍要求 UTF-8。范例逐项断言结果，不只打印。

```powershell
release/xl.exe run release/extlibs/gettext/examples/layout.xl -- release/extlibs/gettext/test/po_layout.po
release/xl.exe run release/extlibs/gettext/examples/layout_c.c -- release/extlibs/gettext/test/po_layout.po
release/xl.exe build release/extlibs/gettext/examples/layout.xl -o build/test/gettext_layout.exe
build/test/gettext_layout.exe release/extlibs/gettext/test/po_layout.po
```

固定六行输出见 `layout.expected.txt`。安装 GNU `msgfmt` 后，可将同一 PO
编译成 MO，再给两份范例追加 `mo` 参数；Linux 包级门禁实际验证了该路径，
并由 Python `GNUTranslations` 独立核对结果。

C 的 `*_borrowed` 查询结果只在目录/原文存活期间借用，不释放也不接管；
`layout_c.c` 在目录释放之前打印。需要跨目录释放保留时，使用返回独立
字符串的 `xgt_t/tc/tn/tnc`，最终 `xrtFree`。XLang 查询返回独立普通
`string`，不能为了减少复制而把它改成悬空借用。

## 基础查询与边界

`gettext.xl` / `gettext_c.c` 展示基础查询和错误处理，中央
`release/examples/extlibs/gettext.xl` 保留完整使用入口。C 范例包含包内
实现以便单文件 `xl run/build`；正式包仍由 manifest 分别编译实现文件，
不会加入第二份 XRT。

长正文、UTF-8、混合/密集转义、续行、分配失败和结果独立寿命另有可运行测试：

```powershell
release/xl.exe run release/extlibs/gettext/test/po_spans_c.c
release/xl.exe run release/extlibs/gettext/test/po_spans.xl
```

可选 `-- --bench` 只用于 C 测试程序的本机基准，不进入默认正确性门禁；
输出为 `正文模式,样本序号,微秒,校验和`，每组先丢弃一次预热。
性能边界、原始对照位置及已观察到的取舍见包内
`PO_SPAN_AUDIT_2026-10-30.md`，不能把单次本机结果当作全语言性能。
