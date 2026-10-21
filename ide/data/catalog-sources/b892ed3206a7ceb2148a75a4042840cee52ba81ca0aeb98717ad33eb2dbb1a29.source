# chardet

基于 XRT 的 Unicode 编码猜测和字节有效性检查。支持 UTF-8、带 BOM 的 UTF-8、
UTF-16LE/BE、UTF-32LE/BE；没有 GBK、Shift-JIS 等传统字符集的统计模型。
猜测不会解码、转码、修改输入或去掉 BOM。

## xlang API

```xlang
import chardet

println(chardet.detect("你好"))       // UTF-8
println(chardet.confidence("你好"))   // 90
println(chardet.isUtf8("你好"))       // true
println(chardet.isAscii("你好"))      // false
```

五项函数都提供 `string` 和 `Buffer` 两个重载，没有额外可选参数。

| 调用 | 返回类型 | 含义 |
| --- | --- | --- |
| `detect(data)` | `string` | `UNKNOWN`、`UTF-8`、`UTF-8-BOM`、`UTF-16LE`、`UTF-16BE`、`UTF-32LE`、`UTF-32BE` |
| `confidence(data)` | `int` | XRT 猜测置信等级 0..100，不是统计概率或校验结论 |
| `bomSize(data)` | `int` | 识别到的 BOM 字节数：0、2、3、4；不消费输入 |
| `isUtf8(data)` | `bool` | 整个输入是否是合法 UTF-8；拒绝截断、过长编码、代理项及超出 Unicode 范围的码点 |
| `isAscii(data)` | `bool` | 每个字节均小于 128，包含 NUL 和控制字符；不是“全是可打印字符” |

空输入得到 `UNKNOWN`、0、0、true、true。不能识别及非法 UTF-8 是正常结果，
不是异常；非法 C 指针/长度参数才是参数错误。输入内存必须真实可读，C API 无法验证
调用方伪造的地址或超出分配范围的长度。

## 猜测与校验

- BOM 优先，置信等级为 100；**不意味着已验证正文**。
- 无 BOM 的 UTF-16/32 依据字节分布及编码合法性进行启发式判断；短样本可能无法识别。
- 只有 ASCII 的普通文本目前得到 UTF-8/40；包含合法非 ASCII UTF-8 的文本得到 UTF-8/90。
- `isUtf8` 和 `isAscii` 判断字节本身，不依赖 `detect` 的结果。因此 `41 00 42 00`
  这类数据既可能被猜作 UTF-16LE，又是合法的 UTF-8/ASCII 字节序列。
- 不应仅凭低置信度猜测自动覆盖原始文件；保留原始字节，由应用决定接受策略。

```xlang
import chardet

Buffer raw = Buffer()
raw.appendByte(239)
raw.appendByte(187)
raw.appendByte(191)
raw.appendByte(255)
println(chardet.detect(raw))    // UTF-8-BOM
println(chardet.bomSize(raw))   // 3
println(chardet.isUtf8(raw))    // false，正文含非法字节
```

文件、网络、UTF-16/32 和任意二进制输入使用 `Buffer`，它保留准确长度及 NUL 后的数据。
当前共享运行库的 `string.byteLen` 对含 NUL 的语言字符串仍有截短缺陷，已保留独立失败
用例；不能用 string 重载替代二进制重载。这是待修复的语言边界，不是新增编码规则。

## C API 与所有权

公共头为 `include/chardet.h`，实现为 `chardet_native.c`，依赖
`XRT_MODULE_CHARSET_DETECT`，不依赖进程内可变结果缓冲。

| C 接口 | 返回值 |
| --- | --- |
| `xchardet_detect_name_n(void* data, size_t size)` | `const char*` 静态只读名称 |
| `xchardet_detect_conf_n(void* data, size_t size)` | `int` 置信等级 |
| `xchardet_detect_bom_size_n(void* data, size_t size)` | `int` BOM 字节数 |
| `xchardet_is_utf8_n(void* data, size_t size)` | `bool` |
| `xchardet_is_ascii_n(void* data, size_t size)` | `bool` |

对应 `_str` 接口首参数为 `const char*`，第二参数仍是 `size_t`，不会调用 strlen
推导长度；两组共 10 个物理 ABI，分别准确匹配语言 point 与 string 的 C 表示。
输入均借用、不修改、不保留；名称是进程期静态数据，**不得 free/xrtFree**。
语言绑定将借用名称复制为语言拥有的 string，没有借用期悬空或静态内存接管。

`{NULL, 0}` 是合法空视图；`{NULL, 非零}` 设置 `XERR_ARGUMENT`，语言侧映射为
ValueError。每个原生入口清除旧错误；C 调用者先检查 `xrtGetError()`，不能把错误时
的返回占位值当作正常检测结果。内存分配仅可能发生在语言返回字符串的复制等桥接环节，
不发生在原生检测本身。

C 需要一次获得编码、BOM 和置信等级时可直接调用 `xrtEncodingGuess(xbytesview)`；
连续调用三个便捷函数会分别探测输入。独立调用可并发，调用期间不能并发修改相同输入。

## 范例与验证

以下开发命令从 `demo6` 目录运行；C 范例需要链接原生实现，不是只给一个 `.c` 文件。

```powershell
./release/xl.exe run release/examples/extlibs/chardet_basic.xl
./release/xl.exe run release/extlibs/chardet/examples/basic.c release/extlibs/chardet/chardet_native.c --inc-path lib/xrt --inc-path release/extlibs/chardet/include
./tools/run_extlib_c.ps1 -Package chardet -Source release/extlibs/chardet/examples/basic.c
./tools/check_extlib_chardet.ps1
```

双语基础范例覆盖全部五项操作，结果对照
`release/examples/extlibs/chardet_basic.expected.txt`。发布目录内运行 XL 范例可以直接使用
`xl run examples/extlibs/chardet_basic.xl`。

Windows 门禁包含 C 严格编译、10 个实际生成原型、22 组字节、非法指针/长度、错误恢复、
无分配验证、四线程 8000 轮、名称保留、XL JIT/AOT、双语同输出及两个 DLL 的上下文复用。
Linux C 门禁为 `bash tools/check_extlib_chardet.sh`，使用 ASan/UBSan 与泄漏检查；设置
`CHARDET_DECLARATIONS` 可核对生成原型的 LP64 合同。

这些证据不覆盖 Linux xlang JIT/AOT、32 位/ARM、TSan、全部输入空间或共享 string 的 NUL
缺陷。具体版本、哈希与尚未闭环事项见 `docs/扩展库现状复核_2026-09-28_当前.md`。
