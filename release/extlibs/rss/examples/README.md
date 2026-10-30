# RSS / Atom 双语范例

从仓库根目录运行，Windows 使用 `release/xl.exe`，Linux 使用 `release/xl`。
这两种语言都调用同一份 RSS/Atom C 实现；C 的多源编译需要 XML DOM、
XML SAX 和 Expat 依赖，不应把范例当作独立单文件实现。

## 选择网页链接

`atom_links.xl` 与 `atom_links_c.c` 读取同形的 Atom feed：前缀元素、
实体解码、订阅自身链接和网页链接。两者都断言 feed/entry 选中网页地址。
完整 IANA `alternate` IRI 与短名称等价，所有权随 feed 一起释放。

Windows：

```powershell
release/xl.exe run release/extlibs/rss/examples/atom_links.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package rss -Source release/extlibs/rss/examples/atom_links_c.c
release/xl.exe build release/extlibs/rss/examples/atom_links.xl -o build/test/rss_atom_links.exe
build/test/rss_atom_links.exe
```

固定输出见 `atom_links.expected.txt`：

```text
News & Notes
https://example.test/
https://example.test/posts/1
```

C 的 GCC、TCC JIT/AOT 和 XLang JIT/AOT 均由包级门禁执行，不要求在
发布目录创建额外 DLL 或临时文件。门禁自身的对象和独立程序位于
`build/test`（Windows）或 `TMPDIR` 下的隔离目录（Linux）。

## 作者继承

`atom_author.xl` 与 `atom_author_c.c` 读取相同 Atom 数据，输出 `Editor`、
`Source` 两行，展示 entry → source → feed 的有效作者选择。
执行方式与上述范例相同，只替换源文件名。

## 边界与当前阻断

这些只读解析范例不使用语言字典生成 feed，不能替代生成 API 验收。
中央 `release/examples/extlibs/rss_c.c`、`rss_pair.xl`、`rss.xl` 仍保留
原生成目标；当前语言字典/数组的 foreign 投影 M1 导致原 XLang 构造
路径失败，包级默认门禁会按失败报告，不隐藏、不改为预期错误。

完整命令与证据见包 README、`ATOM_RELATIONS_AUDIT_2026-10-30.md`。
