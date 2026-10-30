# dns

DNS 主机名辅助与系统地址解析。`encodeName` 将 ASCII 主机名编码为 DNS
标签序列的十六进制文本；`resolve` 委托 XRT 的系统解析器，不提供 DNS
报文客户端、服务器、缓存或 IDNA 转换。测试只解析本机 `localhost`，
不依赖公网 DNS，也不假定地址列表顺序。

## XLang

```xlang
import dns

println(dns.encodeName("example.com"))  // 076578616d706c6503636f6d00
println(dns.typeName(1))                  // A
array<string> ips = dns.resolve("localhost", 4)
```

`typeName`、`className`、`rcodeName`、`opcodeName` 返回已知 DNS 代码名称，
未知值返回 `UNKNOWN`。`validName` 仅验证 ASCII 主机名标签，允许末尾根点，
返回 `1` 或 `0`；不验证任意 DNS 资源记录名称，也不把 Unicode 自动转成
Punycode。`encodeName` 对无效主机名抛 `ValueError`，不会返回空字符串。

`isIpv4` 对合法数字 IPv4 返回 `1`，对无效文本或 IPv6 返回 `0`，
不会把普通验证失败抛为异常。`resolve(host, family = 0)` 的 `family`
可为 `0`（任意）、`4`（IPv4）或 `6`（IPv6）；结果为字符串数组，
顺序由系统解析器决定。无效族值或不可解析主机名会抛异常。
所有接收 C 字符串的语言入口都检查内嵌 NUL：验证函数返回 `0`，
编码与解析入口抛 `ValueError`，不会按截断后的前缀处理。

## C 与所有权

`include/dns_native.h` 声明公开 C 接口。名称映射返回静态借用文本；
`xdns_encode_name` 返回 XRT 分配的 `char*`，使用 `xrtFree` 释放；
`xdns_resolve` 返回 `xvalue*`，使用 `xrtValueRelease` 释放。
XLang 的编码入口以 `@owned string` 直接接管 `char*`，不再通过私有
`void*` 桥；接管时仍需扫描终止符确定长度。

双语范例：`release/examples/extlibs/dns.xl` 与 `examples/basic.c`。
固定输出在 `examples/basic.expected.txt`。

## 验证

从 `demo6` 目录运行：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/dns/test/check.ps1
bash release/extlibs/dns/test/check.sh
```

门禁覆盖原生边界、逐点分配失败与恢复、语言负例、生成 C 的所有权声明、C/XLang 同输出范例、
独立 EXE，以及 Linux ASan/UBSan。`localhost` 是唯一解析目标；长期
网络压力、第三方 DNS 服务和不受支持的平台不在此次验收范围内。
