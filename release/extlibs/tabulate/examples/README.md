# Tabulate 范例

从仓库根目录运行：

```text
release/xl.exe run release/examples/extlibs/tabulate.xl
release/xl.exe run release/extlibs/tabulate/examples/tabulate_c.c
release/xl.exe run release/extlibs/tabulate/examples/bounded_report.xl
release/xl.exe run release/extlibs/tabulate/examples/bounded_report_c.c
```

Linux 将 `xl.exe` 换成 `xl`。两组均有 C/XLang 同输出对照，默认不写
文件。普通 C 构建使用公开头文件并链接包清单中的两个实现单元及
XRT 的 Buffer/Unicode 模块；TCC 单文件运行由范例包含相同实现。

- 基础表格：空单元格、中文和逗号，显示行列数。
- `bounded_report`：稀疏行在宽表头下产生大量空格，超过 4 MiB 输出
  限额后表仍可重用。缩短表头后生成全部 256 行，不丢弃业务记录。

第二组的固定输出：

```text
rows: 256
oversized render: rejected
table preserved: true
recovered render: true
```

这些函数内范例不替代仍开放的全局对象退出清理测试，包级成熟度和
平台验收范围见上级 README 及仓库验收矩阵。
