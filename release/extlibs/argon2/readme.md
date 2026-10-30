# argon2

2026-10-30 PHC版本边界：原共用解码入口接受任意uint32版本，调用当前轮
函数也能对这些未知版本生成的Tag验真。现只接受实际实现的16/19；省略
v字段按该外部编码格式表示16，不能把未知版本改写为19。无效PHC保持普通
查询语义：verify=false、checkNeedsRehash=true、extract*=-1，不留错误；
分配失败仍是MemoryError。修复仅在薄绑定的共用解码入口，不改vendor或ABI。

新增test/test_argon2_phc.c：三种算法的9组显式/省略正常版本、24组未知
版本、85组数值/字段顺序/base64/长度/NUL畸形、3组仅解析的uint32边界，
以及9个查询组合的26分配故障点与同输入恢复。未知版本在旧实现24组均被
接受，修复后归零。大成本字段只读元数据，不执行GiB或数十亿轮的工作。
数值包含回绕后为1/19/32的溢出值，不能只靠下游拒绝零值来掩盖位宽检查退化。
语言test/phc.xl另检查所有extract入口、精确大整数及无效PHC查询的OOM。

新examples/phc_policy.xl与phc_policy_c.c演示先验证16版本，再用新随机
盐重新计算19版本；同时展示未知/损坏拒绝和纯元数据查询，六行固定输出。
这里的低成本和固定旧盐仅为测试，生产使用默认配置并按机器/并发量标定；
不能只替换PHC的版本字段完成升级。两份源码可直接xl run/build。

本轮最终联合门禁Windows64项、Linux66阶段全部通过；Argon2的11个和
原有Bloom的33个实际生成原型分别对正式C头验证，Argon2的错误返回类型
负例在LLP64/LP64均拒绝。Linux C含ASan/UBSan/泄漏检查，原RFC三向量、
原联合34个分配失败点及新旧双语/语言边界JIT/AOT继续通过。
Windows58输入、Linux65复制源/副本与含实际GCC的66输入前后不变；另有
44源文件的Linux独立红绿快照，旧未知版本失败24→0。
隔离去掉DECIMAL_U32检查时，新回绕用例在LP64确实断言失败；实际vendor
未动，不将Windows unsigned long只有32位的行为当成跨平台证明。
PS5/PS7真静默编译器raw37/outer1、Linux缺工具127均保留现场，官网仅做
隔离生成验证。主线/XRT/八件冻结物未改，本轮不是密码学认证或长时压力测试。

2026-10-28 文本所有权收口：四个 C 文本出口统一返回可用 `xrtFree`
释放的 `char*`，XLang 以 `@owned string` 直接接管，不再先写临时区
再复制；删除了只负责字符串转发的 `__a2_require`。生成 C 检查四条
直接接管路径，仍有一次长度扫描和既有外调错误处理，不宣称整条
调用零开销。Windows/Linux 默认联合门禁均退出 0，覆盖 RFC 向量、
C ASan/UBSan（Linux）、分配故障、正常空结果、语言 JIT/AOT 与
双语固定输出；C 范例显式释放全部文本。新增
[所有权用例](test/ownership.xl) 已登记包清单。Linux 语言阶段使用
独占原生临时目录，不将此通过外推为 WSL 挂载盘性能已修复。

2026-10-26 当前发布物：Windows `tools/check_extlib_argon2_bloom.ps1`
报告 `passed`，47 项检查，严格 C/生成原型、RFC 向量、分配故障、
语言 JIT/AOT 与双语固定输出通过。Linux 同一联合门禁退出 0：
C ASan/UBSan、固定向量、分配故障、Argon2/Bloom 各四条独立 EXE
AOT，以及 Bloom NUL 键 JIT/AOT 通过。Argon2 的自动随机盐输出
不做跨运行文本比较。这里只签收当前 Windows/Linux x64 短时路径；
实际生产成本仍需按部署机器和并发量标定，32 位/ARM、TSan、
长时间压力及独立密码学审计不在本次证明范围。

2026-10-25 历史记录：当时 Windows 发布版运行 `tools/check_extlib_argon2_bloom.ps1`：
严格 C/生成原型、RFC 向量、分配故障、XLang 完整语言用例、C/XLang
固定输出及两种独立 EXE 的 AOT 构建与执行均通过。先前的
`uint8` 与 `int` 混合算术 XLIR 最小复现现退出 0；语言用例中的内嵌
NUL 密码、盐、secret 和 ad 往返也已在 JIT/AOT 实跑。
Linux `tools/check_extlib_argon2_bloom.sh` 的 C ASan/UBSan、固定向量、
范例与分配故障通过；当时未用重建的 Linux xlang 执行语言/AOT 门禁。
下面保留旧构建的失败记录作追溯，不代表当前发布版仍被阻断。

官方 Argon2 参考实现的 xlang/C 包，内存、文本结果、安全随机数和错误槽复用 XRT。
支持 Argon2d、Argon2i、Argon2id，默认 Argon2id。当前后端按顺序处理各 lane；
`parallelism` 是算法参数，不承诺创建相同数量的系统线程。

## 使用

```xlang
import argon2

string stored = argon2.hash("password")
assert argon2.verify("password", stored)
assert not argon2.verify("wrong", stored)
```

正式密码存储使用自动随机盐，不复制范例中的固定盐或低成本配置。
成本需要结合部署机器和服务并发量测量，不由库保证统一的响应时间。

## API 与边界

| API | 合同 |
| --- | --- |
| `hash(password, salt="", secret="", ad="", timeCost=3, memoryCost=65536, parallelism=4, hashLen=32, saltLen=16, type=ARGON2ID)` | PHC 字符串；空 salt 自动生成安全随机盐 |
| `hashRaw(...)` | 同参数，输出小写 hex；不携带成本和盐元数据 |
| `verify(password, encoded, secret="", ad="")` | 验证 PHC；错误密码或错误上下文返回 false；原生资源错误通过 XRT 错误槽进入 XLang 异常 |
| `checkNeedsRehash(encoded, timeCost=3, memoryCost=65536, parallelism=4)` | 参数或版本不同、编码无效时为 true；目标成本必须有效 |
| `extractType/TimeCost/MemoryCost/Parallelism(encoded)` | 无效编码返回 -1，不执行密码计算 |
| `generateSalt(len=16)` | len 字节随机数的 hex，字符串长为 len*2 |
| `recommendedTime/Memory/Parallelism/HashLen/SaltLen()` | 当前默认配置，分别为 3/65536/4/32/16 |

- C ABI 的密码、手动盐、secret、ad 和 PHC 文本均接收显式字节长度；XLang
  封装传入 `string.byteLen`。含 NUL 的密码与上下文参与完整哈希，PHC 文本
  内嵌 NUL 被拒绝，不存在按前缀处理的旧接口。C 回归及 Linux sanitizer 已通过；
  当前 Windows 发布版的 XLang 往返已通过；旧 XLIR 阻断记录见
  `dev/extlib_blockers/argon2_text_nul.md`。当前没有对外承诺 Buffer 重载。
- 输出 4..256 字节，盐 8..64 字节；成本为正数，内存单位 KiB，最大 2097152 KiB。
  验证外部 PHC 同样先检查此边界。算法还要求内存与 lane 数匹配。
- 上限不是请求配额：不要把未受信任的 PHC 直接当成可无限运行的任务。
  本层没有取消、中途预算或自动限制并发。
- PHC 不存储 `secret` 与 `ad`；验证时必须传入与创建时相同的值。
  不推荐普通字符串比较代替认证比较。
- PHC仅接受已实现版本16/19，省略版本号表示16。未知版本、整数超出
  uint32、非最小十进制编码和非规范base64均作为损坏输入拒绝。
  extract/checkNeedsRehash只检查编码和元数据，不执行昂贵的密码计算；
  它们能读取uint32上界，不表示verify获准突破内存预算或具备任务取消能力。
- 失败使用 XRT 结构化错误。无效参数为 ValueError，分配失败为 MemoryError，
  随机源失败为 IOError。上述六个操作入口会清除调用前的旧错误；成功、密码
  不匹配、无效 PHC 的普通查询结果不留下旧错误。调用方应在本次调用后立即读取错误槽。

`PROFILE_LOW_MEMORY_*` 使用 t=3/m=65536/p=4；`PROFILE_HIGH_MEMORY_*` 使用
t=1/m=2097152/p=4。两组对应 [RFC 9106 第 4 节](https://www.rfc-editor.org/rfc/rfc9106.html#section-4)，
此前混入的非 RFC 参数已纠正。默认值使用 64 MiB 组，高内存组不是自动升级行为。

## C 与验证

公开声明位于 `include/argon2_native.h`，每个输入指针均紧邻其 `size_t` 字节长度；
零长度盐触发随机盐生成。四条 C 文本返回值均由调用方拥有，以
`xrtFree` 释放，其他调用或临时区清理不会使先前结果失效。
资源或参数失败返回 `NULL`，详情读取 `xrtGetError()`；
`xargon2_extract` 对无效 PHC 返回拥有的空文本且不留错误，仍须
释放，XLang 的 `extract*` 将它映射为 `-1`。空结果的分配失败
仍是 `NULL`/`MemoryError`，不能当作普通未命中。
源文件以 package manifest 为准，链接一份 XRT。输入密码由调用方拥有，库不会擦除它。

范例：`examples/extlibs/argon2.xl` 为教学覆盖；`argon2_basic.xl` 与 `argon2_c.c`
为六行同结果对照。当前 Windows 发布版的 C/XLang 对照与 AOT 均已通过。

专项门禁：`tools/check_extlib_argon2_bloom.ps1`，包含生成的全部 C 原型、严格 GCC、
C/xlang JIT/AOT、负例和分配故障。具体运行结果应查看该次 `results.json`，不能只凭
测试脚本存在认定通过。二进制重载、验证任务预算和性能标定仍是后续项。
Linux 对应入口为 `bash tools/check_extlib_argon2_bloom.sh`。新增
`test/ownership.xl` 覆盖 PHC、raw hex、参数文本、随机盐及普通未命中
的 OOM 与恢复；C 分配故障扫描检查活动分配、重复释放及越界计数。
`test/test_argon2.c` 另核对 RFC 9106 第 5 节 Argon2d/i/id 的三个固定 Tag，
覆盖密码、盐、secret 和 ad 的完整字节传递，避免仅用本库生成再由本库验证。
该用例已通过 Windows 严格 GCC、TCC C JIT 与 Linux ASan/UBSan；不替代 XLang 验收。

PHC专项也已进入默认联合门禁，分配钩子只在独立原生进程执行。Linux将
所有编译/运行输入一次复制到独占临时目录，逐文件核对源件/副本/前后
身份；Windows直接使用当前根目录。命令有超时、原始状态与双流，失败
保留现场。对应新范例从发布目录执行
`./xl run extlibs/argon2/examples/phc_policy.xl`或`phc_policy_c.c`；从仓库
根目录执行`release/xl.exe run release/extlibs/argon2/examples/phc_policy.xl`。
旧版 `xl.exe` 曾在包内 `string.byteAt(info, 0) - 48` 处报告 typed XLIR
物理转换错误；最小复现及当时的修复建议见
`dev/extlib_blockers/argon2_byteat_sub.md`。当前 Windows 发布版已运行
最小复现和含 NUL 的完整语言用例，不再以该历史错误阻断验收。

2026-10-21：C 测试原在 `assert` 中执行 PHC 校验、非法输入和随机盐调用，
定义 `NDEBUG` 会跳过这些操作。测试单元现保持断言启用，仅 `argon2`
在 Windows/Linux 共用门禁中以 `-DNDEBUG` 编译，未改变范围外 `bloom`
的测试配置；预处理确认盐生成调用仍在。Windows 严格 C 与 C JIT、
Linux C ASan/UBSan 和分配故障门禁通过。Windows 随后的语言 cgen
仍在 `extlib_argon2.xl:93` 遇到既有 typed XLIR 错误，未签收发布级。

2026-10-24：当前发布版通过 Windows Argon2/Bloom 联合门禁 44 项，
其中 Argon2 的严格 C、RFC 9106 固定向量、生成原型、XLang JIT/AOT、
C/XLang 基础范例及教学范例均通过。门禁现以 XLang 实际 extern 声明
核对生成原型，并要求它们全都存在于 C 头文件；C 独有入口不再被
误判为缺少 XLang 原型。Linux C ASan/UBSan 与分配故障门禁通过。
上述 typed XLIR 阻断是旧发布物历史状态；Linux XLang JIT/AOT、
长期压力和性能标定仍未在本轮验证。
