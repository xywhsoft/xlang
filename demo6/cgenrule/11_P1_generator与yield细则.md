# P1 generator 与 yield 生成细则

本文细化 `xl6_cgen_generator.c` 中的 generator runtime、`generator<T>` 函数、frame、worker 和 `yield` 生成规则。`for item in generator` 的循环消费路径已在 `08_P0_控制流与循环细则.md` 中记录，这里补全 generator 的生产端。

## CGEN-GEN-001 generator runtime 注入

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 使用 `generator<T>` 或 `yield` 时隐式需要 |
| 生成规则描述 | generator runtime 定义 `XL6_Generator`，内部保存协程句柄 `xcoro`、frame、当前 value 缓冲、valueSize、hasValue、done。还声明 XRT coroutine API。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:1`、`xl6_cgen_generator.c:6`、`xl6_cgen_generator.c:18` |
| 对应生成的 C 代码 | `typedef struct XL6_Generator { xcoro co; void* frame; void* value; size_t valueSize; int hasValue; int done; } XL6_Generator;` |
| 中间形态伪代码 | `Generator = {coroutine, frame, valueBuffer, state}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要定义同等状态结构并接入 XRT coroutine。 |
| 额外依赖的封装代码 | generator runtime。 |

## CGEN-GEN-002 generator 创建

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | 调用返回 `generator<T>` 的函数 |
| 生成规则描述 | `xl6_generator_create` 分配 `XL6_Generator`，分配 value 缓冲，保存 frame，创建 coroutine。任一步失败都会释放已分配资源并返回 0。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:26`、`xl6_cgen_generator.c:31`、`xl6_cgen_generator.c:36`、`xl6_cgen_generator.c:41` |
| 对应生成的 C 代码 | `gen = xrtMalloc(sizeof(XL6_Generator)); gen->value = xrtMalloc(valueSize); gen->co = xrtCoCreate(entry, gen, 0);` |
| 中间形态伪代码 | `create(valueSize, frame, entry) -> Generator*` |
| 实现同等功能手写的 C 代码 | 手写 C 要按相同顺序释放 frame/value/gen/co。 |
| 额外依赖的封装代码 | `xl6_generator_create` helper。 |

## CGEN-GEN-003 generator 销毁

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | generator 变量离开作用域 |
| 生成规则描述 | `xl6_generator_destroy` 依次销毁 coroutine、释放 value 缓冲、释放 frame、释放 generator 本体。该函数由 cleanup 机制调用。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:47` |
| 对应生成的 C 代码 | `if (gen->co) xrtCoDestroy(gen->co); xrtFree(gen->value); xrtFree(gen->frame); xrtFree(gen);` |
| 中间形态伪代码 | `destroy(co); free(value); free(frame); free(gen)` |
| 实现同等功能手写的 C 代码 | 与生成 helper 一致。 |
| 额外依赖的封装代码 | `xl6_generator_destroy` helper。 |

## CGEN-GEN-004 generator yield copy runtime

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `yield expr` |
| 生成规则描述 | runtime helper `xl6_generator_yield_copy` 校验 generator、value 指针、valueSize，然后把 yield 值复制到 generator 的 value 缓冲，设置 hasValue，并调用 `xrtCoYield` 暂停协程。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:62`、`xl6_cgen_generator.c:72` |
| 对应生成的 C 代码 | `memcpy(gen->value, &tmp, sizeof(T)); gen->hasValue = 1; xrtCoYield();` |
| 中间形态伪代码 | `gen.value = copy(value); gen.hasValue = true; yieldCoroutine()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要保存当前值并主动 yield coroutine。 |
| 额外依赖的封装代码 | `xl6_generator_yield_copy` helper。 |

## CGEN-GEN-005 generator next runtime

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `for v in gen ... end` |
| 生成规则描述 | `xl6_generator_next` 检查空 generator、done、co 是否存在；每次 resume 前清空 hasValue；`xrtCoResume` 失败或 coroutine dead 时标记 done；如果本轮产生 value 则返回 true。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:77`、`xl6_cgen_generator.c:85`、`xl6_cgen_generator.c:90` |
| 对应生成的 C 代码 | `gen->hasValue = 0; if (!xrtCoResume(gen->co)) { gen->done = 1; return false; } return gen->hasValue;` |
| 中间形态伪代码 | `resume coroutine; return whether yield produced value` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护 done/hasValue。 |
| 额外依赖的封装代码 | `xl6_generator_next` helper。 |

## CGEN-GEN-006 generator value_into runtime

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `for v in gen ... end` 中取当前值 |
| 生成规则描述 | `xl6_generator_value_into` 校验 generator、out、value、hasValue 和 valueSize，然后把当前 value 缓冲复制到输出地址。出错时设置 `GeneratorError`。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:94` |
| 对应生成的 C 代码 | `memcpy(outValue, gen->value, valueSize);` |
| 中间形态伪代码 | `out = copy(gen.value)` |
| 实现同等功能手写的 C 代码 | `xl6_generator_value_into(gen, &v, sizeof(T), file, line, col);` |
| 额外依赖的封装代码 | `xl6_generator_value_into` helper。 |

## CGEN-GEN-007 generator 函数识别

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func seq() generator<int>` |
| 生成规则描述 | 普通函数声明如果唯一返回类型是 `generator<T>`，会被识别为 generator 函数并走 generator 专用生成，而不是普通函数生成。`generator<T>` 的 item type 通过单泛型参数克隆得到。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:94`、`xl6_cgen_generator.c:104`、`xl6_cgen_generator.c:109` |
| 对应生成的 C 代码 | 对外函数返回 `XL6_Generator*`，内部创建 coroutine worker。 |
| 中间形态伪代码 | `if returns == generator<T>: emitGeneratorFunc(T)` |
| 实现同等功能手写的 C 代码 | `XL6_Generator* seq(void) { return xl6_generator_create(sizeof(long long), 0, worker); }` |
| 额外依赖的封装代码 | generator runtime、worker、可选 frame。 |

## CGEN-GEN-008 generator frame 声明

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func seq(int start, int stop) generator<int>` |
| 生成规则描述 | generator 函数有参数时生成 `XL6_GenFrame_<func>` 结构体，字段保存每个参数的 C 类型和值。无参数时不生成 frame typedef。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:133`、`xl6_cgen_generator.c:139`、`xl6_cgen_generator.c:151` |
| 对应生成的 C 代码 | `typedef struct XL6_GenFrame_seq { long long start; long long stop; } XL6_GenFrame_seq;` |
| 中间形态伪代码 | `frame = {params...}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要把函数参数复制到 coroutine 可访问的堆 frame。 |
| 额外依赖的封装代码 | frame typedef。 |

## CGEN-GEN-009 generator worker

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | generator 函数体 |
| 生成规则描述 | worker 签名为 `static void worker(void* __xl6_arg)`。它把参数转换为 `XL6_Generator*`，从 generator 中取 frame，复制参数为局部变量，设置当前 return/generator/error 上下文，然后生成函数体。函数体结束后执行 cleanup 并落到错误/结束标签返回。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:165`、`xl6_cgen_generator.c:190`、`xl6_cgen_generator.c:196`、`xl6_cgen_generator.c:220`、`xl6_cgen_generator.c:232` |
| 对应生成的 C 代码 | `static void xl_gen_worker_seq(void* arg) { XL6_Generator* gen = (XL6_Generator*)arg; Frame* frame = (Frame*)xl6_generator_frame(gen); long long start = frame->start; ... }` |
| 中间形态伪代码 | `worker(gen): load frame; run body; cleanup; return` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在 coroutine entry 内恢复 frame 和参数。 |
| 额外依赖的封装代码 | worker 函数。 |

## CGEN-GEN-010 generator 对外函数

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func seq(int n) generator<int>` |
| 生成规则描述 | 对外函数先生成 frame 并复制参数，然后调用 `xl6_generator_create(sizeof(T), frame, worker)` 返回 generator。frame 分配失败返回 0。无参数时 frame 传 0。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:253`、`xl6_cgen_generator.c:270`、`xl6_cgen_generator.c:281`、`xl6_cgen_generator.c:300` |
| 对应生成的 C 代码 | `Frame* frame = xrtMalloc(sizeof(Frame)); frame->n = n; return xl6_generator_create(sizeof(long long), frame, xl_gen_worker_seq);` |
| 中间形态伪代码 | `frame = heapCopy(params); return createGenerator(sizeof(item), frame, worker)` |
| 实现同等功能手写的 C 代码 | 与生成代码基本一致。 |
| 额外依赖的封装代码 | frame typedef、worker、generator runtime。 |

## CGEN-GEN-011 yield 语句

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `yield value` |
| 生成规则描述 | `yield` 只能位于 generator 函数体内，且必须有表达式。生成代码把表达式放进 `&(T){expr}` 复合字面量，传给 `xl6_generator_yield_copy(currentGenerator, ptr, sizeof(T), file, line, column)`。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:314`、`xl6_cgen_generator.c:321`、`xl6_cgen_generator.c:335` |
| 对应生成的 C 代码 | `xl6_generator_yield_copy(__xl6_gen, &(long long){value}, sizeof(long long), "", 12, 4);` |
| 中间形态伪代码 | `yield_copy(generator, valueAsItemType)` |
| 实现同等功能手写的 C 代码 | `long long tmp = value; xl6_generator_yield_copy(gen, &tmp, sizeof tmp, file, line, col);` |
| 额外依赖的封装代码 | `xl6_generator_yield_copy` helper。 |

## CGEN-GEN-012 generator for-in 消费端

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `for item in seq(10) ... end` |
| 生成规则描述 | for-in 消费端在 `xl6_cgen_stmt_loop.c` 中实现：只支持一个迭代变量；把 iterable 表达式生成到 `XL6_Generator*` 临时值，循环调用 `xl6_generator_next`，每轮用 `xl6_generator_value_into` 拷贝当前值。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_loop.c:754`、`xl6_cgen_stmt_loop.c:789`、`xl6_cgen_stmt_loop.c:811` |
| 对应生成的 C 代码 | `while (xl6_generator_next(gen, "", line, col)) { T item = default(T); xl6_generator_value_into(gen, &item, sizeof(T), "", line, col); ... }` |
| 中间形态伪代码 | `while next(gen): item = valueInto<T>(gen); body` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行循环 next/value_into 并销毁 generator。 |
| 额外依赖的封装代码 | generator cleanup。 |

## 本文件观察到的优化风险

| 编号 | 观察 |
| --- | --- |
| RISK-GEN-001 | generator yield 当前通过 `memcpy` 复制 `T`，对含 function 字段的 class 或需要 retain/release 的复杂类型还缺少精细生命周期策略。 |
| RISK-GEN-002 | generator frame 销毁只是 `xrtFree(frame)`，参数若包含 function/class/custom 资源不会自动析构。 |
| RISK-GEN-003 | generator worker 错误出口只是 `return`，没有把错误对象挂到 generator 状态；消费端只能通过 active error 机制感知。 |
| RISK-GEN-004 | generator for-in 只支持一个目标变量，不支持 `(index, value)` 形式。 |
