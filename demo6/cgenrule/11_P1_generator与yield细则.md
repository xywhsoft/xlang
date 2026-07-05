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
| 生成规则描述 | `xl6_generator_destroy` 依次销毁 coroutine、释放 value 缓冲、释放 generator 内保存的错误对象、释放 frame、释放 generator 本体。该函数由 cleanup 机制调用。第一版语义层已限制 generator 参数只能使用 bool、整数、浮点、time、point 等可按值复制类型，因此 frame 释放不需要额外析构资源字段。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:58` |
| 对应生成的 C 代码 | `if (gen->co) xrtCoDestroy(gen->co); xrtFree(gen->value); xl6_error_free(gen->error); xrtFree(gen->frame); xrtFree(gen);` |
| 中间形态伪代码 | `destroy(co); free(value); free(storedError); free(frame); free(gen)` |
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
| 生成规则描述 | `xl6_generator_next` 检查空 generator、已有存储错误、done、co 是否存在；每次 resume 前清空 hasValue；`xrtCoResume` 失败或 resume 后发现 active error 时，把错误转存到 generator，再重新抛给消费端；如果本轮产生 value 则返回 true。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:84`、`xl6_cgen_generator.c:90`、`xl6_cgen_generator.c:95`、`xl6_cgen_generator.c:98`、`xl6_cgen_generator.c:99` |
| 对应生成的 C 代码 | `if (storedError) reraise; gen->hasValue = 0; if (!xrtCoResume(gen->co)) { storeActiveError(gen); return false; } if (xl6_error_has()) { storeActiveError(gen); reraise; return false; } return gen->hasValue;` |
| 中间形态伪代码 | `resume coroutine; if worker failed: gen.error = activeError; throw gen.error; else return whether yield produced value` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护 done/hasValue/error，并在消费端重新抛出 worker 内部错误。 |
| 额外依赖的封装代码 | `xl6_generator_next` helper。 |

## CGEN-GEN-006 generator value_into runtime

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `for v in gen ... end` 中取当前值 |
| 生成规则描述 | `xl6_generator_value_into` 先重抛 generator 内保存的错误，再校验 generator、out、value、hasValue 和 valueSize，然后把当前 value 缓冲复制到输出地址。出错时设置 `GeneratorError`。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:105`、`xl6_cgen_generator.c:107` |
| 对应生成的 C 代码 | `if (storedError) reraise; memcpy(outValue, gen->value, valueSize);` |
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
| 生成规则描述 | generator 函数有参数时生成 `XL6_GenFrame_<func>` 结构体，字段保存每个参数的 C 类型和值。无参数时不生成 frame typedef。语义检查会拒绝 function、class、custom、string 等需要额外所有权处理的参数类型，避免生成只能 `xrtFree(frame)` 却无法释放资源字段的 frame。 |
| 生成器文件名和行号 | `xl6_sem.c:7995`、`xl6_cgen_generator.c:133`、`xl6_cgen_generator.c:139`、`xl6_cgen_generator.c:151` |
| 对应生成的 C 代码 | `typedef struct XL6_GenFrame_seq { long long start; long long stop; } XL6_GenFrame_seq;` |
| 中间形态伪代码 | `frame = {params...}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要把函数参数复制到 coroutine 可访问的堆 frame。 |
| 额外依赖的封装代码 | frame typedef。 |

## CGEN-GEN-009 generator worker

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | generator 函数体 |
| 生成规则描述 | worker 签名为 `static void worker(void* __xl6_arg)`。它把参数转换为 `XL6_Generator*`，从 generator 中取 frame，复制参数为局部变量，设置当前 return/generator/error 上下文，然后生成函数体。函数体结束后执行 cleanup；如果走错误标签，则把 active error 转存到 generator 再返回，让消费端 `next/value_into` 能重新抛出同一个错误对象。 |
| 生成器文件名和行号 | `xl6_cgen_generator.c:246`、`xl6_cgen_generator.c:256`、`xl6_cgen_generator.c:259` |
| 对应生成的 C 代码 | `static void xl_gen_worker_seq(void* arg) { XL6_Generator* gen = (XL6_Generator*)arg; Frame* frame = (Frame*)xl6_generator_frame(gen); long long start = frame->start; ... }` |
| 中间形态伪代码 | `worker(gen): load frame; run body; cleanup; return` |
| 实现同等功能手写的 C 代码 | 手写 C 需要在 coroutine entry 内恢复 frame 和参数。 |
| 额外依赖的封装代码 | worker 函数。 |

## CGEN-GEN-010 generator 对外函数

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `func seq(int n) generator<int>` |
| 生成规则描述 | 对外函数先生成 frame 并复制参数，然后调用 `xl6_generator_create(sizeof(T), frame, worker)` 返回 generator。frame 分配失败返回 0。无参数时 frame 传 0。由于语义层已拒绝携带资源所有权的 generator 参数，该路径只需要按值复制 frame 字段。 |
| 生成器文件名和行号 | `xl6_sem.c:7995`、`xl6_cgen_generator.c:253`、`xl6_cgen_generator.c:270`、`xl6_cgen_generator.c:281`、`xl6_cgen_generator.c:300` |
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
| xlang 写法 | `for item in seq(10) ... end`、`for index, item in seq(10) ... end` |
| 生成规则描述 | for-in 消费端在 `xl6_cgen_stmt_loop.c` 中实现：支持一个或两个迭代变量；把 iterable 表达式生成到 `XL6_Generator*` 临时值，循环调用 `xl6_generator_next`，每轮用 `xl6_generator_value_into` 拷贝当前值；双目标形式额外生成 0 基 index 计数器，并在循环体和 `continue` cleanup 后递增；循环体内和 `while(next)` 退出后都执行异常传播检查，因此 generator worker 内部错误可以被外层 `try/catch` 捕获。 |
| 生成器文件名和行号 | `xl6_cgen_stmt_loop.c:787`、`xl6_cgen_stmt_loop.c:843`、`xl6_cgen_stmt_loop.c:846`、`xl6_cgen_stmt_loop.c:857`、`xl6_cgen_stmt_loop.c:873`、`xl6_cgen_stmt_loop.c:883` |
| 对应生成的 C 代码 | `long long genIndex = 0; while (xl6_generator_next(gen, "", line, col)) { long long index = genIndex; T item = default(T); xl6_generator_value_into(gen, &item, sizeof(T), "", line, col); if (xl6_error_has()) goto catch; ...; genIndex += 1; } if (xl6_error_has()) goto catch;` |
| 中间形态伪代码 | `index = 0; while next(gen): item = valueInto<T>(gen); propagateError(); body; index += 1; propagateError()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自行循环 next/value_into 并销毁 generator。 |
| 额外依赖的封装代码 | generator cleanup。 |

## 本文件观察到的优化风险

| 编号 | 观察 |
| --- | --- |
| RISK-GEN-001 | 已收口：第一版 `generator<T>` 语义层只允许 bool/int/float/time/point 等可按值复制元素；string、class、function 等复杂所有权类型由 `sem_generator_string_element.xl`、`sem_generator_class_element.xl`、`sem_generator_function_element.xl` 负例和集中诊断断言锁住，因此不会进入当前 `memcpy` yield 路径。 |
| RISK-GEN-002 | 已收口：第一版 generator 参数语义层只允许 bool、整数、浮点、time、point 等可按值复制类型；function、class/custom 等资源参数由 `sem_generator_function_param.xl`、`sem_generator_class_param.xl` 负例和集中诊断断言锁住，因此当前 `xrtFree(frame)` 销毁路径不会遗漏 frame 内资源析构。 |
| RISK-GEN-003 | 已收口：generator 结构体持有 `XL6_Error* error`，worker 错误出口通过 `xl6_generator_store_active_error(__xl6_gen)` 转存 active error；`xl6_generator_next` 和 `xl6_generator_value_into` 通过 `xl6_generator_raise_stored_error` 重新抛出，for-in 消费端在 `while(next)` 退出后补充异常传播检查。`cgen_generator.xl` 和 feature suite `11_concurrency.xl` 覆盖 yield 后 throw 且外层 `try/catch` 捕获的路径。 |
| RISK-GEN-004 | 已收口：generator for-in 支持 `(index, value)` 双目标形式，第一个目标为 0 基 `int` index，第二个目标为 `generator<T>` 的元素类型；`cgen_generator_two_targets.xl` 覆盖 check/run/build/cgen、break/continue 后 index 递增和生成形态守门。 |

## 本轮补充：首次 yield 前异常传播

本轮不新增 `CGEN-*` 规则编号，只补齐 `CGEN-GEN-005`、`CGEN-GEN-009`、`CGEN-GEN-012` 的验证边界：

- 普通 generator：`cgen_generator.xl` 新增 `brokenEarly()`，在首次 `yield` 前抛出 `generator boom before yield`，外层 `try/catch` 捕获原始错误。
- async generator：`cgen_async_generator.xl` 新增 `brokenEarly()`，在内部 `await` 成功后、首次 `yield` 前抛出 `async generator boom before yield`。
- 生成形态：`test.bat` 锁定 `xl_gen_worker_brokenEarly` 与两条 early-error 诊断字符串。

该补充确认 worker 在没有设置 `hasValue` 的情况下仍能通过 `xl6_generator_store_active_error` 转存 active error，消费端 `xl6_generator_next` 通过 `xl6_generator_raise_stored_error` 重抛，`for-in` 在 `while(next)` 退出后继续执行异常传播检查。

## 本轮补充：空产出正常完成

本轮不新增 `CGEN-*` 规则编号，只补齐 `CGEN-GEN-005`、`CGEN-GEN-009`、`CGEN-GEN-012` 的完成态验证：

- 普通 generator：`cgen_generator.xl` 新增 `emptyNums()`，函数体正常结束但不触发 `yield`，`for-in` 循环体不执行。
- async generator：`cgen_async_generator.xl` 新增 `emptyStream()`，内部 `await` 成功后正常结束但不触发 `yield`，`for await` 循环体不执行。
- 生成形态：`test.bat` 锁定 `xl_gen_worker_emptyNums` 与 `xl_gen_worker_emptyStream`。

该补充确认 `xl6_generator_next` 在 coroutine 已结束、`hasValue == 0` 且无 active error 时，把结果解释为正常迭代结束，而不是错误或伪值产出。

## 本轮补充：保存型 generator 空句柄与重复消费

本轮不新增 `CGEN-*` 规则编号，只补齐 `CGEN-GEN-012` 消费端的 use-after-move 语义边界：

- 普通 generator：`cgen_generator.xl` 新增 `generator<int> consumedGen = nums(1)`，首次 `for item in consumedGen` 正常得到 `1`，第二次 `for item in consumedGen` 必须进入 `catch` 并匹配 `iterate null generator`。
- 空句柄 generator：同一用例新增未初始化的 `generator<int> missingGen`，直接 `for item in missingGen` 必须进入同一 `iterate null generator` 诊断路径。
- async generator：`cgen_async_generator.xl` 采用同一单次消费规则，`consumedAsync` 首次 `for await` 成功，二次 `for await` 和未初始化 `missingAsync` 均抛出 `iterate null generator`。
- 生成形态：`test.bat` 锁定 `consumedGen` / `consumedAsync` 首次消费时 move 到 `__xl6_gen_iter_N` 并把源变量置 `0`，二次消费时从空源变量创建迭代器并进入 `catch`；未初始化变量必须生成为 `XL6_Generator* missingGen = 0;` / `missingAsync = 0;`。

该补充确认 `generator<T>` 保存值不是可重复遍历容器，而是单次消费句柄。`for in` / `for await` 会接管句柄所有权；被接管后的源变量保持空句柄，再次迭代必须报错。这一规则让所有权模型保持简单，也避免重复销毁底层 coroutine frame。

## 本轮补充：yield 非 generator 函数体负例

本轮不新增 `CGEN-*` 规则编号，只补齐 `CGEN-GEN-011` 的语义守门边界：

- 顶层：既有 `sem_yield_outside_generator.xl` 覆盖 `yield 1` 不在任何函数体内。
- 普通函数：新增 `sem_yield_sync_func.xl`，覆盖 `func bad() as int` 体内误写 `yield 1`。
- async 非 generator 函数：新增 `sem_yield_async_non_generator.xl`，覆盖 `async func bad() as int` 体内误写 `yield 1`。
- 生成形态：这些负例均在语义阶段终止，不进入 `xl6_generator_yield_copy(...)` 生成路径。

该补充确认 `yield` 只属于返回 `generator<T>` 的函数体。普通 async 函数即使未来和 generator 共享更多 runtime，也不能因为带有 `async` 标记就允许 `yield`。

## 本轮补充：generator string/var 类型边界负例

本轮不新增 `CGEN-*` 规则编号，只补齐 `RISK-GEN-001` 和 `RISK-GEN-002` 的类型矩阵：

- 元素类型：既有 `sem_generator_string_element.xl`、`sem_generator_class_element.xl`、`sem_generator_function_element.xl` 覆盖 string/class/function；新增 `sem_generator_var_element.xl` 覆盖 `generator<var>`。
- 参数类型：既有 `sem_generator_class_param.xl`、`sem_generator_function_param.xl` 覆盖 class/function；新增 `sem_generator_string_param.xl` 和 `sem_generator_var_param.xl` 覆盖 string/var。
- 生成形态：这些负例均在语义阶段终止，不进入 generator frame 复制、`xl6_generator_yield_copy(...)` 或 worker frame 销毁路径。

该补充确认第一阶段 generator 的元素和参数都保持“可按值复制类型”边界。`string` 和 `var` 虽然是语言基础类型，但它们涉及动态所有权和资源生命周期，不能在没有 frame 资源管理协议前进入 generator worker。
