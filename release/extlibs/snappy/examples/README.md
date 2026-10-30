# Snappy 双语范例

命令均从仓库根目录运行，Windows 用已构建的 `release/xl.exe`；Linux
改为 `./release/xl`，运行和构建均不需要安装 Python。范例不读写文件，
正常执行无外部服务依赖，也不要求第三方 Python codec。

## 一次性往返

`basic.c` 与中央 `release/examples/extlibs/snappy_basic.xl` 输出同样
的 `hello snappy`。C 通过 `xrtSnappyCompress` / `xrtSnappyDecompress`
直接使用标准 framing，释放返回的 Buffer；语言端用公开标准包。

```powershell
release/xl.exe run release/extlibs/snappy/examples/basic.c
release/xl.exe run release/examples/extlibs/snappy_basic.xl
```

## 串联流、空块和逐字节输入

`framed_records_c.c` 与 `framed_records.xl` 使用同一条 59 字节标准流：
正文 abc、未压缩空块、压缩空块、第二个 identifier、正文 abc。
它们逐字节 update 并排空结果，检查总输入、总输出、成员数和完成态。
空块不输出正文，也不表示流结束；只有显式 finish 才确认 framing 完整。
完整正文仍为 `abcabc`，不能把第二个 identifier 当作丢弃既有正文的信号。

```powershell
release/xl.exe run release/extlibs/snappy/examples/framed_records_c.c
release/xl.exe run release/extlibs/snappy/examples/framed_records.xl
```

固定输出（`framed_records.expected.txt`）：

```text
text: abcabc
members: 2
bytes: 6
```

C 的每次 DrainBuffer 结果均由调用者拥有并释放，输入静态借用；语言
中的 Buffer 和解压器自然退役。结果收集有必要的追加复制，不宣称
这份逐字节教学示例零复制或高吞吐。生产输入可按网络/文件批量喂入，
流接口相同；此处选逐字节是为了展示分块不影响语义。

两个 C 范例在 TCC 单文件模式带入 core/vendor，正式 GCC 构建继续
按清单分离编译，不再编译第二份宿主 XRT。也可使用 Windows 包运行器：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package snappy -Source release/extlibs/snappy/examples/framed_records_c.c
```

独立程序可直接构建：

```powershell
release/xl.exe build release/extlibs/snappy/examples/framed_records_c.c -o build/test/snappy_records_c.exe
release/xl.exe build release/extlibs/snappy/examples/framed_records.xl -o build/test/snappy_records.exe
```

输出目录须先存在，调用者负责运行并清理这两个显式产物。Linux 可用
`mktemp -d` 创建独占输出目录、将 `-o` 指向其中，结束后删除本次目录。

## 验证范围

包级 `test/check.ps1` 与 `test/check.sh` 验证 C/XL 同输出及正常 AOT，
后者还有 sanitizer。独立 `test/check_empty_interop.py <interop-exe>`
需要 python-snappy 0.7.3：它先用独立实现解码八组固定标准流，再调用
C 测试程序核对字节与原始退出码，不把错误退出的空 stdout 算成功。
测试依赖不是扩展库运行依赖，安装位置可通过 PYTHONPATH 指定。

顶层真实 Snappy 对象仍有 M2：JIT 成功，AOT 输出正确后退出 1。
旧正例 `test/top_level.xl` 保留在默认门禁，不因本目录的局部 main
范例通过而改写或忽略。整包仍未签收发布完成。
