# Brotli C 范例的裸单文件运行缺包内编译单元

## 最小复现

在 `demo6` 目录使用当前发布物：

```powershell
./release/xl.exe run release/extlibs/brotli/examples/basic.c
```

实际：`tcc: error: unresolved reference to 'xrtBrotliDecompressorDestroy'`。
预期：像包级运行器一样，分别编译该包 manifest 中的 C 编译单元并运行范例。

对照命令可正常输出 `utf8: ok`、`binary: ok`、`stream: ok`、
`truncated: rejected`：

```powershell
pwsh -NoProfile -File tools/run_extlib_c.ps1 -Package brotli -Source release/extlibs/brotli/examples/basic.c
```

## 边界和建议

`brotli` 依赖 30 多个上游 C 编译单元。把它们作为 `#include` 直接塞入范例
并不能替代分离编译：TCC 在 `third_party/enc/compress_fragment.c` 报
`kHashMul32` 重定义。已撤回该试验，未修改上游或编译器主线。

后续主线若要求裸 `xl run` 识别 C 范例所属扩展包，应从明确的包/构建元数据
取得编译单元并分别交给 TCC，而不是强制上游源码变为 unity build。
在此之前使用包级运行器；本任务继续其他普通扩展包。
