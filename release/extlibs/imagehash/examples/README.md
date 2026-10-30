# Imagehash 双语范例

从仓库根目录运行，C 与 XLang 使用相同图片和同一公开算法：

```powershell
release\xl.exe run release\extlibs\imagehash\examples\similarity.xl
release\xl.exe run release\extlibs\imagehash\examples\similarity_c.c
```

默认比较 `tiny.png` 与 `player.png`，输出见 `similarity.expected.txt`。
也可提供两张图片（中文路径受支持）：

```powershell
release\xl.exe run release\extlibs\imagehash\examples\similarity.xl -- photo.png thumbnail.png
release\xl.exe run release\extlibs\imagehash\examples\similarity_c.c -- photo.png thumbnail.png
```

`phash` 生成独立拥有的指纹，`valid` 检查格式，`hamming` 计算位差。
阈值 4 只是应用示例，实际需要依据数据集调节；不能将近似当作内容相等
或安全认证。算法不同的指纹不可比较，pHash 的位布局也不保证兼容其他
软件。C 用 `xrtFree` 释放两个结果，XLang 自动释放；输入图片不修改，
不创建输出文件。C 的单文件 TCC 入口包含实现，仅供范例；正式包仍按
清单分离编译。

原 `imagehash.c` 和中央 `release/examples/extlibs/imagehash.xl` 展示三种
算法与固定输出。默认包门禁同时核对两个新例的不同图片/相同图片输出，
包括独立 GCC、TCC JIT/AOT 和 XLang JIT/AOT。
