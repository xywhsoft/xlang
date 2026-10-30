# ZIP 双语范例

以下命令从仓库根目录执行。XLang 使用已构建的 `release/xl.exe`（Linux
用 `release/xl`）；C 的包运行器使用同一发布物并按 manifest 加入 ZIP、
libzip 与 zlib 源，不额外编译一份宿主 XRT。Linux sanitizer 门禁另需
GCC、Python 3。Python 只使用标准库。

## 大正文的内存往返

`memory_roundtrip.xl` 与 `memory_roundtrip_c.c` 使用相同的 131072 个
`z`、STORE 方法、条目名及注释，并核对完整正文，而不是只比较长度。
展示输入在 add 后释放、提交/取走 Buffer、元数据读取，以及关闭归档
后独立结果继续有效。所有拥有对象都有对应自动退役或显式释放。

```powershell
release/xl.exe run release/extlibs/zip/examples/memory_roundtrip.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package zip -Source release/extlibs/zip/examples/memory_roundtrip_c.c
```

两端固定输出为 `memory_roundtrip.expected.txt`：

```text
entries: 1
bytes: 131072
comment: stable
snapshot: true
```

默认不读写文件。两份程序均可接受一个输出路径，只有明确传入时才
写出标准 ZIP，便于独立工具检查。例如：

```powershell
release/xl.exe run release/extlibs/zip/examples/memory_roundtrip.xl -- build/test/memory.zip
python -I release/extlibs/zip/test/check_memory_wire.py build/test/memory.zip
```

须先确保输出目录存在；该路径由调用者选择并负责清理。文件导出时
XLang 的 `packed.toBytes()` 有显式转换成本，不将这条验证路径称为
零复制。C 用 `xrtFileWriteAtomic`，借用元数据注释跨关闭时显式复制。
XLang 保留 `cstring` 读取结果，其释放函数与来源 XRT 匹配。

## C 独立程序（Windows/Linux）

现有 `test/c_aot_import.xl` 仅声明 `import zip`，可让构建命令按包清单
链接 C 示例。Windows 下使用 `release/xl.exe`、输出 `.exe`；Linux：

```bash
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
./release/xl build release/extlibs/zip/test/c_aot_import.xl \
    release/extlibs/zip/examples/memory_roundtrip_c.c -o "$work/memory"
"$work/memory"
```

这里是 `build` 后运行 C main，不把只有 import 的 XLang 入口拿去
`run` 后就声称执行了 C main。单独运行 C 文件不会自动导入 ZIP 包；
Windows 的包运行器或上述显式导入构建才会补齐依赖。

## 范围与验收

中央 `release/examples/extlibs/zip.xl` / `zip_c.c` 另展示 DEFLATE、目录、
二进制、注释。门禁比较 GCC C、TCC C JIT/AOT、XLang JIT/AOT 的新范例
输出，并用独立 Python zipfile 检查五份实际归档的正文、CRC、STORE
和注释；不要求变化的 mtime 或压缩文件逐字节相同。

包级测试：`test/check.ps1` 或 `bash release/extlibs/zip/test/check.sh`。
默认门禁目前因 M2/M3/M12 返回失败，RFS 也未闭环；不能因本地 main
内的示例通过就把整包标成发布完成，详见包 README 和审查报告。
