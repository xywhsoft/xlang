# ZIP 中央 C 范例单文件运行阻断

复现（在 `demo6` 根目录）：

```powershell
release/xl.exe run release/examples/extlibs/zip_c.c
```

当前报 `unresolved reference to 'xrtZipArchiveDestroy'`。对照入口通过：

```powershell
pwsh -NoProfile -File tools/run_extlib_c.ps1 -Package zip -Source release/examples/extlibs/zip_c.c
```

`zip` 清单依赖 `_libzip`、`_zlib`；仅 `_libzip` 就有上百个分离编译源文件。不能把这些文件强行 `#include` 到范例里，既容易出现同名静态符号冲突，也会把教学范例变成源码清单。应在 xlang 主线的 C 输入/包构建能力中支持按包元数据追加多编译单元；本扩展库任务不改主线。修复后需验证裸 `xl run`、`xl build` 和包运行器的同输出。
