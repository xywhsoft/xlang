# 长文本绘制结果复用实验

SDK23 私有候选，基于 SDK21，不包含未采用的 SDK19 度量补丁。保留一份 4,096 字节至 16 MiB 的物理字体绘制结果，在串行的内置 Emoji 默认包状态下用精确自有文本判断复用；大布局／测量和匹配字体销毁前释放。公开头不变。

严格冷构建、三个 SDK 测试、六个相关应用程序、16 组原生文件比较通过。第一轮成功绘制对照完成 787,058 项断言，独立落盘入口增加直接字体切换和首次加载检查，完成 803,481 项断言、307 组成功像素比较、48 组一致错误和 144 次重复结果复用。

16 MiB 单行重绘观察有所改善，插入／撤销仍约两秒，保留整行数组明显增加私有内存。本版不选入应用，未通过性能或完整 SDK 替换验收。默认包并发切换也不在本轮验证范围，推广前须完善稳定包快照或变更代数。详见[记录与限制](../../../docs/large-source-draw-cache.md)。

`patch-manifest.json` 绑定八个源材料及对应 diff；`source-materials-before.json` 绑定原材料。十六个测试材料包含前版对照，新绘制测试三项为 `draw_cache_proxy.inc`、`old_draw_text_spans.c` 和 `draw_cache_test.c`。只有提取后的测试中有分配失败和整形计数钩子，SDK 没有诊断钩子。

```powershell
& 'C:\Program Files\Python312\python.exe' -X utf8 tools\prepare_draw_cache_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output build\draw-cache-source-review
& 'C:\Program Files\Python312\python.exe' -X utf8 tools\test_draw_cache_sdk.py --build build\editor-sdk-draw-v23-build.json --output build\draw-cache-proof-review
```

准备和比较均要求新输出目录。准备不构建、不安装。比较程序编译实际引擎源码，由程序单独持有 XRT，不链接 DLL；它在未切换到前台的私有桌面运行。其他 profile 保持原绘制路径，当前应用和便携候选未更新。
