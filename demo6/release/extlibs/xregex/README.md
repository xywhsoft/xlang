# xregex - 正则表达式扩展库

`xregex` 是从 XRT 核心拆出的完整正则体系。它保留明确长度 C API、可复用匹配器、模板与回调替换、流式拆分和多模式集合，并在 XLang 中提供对象化接口。



## 快速开始

```xlang
import xregex

auto route = xregex.compile("^/users/(?<id>[0-9]+)$")
auto matcher = route.matcher()

if matcher.full("/users/42") then
    println(matcher.capture(1).text)
end
```

`Regex` 是不可变、引用计数的编译结果，可跨线程共享。`RegexMatcher` 保存可变执行缓存，应由单个执行流独占并重复使用；这样热路径不会反复编译表达式或分配执行程序。



## XLang 能力

| 类型或函数 | 说明 |
|---|---|
| `compile` / `test` / `fullMatch` | 编译和一行式匹配 |
| `valid` / `escape` | 验证和字面量转义 |
| `Regex.test` / `fullTest` / `matchAt` | 快速判断与锚定匹配 |
| `Regex.find` / `findAll` | 返回独立匹配结果 |
| `Regex.matcher` | 创建可复用、可读取全部捕获的匹配器 |
| `Regex.replace` / `replaceFirst` | 使用 `$0`、`$1`、`${name}` 模板替换 |
| `Regex.replaceWith` | 使用 XLang 回调生成替换文本 |
| `Regex.splitter` / `splitParts` / `split` | 流式或一次性拆分 |
| `compileSet` / `RegexSet` / `RegexSetMatcher` | 一次执行多个模式，返回命中索引 |

所有位置和长度均为 UTF-8 原始字节单位，区间使用 `[begin, endPos)`。未参与匹配的可选捕获通过 `matched = false` 与合法空捕获区分。



## C 能力

公开头文件为 `include/xregex.h`。核心对象包括 `xregex`、`xregexmatcher`、`xregexsplitter`、`xregexset` 和 `xregexsetmatcher`；所有文本入口都使用 `xstrview`，不会把嵌入零字节误认为结尾。

编译配置包含模式字节数和捕获数预算。语法错误、资源超限、替换模板错误和执行错误使用 XRT 结构化错误返回，并可读取错误字节位置或集合中的失败模式索引。



## 裁剪与实现

包清单只声明 `ERROR_FORMAT`、`IO_BUFFER`、`STRING_SPLIT`、`UNICODE` 和 xruntime `TYPED_ARRAY` 五个根模块。`RegexSet` 直接借用 `array<string>` 的连续存储，不先装箱为动态 `xvalue` 数组。静态构建不会隐式启用 `XRT_MODULE_ALL`，也不复制 XRT 单头文件。

正则执行器使用 BBRE。XRT 适配层统一使用 XRT 分配器、结构化错误和资源预算；源码保留在 `src/third_party/bbre`，许可证见同目录 `LICENSE.md`。



## 测试

`test/xregex_test.xl` 覆盖编译、命名捕获、复用 matcher、空匹配推进、模板与回调替换、拆分、RegexSet、Unicode 字节边界和错误映射。XRT 上游模块另有 OOM 故障注入、随机差分和单头文件门禁。
