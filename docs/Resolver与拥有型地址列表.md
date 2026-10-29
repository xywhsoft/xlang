# Resolver 与拥有型地址列表

本批为已批准网络接入中的 Resolver／Future 增量。复用 XRT 原生解析器，
不新增 DNS 算法、每请求线程、生成 C 包装函数体或专用异步结果 carrier。

## 地址与结果

`NetAddressList(addresses)` 接受非空 `array<NetAddress>`，验证完整类布局与
每个原生句柄后，复制 POD 地址并按完整端点去重。`NetAddressList(host, port=0,
family=0)` 为同步解析；`net.resolveAll` 使用同一入口。host 必须为完整、非空、
有效 UTF-8 文本，内嵌 NUL checked 拒绝，不将后缀静默丢弃。

- `count()`／`length`、`at(index)`／`[index]`、`text(index)`、`family(index)`、`port(index)`。
- `clone()` 持有不可变原生列表的另一个真实引用。
- `withPort(port)` 创建新列表，并在端口替换后再次去重，不修改原列表。
- `at` 返回独立拥有的 NetAddress；无裸指针、原生 adopt 或可变空列表入口。
- family 只能为 0／4／6，port 只能为 0—65535，索引越界报 RangeError。

## 受管理服务

```xlang
auto options = NetResolverOptions().withWorkers(2).withCacheEntries(256)
auto resolver = NetResolver(options)
auto addresses = await resolver.resolveAsync("127.0.0.1", 8080, 4)
resolver.close()
assert resolver.wait(5000)
assert addresses[0].port == 8080
```

`NetResolverOptions` 不可变，with 方法返回新值。公开 workers、requestLimit、
queryLimit、cacheEntries、hostLimit、threadStackBytes、successTtlMillis、
failureTtlMillis。对应 with 方法同名，线程栈使用 `withThreadStack`。
容量按照真实 size_t 范围校验；workers／请求／查询／host 上限必须正数，缓存
项数、线程栈和 TTL 允许零。TTL 的毫秒转原生微秒在乘法前校验溢出；原生正的
亚毫秒值向上取整展示。原生平台对工作线程／栈的额外限制仍由原生构造 checked 报错。

`configuration()` 返回独立配置，`snapshot()` 返回独立只读 NetResolverStats，
`clearCache()` 清空已有缓存。统计含 workers、submitted、rejected、cacheHits、
cacheMisses、coalesced、queriesStarted、resolved、failed、cancelled、outstanding、
activeQueries、queuedQueries、runningQueries、readyCallbacks、cachedResults，
保持 uint64 完整宽度。

服务采用已有 ExecutionSource／Resource 家族：close 拒绝新查询，wait 在独占
控制期间推进原生 TryDestroy，只有真实线程及 TLS 退出后才消费原生创建者槽。
wait 需先 close；默认 -1 等待，0 单次检查，其他负数 checked 拒绝。重复 wait 可行。
配置与已取得的结果／统计继续有效；服务原生对象销毁后不能再取新的统计。
retryCleanup 用于重试已记录的退役请求，不把历史错误伪装成成功。最后语言引用
消失后由所属源实际排空／退役；不在任意析构线程强制 join 或制造完成状态。

## 操作与取消

`query(host, family=0)` 返回无公开构造器的 NetResolveOperation：

- `done`、`cancel()`、`waitAsync() as future<bool>`、`result(port=0)`。
- waitAsync 成功结果为 true；失败、取消、关闭保留既有 Future 语义。
- waitAsync 直接订阅原生 Future，无新线程／协程栈／阻塞等待；取消该等待会请求取消原操作。
- result 必须在操作就绪后读取，成功列表由调用者独立拥有；端口零直接 retain，其他端口复制映射。
- `resolveAsync` 以普通语言 async 方法组合 waitAsync 和 result。
- `resolveAsyncCancel(host, token, port=0, family=0)` 使用既有注册／finally close 合同；
  已取消 token 不提交查询。取消是协作请求，不承诺撤销已完成结果或阻断系统名称查询。

`net.resolveAsync` 与 `net.resolverStats` 保留便利名称，共享一个按需注入、普通
模块全局持有的 NetResolver；不用不被追踪的 TLS 单例，也不每次创建池。

## ABI、拥有图与裁剪

61 个闭合声明合同明确错误、效果、阻塞、结果拥有方、类数组布局、Future<bool>
元素和 ExecutionSource 服务。适配实现驻留 runtime；Cgen 只输出通用声明、
调用、拥有清理。旧列表／Resolver 统计／异步解析生成包装已删除。

列表、配置、统计、受管理服务、操作及其拥有域独立选择。原生 Resolver 域作为
唯一显式能力依赖，避免全局目录重复登记。直接 waitAsync 不引入 queued callback
executor；配置／列表独立使用不引入 ExecutionSource，服务不附带 NetEngine／TLS。

XRT 增加三个只读 quiescence 策略：结果就绪后仍在回调或线程退出收尾的真实节点
报告 BUSY，而不是未知节点。Busy false 仍须通过严格 Count／Trace／策略身份
检查，未放宽任何引用计数，也未用延迟等待掩盖不支持的拥有边。

## 验收边界

仅离线值／ABI／拥有图、受控真实回调与 TLS 退出，以及数字地址快速路径。
没有外部 DNS、Socket 通信、探测、扫描或网络安全性质测试。测试宿主必须分别
执行语言全局终结和 checked ExecutionSource 物理退役，不能在源线程尚存时比较堆账本。

本批不宣称完整网络、P0／P1 shipping 联合验收或 P3—P7 完成，也不更新 release／VFS。
### 2026-10-21 增量证据

独立 XRT 模块化／单头 Windows 40／Linux 50 阶段，驻留六闭包 Windows
76／Linux 88 阶段通过，含 GCC／TCC O0／O2 和 Linux ASan／UBSan。XRT
前置提交 `1d07845e` 只包含精确 quiescence，未混入原生 memory／file_root 改动。

工作区源码驱动 Windows 688／Linux 687 单元；两端语言各 141 阶段通过，含
11 用例、10 负例、JIT／模块／严格 C／独立 AOT、导入及裁剪检查。每级拒绝
434 项损坏调用合同，O0／O2 各完成 188 个语言对象分配失败前缀并核对全部账本。
独立默认服务 AOT 额外重复四次退出。1358 个编译／镜像路径与暂存内容一致，
用户原有 IR 空白行单独保留。

证据目录为 `F:/xlang-validation/five-groups-20261020/p2-resolver`，最终分别为
`native-win-r1`／`native-linux-r1`、`runtime-win-r7`／`runtime-linux-r2`、
`source-win-r6`／`source-linux-r3`、`language-win-r2`／`language-linux-r2`。
早期 r1 语言失败报告完整保留；修复实际退役 BUSY 合同，并修正测试宿主和
负例断言后，冻结新输入统一复测，没有改写旧证据或用睡眠掩盖退出问题。
